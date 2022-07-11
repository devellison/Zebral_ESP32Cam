#ifndef ZEBRAL_ESP32CAM_ZBA_WIFI_H_
#define ZEBRAL_ESP32CAM_ZBA_WIFI_H_

#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif
  DECLARE_ZBA_MODULE(zba_wifi);
  /// initialize the wifi.
  zba_err_t zba_wifi_init();

  /// deinitialize the wifi
  zba_err_t zba_wifi_deinit();

  /// Retrieves a unique name for this device based off
  /// the factory MAC address.
  const char* zba_wifi_get_device_name();

#ifdef __cplusplus
}
#endif
#endif  // ZEBRAL_ESP32CAM_ZBA_WIFI_H_
