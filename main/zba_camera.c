#include "zba_camera.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>

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
  zba_resolution_t resolution;
  sensor_t* camera_sensor;               ///< Camera sensor
  zba_camera_frame_callback_t callback;  ///< image processing callback
  void* context;
  camera_fb_t* process_frame;
} zba_camera_t;

/// Static camera state
static zba_camera_t camera_state = {.stackSize     = 8192,
                                    .capturing     = false,
                                    .captureTask   = NULL,
                                    .frameCount    = 0,
                                    .accumSize     = 0,
                                    .start         = 0,
                                    .frameNum      = 0,
                                    .resolution    = ZBA_SVGA,
                                    .camera_sensor = NULL,
                                    .callback      = NULL,
                                    .context       = NULL,
                                    .process_frame = NULL};

zba_err_t zba_camera_set_res(zba_resolution_t res)
{
  camera_state.resolution = res;
  return ZBA_OK;
}
zba_resolution_t zba_camera_get_res()
{
  return camera_state.resolution;
}

zba_err_t zba_camera_init()
{
  esp_err_t err;
  zba_err_t init_err   = ZBA_OK;
  zba_resolution_t res = camera_state.resolution;

  // Defaults
  int frameSize                 = FRAMESIZE_SVGA;
  pixformat_t format            = PIXFORMAT_JPEG;
  int quality                   = 4;
  int bufferCount               = 2;  // 2;
  camera_grab_mode_t grabMode   = CAMERA_GRAB_WHEN_EMPTY;
  camera_fb_location_t location = CAMERA_FB_IN_PSRAM;

  // I've set the jpeg qualities to the best/ that I could get to work
  // reliably.
  switch (res)
  {
    case ZBA_96x96_INTERNAL:
      frameSize   = FRAMESIZE_96X96;
      format      = PIXFORMAT_RGB565;
      grabMode    = CAMERA_GRAB_WHEN_EMPTY;
      bufferCount = 1;
      location    = CAMERA_FB_IN_DRAM;
      break;
    case ZBA_QCIF_INTERNAL:
      frameSize   = FRAMESIZE_QCIF;
      format      = PIXFORMAT_RGB565;
      grabMode    = CAMERA_GRAB_WHEN_EMPTY;
      bufferCount = 1;
      location    = CAMERA_FB_IN_DRAM;
      break;
    case ZBA_QVGA_INTERNAL:
      frameSize   = FRAMESIZE_QVGA;
      format      = PIXFORMAT_RGB565;
      grabMode    = CAMERA_GRAB_WHEN_EMPTY;
      bufferCount = 1;
      location    = CAMERA_FB_IN_PSRAM;
      break;
    case ZBA_VGA_INTERNAL:
      frameSize   = FRAMESIZE_VGA;
      format      = PIXFORMAT_RGB565;
      grabMode    = CAMERA_GRAB_WHEN_EMPTY;
      bufferCount = 1;
      location    = CAMERA_FB_IN_PSRAM;
      break;
    case ZBA_SVGA_INTERNAL:
      frameSize   = FRAMESIZE_SVGA;
      format      = PIXFORMAT_RGB565;
      grabMode    = CAMERA_GRAB_WHEN_EMPTY;
      bufferCount = 1;
      location    = CAMERA_FB_IN_PSRAM;
      break;

    // Tiny modes need worse quality until
    // bug is fixed for frame buffer size allocation
    // in cam_hal - it just does full frame / 5, which
    // is...
    case ZBA_96x96:
      frameSize = FRAMESIZE_96X96;
      quality   = 14;
      break;

    case ZBA_QVGA:
      frameSize = FRAMESIZE_QVGA;
      quality   = 12;
      break;
    case ZBA_QCIF:
      frameSize = FRAMESIZE_QCIF;
      quality   = 12;
      break;

    case ZBA_VGA:
      frameSize = FRAMESIZE_VGA;
      quality   = 3;
      break;
    default:
    case ZBA_SVGA:
      frameSize = FRAMESIZE_SVGA;
      quality   = 3;
      break;
    case ZBA_HD:
      frameSize = FRAMESIZE_HD;
      quality   = 4;
      break;
    case ZBA_SXGA:
      frameSize = FRAMESIZE_SXGA;
      quality   = 5;
      break;
    case ZBA_UXGA:
      frameSize = FRAMESIZE_UXGA;
      quality   = 8;
      break;
  }

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
                            .pixel_format = format,
                            .frame_size   = frameSize,
                            .jpeg_quality = quality,
                            .fb_count     = bufferCount,
                            .grab_mode    = grabMode,
                            .fb_location  = location};

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

  ZBA_MODULE_INITIALIZED(zba_camera) = init_err;
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

  ZBA_MODULE_INITIALIZED(zba_camera) =
      (ZBA_OK == deinit_error) ? ZBA_MODULE_NOT_INITIALIZED : deinit_error;

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
