#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#include "zba_auth.h"
#include "zba_camera.h"
#include "zba_commands.h"
#include "zba_config.h"
#include "zba_led.h"
#include "zba_pins.h"
#include "zba_sd.h"
#include "zba_stream.h"
#include "zba_util.h"
#include "zba_vision.h"
#include "zba_web.h"
#include "zba_wifi.h"

DEFINE_ZBA_TAG;

/// Test LED strip config.
static zba_led_seg_t kTestLEDConfig[] = {
    {.name              = "Color",
     .led_type          = LED_STRIP_RGBW,
     .num_leds          = 8,
     .num_pixels        = 0,
     .first_pixel_index = 0,
     .first_led_index   = 0,
     .data              = NULL,
     .data_buf          = NULL},
    {.name              = "UV",
     .led_type          = LED_STRIP_UV3,
     .num_leds          = 3,
     .num_pixels        = 0,
     .first_pixel_index = 0,
     .first_led_index   = 0,
     .data              = NULL,
     .data_buf          = NULL},
};

void app_init()
{
  ZBA_LOG("Initializing System...");
  zba_led_strip_cfg(kTestLEDConfig, sizeof(kTestLEDConfig) / sizeof(zba_led_seg_t));
  zba_led_init();
  zba_config_init();
  zba_auth_init();
  zba_stream_init(true);
  zba_camera_init(ZBA_SXGA);
  zba_wifi_init();
  zba_web_init();
  zba_vision_init();
  // Skip this. LED and SD conflict, and once SD has been used
  // you also can't program until the SD is removed.
  // zba_sd_init();
  ZBA_LOG("System Initialized.");
}

void app_deinit()
{
  ZBA_LOG("Deininitializing System...");
  zba_vision_deinit();
  zba_sd_deinit();
  zba_web_deinit();
  zba_wifi_deinit();
  zba_camera_deinit();
  zba_stream_deinit();
  zba_auth_deinit();
  zba_config_deinit();
  zba_led_deinit();
  ZBA_LOG("System Deinitialized.");
}

void app_main()
{
  zba_util_init();
  app_init();

  for (;;)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
