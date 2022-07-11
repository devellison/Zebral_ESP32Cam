#include "zba_led.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "zba_pins.h"
#include "zba_sd.h"
#include "zba_util.h"

DEFINE_ZBA_MODULE(zba_led);

#define WHITE_INIT 0x01
#define WHITE_ON   0x02
/// State struct
/// So - both the LED pins used right now interfere
/// with other components.
/// {TODO} work out exactly when it's safe and how to transition.
typedef struct zba_led_state
{
  int onboard_leds;
} zba_led_state_t;

static zba_led_state_t led_state = {0};

// Right now we're not initializing the LEDs until use
// due to conflicts with other systems for the pins
zba_err_t zba_led_init()
{
  zba_err_t init_error = ZBA_OK;
  if (ZBA_OK != init_error)
  {
    ZBA_ERR("Error initializing pins for leds");
  }
  ZBA_MODULE_INITIALIZED(zba_led) = init_error;
  return init_error;
}

zba_err_t zba_led_deinit()
{
  zba_err_t deinit_error = ZBA_OK;

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
