#ifndef ZEBRAL_ESP32CAM_ZBA_UTIL_H_
#define ZEBRAL_ESP32CAM_ZBA_UTIL_H_

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "zba_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

  int64_t zba_now();
  float zba_elapsed_sec(int64_t prev_time);
  float zba_elapsed_ms(int64_t prev_time);
  float zba_elapsed_usec(int64_t prev_time);

  static __inline size_t ZBA_MIN(size_t a, size_t b)
  {
    return (a < b) ? a : b;
  }
  static __inline size_t ZBA_MAX(size_t a, size_t b)
  {
    return (a > b) ? a : b;
  }

  static __inline uint8_t ZBA_MAX_BYTE(uint8_t a, uint8_t b)
  {
    return (a > b) ? a : b;
  }

  static __inline uint8_t ZBA_MAX_BYTE3(uint8_t a, uint8_t b, uint8_t c)
  {
    return ZBA_MAX_BYTE(ZBA_MAX_BYTE(a, b), c);
  }

  static __inline uint8_t ZBA_MIN_BYTE(uint8_t a, uint8_t b)
  {
    return (a < b) ? a : b;
  }

  static __inline uint8_t ZBA_MIN_BYTE3(uint8_t a, uint8_t b, uint8_t c)
  {
    return ZBA_MIN_BYTE(ZBA_MIN_BYTE(a, b), c);
  }

  /// Log flags
  typedef enum zba_log_flags
  {
    ZBA_LOG_NONE  = 0x00,
    ZBA_LOG_ERROR = 0x10
  } zba_log_flags_t;

// Sets a bit flag in val
#define ZBA_SET_BIT_FLAG(val, flag) val |= (flag)
// Removes a bit flag from val
#define ZBA_UNSET_BIT_FLAG(val, flag) val &= (~flag)
// Returns true if all bits in flag are set in val
#define ZBA_TEST_BIT_FLAG(val, flag) ((val & (flag)) == (flag))

/// For now, just an error code everyone can use to check
/// if the module has been initialized, or if not, if an error happened
/// that caused it not to.
/// It'll be ZBA_OK if initialized without errors.
#define DECLARE_ZBA_MODULE(x) extern zba_err_t x##_initialized;

  /// Declare the zba_util_module structure.
  DECLARE_ZBA_MODULE(zba_util);

/// Defines the public data struct for a module.
/// This goes in the module C file.
/// Note that it includes the ZBA_TAG so only one macro is needed.
#define DEFINE_ZBA_MODULE(x)                              \
  zba_err_t x##_initialized = ZBA_MODULE_NOT_INITIALIZED; \
  static const char* TAG    = __FILE__

#define ZBA_MODULE_INITIALIZED(x) x##_initialized

/// This should be at the top of each c file that isn't a module.
/// It creates a logging identifier for esp_log so we're not peppering
/// __FILE___ everywhere (do compilers optimize that these days?)
#define DEFINE_ZBA_TAG static const char* TAG = __FILE__

/// Logging macro
#define ZBA_LOG(...) ESP_LOGI(TAG, __VA_ARGS__)

/// Error log macro
#define ZBA_ERR(...) ESP_LOGE(TAG, __VA_ARGS__)

/// If turned on, this will emit debug prints
/// anywhere mutexes are used.
/// Noisy, but on occasion, helpful.
#define ZBA_DEBUG_LOCKS 0
#if ZBA_DEBUG_LOCKS

#define ZBA_LOCK(x)                 \
  ZBA_LOG("Waiting " #x);           \
  xSemaphoreTake(x, portMAX_DELAY); \
  ZBA_LOG("Locking " #x)

#define ZBA_UNLOCK(x)       \
  ZBA_LOG("Unlocking " #x); \
  xSemaphoreGive(x)

#else  // ZBA_DEBUG_LOCKS

#define ZBA_LOCK(x)   xSemaphoreTake(x, portMAX_DELAY)
#define ZBA_UNLOCK(x) xSemaphoreGive(x)

#endif  // ZBA_DEBUG_LOCKS

  /// Do any log/util initialization we need
  zba_err_t zba_util_init();

#ifdef __cplusplus
}
#endif

#endif  // ZEBRAL_ESP32CAM_ZBA_UTIL_H_
