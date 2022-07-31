#include "zba_i2c.h"
#include <driver/i2c.h>
#include "zba_pins.h"

DEFINE_ZBA_MODULE(zba_i2c);

// I2C devices:
//  AW9523 GPIO extender @ 0x58
//  Right now we're just using it as a digital output, but it can do a lot more.
//    https://cdn-shop.adafruit.com/product-files/4886/AW9523+English+Datasheet.pdf
//    https://www.adafruit.com/product/4886
//

#define AW9523_ADDR           0x58
#define AW9523_LED_MODE       0x3   ///< Special pinMode() macro for constant current
#define AW9523_REG_CHIPID     0x10  ///< Register for hardcode chip ID
#define AW9523_REG_SOFTRESET  0x7F  ///< Register for soft resetting
#define AW9523_REG_INPUT0     0x00  ///< Register for reading input values
#define AW9523_REG_OUTPUT0    0x02  ///< Register for writing output values
#define AW9523_REG_CONFIG0    0x04  ///< Register for configuring direction
#define AW9523_REG_INTENABLE0 0x06  ///< Register for enabling interrupt
#define AW9523_REG_GCR        0x11  ///< Register for general configuration
#define AW9523_REG_LEDMODE    0x12  ///< Register for configuring const current
#define I2C_FREQ              400000

typedef struct
{
  int port;
  int timeout_ms;
  uint8_t lowBits;
  uint8_t highBits;
} zba_i2c_state_t;

zba_i2c_state_t i2c_state = {.port = I2C_NUM_0, .timeout_ms = 1000, .lowBits = 0, .highBits = 0};

esp_err_t zba_i2c_read(uint8_t dev_address, uint8_t register, uint8_t* data, size_t len);
esp_err_t zba_i2c_read_byte(uint8_t dev_address, uint8_t register, uint8_t* dataByte);
esp_err_t zba_i2c_write(uint8_t dev_address, uint8_t* data, size_t len);
esp_err_t zba_i2c_write_byte(uint8_t dev_address, uint8_t register, uint8_t dataByte);
zba_err_t zba_i2c_aw9523_init();

/// Resolution as defined above
zba_err_t zba_i2c_init()
{
  zba_err_t result = ZBA_OK;

  i2c_config_t i2c_cfg = {
      .mode             = I2C_MODE_MASTER,
      .sda_io_num       = PIN_I2C_SDA,
      .scl_io_num       = PIN_I2C_SCL,
      .sda_pullup_en    = GPIO_PULLUP_ENABLE,
      .scl_pullup_en    = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_FREQ,
  };

  if (ESP_OK != i2c_param_config(i2c_state.port, &i2c_cfg))
  {
    ZBA_ERR("Failed to configure I2C.");
    result = ZBA_I2C_INIT_ERROR;
  }
  else if (ESP_OK != i2c_driver_install(i2c_state.port, i2c_cfg.mode, 0, 0, 0))
  {
    ZBA_ERR("Failed to initialize I2C driver");
    result = ZBA_I2C_INIT_ERROR;
  }

  if (ZBA_OK == result)
  {
    // Initialize the AW9523
    result = zba_i2c_aw9523_init();
  }

  ZBA_SET_INIT(zba_i2c, result);
  return result;
}

/// Deinitialize the camera
zba_err_t zba_i2c_deinit()
{
  zba_err_t deinit_error = ZBA_OK;
  if (ESP_OK != i2c_driver_delete(i2c_state.port))
  {
    deinit_error = ZBA_I2C_DEINIT_ERROR;
  }
  // ...
  ZBA_SET_DEINIT(zba_i2c, deinit_error);
  return deinit_error;
}

zba_err_t zba_i2c_aw9523_init()
{
  zba_err_t res  = ZBA_I2C_ERROR;
  uint8_t chipId = 0;

  if (ESP_OK != zba_i2c_write_byte(AW9523_ADDR, AW9523_REG_SOFTRESET, 0))
  {
    ZBA_ERR("Failed to reset device.");
  }

  if (ESP_OK != zba_i2c_read(AW9523_ADDR, AW9523_REG_CHIPID, &chipId, sizeof(chipId)))
  {
    ZBA_ERR("Failed i2c request for chip id for AW9523. Is it connected and powered?");
  }
  else
  {
    if (chipId == 0x23)
    {
      ZBA_LOG("Found AW9523!");
      res = ZBA_OK;
    }
    else
    {
      ZBA_LOG("Unknown device found at 0x58 (Looking for an AW9523)");
    }
  }

  if (res != ZBA_OK)
  {
    return res;
  }

  // Ok, so we got hardware that responds - set the AW9523 up.
  // First set it to all outputs.
  if (ESP_OK != zba_i2c_write_byte(AW9523_ADDR, AW9523_REG_CONFIG0, 0))
  {
    ZBA_ERR("Failed writing output config 0.");
    return ZBA_I2C_ERROR;
  }
  else if (ESP_OK != zba_i2c_write_byte(AW9523_ADDR, AW9523_REG_CONFIG0 + 1, 0))
  {
    ZBA_ERR("Failed writing output config 1.");
    return ZBA_I2C_ERROR;
  }

  if (ESP_OK != zba_i2c_write_byte(AW9523_ADDR, AW9523_REG_LEDMODE, 0xFF))
  {
    ZBA_ERR("Failed writing output config 0.");
    return ZBA_I2C_ERROR;
  }
  else if (ESP_OK != zba_i2c_write_byte(AW9523_ADDR, AW9523_REG_LEDMODE + 1, 0xFF))
  {
    ZBA_ERR("Failed writing output config 1.");
    return ZBA_I2C_ERROR;
  }

  // Now set all the outputs to 0
  if (ZBA_OK != (res = zba_i2c_aw9523_set_pins(0, 0)))
  {
    return res;
  }

  if (ESP_OK != zba_i2c_write_byte(AW9523_ADDR, AW9523_REG_INTENABLE0, 0x00))
  {
    ZBA_ERR("Failed disabling interrupts 0");
    return ZBA_I2C_ERROR;
  }
  if (ESP_OK != zba_i2c_write_byte(AW9523_ADDR, AW9523_REG_INTENABLE0 + 1, 0x00))
  {
    ZBA_ERR("Failed disabling interrupts 1");
    return ZBA_I2C_ERROR;
  }

  return ZBA_OK;
}

zba_err_t zba_i2c_aw9523_set_pins(uint8_t lowPins, uint8_t highPins)
{
  // Now set all the outputs to 0
  ZBA_LOG("Setting pins to 0x%02X : 0x%02X", lowPins, highPins);

  if (ESP_OK != zba_i2c_write_byte(AW9523_ADDR, AW9523_REG_OUTPUT0, ~lowPins))
  {
    ZBA_ERR("Failed setting low pins to 0x%02x", lowPins);
    return ZBA_I2C_ERROR;
  }
  i2c_state.lowBits = lowPins;

  if (ESP_OK != zba_i2c_write_byte(AW9523_ADDR, AW9523_REG_OUTPUT0 + 1, ~highPins))
  {
    ZBA_ERR("Failed setting high pins to 0x%02x", highPins);
    return ZBA_I2C_ERROR;
  }
  i2c_state.highBits = highPins;

  return ZBA_OK;
}

zba_err_t zba_i2c_aw9523_set_pin(uint8_t pin, bool val)
{
  int regaddr = AW9523_REG_OUTPUT0;
  int pin_bit;

  if (pin >= 8)
  {
    regaddr++;
    pin_bit = 1 << (pin - 8);
  }
  else
  {
    pin_bit = 1 << pin;
  }

  uint8_t currentPins = 0;
  if (ESP_OK != zba_i2c_read_byte(AW9523_ADDR, regaddr, &currentPins))
  {
    ZBA_ERR("Failed to read pins before setting.");
    return ZBA_I2C_ERROR;
  }

  ZBA_LOG("Pins around %d: 0x%02x", pin, currentPins);

  // inverted!
  uint8_t setPins = (currentPins & ~pin_bit) | (val ? 0 : pin_bit);
  ZBA_LOG("Setting to (%d = %d): 0x%02x", pin, val ? 1 : 0, setPins);

  if (ESP_OK != zba_i2c_write_byte(AW9523_ADDR, regaddr, setPins))
  {
    ZBA_ERR("Failed to write pins.");
  }

  if (pin >= 8)
  {
    i2c_state.highBits = ~setPins;
  }
  else
  {
    i2c_state.lowBits = ~setPins;
  }

  return ZBA_OK;
}

esp_err_t zba_i2c_read(uint8_t dev_address, uint8_t reg, uint8_t* data, size_t len)
{
  return i2c_master_write_read_device(i2c_state.port, dev_address, &reg, 1, data, len,
                                      i2c_state.timeout_ms / portTICK_PERIOD_MS);
}

esp_err_t zba_i2c_read_byte(uint8_t dev_address, uint8_t reg, uint8_t* data)
{
  return zba_i2c_read(dev_address, reg, data, 1);
}

esp_err_t zba_i2c_write(uint8_t dev_address, uint8_t* data, size_t len)
{
  return i2c_master_write_to_device(i2c_state.port, dev_address, data, len,
                                    i2c_state.timeout_ms / portTICK_PERIOD_MS);
}

esp_err_t zba_i2c_write_byte(uint8_t dev_address, uint8_t reg, uint8_t dataByte)
{
  uint8_t write_buf[2] = {reg, dataByte};
  return zba_i2c_write(dev_address, &write_buf[0], 2);
}

// Gets the low byte of output pins (pins 0-7)
uint8_t zba_i2c_aw9523_get_out_low()
{
  return i2c_state.lowBits;
}
// Gets the low byte of output pins (pins 8-15)
uint8_t zba_i2c_aw9523_get_out_high()
{
  return i2c_state.highBits;
}
// Get what we've sent as an output
bool zba_i2c_aw9523_get_out_pin(uint8_t pin)
{
  if (pin >= 8)
  {
    return i2c_state.highBits & (1 << (pin - 8));
  }
  else
  {
    return i2c_state.highBits & (1 << pin);
  }
}
