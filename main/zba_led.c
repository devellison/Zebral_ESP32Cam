#include "zba_led.h"
#include <string.h>
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zba_pins.h"
#include "zba_priority.h"
#include "zba_sd.h"
#include "zba_util.h"

DEFINE_ZBA_MODULE(zba_led);

#define WHITE_INIT 0x01
#define WHITE_ON   0x02

// WS2812B timings for data communication
// https://www.mouser.com/pdfDocs/WS2812B-2020_V10_EN_181106150240761.pdf
// For WS2812:
// '0' is high for 220 to 380 nanoseconds, followed by low for 580 to 1000 nanoseconds
// '1' is high for 580 to 1000 nanoseconds, followed by low for 220 to 420 nanoseconds.
#define WS2812_BIT_T0H 380  // 350
#define WS2812_BIT_T0L 900  // 1000
#define WS2812_BIT_T1H 900  // 1000
#define WS2812_BIT_T1L 380  // 350

// a reset is > 280 microseconds at low
#define WS2812_RESET_DELAY_MICROSEC 500  // 280
//
// To set an LED in a strip, we send all 8 bits of data for each pixel (color) (e.g. G,R,B)
// Once all of the pixels in an LED are set, it passes future data to the next LED.
// So we just stream the bits until we've set all the LEDs we want,
// then set the line low for 280 microseconds to reset the chain for the next run.

/// Remote control channel used to encode data
#define ZBA_LED_RMT_CHANNEL RMT_CHANNEL_0

/// State struct for LED module
typedef struct zba_led_state
{
  int onboard_leds;                ///< On board led status bits (init/on/off)
  volatile bool exiting;           ///< Are we exiting/deinitializing?
  TaskHandle_t led_task;           ///< LED refresh task
  zba_led_seg_t* led_segments;     ///< Configured LED strip segments
  size_t num_led_segments;         ///< Number of strip segments
  uint8_t* led_memory;             ///< Raw LED pixel memory
  uint8_t* led_buffer;             ///< Buffer memory for LED pixels
  volatile bool led_buffer_dirty;  ///< Buffer memory is dirty and ready to be written.
  size_t led_memory_size;          ///< Memory size
  bool rmt_initialized;            ///< Has the remote control peripheral been initialized?
  uint32_t rmt_clock_hz;           ///< Remote clock in Hz
  rmt_item32_t rmt_bit0;           ///< signal that represents a 0 bit to ws2812
  rmt_item32_t rmt_bit1;           ///< signal that represents a 1 bit to ws2812
  int64_t last_refresh;            ///< Last refresh of the LED strips
  SemaphoreHandle_t buffer_mutex;  ///< Mutex to protect LED buffer
  ZBA_LED_ANIMATOR_CB animator;    ///< LED animator routine, or null
  ZBA_LED_FILTER_CB filter;        ///< LED filter routine, or null.
} zba_led_state_t;

static zba_led_state_t led_state = {.onboard_leds     = 0,
                                    .exiting          = 0,
                                    .led_task         = NULL,
                                    .led_segments     = NULL,
                                    .num_led_segments = 0,
                                    .led_memory       = NULL,
                                    .led_buffer       = NULL,
                                    .led_buffer_dirty = false,
                                    .led_memory_size  = 0,
                                    .rmt_initialized  = 0,
                                    .rmt_clock_hz     = 0,
                                    .rmt_bit0         = {},
                                    .rmt_bit1         = {},
                                    .last_refresh     = 0,
                                    .buffer_mutex     = 0,
                                    .animator         = NULL,
                                    .filter           = NULL};

static const char* kLEDTaskStackName   = "LEDRefresh";  ///< LED refresh task name
static const size_t kLEDTaskStackSize  = 8192;          ///< LED task stack size
static const size_t kLEDRefreshSpeedMS = 10;            ///< LED refresh rate

zba_err_t zba_led_strip_refresh();
void led_update_task(void* context);

/// This function converts pixels to the Remote Tranceiver data format for transmission
/// to the LED strip on the ESP32.
static void IRAM_ATTR ws2812_sample_to_rmt(const void* src, rmt_item32_t* dest, size_t src_size,
                                           size_t wanted_num, size_t* translated_size,
                                           size_t* item_num);

// Right now we're not initializing the onboard LED pins until use them
// due to conflicts with other systems for the pins.  We
zba_err_t zba_led_init()
{
  zba_err_t init_error = ZBA_OK;
  esp_err_t esp_error  = ESP_OK;

  for (;;)
  {
    // Configure our data pin as output
    zba_pin_mode(PIN_LED_STRIP_DATA, PIN_MODE_DIGITAL_OUT);

    // Initialize remote control peripheral for sending signal
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(PIN_LED_STRIP_DATA, ZBA_LED_RMT_CHANNEL);
    config.clk_div      = 1;
    if (ESP_OK != (esp_error = rmt_config(&config)))
    {
      ZBA_LOG("Failed RMT config");
      init_error = ZBA_LED_RMT_INIT_FAILED;
      break;
    }
    // install the driver
    if (ESP_OK != (esp_error = rmt_driver_install(config.channel, 0, 0)))
    {
      ZBA_LOG("Failed RMT driver install");
      init_error = ZBA_LED_RMT_INIT_FAILED;
      break;
    }
    // get clock frequency
    if (ESP_OK != (esp_error = rmt_get_counter_clock(ZBA_LED_RMT_CHANNEL, &led_state.rmt_clock_hz)))
    {
      ZBA_LOG("Failed RMT get clock");
      init_error = ZBA_LED_RMT_INIT_FAILED;
      break;
    }

    // '0' is high for 220 to 380 nanoseconds, followed by low for 580 to 1000 nanoseconds
    led_state.rmt_bit0.duration0 =
        (uint32_t)(((float)WS2812_BIT_T0H * led_state.rmt_clock_hz) / 1000000000.0);
    led_state.rmt_bit0.level0 = 1;
    led_state.rmt_bit0.duration1 =
        (uint32_t)(((float)WS2812_BIT_T0L * led_state.rmt_clock_hz) / 1000000000.0);
    led_state.rmt_bit0.level1 = 0;

    // '1' is high for 580 to 1000 nanoseconds, followed by low for 220 to 420 nanoseconds.
    led_state.rmt_bit1.duration0 =
        (uint32_t)(((float)WS2812_BIT_T1H * led_state.rmt_clock_hz) / 1000000000.0);
    led_state.rmt_bit1.level0 = 1;
    led_state.rmt_bit1.duration1 =
        (uint32_t)(((float)WS2812_BIT_T1L * led_state.rmt_clock_hz) / 1000000000.0);
    led_state.rmt_bit1.level1 = 0;

    // set the translation function
    rmt_translator_init(ZBA_LED_RMT_CHANNEL, ws2812_sample_to_rmt);

    led_state.rmt_initialized = 1;

    // Create our buffer mutex
    led_state.buffer_mutex = xSemaphoreCreateMutex();

    // init led strips
    // {TODO} This is hacked to just go with what we've got configured to separate
    // config from init.  Revisit this.
    if (ZBA_OK !=
        (init_error = zba_led_strip_init(led_state.led_segments, led_state.num_led_segments)))
    {
      ZBA_LOG("Failed init LED strip");
      break;
    }

    // create refresh task
    led_state.exiting = false;
    xTaskCreate(led_update_task, kLEDTaskStackName, kLEDTaskStackSize, NULL,
                ZBA_LED_UPDATE_PRIORITY, &led_state.led_task);

    break;
  }

  if (ZBA_OK != init_error)
  {
    ZBA_ERR("Error initializing LEDs");
    zba_led_deinit();
  }

  ZBA_MODULE_INITIALIZED(zba_led) = init_error;
  return init_error;
}

// deinitialize the led module
zba_err_t zba_led_deinit()
{
  zba_err_t deinit_error = ZBA_OK;

  if (led_state.led_task)
  {
    led_state.exiting = true;
    while (eTaskGetState(led_state.led_task) != eDeleted)
    {
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    led_state.led_task = 0;

    zba_led_strip_deinit();
  }

  if (led_state.rmt_initialized)
  {
    rmt_driver_uninstall(ZBA_LED_RMT_CHANNEL);
    led_state.rmt_initialized = 0;
  }

  if (led_state.buffer_mutex)
  {
    vSemaphoreDelete(led_state.buffer_mutex);
    led_state.buffer_mutex = 0;
  }

  // Keep these configured for now
  // led_state.filter           = NULL;
  // led_state.animator         = NULL;
  led_state.led_buffer_dirty = false;

  // Still seems to be leaving pins in an unhappy state for SD :(
  zba_pin_mode(PIN_MODULE_3, PIN_MODE_DIGITAL_IN_PULLUP);
  zba_pin_mode(PIN_MODULE_2, PIN_MODE_DIGITAL_IN_PULLUP);

  ZBA_MODULE_INITIALIZED(zba_led) =
      (ZBA_OK == deinit_error) ? ZBA_MODULE_NOT_INITIALIZED : deinit_error;

  return deinit_error;
}

zba_err_t zba_led_light(bool on)
{
  if (ZBA_MODULE_INITIALIZED(zba_sd) == ZBA_OK)
  {
    ZBA_ERR("SD module is active. Ignoring light request.");
    return ZBA_LED_ERR_SD_ACTIVE;
  }

  // Right now, just set it each time. SD Module unsets it.
  zba_pin_mode(PIN_LED_WHITE, PIN_MODE_DIGITAL_OUT);
  ZBA_SET_BIT_FLAG(led_state.onboard_leds, WHITE_INIT);

  zba_pin_digital_write(PIN_LED_WHITE, on ? PIN_HIGH : PIN_LOW);
  return ZBA_OK;
}

zba_err_t zba_led_light_blink()
{
  led_state.onboard_leds ^= WHITE_ON;
  return zba_led_light(ZBA_TEST_BIT_FLAG(led_state.onboard_leds, WHITE_ON));
}

int pixels_per_led(zba_led_type_t led_type)
{
  switch (led_type)
  {
    case LED_STRIP_RGB:
    case LED_STRIP_UV3:
      return 3;
      break;
    case LED_STRIP_RGBW:
      return 4;
    default:
      break;
  }
  ZBA_ERR("Invalid LED type %d", led_type);
  return 0;
}

zba_err_t zba_led_strip_cfg(zba_led_seg_t* strips, size_t strip_count)
{
  led_state.led_segments     = strips;
  led_state.num_led_segments = strip_count;
  return ZBA_OK;
}

zba_err_t zba_led_strip_init(zba_led_seg_t* strips, size_t strip_count)
{
  size_t led_index        = 0;
  size_t pixel_index      = 0;
  zba_led_seg_t* curStrip = NULL;
  uint8_t* curStripPtr    = NULL;
  uint8_t* curStripBufPtr = NULL;
  if (led_state.led_segments || led_state.led_memory)
  {
    zba_led_strip_deinit();
  }

  led_state.led_segments     = strips;
  led_state.num_led_segments = strip_count;
  led_state.led_memory_size  = 0;

  curStrip = strips;

  // Calculate memory required and fill in offsets
  for (size_t i = 0; i < strip_count; i++)
  {
    curStrip->num_pixels        = pixels_per_led(curStrip->led_type) * curStrip->num_leds;
    curStrip->first_pixel_index = pixel_index;
    curStrip->first_led_index   = led_index;
    led_state.led_memory_size += curStrip->num_pixels;
    led_index += curStrip->num_leds;
    pixel_index += curStrip->num_pixels;
    curStrip++;
  }

  // Allocate memory for the strips and a buffer
  led_state.led_memory = calloc(1, led_state.led_memory_size);
  led_state.led_buffer = calloc(1, led_state.led_memory_size);

  if ((NULL == led_state.led_memory) || (NULL == led_state.led_buffer))
  {
    ZBA_ERR("Failed to allocate memory for LED pixels! Size: %d (x2)", led_state.led_memory_size);
    return ZBA_OUT_OF_MEMORY;
  }

  // Save data ptrs
  curStrip       = strips;
  curStripPtr    = led_state.led_memory;
  curStripBufPtr = led_state.led_buffer;

  ZBA_LOG("LED Strip sequence:");
  for (size_t i = 0; i < strip_count; i++)
  {
    curStrip->data     = curStripPtr;
    curStrip->data_buf = curStripBufPtr;

    curStripPtr += curStrip->num_pixels;
    curStripBufPtr += curStrip->num_pixels;

    ZBA_LOG("%s: type: %d, num: %d first address: %d", curStrip->name, (int)curStrip->led_type,
            curStrip->num_leds, curStrip->first_pixel_index);

    curStrip++;
  }

  return ZBA_OK;
}

zba_err_t zba_led_strip_deinit()
{
  zba_led_seg_t* curStrip = NULL;

  // Free strip memory
  if (NULL != led_state.led_memory)
  {
    free(led_state.led_memory);
    led_state.led_memory = NULL;
  }

  if (NULL != led_state.led_buffer)
  {
    free(led_state.led_buffer);
    led_state.led_buffer = NULL;
  }

  // Do we need to reset table info? probably should, just so dangling ptrs go away
  curStrip = led_state.led_segments;
  if (curStrip)
  {
    for (size_t i = 0; i < led_state.num_led_segments; ++i)
    {
      curStrip->num_pixels        = 0;
      curStrip->first_pixel_index = 0;
      curStrip->first_led_index   = 0;
      curStrip->data              = NULL;
      curStrip->data_buf          = NULL;
      curStrip++;
    }

    // Keep the config there for now to allow reinit.
    //
    // led_state.led_segments     = NULL;
    // led_state.num_led_segments = 0;
  }

  return ZBA_OK;
}

zba_err_t zba_led_strip_clear()
{
  if (led_state.led_memory)
  {
    memset(led_state.led_memory, 0, led_state.led_memory_size);
  }
  return ZBA_OK;
}

zba_err_t zba_led_strip_set_led(const char* seg_name, int led_index, uint8_t r, uint8_t g,
                                uint8_t b, uint8_t w)
{
  zba_led_seg_t* curStrip = NULL;
  if (seg_name == NULL)
  {
    return ZBA_LED_INVALID;
  }

  curStrip = led_state.led_segments;
  if (curStrip && led_state.led_memory)
  {
    for (size_t i = 0; i < led_state.num_led_segments; ++i)
    {
      if (0 == strcmp(seg_name, curStrip->name))
      {
        // found strip
        int ppl = pixels_per_led(curStrip->led_type);
        if (led_index == -1)
        {
          // Set entire strip
          // {TODO} keeping this simple while bringing it up, optimize later
          for (size_t j = 0; j < curStrip->num_leds; j++)
          {
            size_t offset                = curStrip->first_pixel_index + j * ppl;
            led_state.led_memory[offset] = g;
            if (ppl > 1) led_state.led_memory[offset + 1] = r;
            if (ppl > 2) led_state.led_memory[offset + 2] = b;
            if (ppl > 3) led_state.led_memory[offset + 3] = w;
          }
        }
        else
        {
          // Set just the pixel
          size_t offset = curStrip->first_pixel_index + led_index * ppl;

          led_state.led_memory[offset] = g;
          if (ppl > 1) led_state.led_memory[offset + 1] = r;
          if (ppl > 2) led_state.led_memory[offset + 2] = b;
          if (ppl > 3) led_state.led_memory[offset + 3] = w;
        }
        return ZBA_OK;
      }
      curStrip++;
    }
  }
  ZBA_ERR("Invalid LED set request %s:%d", seg_name, led_index);
  return ZBA_LED_INVALID;
}

zba_err_t zba_led_strip_get_led(const char* seg_name, int led_index, zba_led_type_t* led_type,
                                uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* w)
{
  zba_led_seg_t* curStrip = NULL;
  if ((led_index == -1) || (seg_name == NULL))
  {
    return ZBA_LED_INVALID;
  }

  curStrip = led_state.led_segments;
  if (curStrip && led_state.led_memory)
  {
    for (size_t i = 0; i < led_state.num_led_segments; ++i)
    {
      if (0 == strcmp(seg_name, curStrip->name))
      {
        // found strip
        int ppl       = pixels_per_led(curStrip->led_type);
        size_t offset = curStrip->first_pixel_index + led_index * ppl;

        if (g) *g = led_state.led_memory[offset];
        if (r && (ppl > 1)) *r = led_state.led_memory[offset + 1];
        if (b && (ppl > 2)) *b = led_state.led_memory[offset + 2];
        if (w && (ppl > 3)) *w = led_state.led_memory[offset + 3];
        return ZBA_OK;
      }
    }
  }
  return ZBA_LED_INVALID;
}

// Retrieve an initialized segment by name. If null, gets the first one.
zba_led_seg_t* zba_led_strip_get_segment(const char* seg_name);

// Retrieve number of segments.
size_t zba_led_strip_get_num_segments();

/// Once you're done setting the LEDs, call flip() to
/// make the buffer active.
zba_err_t zba_led_strip_flip()
{
  if ((NULL == led_state.led_memory) || (NULL == led_state.led_buffer)) return ZBA_LED_ERROR;
  ZBA_LOCK(led_state.buffer_mutex);
  {
    if (led_state.filter)
    {
      led_state.filter(led_state.led_buffer, led_state.led_memory, led_state.led_memory_size);
    }
    else
    {
      memcpy(led_state.led_buffer, led_state.led_memory, led_state.led_memory_size);
    }
    led_state.led_buffer_dirty = true;
  }
  ZBA_UNLOCK(led_state.buffer_mutex);
  return ZBA_OK;
}

/// Updates the led strip
zba_err_t zba_led_strip_refresh()
{
  zba_err_t zba_error = ZBA_LED_WRITE_FAILED;
  size_t timeout_ms   = 100;
  int64_t elapsed;
  static size_t counter = 0;
  static float accum    = 0;

  // Bail if we don't have a new buffer yet - no point in locking or doing work.
  if (!led_state.led_buffer_dirty) return ZBA_OK;

  elapsed = zba_elapsed_usec(led_state.last_refresh);
  if (elapsed < WS2812_RESET_DELAY_MICROSEC)
  {
    vTaskDelay(1);
  }

  // Lock the buffer mutex while we're reading from it
  uint64_t start = esp_timer_get_time();

  ZBA_LOCK(led_state.buffer_mutex);
  // int64_t start = esp_timer_get_time();
  if (ESP_OK ==
      rmt_write_sample(ZBA_LED_RMT_CHANNEL, led_state.led_buffer, led_state.led_memory_size, true))
  {
    if (ESP_OK == rmt_wait_tx_done(ZBA_LED_RMT_CHANNEL, pdMS_TO_TICKS(timeout_ms)))
    {
      led_state.led_buffer_dirty = false;

      zba_error = ZBA_OK;
    }
  }
  ZBA_UNLOCK(led_state.buffer_mutex);
  led_state.last_refresh = esp_timer_get_time();
  accum += zba_elapsed_sec(start);
  counter++;
  if (0 == (counter % 1000))
  {
    ZBA_LOG("LED Refresh time: %f (accum: %f, count: %u)", accum / counter, accum, counter);
  }

  if (zba_error != ZBA_OK)
  {
    ZBA_ERR("Refresh LED strip failed");
  }
  return zba_error;
}

void led_update_task(void* context)
{
  ZBA_LOG("Entering LED update task.");
  while (!led_state.exiting)
  {
    bool updated = false;
    // If we have an animator, call it before refreshing the strip;
    // flip buffers if it returns true for any segments.
    if (led_state.animator)
    {
      zba_led_seg_t* curSegment = led_state.led_segments;
      for (size_t i = 0; i < led_state.num_led_segments; ++i)
      {
        updated |= led_state.animator(curSegment);
        curSegment++;
      }
      led_state.led_buffer_dirty = updated;
      zba_led_strip_refresh();
      // Animators can do their own delay.
      vTaskDelay(2);
    }
    else
    {
      zba_led_strip_refresh();
      vTaskDelay(kLEDRefreshSpeedMS / portTICK_PERIOD_MS);
    }
  }

  ZBA_LOG("Exiting LED update task");
  zba_led_strip_clear();
  zba_led_strip_flip();
  zba_led_strip_refresh();
  vTaskDelete(led_state.led_task);
}

// This gets called by the rmt_translator to convert LED data into Remote Tranceiver output
static void IRAM_ATTR ws2812_sample_to_rmt(const void* src, rmt_item32_t* dest, size_t src_size,
                                           size_t wanted_num, size_t* translated_size,
                                           size_t* item_num)
{
  uint8_t curByte;
  uint8_t j;
  // Encode the minimum of the number of bytes we have or the number of encoded bits they want
  // Right now assuming they'll always want a multiple of 8 bits...
  const size_t wanted_bytes  = wanted_num / 8;
  size_t num_bytes_to_decode = (src_size < wanted_bytes) ? (src_size) : (wanted_bytes);
  uint8_t* src_ptr           = (uint8_t*)src;

  // Not sure if this is useful with this compiler/chip, should check.
  // but get the bit encodings into registers/stack
  // const rmt_item32_t bit0 = led_state.rmt_bit0;
  // const rmt_item32_t bit1 = led_state.rmt_bit1;

  *translated_size = num_bytes_to_decode;
  *item_num        = (num_bytes_to_decode * 8);

  while (num_bytes_to_decode != 0)
  {
    curByte = *src_ptr;
    for (j = 0; j < 8; ++j)
    {
      // If high bit is set, send a 1
      if (curByte & 0x80)
      {
        dest->val = led_state.rmt_bit1.val;
      }
      else
      {
        // Otherwise send a 0
        dest->val = led_state.rmt_bit0.val;
      }
      // Shift it left a bit for next check
      curByte = curByte << 1;
      ++dest;
    }
    ++src_ptr;
    --num_bytes_to_decode;
  }
}

void zba_led_strip_set_animator(ZBA_LED_ANIMATOR_CB animator)
{
  led_state.animator = animator;
}

void zba_led_strip_set_filter(ZBA_LED_FILTER_CB filter)
{
  led_state.filter = filter;
}

// Sample animator - takes what's given in the runtime memory and cycles hue on it into buffer
bool zba_hue_cycle_animator(zba_led_seg_t* segment)
{
  // Only supporting RGBW segments with this one for simplicity.
  static uint8_t hueVal = 0;
  GRBWPixel_t* src      = (GRBWPixel_t*)segment->data;
  GRBWPixel_t* dst      = (GRBWPixel_t*)segment->data_buf;

  if (segment->led_type != LED_STRIP_RGBW)
  {
    return false;
  }

  for (int i = 0; i < segment->num_leds; ++i)
  {
    HSVPixel_t hsv = rgbw2hsv(*src);
    hsv.h          = (hsv.h + hueVal) % 180;
    *dst           = hsv2rgbw(hsv);
    dst++;
    src++;
  }
  hueVal = (hueVal + 1) % 180;

  return true;
}

// Sample animator - takes what's given in the runtime memory and chases
bool zba_chase_animator(zba_led_seg_t* segment)
{
  static int offset = 0;
  // Only supporting RGBW segments with this one for simplicity.
  GRBWPixel_t* src = (GRBWPixel_t*)segment->data;
  GRBWPixel_t* dst = (GRBWPixel_t*)segment->data_buf;

  if (segment->led_type != LED_STRIP_RGBW)
  {
    return false;
  }

  for (int i = 1; i < segment->num_leds; ++i)
  {
    *dst = src[(i + offset) % segment->num_leds];
    dst++;
  }
  vTaskDelay(30);
  offset++;
  return true;
}

// int 3.748935 float 5.954633 count: 60000

// clang-format off
HSVPixel_t rgbw2hsv(GRBWPixel_t rgbw)
{
  float r     = rgbw.r / 255.0f;
  float g     = rgbw.g / 255.0f;
  float b     = rgbw.b / 255.0f;
  // ignoring W
  float h;
  float max   = ZBA_MAX_FLOAT3(r, g, b);
  float min   = ZBA_MIN_FLOAT3(r, g, b);
  float range = max - min;
  HSVPixel_t hsvPixel = {0};

  hsvPixel.v = (uint8_t)(max*255);

  if ((max < __FLT_EPSILON__) || (range < __FLT_EPSILON__))
  {
    return hsvPixel;
  }
  
  hsvPixel.s = (uint8_t)(((range) / max)*255);

  if      (max == r)  h = 60 * ((g - b) / (range));
  else if (max == g)  h = 60 * ((b - r) / (range)) + 120;
  else                h = 60 * ((r - g) / (range)) + 240;

  if (h < 0) h += 360.0f;
  hsvPixel.h = (uint8_t)(h / 2);
  return hsvPixel;
}

#define TOBYTE(x) (uint8_t)(x*255)
GRBWPixel_t hsv2rgbw(HSVPixel_t hsv)
{
  float h = hsv.h*2.0f;
  float s = hsv.s / 255.0f;
  float v = hsv.v / 255.0f;
  int hi  = (int)(h / 60.0f) % 6;
  float f = (h / 60.0f) - hi;
  float p = v * (1.0f - s);
  float q = v * (1.0f - s * f);
  float t = v * (1.0f - s * (1.0f - f));
  GRBWPixel_t px;

  switch (hi)
  {
    case 0:  px.r = TOBYTE(v); px.g = TOBYTE(t); px.b = TOBYTE(p); break;
    case 1:  px.r = TOBYTE(q); px.g = TOBYTE(v); px.b = TOBYTE(p); break;
    case 2:  px.r = TOBYTE(p); px.g = TOBYTE(v); px.b = TOBYTE(t); break;
    case 3:  px.r = TOBYTE(p); px.g = TOBYTE(q); px.b = TOBYTE(v); break;
    case 4:  px.r = TOBYTE(t); px.g = TOBYTE(p); px.b = TOBYTE(v); break;
    default: px.r = TOBYTE(v); px.g = TOBYTE(p); px.b = TOBYTE(q); break;
  }
  px.w = 0;
  return px;
}
// clang-format on
