#ifndef ZEBRAL_ESP32CAM_ZBA_LED_H_
#define ZEBRAL_ESP32CAM_ZBA_LED_H_
#include <stdbool.h>
#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif
  DECLARE_ZBA_MODULE(zba_led);
  /// Initializes leds
  zba_err_t zba_led_init();
  zba_err_t zba_led_deinit();

  /// Turn the white LED on the top of the board on/off
  zba_err_t zba_led_light(bool on);
  zba_err_t zba_led_light_blink();

#ifdef __cplusplus
}
#endif
#endif  // ZEBRAL_ESP32CAM_ZBA_LED_H_
