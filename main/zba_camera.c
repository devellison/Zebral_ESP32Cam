#include "zba_camera.h"

#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory.h>
#include <stdbool.h>
#include <string.h>

#include "zba_pins.h"
#include "zba_priority.h"
#include "zba_util.h"

DEFINE_ZBA_MODULE(zba_camera);

/// Camera state struct
typedef struct
{
  int stackSize;             ///< Task stack size
  TaskHandle_t captureTask;  ///< Capture task
  bool capturing;            ///< Are we currently capturing?

  int frameCount;  ///< Number of frames since starting timing
  int accumSize;   ///< Total bytes in frames since start
  int64_t start;   ///< Start time of current timing seg (10 frames then restarts)

  int64_t frameNum;  ///< Absolute frame number since init
  zba_resolution_t desired_resolution;
  zba_resolution_t resolution;
  sensor_t* camera_sensor;               ///< Camera sensor
  zba_camera_frame_callback_t callback;  ///< image processing callback
  void* context;
  camera_fb_t* process_frame;
} zba_camera_t;

int zba_framesize(zba_resolution_t res);
int zba_quality(zba_resolution_t res);

/// Static camera state
static zba_camera_t camera_state = {.stackSize          = 8192,
                                    .capturing          = false,
                                    .captureTask        = NULL,
                                    .frameCount         = 0,
                                    .accumSize          = 0,
                                    .start              = 0,
                                    .frameNum           = 0,
                                    .desired_resolution = ZBA_SVGA,
                                    .resolution         = ZBA_SVGA,
                                    .camera_sensor      = NULL,
                                    .callback           = NULL,
                                    .context            = NULL,
                                    .process_frame      = NULL};

zba_err_t zba_camera_set_res(zba_resolution_t res)
{
  camera_state.desired_resolution = res;
  return ZBA_OK;
}

zba_err_t zba_camera_set_autoexposure(bool on)
{
  // Bail if camera not initialized
  zba_err_t result = ZBA_MODULE_INITIALIZED(zba_camera);
  if (result != ZBA_OK)
  {
    return result;
  }

  camera_state.camera_sensor->set_gain_ctrl(camera_state.camera_sensor, on ? 1 : 0);
  camera_state.camera_sensor->set_exposure_ctrl(camera_state.camera_sensor, on ? 1 : 0);
  return ZBA_OK;
}

zba_resolution_t zba_camera_get_res()
{
  return camera_state.resolution;
}

bool zba_camera_need_restart()
{
  return (camera_state.resolution != camera_state.desired_resolution);
}

// clang-format off
const zba_res_info_t resolution_info[] = 
{{ZBA_96x96_INTERNAL, "96I",   FRAMESIZE_96X96, PIXFORMAT_RGB565, 0, 1, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_DRAM},
{ZBA_QVGA_INTERNAL,   "QVGAI", FRAMESIZE_QVGA,  PIXFORMAT_RGB565, 0, 1, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_DRAM},
{ZBA_QCIF_INTERNAL,   "QCIFI", FRAMESIZE_QCIF,  PIXFORMAT_RGB565, 0, 1, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_DRAM},
{ZBA_VGA_INTERNAL,    "VGAI",  FRAMESIZE_VGA,   PIXFORMAT_RGB565, 0, 1, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_DRAM},
{ZBA_SVGA_INTERNAL,   "SVGAI", FRAMESIZE_SVGA,  PIXFORMAT_RGB565, 0, 1, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_DRAM},
{ZBA_96x96,           "96",    FRAMESIZE_96X96, PIXFORMAT_JPEG,  14, 2, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_PSRAM},
{ZBA_QVGA,            "QVGA",  FRAMESIZE_QVGA,  PIXFORMAT_JPEG,  12, 2, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_PSRAM},
{ZBA_QCIF,            "QCIF",  FRAMESIZE_QCIF,  PIXFORMAT_JPEG,  12, 2, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_PSRAM},
{ZBA_VGA,             "VGA",   FRAMESIZE_VGA,   PIXFORMAT_JPEG,  4,  2, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_PSRAM},
{ZBA_SVGA,            "SVGA",  FRAMESIZE_SVGA,  PIXFORMAT_JPEG,  4,  2, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_PSRAM},
{ZBA_HD,              "HD",    FRAMESIZE_HD,    PIXFORMAT_JPEG,  4,  2, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_PSRAM},
{ZBA_SXGA,            "SXGA",  FRAMESIZE_SXGA,  PIXFORMAT_JPEG,  5,  2, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_PSRAM},
{ZBA_UXGA,            "UXGA",  FRAMESIZE_UXGA,  PIXFORMAT_JPEG,  8,  2, CAMERA_GRAB_WHEN_EMPTY,CAMERA_FB_IN_PSRAM}};
// clang-format on
const int kNumResolutionInfo = sizeof(resolution_info) / sizeof(zba_res_info_t);

const zba_res_info_t* zba_camera_get_resolution_info(zba_resolution_t res)
{
  for (int i = 0; i < kNumResolutionInfo; ++i)
  {
    const zba_res_info_t* curRes = &resolution_info[i];
    if (res == curRes->res)
    {
      return curRes;
    }
  }
  return 0;
}
const zba_res_info_t* zba_camera_get_res_from_name(const char* name)
{
  for (int i = 0; i < kNumResolutionInfo; ++i)
  {
    const zba_res_info_t* curRes = &resolution_info[i];
    if (0 == strcasecmp(curRes->name, name))
    {
      return curRes;
    }
  }
  return 0;
}
const char* zba_camera_get_res_name(zba_resolution_t res)
{
  for (int i = 0; i < kNumResolutionInfo; ++i)
  {
    const zba_res_info_t* curRes = &resolution_info[i];
    if (res == curRes->res)
    {
      return curRes->name;
    }
  }
  return "";
}

int zba_framesize(zba_resolution_t res)
{
  const zba_res_info_t* resInfo = zba_camera_get_resolution_info(res);
  if (!resInfo) return 0;
  return resInfo->frameSize;
}

int zba_quality(zba_resolution_t res)
{
  const zba_res_info_t* resInfo = zba_camera_get_resolution_info(res);
  if (!resInfo) return 0;
  return resInfo->quality;
}

zba_err_t zba_camera_init()
{
  esp_err_t err;
  zba_err_t init_err   = ZBA_OK;
  zba_resolution_t res = camera_state.resolution;
  if (res != camera_state.desired_resolution)
  {
    res                     = camera_state.desired_resolution;
    camera_state.resolution = camera_state.desired_resolution;
  }

  const zba_res_info_t* resInfo = zba_camera_get_resolution_info(res);

  camera_config_t config = {.pin_d0       = PIN_CAM_D0,
                            .pin_d1       = PIN_CAM_D1,
                            .pin_d2       = PIN_CAM_D2,
                            .pin_d3       = PIN_CAM_D3,
                            .pin_d4       = PIN_CAM_D4,
                            .pin_d5       = PIN_CAM_D5,
                            .pin_d6       = PIN_CAM_D6,
                            .pin_d7       = PIN_CAM_D7,
                            .pin_xclk     = PIN_CAM_XCLK,
                            .pin_pclk     = PIN_CAM_PCLK,
                            .pin_vsync    = PIN_CAM_VSYNC,
                            .pin_href     = PIN_CAM_HREF,
                            .pin_sscb_sda = PIN_CAM_SDA,
                            .pin_sscb_scl = PIN_CAM_SCL,
                            .pin_pwdn     = PIN_CAM_PWDN,
                            .pin_reset    = PIN_CAM_RESET,
                            .xclk_freq_hz = 20000000,
                            .ledc_channel = LEDC_CHANNEL_0,
                            .ledc_timer   = LEDC_TIMER_0,
                            .pixel_format = resInfo->format,
                            .frame_size   = resInfo->frameSize,
                            .jpeg_quality = resInfo->quality,
                            .fb_count     = resInfo->bufferCount,
                            .grab_mode    = resInfo->grabMode,
                            .fb_location  = resInfo->location};

  err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    ZBA_ERR("Error initializing camera: ESP: %d", err);
    init_err = ZBA_CAM_INIT_FAILED;
  }

  camera_state.camera_sensor = esp_camera_sensor_get();
  if (NULL == camera_state.camera_sensor)
  {
    ZBA_ERR("Couldn't get sensor!");
    init_err = ZBA_CAM_INIT_FAILED;
    zba_camera_deinit();
  }

  ZBA_SET_INIT(zba_camera, init_err);
  return init_err;
}

zba_err_t zba_camera_set_status_default()
{
  return ZBA_OK;
}

zba_err_t zba_camera_dump_status()
{
  camera_status_t* s;
  if (!camera_state.camera_sensor)
  {
    return ZBA_CAM_ERROR;
  }
  s = &camera_state.camera_sensor->status;

  ZBA_LOG("scale: %d binning: %d", s->scale, s->binning);
  ZBA_LOG("quality: %d bright: %d contrast: %d saturation: %d", s->quality, s->brightness,
          s->contrast, s->saturation);
  ZBA_LOG("sharpness: %d denoise: %d special effect: %d wb_mode: %d", s->sharpness, s->denoise,
          s->special_effect, s->wb_mode);
  ZBA_LOG("awb: %d awb_gain: %d aec: %d aec2: %d ae_level: %d aec_value: %d", s->awb, s->awb_gain,
          s->aec, s->aec2, s->ae_level, s->aec_value);
  ZBA_LOG("agc: %d agc_gain: %d gainceiling: %d bpc: %d wpc: %d raw_gma: %d lenc:%d", s->agc,
          s->agc_gain, s->gainceiling, s->bpc, s->wpc, s->raw_gma, s->lenc);
  ZBA_LOG("hmirror: %d vflip: %d dcw: %d colorbar: %d", s->hmirror, s->vflip, s->dcw, s->colorbar);
  /*
    bool scale;
    bool binning;
    uint8_t quality;//0 - 63
    int8_t brightness;//-2 - 2
    int8_t contrast;//-2 - 2
    int8_t saturation;//-2 - 2
    int8_t sharpness;//-2 - 2
    uint8_t denoise;
    uint8_t special_effect;//0 - 6
    uint8_t wb_mode;//0 - 4
    uint8_t awb;
    uint8_t awb_gain;
    uint8_t aec;
    uint8_t aec2;
    int8_t ae_level;//-2 - 2
    uint16_t aec_value;//0 - 1200
    uint8_t agc;
    uint8_t agc_gain;//0 - 30
    uint8_t gainceiling;//0 - 6
    uint8_t bpc;
    uint8_t wpc;
    uint8_t raw_gma;
    uint8_t lenc;
    uint8_t hmirror;
    uint8_t vflip;
    uint8_t dcw;
    uint8_t colorbar;
*/

  return ZBA_OK;
}

zba_err_t zba_camera_deinit()
{
  esp_err_t esp_err;
  zba_err_t deinit_error = ZBA_OK;

  zba_camera_capture_stop();

  camera_state.camera_sensor = NULL;

  esp_err = esp_camera_deinit();
  if (ESP_OK != esp_err)
  {
    ZBA_ERR("Error deinitializing the camera: ESP: %d", esp_err);
    deinit_error = ZBA_CAM_DEINIT_FAILED;
  }
  ZBA_SET_DEINIT(zba_camera, deinit_error);

  return deinit_error;
}

camera_fb_t* zba_camera_capture_frame()
{
  // capture a frame
  camera_fb_t* frame;

  if (0 == camera_state.start)
  {
    camera_state.start      = zba_now();
    camera_state.frameCount = 0;
    camera_state.accumSize  = 0;
  }

  frame = esp_camera_fb_get();
  if (!frame)
  {
    return frame;
  }

  camera_state.process_frame = NULL;
  if (camera_state.callback)
  {
    camera_state.process_frame = camera_state.callback(frame, camera_state.context);
  }

  if (camera_state.process_frame)
  {
    // Vision can return an alternative buffer - if it does,
    // then use our local process buffer instead and free
    // the camera driver frame now.
    esp_camera_fb_return(frame);
    frame = camera_state.process_frame;
  }

  camera_state.accumSize += frame->len;
  camera_state.frameCount++;
  camera_state.frameNum++;

  float elapsed = zba_elapsed_sec(camera_state.start);
  if (elapsed >= 10.0)
  {
    ZBA_LOG("%d frames in %f seconds = %f fps. Avg %d bytes per frame. %dx%d",
            camera_state.frameCount, elapsed, ((float)camera_state.frameCount) / elapsed,
            camera_state.accumSize / camera_state.frameCount, frame->width, frame->height);
    ZBA_LOG("Stack usage: %d of %d", uxTaskGetStackHighWaterMark(camera_state.captureTask),
            camera_state.stackSize);

    camera_state.start      = zba_now();
    camera_state.frameCount = 0;
    camera_state.accumSize  = 0;
  }

  return frame;
}

void zba_camera_release_frame(camera_fb_t* frame)
{
  if (frame)
  {
    // Don't free our internal process buffers!
    if (frame != camera_state.process_frame)
    {
      esp_camera_fb_return(frame);
    }
  }
}

void zba_camera_capture_task()
{
  ZBA_ERR("Camera Task running!");
  camera_fb_t* frame = NULL;
  while (camera_state.capturing)
  {
    frame = zba_camera_capture_frame();
    if (!frame)
    {
      ZBA_LOG("Frame buffer could not be acquired.");
      vTaskDelay(5);
      continue;
    }
    // Callback on frame is in the zba_camera_capture_frame() function itself,
    // so that it can happen in a capture task OR inline with other things
    // like web
    zba_camera_release_frame(frame);
  }

  vTaskDelete(camera_state.captureTask);
}

void zba_camera_set_on_frame(zba_camera_frame_callback_t callback, void* context)
{
  camera_state.callback = callback;
  camera_state.context  = context;
}

void zba_camera_capture_start()
{
  if (camera_state.capturing)
  {
    ZBA_ERR("Camera already capturing!");
    return;
  }
  camera_state.capturing = true;
  xTaskCreate(zba_camera_capture_task, "CameraCapture", camera_state.stackSize, NULL,
              ZBA_CAMERA_LOC_CAP_PRIORITY, &camera_state.captureTask);
}

void zba_camera_capture_stop()
{
  if (!camera_state.capturing)
  {
    return;
  }
  while (eTaskGetState(camera_state.captureTask) != eDeleted)
  {
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
  camera_state.captureTask = NULL;
  camera_state.callback    = NULL;
  camera_state.capturing   = false;
  camera_state.context     = NULL;
}
size_t zba_camera_get_height()
{
  switch (camera_state.resolution)
  {
    case ZBA_96x96_INTERNAL:
      return 96;  // 96x96
    case ZBA_QVGA_INTERNAL:
      return 240;  // 320x240
    case ZBA_QCIF_INTERNAL:
      return 144;  // 176x144
    case ZBA_VGA_INTERNAL:
      return 480;  // 640x480
    case ZBA_SVGA_INTERNAL:
      return 600;  // 800x600
    case ZBA_96x96:
      return 96;
    case ZBA_QVGA:
      return 240;  // 320x240   JPEG
    case ZBA_QCIF:
      return 144;  // 176x144   JPEG
    case ZBA_VGA:
      return 480;  // 640x480   JPEG
    case ZBA_SVGA:
      return 600;  // 800x600   JPEG
    case ZBA_HD:
      return 720;  // 1280x720  JPEG
    case ZBA_SXGA:
      return 1024;  // 1280x1024 JPEG
    case ZBA_UXGA:
      return 1200;  // 1600x1200 JPEG
    default:
      return 0;
  }
}
size_t zba_camera_get_width()
{
  switch (camera_state.resolution)
  {
    case ZBA_96x96_INTERNAL:
      return 96;  // 96x96
    case ZBA_QVGA_INTERNAL:
      return 320;  // 320x240
    case ZBA_QCIF_INTERNAL:
      return 176;  // 176x144
    case ZBA_VGA_INTERNAL:
      return 640;  // 640x480
    case ZBA_SVGA_INTERNAL:
      return 800;  // 800x600
    case ZBA_96x96:
      return 96;
    case ZBA_QVGA:
      return 320;  // 320x240   JPEG
    case ZBA_QCIF:
      return 176;  // 176x144   JPEG
    case ZBA_VGA:
      return 640;  // 640x480   JPEG
    case ZBA_SVGA:
      return 800;  // 800x600   JPEG
    case ZBA_HD:
      return 1280;  // 1280x720  JPEG
    case ZBA_SXGA:
      return 1280;  // 1280x1024 JPEG
    case ZBA_UXGA:
      return 1600;  // 1600x1200 JPEG
    default:
      return 0;
  }
}
