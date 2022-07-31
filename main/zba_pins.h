#ifndef ZEBRAL_ESP32CAM_ZBA_PINS_H_
#define ZEBRAL_ESP32CAM_ZBA_PINS_H_
#ifdef __cplusplus
extern "C"
{
#endif

#include <driver/gpio.h>
#include <esp_system.h>
#include "zba_util.h"

  // Modes we use the pins in.
  typedef enum zba_pin_mode
  {
    PIN_MODE_DIGITAL_OUT,
    PIN_MODE_DIGITAL_IN,
    PIN_MODE_DIGITAL_IN_PULLUP,
    PIN_MODE_DIGITAL_IN_PULLDOWN,
    PIN_MODE_RESET
  } zba_pin_mode_t;

  typedef int zba_pin_digital_value_t;
  static const zba_pin_digital_value_t PIN_LOW  = 0;
  static const zba_pin_digital_value_t PIN_HIGH = 1;

  zba_err_t zba_pin_mode(int pin, zba_pin_mode_t pinMode);
  zba_err_t zba_pin_digital_write(int pin, zba_pin_digital_value_t value);
  zba_pin_digital_value_t zba_pin_digital_read(int pin);

// clang-format off
/// -------------------------------------------------------------------
/// Camera connector pinout / ESP32 -> Camera pins
///     Y0
///     Y1
#define PIN_CAM_D2    GPIO_NUM_19
#define PIN_CAM_D1    GPIO_NUM_18
#define PIN_CAM_D3    GPIO_NUM_21
#define PIN_CAM_D0    GPIO_NUM_5
#define PIN_CAM_D4    GPIO_NUM_36
#define PIN_CAM_PCLK  GPIO_NUM_22
#define PIN_CAM_D5    GPIO_NUM_39
///     DGND
#define PIN_CAM_D6    GPIO_NUM_34
#define PIN_CAM_XCLK  GPIO_NUM_0
#define PIN_CAM_D7    GPIO_NUM_35
///     DOVDD
///     DVDD
#define PIN_CAM_HREF  GPIO_NUM_23
#define PIN_CAM_PWDN  GPIO_NUM_32
#define PIN_CAM_VSYNC GPIO_NUM_25
#define PIN_CAM_RESET -1
#define PIN_CAM_SCL  GPIO_NUM_27
///     AVDD
#define PIN_CAM_SDA  GPIO_NUM_26
///     AGND
///     NC
///     PAD1
///     PAD2
/// -------------------------------------------------------------------
/// AI Thinker ESP32-CAM Module pinout
/// (camera and SD card facing up/top)
/// 
/// 5v   0              8
/// GND  1      SD      9
///      2              10 
///      3              11
///      4              12
///      5    CAMERA    13
///      6              14
///      7              15
///
/// Top-down left side
///     5v         0
///     GND        1
#define PIN_MODULE_2  GPIO_NUM_12
#define PIN_MODULE_3  GPIO_NUM_13
#define PIN_MODULE_4  GPIO_NUM_15
#define PIN_MODULE_5  GPIO_NUM_14
#define PIN_MODULE_6  GPIO_NUM_2
#define PIN_MODULE_7  GPIO_NUM_4

// Top-down right side (camera and SD card facing up/top)
//      3.3V       8
#define PIN_MODULE_9  GPIO_NUM_16
#define PIN_MODULE_10 GPIO_NUM_0
//      GND        11
//      Pwr OUT    12
#define PIN_MODULE_13 GPIO_NUM_3  ///< GPIO3,  U0RXD <- programming / serial
#define PIN_MODULE_14 GPIO_NUM_1  ///< GPIO1,  U0TXD <-/
//      GND        15
/// -------------------------------------------------------------------
// PIN_MODULE_2 and PIN_MODULE_3 seem available.
//    ^ I2C?
// PIN_MODULE_4, PIN_MODULE_5, PIN_MODULE_6 are usable if not using sd cards.
//    ^ LEDs, and just not use the SD card except at boot before LED init?
// PIN_MODULE_7 is the White LED ><
// PIN_MODULE_9 gets unhappy if touched.
// PIN_MODULE_10 is used for programming
// PIN_MODULE_13 / PIN_MODULE_14 are RX/TX for debugging / console.

// clang format-on

// LEDs 
#define PIN_LED_WHITE GPIO_NUM_4   ///< LED pin is also used for SD Card and flashes when its in use.

#define PIN_LED_STRIP_DATA PIN_MODULE_3

// I2C chain
#define PIN_I2C_SDA PIN_MODULE_5
#define PIN_I2C_SCL PIN_MODULE_6

#ifdef __cplusplus
}
#endif

#endif  // ZEBRAL_ESP32CAM_ZBA_PINS_H_
