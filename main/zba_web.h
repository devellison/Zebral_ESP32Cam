#ifndef ZEBRAL_ESP32CAM_ZBA_WEB_H_
#define ZEBRAL_ESP32CAM_ZBA_WEB_H_

#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

  DECLARE_ZBA_MODULE(zba_web);
  /// initialize the camera web server
  zba_err_t zba_web_init();
  /// deinitialize the camera web server
  zba_err_t zba_web_deinit();

#ifdef __cplusplus
}
#endif
#endif  // ZEBRAL_ESP32CAM_ZBA_WEB_H_
