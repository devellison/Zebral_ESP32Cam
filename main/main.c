#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#include "zba_camera.h"
#include "zba_commands.h"
#include "zba_config.h"
#include "zba_led.h"
#include "zba_pins.h"
#include "zba_sd.h"
#include "zba_stream.h"
#include "zba_util.h"
#include "zba_web.h"
#include "zba_wifi.h"

DEFINE_ZBA_TAG;

void app_init()
{
  ZBA_LOG("Initializing System...");
  zba_led_init();
  zba_config_init();
  zba_stream_init(true);
  zba_camera_init(ZBA_VGA);
  zba_wifi_init();
  zba_web_init();
  // Skip this, since once SD is initialized and used,
  // we have to eject the SD to flash.
  // zba_sd_init();
  ZBA_LOG("System Initialized.");
}

void app_deinit()
{
  ZBA_LOG("Deininitializing System...");
  zba_sd_deinit();
  zba_web_deinit();
  zba_wifi_deinit();
  zba_camera_deinit();
  zba_stream_deinit();
  zba_config_deinit();
  zba_led_deinit();
  ZBA_LOG("System Deinitialized.");
}

void test_pins()
{
  // Testing pins
  int zba_pins[] = {PIN_MODULE_2, PIN_MODULE_3, PIN_MODULE_4, PIN_MODULE_5, PIN_MODULE_6};
  int num_pins   = sizeof(zba_pins) / sizeof(int);

  for (int i = 0; i < num_pins; ++i)
  {
    zba_pin_mode(zba_pins[i], PIN_MODE_DIGITAL_OUT);
  }

  int current = 0;
  for (;;)
  {
    ZBA_LOG("pin %d [%d]", current, zba_pins[current]);
    for (int i = 0; i < num_pins; i++)
    {
      zba_pin_digital_write(zba_pins[i], (i == current) ? PIN_HIGH : PIN_LOW);
    }

    current = (current + 1) % num_pins;

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
void test_led_strips()
{
  ///============
  zba_led_strip_set_led("Color", 0, 64, 0, 0, 0);
  zba_led_strip_set_led("Color", 1, 64, 64, 0, 0);
  zba_led_strip_set_led("Color", 2, 0, 64, 0, 0);
  zba_led_strip_set_led("Color", 3, 0, 64, 64, 0);
  zba_led_strip_set_led("Color", 4, 0, 0, 64, 0);
  zba_led_strip_set_led("Color", 5, 64, 0, 64, 0);
  zba_led_strip_set_led("Color", 6, 0, 0, 0, 64);
  zba_led_strip_set_led("Color", 6, 50, 50, 50, 50);
  zba_led_strip_set_led("UV", 0, 64, 0, 0, 0);
  zba_led_strip_set_led("UV", 1, 64, 64, 0, 0);
  zba_led_strip_set_led("UV", 2, 64, 64, 64, 0);
  ///============
  zba_led_strip_flip();
  // zba_led_strip_set_animator(zba_hue_cycle_animator);
  zba_led_strip_set_animator(zba_chase_animator);
}

void app_main()
{
  zba_util_init();
  ZBA_LOG("Utils initialized.");
  app_init();
  test_led_strips();

  for (;;)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
