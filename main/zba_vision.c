#include "zba_vision.h"
#include <string.h>
#include "zba_util.h"
#include "zba_web.h"

DEFINE_ZBA_MODULE(zba_vision);

typedef struct
{
  zba_resolution_t old_res;  ///< Resolution prior to switching to vision mode
  uint32_t tasks;            ///< Flags for what tasks vision should do
  camera_fb_t rgb565_frame;  ///< Processing buffer for color
  camera_fb_t gray_frame;    ///< Processing buffer for grayscale
  bool first;                ///< Is this first pass? (may need buffer init for motion, etc)
  zba_resolution_t resolution;
} vision_state_t;

// resolution / pixel mode for vision
#define VISION_PIXELFORMAT ZBA_96x96_INTERNAL  // ZBA_QVGA_INTERNAL
#define VISION_WIDTH       96                  // 320
#define VISION_HEIGHT      96                  // 240
#define VISION_BUFFER_SIZE (VISION_WIDTH * VISION_HEIGHT * 2)

static vision_state_t vision_state = {.old_res      = ZBA_VGA,
                                      .tasks        = ZBA_VISION_NONE,
                                      .rgb565_frame = {0},
                                      .gray_frame   = {0},
                                      .first        = true,
                                      .resolution   = VISION_PIXELFORMAT};

camera_fb_t* zba_vision_on_frame(camera_fb_t* frame, void* context);

zba_err_t zba_vision_init()
{
  zba_err_t result = ZBA_OK;
  ZBA_LOG("Init vision.");
  // Setting up for vision - stop camera, set it to a vision-sized resolution,
  // start camera again.
  for (;;)
  {
    if (ZBA_OK == ZBA_MODULE_INITIALIZED(zba_camera))
    {
      ZBA_LOG("Deinitializing camera");
      zba_camera_deinit();
    }

    if (ZBA_OK != (result = zba_camera_set_res(vision_state.resolution)))
    {
      ZBA_ERR("Error setting camera resolution!");
      break;
    }

    zba_camera_set_on_frame(zba_vision_on_frame, NULL);

    if (ZBA_OK != (result = zba_camera_init()))
    {
      ZBA_ERR("Error bringing camera back up!");
      break;
    }

    // The esp32 driver doesn't seem to successfully handle
    // grayscale. Incoming frame buffer is too small, gets an error.
    //
    // if (vision_state.resolution == ZBA_SXGA_INTERNAL_GRAY)
    // {
    // Grayscale only, no RGB565 buffer
    // vision_state.rgb565_frame.buf = NULL;
    // vision_state.gray_frame.buf   = calloc(1, VISION_BUFFER_SIZE + VISION_BUFFER_SIZE / 2);
    //}
    // else

    // allocate frame buffer and a grayscale buffer in one go
    {
      vision_state.rgb565_frame.buf    = calloc(1, VISION_BUFFER_SIZE + VISION_BUFFER_SIZE / 2);
      vision_state.gray_frame.buf      = vision_state.rgb565_frame.buf + VISION_BUFFER_SIZE;
      vision_state.rgb565_frame.width  = VISION_WIDTH;
      vision_state.rgb565_frame.height = VISION_HEIGHT;
      vision_state.rgb565_frame.format = PIXFORMAT_RGB565;
      vision_state.rgb565_frame.len    = VISION_WIDTH * VISION_HEIGHT * 2;
    }
    vision_state.gray_frame.width  = VISION_WIDTH;
    vision_state.gray_frame.height = VISION_HEIGHT;
    vision_state.gray_frame.format = PIXFORMAT_GRAYSCALE;
    vision_state.gray_frame.len    = VISION_WIDTH * VISION_HEIGHT;

    if (vision_state.gray_frame.buf == NULL)
    {
      ZBA_ERR("Couldn't allocate RAM for frame buffer!");
      return ZBA_OUT_OF_MEMORY;
    }

    vision_state.first = true;

    result = ZBA_OK;
    break;
  }

  ZBA_MODULE_INITIALIZED(zba_vision) = result;
  return result;
}

zba_err_t zba_vision_deinit()
{
  zba_err_t deinit_error = ZBA_OK;
  ZBA_LOG("Deinit vision.");
  zba_camera_deinit();
  zba_camera_set_res(ZBA_SVGA);
  zba_camera_set_on_frame(NULL, NULL);

  if (vision_state.rgb565_frame.buf != NULL)
  {
    free(vision_state.rgb565_frame.buf);
    vision_state.rgb565_frame.buf = NULL;
    vision_state.gray_frame.buf   = NULL;
  }
  else if (vision_state.gray_frame.buf != NULL)
  {
    free(vision_state.gray_frame.buf);
    vision_state.gray_frame.buf = NULL;
  }

  ZBA_MODULE_INITIALIZED(zba_vision) =
      (ZBA_OK == deinit_error) ? ZBA_MODULE_NOT_INITIALIZED : deinit_error;

  return deinit_error;
}

zba_err_t zba_vision_set_task(zba_vision_task_t task)
{
  vision_state.tasks = task;
  return ZBA_OK;
}

camera_fb_t* zba_vision_on_frame(camera_fb_t* frame, void* context)
{
  if (!frame)
  {
    ZBA_ERR("Empty frame in zba_vision_on_frame");
    return 0;
  }
  if (!vision_state.gray_frame.buf)
  {
    ZBA_ERR("Empty vision buffer!");
    return 0;
  }

  (void)context;
  bool can_process = false;

  vision_state.gray_frame.timestamp   = frame->timestamp;
  vision_state.rgb565_frame.timestamp = frame->timestamp;

  camera_fb_t* ret_frame = &vision_state.gray_frame;
  switch (frame->format)
  {
    case PIXFORMAT_JPEG:
    case PIXFORMAT_RAW:
    case PIXFORMAT_RGB444:
    case PIXFORMAT_RGB555:
    case PIXFORMAT_RGB888:
    case PIXFORMAT_YUV420:
    case PIXFORMAT_YUV422:
      break;
    case PIXFORMAT_RGB565:
      zba_imgproc_rgb565_to_gray((uint16_t*)frame->buf, frame->width, frame->height,
                                 vision_state.gray_frame.buf);
      can_process = true;

      break;
    case PIXFORMAT_GRAYSCALE:
      // Already in grayscale? Ok, just use the camera frame.
      can_process = true;
      ret_frame   = frame;
      break;
  }
  if (!can_process) return 0;

  // zba_vision_mean_rgb565(frame, NULL);
  // zba_vision_erode_rgb565(frame, NULL);
  // zba_vision_edgex_rgb565(frame, NULL);
  return ret_frame;
}
