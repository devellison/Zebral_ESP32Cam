#ifndef ZEBRAL_ESP32CAM_ZBA_SD_H_
#define ZEBRAL_ESP32CAM_ZBA_SD_H_

/// {TODO} Right now, there are some conflicts with the SD Card.
///        The white LED will flash because it uses one of the same lines.
///        That's annoying, but ok.
///
///        But - if an SD card is present and the SD code is used, it currently
///        leaves the system in a state where it can not be flashed unless
///        the SD card is removed.
///
///        Note that if the SD card is NOT initialized, it can sometimes flash
///        just fine with it in.  Rebooting, however, doesn't necessarily get it
///        out of that state.
///
#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

  DECLARE_ZBA_MODULE(zba_sd);
  /// Initializes SD system
  /// \return ZBA_OK on success
  zba_err_t zba_sd_init();

  /// Deinitializes SD system.
  /// {TODO} Still doesn't really let go of everything needed for flashing unless the
  ///        card is removed.
  zba_err_t zba_sd_deinit();

  const char* zba_sd_get_root();

  /// File callback.
  /// Returns true to continue enumeration, false to exit.
  typedef bool (*zba_file_callback)(const char* path, void* context, int depth);

  zba_err_t zba_sd_enum_files(const char* path, zba_file_callback cb, void* context, bool recurse,
                              int curDepth);
  zba_err_t zba_sd_list_files();

#ifdef __cplusplus
}
#endif

#endif  // ZEBRAL_ESP32CAM_ZBA_SD_H_
