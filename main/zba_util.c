#include "zba_util.h"
#include <driver/uart.h>
#include <esp_log.h>

#include <esp_timer.h>
#include <stdarg.h>
#include <stdio.h>

#include "zba_pins.h"

DEFINE_ZBA_MODULE(zba_util);

zba_err_t zba_util_init()
{
  esp_err_t esp_error;
  zba_err_t init_error = ZBA_OK;

  const uart_config_t uart_config = {.baud_rate = 115200,
                                     .data_bits = UART_DATA_8_BITS,
                                     .parity    = UART_PARITY_DISABLE,
                                     .stop_bits = UART_STOP_BITS_1,
                                     .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

  if (ESP_OK != (esp_error = uart_driver_install(UART_NUM_0, 4096, 4096, 0, NULL, 0)))
  {
    ZBA_LOG("Error installing uart driver! 0x%X", esp_error);
    init_error = ZBA_UTIL_UART_ERROR;
  }

  if (ESP_OK != (esp_error = uart_param_config(UART_NUM_0, &uart_config)))
  {
    ZBA_LOG("Error configuring UART! 0x%X", esp_error);
    init_error = ZBA_UTIL_UART_ERROR;
  }

  if (ESP_OK != (esp_error = uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, -1, -1)))
  {
    ZBA_LOG("Error Setting UART pins! 0x%X", esp_error);
    init_error = ZBA_UTIL_UART_ERROR;
  }

  // Save module initialization state.
  // For utils, we leave it as initialized as it got (it's the base level and should always work)
  ZBA_MODULE_INITIALIZED(zba_util) = init_error;
  return init_error;
}

float zba_elapsed_sec(int64_t start_time)
{
  return zba_elapsed_ms(start_time) / 1000.0;
}

float zba_elapsed_ms(int64_t start_time)
{
  float diff = zba_now() - start_time;
  return diff / 1000.0;
}

float zba_elapsed_usec(int64_t start_time)
{
  float diff = zba_now() - start_time;
  return diff;
}

int64_t zba_now()
{
  return esp_timer_get_time();
}
