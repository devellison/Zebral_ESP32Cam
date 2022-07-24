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
  sensor_t* camera_sensor;  ///< Camera sensor
} zba_camera_t;

/// Static camera state
static zba_camera_t camera_state = {.stackSize     = 8192,
                                    .capturing     = false,
                                    .captureTask   = NULL,
                                    .frameCount    = 0,
                                    .accumSize     = 0,
                                    .start         = 0,
                                    .frameNum      = 0,
                                    .resolution    = ZBA_VGA,
                                    .camera_sensor = NULL};

zba_err_t zba_camera_init()
{
  esp_err_t err;
  zba_err_t init_err   = ZBA_OK;
  zba_resolution_t res = camera_state.resolution;

  // Defaults
  int frameSize                 = FRAMESIZE_SVGA;
  pixformat_t format            = PIXFORMAT_JPEG;
  int quality                   = 4;
  int bufferCount               = 2;
  camera_grab_mode_t grabMode   = CAMERA_GRAB_LATEST;
  camera_fb_location_t location = CAMERA_FB_IN_PSRAM;

  // I've set the jpeg qualities to the best/ that I could get to work
  // reliably.
  switch (res)
  {
    case ZBA_QVGA_INTERNAL:
      frameSize   = FRAMESIZE_QCIF;
      format      = PIXFORMAT_RGB565;
      grabMode    = CAMERA_GRAB_WHEN_EMPTY;
      bufferCount = 1;
      location    = CAMERA_FB_IN_DRAM;
      break;
    case ZBA_QCIF_INTERNAL:
      // 112 frames in 10.075424 seconds = 11.116158 fps. Avg 50688 bytes per frame. 176x144
      // Might be useful for on-board image processing.
      frameSize   = FRAMESIZE_QCIF;
      format      = PIXFORMAT_RGB565;
      grabMode    = CAMERA_GRAB_WHEN_EMPTY;
      bufferCount = 1;
      location    = CAMERA_FB_IN_DRAM;
      break;
    case ZBA_VGA:
      // 251 frames in 10.035354 seconds = 25.011576 fps. Avg 56162 bytes per frame. 640x480
      frameSize = FRAMESIZE_VGA;
      quality   = 3;
      break;
    default:
    case ZBA_SVGA:
      // 250 frames in 10.035060 seconds = 24.912657 fps. Avg 86643 bytes per frame. 800x600
      frameSize = FRAMESIZE_SVGA;
      quality   = 3;
      break;
    case ZBA_HD:
      // 128 frames in 10.078775 seconds = 12.699956 fps. Avg 128282 bytes per frame. 1280x720
      frameSize = FRAMESIZE_HD;
      quality   = 4;
      break;
    case ZBA_SXGA:
      // 110 frames in 10.073824 seconds = 10.919389 fps. Avg 200507 bytes per frame. 1280x1024
      frameSize = FRAMESIZE_SXGA;
      quality   = 5;
      break;
    case ZBA_UXGA:
      // 125 frames in 10.074371 seconds = 12.407722 fps. Avg 185072 bytes per frame. 1600x1200
      frameSize = FRAMESIZE_UXGA;
      quality   = 7;
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
    esp_camera_fb_return(frame);
  }
}

void zba_camera_capture_task()
{
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

    zba_camera_release_frame(frame);
  }

  vTaskDelete(camera_state.captureTask);
}

void zba_camera_capture_start()
{
  if (camera_state.capturing)
  {
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
  camera_state.capturing = false;
  while (eTaskGetState(camera_state.captureTask) != eDeleted)
  {
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}
