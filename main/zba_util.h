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
  int64_t zba_now_ms();

  float zba_elapsed_sec(int64_t prev_time);
  float zba_elapsed_ms(int64_t prev_time);
  float zba_elapsed_usec(int64_t prev_time);

  void zba_delay_ms(uint32_t ms);

  static int32_t __inline ZBA_CLAMP(int32_t min, int32_t max, int32_t val)
  {
    if (val < min) return min;
    if (val > max) return max;
    return val;
  }

  /// Min/max. Ugh. Templates are nice sometimes.
  /// Macros would work, but dislike possible side effects.
  static __inline size_t ZBA_MIN(size_t a, size_t b)
  {
    return (a < b) ? a : b;
  }
  static __inline size_t ZBA_MAX(size_t a, size_t b)
  {
    return (a > b) ? a : b;
  }

  static __inline float ZBA_MAX_FLOAT(float a, float b)
  {
    return (a > b) ? a : b;
  }
  static __inline float ZBA_MIN_FLOAT(float a, float b)
  {
    return (a < b) ? a : b;
  }

  static __inline uint8_t ZBA_MAX_BYTE(uint8_t a, uint8_t b)
  {
    return (a > b) ? a : b;
  }

  static __inline uint8_t ZBA_MIN_BYTE(uint8_t a, uint8_t b)
  {
    return (a < b) ? a : b;
  }

  static __inline uint8_t ZBA_MAX_BYTE3(uint8_t a, uint8_t b, uint8_t c)
  {
    return ZBA_MAX_BYTE(ZBA_MAX_BYTE(a, b), c);
  }

  static __inline uint8_t ZBA_MIN_BYTE3(uint8_t a, uint8_t b, uint8_t c)
  {
    return ZBA_MIN_BYTE(ZBA_MIN_BYTE(a, b), c);
  }

  static __inline float ZBA_MAX_FLOAT3(float a, float b, float c)
  {
    return ZBA_MAX_FLOAT(ZBA_MAX_FLOAT(a, b), c);
  }
  static __inline float ZBA_MIN_FLOAT3(float a, float b, float c)
  {
    return ZBA_MIN_FLOAT(ZBA_MIN_FLOAT(a, b), c);
  }

  uint8_t zba_hex_to_byte(const char* asciiHex);

  uint8_t zba_char_to_nibble(char ascii);

  /// Log flags
  typedef enum zba_log_flags
  {
    ZBA_LOG_NONE  = 0x00,
    ZBA_LOG_ERROR = 0x10
  } zba_log_flags_t;

// Sets a bit flag in val
#define ZBA_SET_BIT(val, flag) val |= (flag)
// Removes a bit flag from val
#define ZBA_UNSET_BIT(val, flag) val &= (~flag)
// Returns true if all bits in flag are set in val
#define ZBA_TEST_BIT(val, flag) ((val & (flag)) == (flag))

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

/// Specifies the variable declared/defined by declare/define ZBA_MODULE
#define ZBA_MODULE_INITIALIZED(x) x##_initialized

/// Sets the initialized variable to the result
#define ZBA_SET_INIT(module, result) ZBA_MODULE_INITIALIZED(module) = result

/// On successful deinit, sets the module to uninitialized. Otherwise, keeps error
#define ZBA_SET_DEINIT(module, result) \
  ZBA_MODULE_INITIALIZED(module) = (ZBA_OK == result) ? ZBA_MODULE_NOT_INITIALIZED : result

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

#define ZBA_TRY_LOCK(x, y) \
  ZBA_LOG("Waiting " #x);  \
  xSemaphoreTake(x, y);    \
  ZBA_LOG("Locking " #x)

#define ZBA_UNLOCK(x)       \
  ZBA_LOG("Unlocking " #x); \
  xSemaphoreGive(x)

#else  // ZBA_DEBUG_LOCKS

#define ZBA_LOCK(x)        xSemaphoreTake(x, portMAX_DELAY)
#define ZBA_TRY_LOCK(x, y) xSemaphoreTake(x, y)
#define ZBA_UNLOCK(x)      xSemaphoreGive(x)

#endif  // ZBA_DEBUG_LOCKS

  /// Do any log/util initialization we need
  zba_err_t zba_util_init();

#ifdef __cplusplus
}
#endif

#endif  // ZEBRAL_ESP32CAM_ZBA_UTIL_H_
