#ifndef ZEBRAL_ESP32CAM_ZBA_CONFIG_H_
#define ZEBRAL_ESP32CAM_ZBA_CONFIG_H_

#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

  DECLARE_ZBA_MODULE(zba_config);
/// {TODO} switch to encrypted NVRAM, and/or just store a hash of the
/// device password.
#define kAdminUser "admin"
/// By default, wait for 60 seconds before failing to connect.
#define kWifiTimeoutSeconds 30
#define kMaxSSIDLen         32
#define kMaxPasswordLen     64
#define kMaxUserLen         32
#define kSerialBufferLength 255

  /// Init the global config
  zba_err_t zba_config_init();

  /// Deinitialize the config
  zba_err_t zba_config_deinit();

  zba_err_t zba_config_last_error();

  /// Reset all data in the config.
  zba_err_t zba_config_reset();

  /// Write config
  zba_err_t zba_config_write();

  int zba_config_get_wifi_timeout_sec();

  zba_err_t zba_config_set_wifi_timeout_sec(int seconds);

  /// Buffer must be kMaxSSIDLen or larger.
  zba_err_t zba_config_get_ssid(void *buffer, size_t maxLen);
  /// Set the SSID
  zba_err_t zba_config_set_ssid(const char *ssid);

  /// Get the wifi password
  zba_err_t zba_config_get_wifi_pwd(void *buffer, size_t maxLen);

  /// Set the wifi password
  zba_err_t zba_config_set_wifi_pwd(const char *wifi_pwd);

  /// Get the device password
  zba_err_t zba_config_get_device_pwd(void *buffer, size_t maxLen);

  /// Set the device password
  zba_err_t zba_config_set_device_pwd(const char *wifi_pwd);

#ifdef __cplusplus
}
#endif
#endif  // ZEBRAL_ESP32CAM_ZBA_CONFIG_H_
