#include "zba_auth.h"
#include "zba_util.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_rom_md5.h>
#include <esp_system.h>
#include <esp_tls_crypto.h>
#include <esp_wifi.h>
#include <esp_wpa.h>
#include <nvs_flash.h>
#include <string.h>
#include <sys/param.h>
#include "zba_config.h"

DEFINE_ZBA_MODULE(zba_auth);

// For Basic auth
// 32 byte user + 64 byte password + ':' = 97 bytes
// base64 encoding gives ((97)*4 + 2)/3 = 130.
// + 6 bytes for "BASIC " header = 136, plus a null and pad = 138.
// ZBA_AUTH_BASIC_BUFFER_SIZE (((((kMaxPasswordLen + kMaxUserLen + 2) * 4) + 2) / 3) + 6 + 2)

// Min buffer for request headers - if we set this as a minimum for our buffer we can reuse it to
// send our challenge as well which should be < 256 bytes.
#define ZBA_AUTH_REQUEST_SIZE 256

#define kAUTH_REALM          "Zebral"
#define kBASIC_REALM_STRING  "Basic realm=\"" kAUTH_REALM "\""
#define kDIGEST_REALM_STRING "Digest realm=\"" kAUTH_REALM "\", "

typedef struct
{
  // Buffer for our stored credentials
  uint8_t basic_buf[ZBA_AUTH_REQUEST_SIZE];
  size_t basic_len;

} zba_auth_state_t;

static zba_auth_state_t auth_state = {.basic_buf = {0}, .basic_len = 0};

void zba_md5_vector(void *src, size_t len, uint8_t *out)
{
  md5_context_t context;
  esp_rom_md5_init(&context);
  esp_rom_md5_update(&context, src, len);
  esp_rom_md5_final(out, &context);
}

uint8_t zba_char_to_nibble(char ascii)
{
  if ((ascii >= 'A') && (ascii <= 'F'))
  {
    return ascii - 'A' + 10;
  }
  if ((ascii >= 'a') && (ascii <= 'f'))
  {
    return ascii - 'a' + 10;
  }
  if ((ascii >= '0') && (ascii <= '9'))
  {
    return ascii - '0';
  }
  ZBA_ERR("Invalid character %c (%d) in hex", ascii, (int)ascii);
  return 0;
}

uint8_t zba_hex_to_byte(const char *asciiHex)
{
  uint8_t value;
  value = zba_char_to_nibble(asciiHex[0]) << 4;
  value |= zba_char_to_nibble(asciiHex[1]);
  return value;
}

zba_err_t zba_auth_init()
{
  char credentials[kMaxPasswordLen + kMaxUserLen + 2] = {0};

  zba_err_t result = ZBA_OK;
  // ...
  strcpy(credentials, kAdminUser);
  strcat(credentials, ":");
  zba_config_get_device_pwd(credentials + strlen(credentials), kMaxPasswordLen);

  // Get the size of the encoding
  size_t n = 0;
  esp_crypto_base64_encode(NULL, 0, &n, (uint8_t *)credentials, strlen(credentials));
  strcpy((char *)auth_state.basic_buf, "Basic ");

  // Encode to auth_state.basic_buf. basic_len is set to encoding length
  // (w/o the 6 chars for "Basic")
  esp_crypto_base64_encode(auth_state.basic_buf + 6, n, &auth_state.basic_len,
                           (uint8_t *)credentials, strlen(credentials));

  auth_state.basic_len += 6;  // Add header length

  ZBA_MODULE_INITIALIZED(zba_config) = result;
  return result;
}

zba_err_t zba_auth_deinit()
{
  zba_err_t deinit_error = ZBA_OK;
  // ...
  memset(&auth_state, 0, sizeof(auth_state));
  ZBA_MODULE_INITIALIZED(zba_config) =
      (ZBA_OK == deinit_error) ? ZBA_MODULE_NOT_INITIALIZED : deinit_error;

  return deinit_error;
}

// Basic auth check
zba_err_t zba_auth_basic_check_web(httpd_req_t *req)
{
  char *auth_buf = NULL;
  size_t buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;

  if (buf_len > 1)
  {
    auth_buf = calloc(1, buf_len);
    if (!auth_buf)
    {
      ZBA_ERR("Not enough memory for auth header (%d)", buf_len);
      return ZBA_OUT_OF_MEMORY;
    }

    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_buf, buf_len) == ESP_OK)
    {
      if (0 == strncmp(auth_buf, (const char *)auth_state.basic_buf, auth_state.basic_len))
      {
        ZBA_LOG("Authentication successful.");
        free(auth_buf);
        return ZBA_OK;
      }
    }
  }
  ZBA_LOG("Sending unauthorized request");
  httpd_resp_set_status(req, "401 UNAUTHORIZED");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");
  httpd_resp_set_hdr(req, "WWW-Authenticate", kBASIC_REALM_STRING);
  httpd_resp_send(req, NULL, 0);
  free(auth_buf);
  return ZBA_ERROR;
}

// Digest auth check
zba_err_t zba_auth_digest_check_web(httpd_req_t *req)
{
  uint8_t authHa1[16];
  uint8_t authHa2[16];
  uint8_t authResponse[16];
  char nonce_req[33]  = {0};
  char opaque_req[33] = {0};
  char nonce[33];
  char pwd[kMaxPasswordLen + 1] = {0};

  char *auth_buf    = NULL;
  char *uri         = NULL;
  char *responseVal = NULL;
  char *valPtr      = NULL;
  size_t valLen     = 0;

  size_t req_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
  size_t buf_len = req_len;
  for (;;)
  {
    // Go ahead and allocate enough to reuse the buffer to send the request
    // on failed auth.
    buf_len  = ZBA_MAX(buf_len, ZBA_AUTH_REQUEST_SIZE);
    auth_buf = calloc(1, buf_len);

    // If the request length was empty, we still want that buffer! but break now.
    if (req_len <= 1) break;

    if (!auth_buf)
    {
      ZBA_ERR("Not enough memory for auth header (%d)", buf_len);
      // TODO add server error response.
      // We don't break here because we don't have the ram to generate the challenge.
      return ZBA_OUT_OF_MEMORY;
    }

    // Bail if we can't get the auth string
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_buf, buf_len) != ESP_OK)
    {
      break;
    }

    // Oooh, this gets into the weeds a bit.
    // Looks like we're on our own for parsing, too.
    // For a proper server, we should have a formal language
    // parser, or at least regexes. But for this? ....

    // Only supporting admin user right now
    if (!zba_auth_get_value(auth_buf, "username", &valPtr, &valLen)) break;
    if (0 != strncasecmp(kAdminUser, valPtr, valLen)) break;

    // Check our realm
    if (!zba_auth_get_value(auth_buf, "realm", &valPtr, &valLen)) break;
    if (0 != strncmp(kAUTH_REALM, valPtr, valLen)) break;

    // Right now ignoring QOP
    // {TODO} Get RFC2069 going, then do 2617
    // if (!zba_auth_get_value(auth_buf, "qop"), &valPtr, &valLen))
    // if (!zba_auth_get_value(auth_buf, "nc", &valPtr, &valLen)) break;
    // if (!zba_auth_get_value(auth_buf, "cnonce", &valPtr, &valLen)) break;

    // Get our password
    zba_config_get_device_pwd(pwd, kMaxPasswordLen);

    // Generate first hash
    if (!zba_auth_gen_ha1("admin", kAUTH_REALM, pwd, authHa1)) break;

    // URI may be quite large, so alloc ram for it and copy it out.
    if (!zba_auth_get_value(auth_buf, "uri", &valPtr, &valLen)) break;
    uri = calloc(1, valLen + 1);
    if (!uri) break;
    strncpy(uri, valPtr, valLen);
    uri[valLen] = 0;

    // {TODO} Right now, we only support get.
    // Generate second hash.
    if (!zba_auth_gen_ha2("GET", uri, authHa2)) break;

    if (!zba_auth_get_value(auth_buf, "nonce", &valPtr, &valLen)) break;
    strncpy(nonce, valPtr, valLen);
    nonce[32] = 0;
    // {TODO} check and see if we've issued nonce or if they just made it up.

    // Gen response from hashes
    if (!zba_auth_gen_response(authHa1, authHa2, nonce, authResponse)) break;

    // Get current response
    if (!zba_auth_get_value(auth_buf, "response", &responseVal, &valLen)) break;

    // Convert each byte in the response string to binary and check against our
    // calculated value.
    bool responseMatch = true;
    for (int i = 0; i < 16; ++i)
    {
      uint8_t val = zba_hex_to_byte(responseVal + i * 2);
      if (authResponse[i] != val)
      {
        responseMatch = false;
        break;
      }
    }

    // Bail if it failed.
    if (!responseMatch)
    {
      ZBA_ERR("Response did not match binary.");
      break;
    }

    // With all that good, do any additional checks on opaque.
    if (!zba_auth_get_value(auth_buf, "opaque", &valPtr, &valLen)) break;
    // {TODO} validate opaque

    ZBA_LOG("Authentication successful.");
    free(uri);
    free(auth_buf);
    return ZBA_OK;
  }

  // If we got here w/o returning, there was a failure.
  ZBA_LOG("Sending unauthorized request");
  httpd_resp_set_status(req, "401 UNAUTHORIZED");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");

  zba_auth_get_nonce(nonce_req);
  zba_auth_get_opaque(opaque_req);

  //"qop=\"auth\",\n\t"  // {TODO}
  snprintf(auth_buf, buf_len, kDIGEST_REALM_STRING " nonce=\"%s\", opaque=\"%s\"", nonce_req,
           opaque_req);

  httpd_resp_set_hdr(req, "WWW-Authenticate", auth_buf);

  httpd_resp_send(req, NULL, 0);

  // clean up
  if (uri) free(uri);
  free(auth_buf);
  return ZBA_ERROR;
}

void zba_auth_get_nonce(char *buffer)
{
  uint8_t buf[16];
  esp_fill_random(buf, 16);
  for (int i = 0; i < 16; ++i)
  {
    sprintf(buffer + i * 2, "%02x", buf[i]);
  }
}

void zba_auth_get_opaque(char *buffer)
{
  uint8_t buf[16];
  esp_fill_random(buf, 16);
  for (int i = 0; i < 16; ++i)
  {
    sprintf(buffer + i * 2, "%02x", buf[i]);
  }
}

bool zba_auth_get_value(const char *buf, const char *key, char **resPtr, size_t *resLen)
{
  // {TODO} this should really be regex, or something more robust.
  // At least a tokenizer.

  // Find matching string to key.
  char *curPtr = strstr(buf, key);
  if (!curPtr) return false;

  // Find key name in string
  curPtr += strlen(key);
  // Make sure it has =" for the value following it
  if (*curPtr != '=') return false;
  curPtr++;
  if (*curPtr != '\"') return false;
  curPtr++;

  // Mark start of string
  if (resPtr)
  {
    *resPtr = curPtr;
  }
  // If we don't have a size out value, we're done.
  if (!resLen) return true;

  // step until the end quote to find the length
  *resLen = 0;
  while ((*curPtr != 0) && (*curPtr != '"'))
  {
    curPtr++;
    (*resLen)++;
  }

  return true;
}

bool zba_auth_gen_ha1(const char *user, const char *realm, const char *pwd, uint8_t *ha1md5)
{
  char *buf = NULL;
  memset(ha1md5, 0, 16);

  size_t len = strlen(user) + strlen(realm) + strlen(pwd) + 3;
  buf        = calloc(1, len);
  if (!buf)
  {
    ZBA_ERR("Failed allocating memory for HA1");
    return false;
  }

  sprintf(buf, "%s:%s:%s", user, realm, pwd);

  zba_md5_vector(buf, len - 1, ha1md5);
  free(buf);
  return true;
}

bool zba_auth_gen_ha2(const char *method, const char *uri, uint8_t *ha2md5)
{
  char *buf = NULL;
  memset(ha2md5, 0, 16);

  size_t len = strlen(method) + strlen(uri) + 2;
  buf        = calloc(1, len);
  if (!buf)
  {
    ZBA_ERR("Failed allocating memory for HA2");
    return false;
  }

  sprintf(buf, "%s:%s", method, uri);
  zba_md5_vector(buf, len - 1, ha2md5);
  free(buf);
  return true;
}

bool zba_auth_gen_response(uint8_t *ha1, uint8_t *ha2, const char *nonce, uint8_t *responseMd5)
{
  // 32+1+32+1+32, plus the null is 99.
  int i;
  char msg[100];
  int offset = 0;

  // Convert ha1 to hex text
  for (i = 0; i < 16; ++i)
  {
    sprintf(msg + i * 2, "%02x", ha1[i]);
  }
  offset += 32;

  // colon
  msg[offset] = ':';
  offset++;

  // nonce
  strncpy(msg + offset, nonce, 32);
  if (strlen(msg + offset) != 32)
  {
    ZBA_ERR("Invalid nonce length.");
    return false;
  }
  offset += strlen(msg + offset);

  // colon
  msg[offset] = ':';
  offset++;

  // Convert ha2 to hex text
  for (i = 0; i < 16; ++i)
  {
    sprintf(msg + offset + i * 2, "%02x", ha2[i]);
  }
  offset += 32;

  zba_md5_vector(msg, offset, responseMd5);
  return true;
}

zba_err_t zba_auth_check(const char *uname, const char *upwd)
{
  // {TODO} change this to a hash-and-compare rather than pull the real pwd and check.
  char pwd[kMaxPasswordLen + 1] = {0};

  zba_config_get_device_pwd(pwd, kMaxPasswordLen);

  // {TODO} Right now we only have one user - admin.
  // Not ideal, although for most use-cases probably fine.
  if (0 != strcmp(uname, kAdminUser))
  {
    ZBA_LOG("Authentication Failed.");
    return ZBA_CONFIG_NOT_AUTHED;
  }

  // If we're not configured, allow access.
  if (pwd[0] == 0)
  {
    ZBA_LOG("No password set. Please set a password now.");
    return ZBA_OK;
  }

  if (!upwd || upwd[0] == 0)
  {
    ZBA_LOG("No password given. Please provide a password.");
    return ZBA_CONFIG_NOT_AUTHED;
  }

  if (0 == strcmp(upwd, pwd))
  {
    ZBA_LOG("Authentication successful.");
    return ZBA_OK;
  }

  ZBA_LOG("Authentication failed.");
  return ZBA_CONFIG_NOT_AUTHED;
}
