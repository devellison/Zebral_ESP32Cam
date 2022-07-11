#ifndef ZEBRAL_ESP32CAM_ZBA_STREAM_H_
#define ZEBRAL_ESP32CAM_ZBA_STREAM_H_

#include <esp_log.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

  DECLARE_ZBA_MODULE(zba_stream);

#define ZBA_INVALID_FD -1
  /// If init_commands is true, will initialize a command interface
  /// on the stream.
  /// NOTE: Work in progress - right now this just does UART_0, but expect it
  ///       to become more generic for serial streams.
  zba_err_t zba_stream_init(bool init_commands);

  zba_err_t zba_stream_deinit();

#ifdef __cplusplus
}
#endif

#endif  // ZEBRAL_ESP32CAM_ZBA_STREAM_H_