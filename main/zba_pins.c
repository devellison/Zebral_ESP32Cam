#include "zba_pins.h"
#include "driver/gpio.h"

DEFINE_ZBA_TAG;

zba_err_t zba_pin_mode(int pin, zba_pin_mode_t pinMode)
{
  esp_err_t result;
  gpio_config_t io_conf = {};

  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;

  switch (pinMode)
  {
    case PIN_MODE_DIGITAL_IN:
      io_conf.mode = GPIO_MODE_INPUT;
      break;
    case PIN_MODE_DIGITAL_OUT:
      io_conf.mode = GPIO_MODE_OUTPUT;
      break;
    case PIN_MODE_DIGITAL_IN_PULLDOWN:
      io_conf.mode         = GPIO_MODE_INPUT;
      io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
      break;
    case PIN_MODE_DIGITAL_IN_PULLUP:
      io_conf.mode       = GPIO_MODE_INPUT;
      io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
      break;
    case PIN_MODE_RESET:
      if (ESP_OK != (result = gpio_reset_pin(pin)))
      {
        ZBA_ERR("Error resetting pin %d. ESP: %d", pin, result);
        return ZBA_PINS_RESET_ERROR;
      }
      return ZBA_OK;
  }

  io_conf.intr_type    = GPIO_INTR_DISABLE;
  io_conf.pin_bit_mask = (1 << pin);

  if (ESP_OK == (result = gpio_config(&io_conf)))
  {
    return ZBA_OK;
  }

  ZBA_ERR("Error configuring pin %d.", pin);
  return ZBA_PINS_GPIO_CONFIG_ERROR;
}

zba_err_t zba_pin_digital_write(int pin, zba_pin_digital_value_t value)
{
  if (ESP_OK == gpio_set_level((gpio_num_t)pin, (int)value))
  {
    return ZBA_OK;
  }
  return ZBA_PINS_DIGITAL_WRITE_ERROR;
}

zba_pin_digital_value_t zba_pin_digital_read(int pin)
{
  return gpio_get_level((gpio_num_t)pin) > 0 ? PIN_HIGH : PIN_LOW;
}
