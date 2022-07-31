/// Handler for i2c devices we might use
#ifndef ZEBRAL_ESP32CAM_ZBA_I2C_H_
#define ZEBRAL_ESP32CAM_ZBA_I2C_H_

#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

  DECLARE_ZBA_MODULE(zba_i2c);
  /// Resolution as defined above
  zba_err_t zba_i2c_init();

  /// Deinitialize the camera
  zba_err_t zba_i2c_deinit();

  // We're just using the AW9523 as an output device for all pins right now.
  // Lots more that it's capable of, just have to code it up if we use it.
  // Note that set_pin currently reads state from the chip and modifies it,
  // where as set_pins() just sets it all.
  //
  // Also, the get() functions currently return what we've SET, starting at 0.
  // This is faster, but it could get out of sync until a set_pin()? or a
  // set_pins() overwrites?  At init time we set it all off.

  // This actually does a read first, then sets the pin specifically in the value.
  // It will resync us if somehow our local lowBits/highBits are out of sync
  // (for the low OR high)
  zba_err_t zba_i2c_aw9523_set_pin(uint8_t pin, bool val);

  // Sets all the pins at once. Does not do a read.
  zba_err_t zba_i2c_aw9523_set_pins(uint8_t lowPins, uint8_t highPins);

  // Gets the low byte of output pins (pins 0-7) from what we've set (no read)
  uint8_t zba_i2c_aw9523_get_out_low();

  // Gets the low byte of output pins (pins 8-15) from what we've set (no read)
  uint8_t zba_i2c_aw9523_get_out_high();

  // Get what we've sent as an output (no read)
  bool zba_i2c_aw9523_get_out_pin(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif  // ZEBRAL_ESP32CAM_ZBA_I2C_H_
