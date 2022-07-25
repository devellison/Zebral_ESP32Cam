#ifndef ZEBRAL_ESP32CAM_ZBA_AUTH_H_
#define ZEBRAL_ESP32CAM_ZBA_AUTH_H_

#include <esp_http_server.h>
#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif
  DECLARE_ZBA_MODULE(zba_auth);
  zba_err_t zba_auth_init();
  zba_err_t zba_auth_deinit();
  zba_err_t zba_auth_basic_check_web(httpd_req_t *req);
  zba_err_t zba_auth_digest_check_web(httpd_req_t *req);

  /// {TODO} Need to add a more serious system, but better than nothing.
  zba_err_t zba_auth_check(const char *uname, const char *pwd);

  void zba_auth_get_nonce(char *buffer);
  void zba_auth_get_opaque(char *buffer);
  bool zba_auth_get_value(const char *buf, const char *key, char **resPtr, size_t *resLen);
  bool zba_auth_gen_ha1(const char *user, const char *realm, const char *pwd, uint8_t *ha1md5);
  bool zba_auth_gen_ha2(const char *method, const char *uri, uint8_t *ha2md5);
  bool zba_auth_gen_response(uint8_t *ha1, uint8_t *ha2, const char *nonce, uint8_t *responseMd5);

#ifdef __cplusplus
}
#endif

#endif