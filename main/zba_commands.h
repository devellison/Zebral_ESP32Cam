#ifndef ZEBRAL_ESP32CAM_ZBA_COMMANDS_H_
#define ZEBRAL_ESP32CAM_ZBA_COMMANDS_H_

#include "zba_config.h"
#include "zba_util.h"

#ifdef __cplusplus
extern "C"
{
#endif
  /// Serial stream that can issue commands
  typedef struct zba_cmd_stream
  {
    int fd;
    char buffer[kSerialBufferLength + 1];
    size_t bufPos;
    bool authed;
  } zba_cmd_stream_t;

  void zba_commands_status(const char *arg, zba_cmd_stream_t *cmd_stream);

  /// Initialize a command stream
  void zba_commands_stream_init(zba_cmd_stream_t *cmd_stream, int fd);

  /// Read any input and execute commands
  void zba_commands_stream_process(zba_cmd_stream_t *cmd_stream);

  /// Login to get access to other commands
  void zba_commands_login(const char *buffer, zba_cmd_stream_t *cmd_stream);

  /// Login to get access to other commands
  void zba_commands_logout(const char *buffer, zba_cmd_stream_t *cmd_stream);

  /// Processes a command buffer and executes commands if found,
  /// or dumps list if unknown.
  void zba_commands_process(const char *buffer, zba_cmd_stream_t *cmd_stream);

  /// Sets the SSID
  void zba_commands_set_ssid(const char *arg, zba_cmd_stream_t *cmd_stream);

  /// Sets the Password
  void zba_commands_set_wifi_pwd(const char *arg, zba_cmd_stream_t *cmd_stream);

  /// Sets the Device Password
  void zba_commands_set_device_pwd(const char *arg, zba_cmd_stream_t *cmd_stream);

  /// Reboots the device
  void zba_commands_reboot(const char *, zba_cmd_stream_t *cmd_stream);

  /// Resets to factory settings
  void zba_commands_reset(const char *, zba_cmd_stream_t *cmd_stream);

  void zba_commands_start(const char *arg, zba_cmd_stream_t *cmd_stream);
  void zba_commands_stop(const char *arg, zba_cmd_stream_t *cmd_stream);

  void zba_commands_light(const char *arg, zba_cmd_stream_t *cmd_stream);

  void zba_commands_dir(const char *arg, zba_cmd_stream_t *cmd_stream);

  void zba_commands_camera_status(const char *arg, zba_cmd_stream_t *cmd_stream);
  void zba_commands_camera_res(const char *arg, zba_cmd_stream_t *cmd_stream);
#ifdef __cplusplus
}
#endif
#endif  // ZEBRAL_ESP32CAM_ZBA_COMMANDS_H_
