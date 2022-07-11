#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#include "zba_camera.h"
#include "zba_commands.h"
#include "zba_config.h"
#include "zba_led.h"
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

void app_main()
{
  zba_util_init();
  ZBA_LOG("Utils initialized.");
  app_init();

  for (;;)
  {
    vTaskDelay(20000 / portTICK_PERIOD_MS);
  }
}
