#ifndef ZEBRAL_ESP32CAM_ZBA_VISION_H_
#define ZEBRAL_ESP32CAM_ZBA_VISION_H_

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

  DECLARE_ZBA_MODULE(zba_vision);
  zba_err_t zba_vision_init();
  zba_err_t zba_vision_deinit();

  // Thinking we can have vision run simultaneous
  // tasks
  typedef enum
  {
    ZBA_VISION_NONE   = 0x0,
    ZBA_VISION_MEDIAN = 0x01,
    ZBA_VISION_EDGES  = 0x08
  } zba_vision_task_t;

  zba_err_t zba_vision_set_task(zba_vision_task_t task);

#ifdef __cplusplus
}
#endif

#endif  // ZEBRAL_ESP32CAM_ZBA_VISION_H_