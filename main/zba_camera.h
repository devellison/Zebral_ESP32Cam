#ifndef ZEBRAL_ESP32CAM_ZBA_CAMERA_H_
#define ZEBRAL_ESP32CAM_ZBA_CAMERA_H_
#include <esp_camera.h>
#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif
  DECLARE_ZBA_MODULE(zba_camera);

  typedef enum zba_resolution
  {
    // Experimental, and report overflows when there shouldn't be in jpeg mode.
    // Trying in RGB modes.
    ZBA_QVGA_INTERNAL,  // 320x240
    ZBA_QCIF_INTERNAL,  // 176x144   PIXFORMAT_RGB444;

    // This don't seem to work in JPEG
    // {TODO} troubleshoot this...
    // ZBA_QVGA,           // 320x240   JPEG
    // ZBA_QCIF,           // 176x144   JPEG

    // These just work...
    ZBA_VGA,   // 640x480   JPEG   @ 25fps
    ZBA_SVGA,  // 800x600   JPEG   @ 25 fps
    ZBA_HD,    // 1280x720  JPEG   @ 12 fps
    ZBA_SXGA,  // 1280x1024 JPEG   @ 10 fps
    ZBA_UXGA   // 1600x1200 JPEG   @ 12 fps
  } zba_resolution_t;

  /// Resolution as defined above
  zba_err_t zba_camera_init();

  /// Deinitialize the camera
  zba_err_t zba_camera_deinit();

  camera_fb_t* zba_camera_capture_frame();
  void zba_camera_release_frame(camera_fb_t* frame);

  /// INTERNAL Start capturing frames
  /// (this will mostly be used for imaging on the chip)
  void zba_camera_capture_start();

  /// INTERNAL Stop capturing frames
  /// (this will mostly be used for imaging on the chip)
  void zba_camera_capture_stop();

  zba_err_t zba_camera_set_status_default();
  zba_err_t zba_camera_dump_status();

#ifdef __cplusplus
}
#endif
#endif  // ZEBRAL_ESP32CAM_ZBA_CAMERA_H_
