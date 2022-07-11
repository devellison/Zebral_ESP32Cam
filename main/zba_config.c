#include "zba_config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <nvs.h>
#include <nvs_flash.h>
#include <string.h>

#include "zba_util.h"

DEFINE_ZBA_MODULE(zba_config);

/// System config struct def
/// This is stored in nv ram
/// Do not change existing entries (!)
/// Only add to end.
typedef struct zba_config
{
  int wifi_timeout_sec;
  char ssid[kMaxSSIDLen + 2];            // 32+2
  char wifi_pwd[kMaxPasswordLen + 2];    // 64+2
  char device_pwd[kMaxPasswordLen + 2];  // 64+2
} zba_config_t;

/// Config state
typedef struct
{
  nvs_handle_t nvsHandle;
  SemaphoreHandle_t configMutex;
  zba_config_t config;
} zba_config_state_t;

// clang-format off
#define DEFAULT_SSID {0}
#define DEFAULT_PWD {0}
// clang-format on

static zba_config_state_t config_state = {
    .nvsHandle   = 0,
    .configMutex = NULL,
    .config      = {kWifiTimeoutSeconds, DEFAULT_SSID, DEFAULT_PWD, DEFAULT_PWD}};

static const char *kConfigName = "zba";

zba_err_t zba_config_init()
{
  esp_err_t esp_result = ESP_OK;
  zba_err_t result     = ZBA_OK;
  size_t len           = 0;

  if (config_state.configMutex == NULL)
  {
    config_state.configMutex = xSemaphoreCreateMutex();
  }

  ZBA_LOCK(config_state.configMutex);

  for (;;)
  {
    if (config_state.nvsHandle)
    {
      ZBA_LOG("WARNING: Config already initialized.");
      result = ZBA_OK;
      break;
    }

    if (ESP_OK != (esp_result = nvs_flash_init()))
    {
      ZBA_ERR("ERROR: Could not init flash storage! ESP_ERROR: 0x%X", esp_result);
      result = ZBA_CONFIG_INIT_ERROR;
      break;
    }

    if (ESP_OK != (esp_result = nvs_open(kConfigName, NVS_READWRITE, &config_state.nvsHandle)))
    {
      ZBA_ERR("ERROR: Could not open NVS! ESP_ERROR: 0x%X", esp_result);
      result = ZBA_CONFIG_INIT_ERROR;
      break;
    }

    len = kMaxSSIDLen + 1;
    nvs_get_str(config_state.nvsHandle, "ssid", config_state.config.ssid, &len);

    len = kMaxPasswordLen + 1;
    nvs_get_str(config_state.nvsHandle, "wifi_pwd", config_state.config.wifi_pwd, &len);

    len = kMaxPasswordLen + 1;
    nvs_get_str(config_state.nvsHandle, "device_pwd", config_state.config.device_pwd, &len);

    // Fields were zerod initially, but ensure termination at maxlength (we've got 2 extra bytes).
    config_state.config.ssid[kMaxSSIDLen]           = 0;
    config_state.config.wifi_pwd[kMaxPasswordLen]   = 0;
    config_state.config.device_pwd[kMaxPasswordLen] = 0;
    break;
  }

  // {TODO} Sanity check values?
  ZBA_UNLOCK(config_state.configMutex);

  if (result != ZBA_OK)
  {
    if (result != ZBA_CONFIG_NEW)
    {
      ZBA_ERR("Config initialization failed. (0x%X)", result);
      zba_config_deinit();
    }
  }

  ZBA_MODULE_INITIALIZED(zba_config) = result;
  return result;
}

zba_err_t zba_config_deinit()
{
  zba_err_t deinit_error = ZBA_OK;

  if (config_state.configMutex)
  {
    ZBA_LOCK(config_state.configMutex);
    if (config_state.nvsHandle)
    {
      nvs_close(config_state.nvsHandle);
      config_state.nvsHandle = 0;

      nvs_flash_deinit();
    }
    ZBA_UNLOCK(config_state.configMutex);

    vSemaphoreDelete(config_state.configMutex);
    config_state.configMutex = 0;
  }

  ZBA_MODULE_INITIALIZED(zba_config) =
      (ZBA_OK == deinit_error) ? ZBA_MODULE_NOT_INITIALIZED : deinit_error;

  return deinit_error;
}

zba_err_t zba_config_reset()
{
  zba_err_t result = ZBA_OK;

  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  ZBA_LOCK(config_state.configMutex);
  {
    if (ESP_OK != nvs_erase_all(config_state.nvsHandle))
    {
      result = ZBA_CONFIG_ERASE_FAILED;
      ZBA_ERR("Failed to erase to NVS (0x%X)", result);
    }
    else
    {
      if (ESP_OK != nvs_commit(config_state.nvsHandle))
      {
        result = ZBA_CONFIG_ERASE_COMMIT_FAILED;
        ZBA_ERR("Failed to commit to NVS (0x%X)", result);
      }
    }

    memset(&config_state.config, 0, sizeof(config_state.config));
  }
  ZBA_UNLOCK(config_state.configMutex);

  return result;
}

zba_err_t zba_config_write()
{
  zba_err_t result = ZBA_OK;

  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  ZBA_LOCK(config_state.configMutex);
  {
    if (ESP_OK != nvs_set_str(config_state.nvsHandle, "ssid", config_state.config.ssid))
    {
      ZBA_ERR("Error writing wifi SSID");
      result = ZBA_CONFIG_WRITE_FAILED;
    }
    if (ESP_OK != nvs_set_str(config_state.nvsHandle, "wifi_pwd", config_state.config.wifi_pwd))
    {
      ZBA_ERR("Error writing wifi pwd");
      result = ZBA_CONFIG_WRITE_FAILED;
    }
    if (ESP_OK != nvs_set_str(config_state.nvsHandle, "device_pwd", config_state.config.device_pwd))
    {
      ZBA_ERR("Error writing device pwd");
      result = ZBA_CONFIG_WRITE_FAILED;
    }

    nvs_commit(config_state.nvsHandle);
  }
  ZBA_UNLOCK(config_state.configMutex);
  return result;
}

int zba_config_get_wifi_timeout_sec()
{
  int timeout = 0;

  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  ZBA_LOCK(config_state.configMutex);
  {
    timeout = config_state.config.wifi_timeout_sec;
  }
  ZBA_UNLOCK(config_state.configMutex);
  return timeout;
}

zba_err_t zba_config_set_wifi_timeout_sec(int seconds)
{
  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  ZBA_LOCK(config_state.configMutex);
  {
    config_state.config.wifi_timeout_sec = seconds;
  }
  ZBA_UNLOCK(config_state.configMutex);
  return ZBA_OK;
}

zba_err_t zba_config_get_ssid(void *buffer, size_t maxLen)
{
  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  ZBA_LOCK(config_state.configMutex);
  {
    strncpy(buffer, config_state.config.ssid, ZBA_MIN(maxLen, kMaxSSIDLen));
  }
  ZBA_UNLOCK(config_state.configMutex);
  return ZBA_OK;
}

zba_err_t zba_config_set_ssid(const char *ssid)
{
  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  ZBA_LOCK(config_state.configMutex);
  {
    memset(&config_state.config.ssid[0], 0, sizeof(config_state.config.ssid));
    if (ssid)
    {
      strncpy(config_state.config.ssid, ssid, kMaxSSIDLen);
    }
  }
  ZBA_UNLOCK(config_state.configMutex);
  return ZBA_OK;
}

zba_err_t zba_config_get_wifi_pwd(void *buffer, size_t maxLen)
{
  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  ZBA_LOCK(config_state.configMutex);
  {
    strncpy(buffer, config_state.config.wifi_pwd, ZBA_MIN(maxLen, kMaxPasswordLen));
  }
  ZBA_UNLOCK(config_state.configMutex);
  return ZBA_OK;
}

zba_err_t zba_config_set_wifi_pwd(const char *wifi_pwd)
{
  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  ZBA_LOCK(config_state.configMutex);
  {
    memset(&config_state.config.wifi_pwd[0], 0, sizeof(config_state.config.wifi_pwd));

    if (wifi_pwd)
    {
      strncpy(config_state.config.wifi_pwd, wifi_pwd, kMaxPasswordLen);
    }
  }
  ZBA_UNLOCK(config_state.configMutex);
  return ZBA_OK;
}

zba_err_t zba_config_get_device_pwd(void *buffer, size_t maxLen)
{
  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  ZBA_LOCK(config_state.configMutex);
  {
    strncpy(buffer, config_state.config.device_pwd, ZBA_MIN(maxLen, kMaxPasswordLen));
  }
  ZBA_UNLOCK(config_state.configMutex);
  return ZBA_OK;
}

zba_err_t zba_config_set_device_pwd(const char *device_pwd)
{
  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  ZBA_LOCK(config_state.configMutex);
  {
    memset(&config_state.config.device_pwd[0], 0, sizeof(config_state.config.device_pwd));

    if (device_pwd)
    {
      strncpy(config_state.config.device_pwd, device_pwd, kMaxPasswordLen);
    }
  }
  ZBA_UNLOCK(config_state.configMutex);
  return ZBA_OK;
}

zba_err_t zba_config_check_auth(const char *uname, const char *pwd)
{
  if ((!config_state.configMutex) || (!config_state.nvsHandle))
  {
    ZBA_ERR("Config not initialized.");
    return ZBA_CONFIG_NOT_INITIALIZED;
  }

  if (uname != NULL)
  {
    ZBA_LOG("There can be only one! (right now).");
  }

  // If we're not configured, allow access.
  if (config_state.config.device_pwd[0] == 0)
  {
    ZBA_LOG("No password set. Please set a password now.");
    return ZBA_OK;
  }

  if (!pwd)
  {
    ZBA_LOG("No password given.");
    return ZBA_CONFIG_NOT_AUTHED;
  }

  if (0 == strcmp(pwd, config_state.config.device_pwd))
  {
    ZBA_LOG("Authenticated");
    return ZBA_OK;
  }

  ZBA_LOG("Authentication failed.");
  return ZBA_CONFIG_NOT_AUTHED;
}
