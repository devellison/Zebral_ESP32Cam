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
    // Trying in less lossy modes...
    ZBA_96x96_INTERNAL,  // 96x96
    ZBA_QVGA_INTERNAL,   // 320x240
    ZBA_QCIF_INTERNAL,   // 176x144
    ZBA_VGA_INTERNAL,    // 640x480
    ZBA_SVGA_INTERNAL,   // 800x600

    // Haven't had it work well yet above SVGA in RGB565,
    // and grayscale wasn't working for me either.

    // This don't seem to work in JPEG
    // {TODO} troubleshoot this...
    // AH! when it allocates the frame size, it's doing RAW/5 in camera_hal.c.
    // For better quality images, that simply isn't true, esp. at small sizes.
    // So use worse quality, or change the /5 to something else closer to reality
    ZBA_96x96,
    ZBA_QVGA,  // 320x240   JPEG
    ZBA_QCIF,  // 176x144   JPEG

    // These just work...
    ZBA_VGA,   // 640x480   JPEG
    ZBA_SVGA,  // 800x600   JPEG
    ZBA_HD,    // 1280x720  JPEG
    ZBA_SXGA,  // 1280x1024 JPEG
    ZBA_UXGA   // 1600x1200 JPEG
  } zba_resolution_t;

  zba_err_t zba_camera_set_res(zba_resolution_t res);
  zba_resolution_t zba_camera_get_res();
  size_t zba_camera_get_height();
  size_t zba_camera_get_width();

  /// Resolution as defined above
  zba_err_t zba_camera_init();

  /// Deinitialize the camera
  zba_err_t zba_camera_deinit();

  camera_fb_t* zba_camera_capture_frame();
  void zba_camera_release_frame(camera_fb_t* frame);

  typedef camera_fb_t* (*zba_camera_frame_callback_t)(camera_fb_t* frame, void* context);

  /// Sets a callback that's called with the frame prior to returning from zba_camera_capture_frame.
  void zba_camera_set_on_frame(zba_camera_frame_callback_t callback, void* context);

  bool zba_camera_need_restart();
  /// INTERNAL Start capturing frames
  /// (this will mostly be used for imaging on the chip)
  void zba_camera_capture_start();

  /// INTERNAL Stop capturing frames
  /// (this will mostly be used for imaging on the chip)
  void zba_camera_capture_stop();

  zba_err_t zba_camera_set_status_default();
  zba_err_t zba_camera_dump_status();

  typedef struct
  {
    zba_resolution_t res;
    const char* name;
    int frameSize;
    pixformat_t format;
    int quality;
    int bufferCount;
    camera_grab_mode_t grabMode;
    camera_fb_location_t location;
  } zba_res_info_t;

  const zba_res_info_t* zba_camera_get_res_from_name(const char* name);
  const zba_res_info_t* zba_camera_get_resolution_info(zba_resolution_t res);
  const char* zba_camera_get_res_name(zba_resolution_t res);

#ifdef __cplusplus
}
#endif
#endif  // ZEBRAL_ESP32CAM_ZBA_CAMERA_H_
