#ifndef ZEBRAL_ESP32CAM_ZBA_RTSP_H_
#define ZEBRAL_ESP32CAM_ZBA_RTSP_H_

#include <esp_log.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include "zba_camera.h"
#include "zba_imgproc.h"
#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

  DECLARE_ZBA_MODULE(zba_rtsp);
  zba_err_t zba_rtsp_init();
  zba_err_t zba_rtsp_deinit();

#ifdef __cplusplus
}
#endif

#endif  // ZEBRAL_ESP32CAM_ZBA_RTSP_H_