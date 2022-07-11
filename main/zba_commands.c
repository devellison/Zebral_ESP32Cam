#include "zba_commands.h"
#include <esp_system.h>
#include <memory.h>
#include "zba_camera.h"
#include "zba_config.h"
#include "zba_led.h"
#include "zba_sd.h"
#include "zba_stream.h"
#include "zba_util.h"
#include "zba_web.h"
#include "zba_wifi.h"

DEFINE_ZBA_TAG;

// {TODO} In .cpp because it uses Printable and Stream.

// Function to receive commands and do something with them.
typedef void (*command_handler)(const char *arg, zba_cmd_stream_t *cmd_stream);

// Entry in our command handler list
typedef struct
{
  const char *command;
  command_handler handler;
  const char *usage;
  const char *description;
} command_entry_t;

// clang-format off

/// Commands that can be run via serial
static const command_entry_t command_handlers[] =
{
// Setup
  {"logout",   zba_commands_logout,         "logout",             "Logs out of the camera"},
  {"pwd",      zba_commands_set_device_pwd, "pwd PASSWORD",       "Sets the device password"},
  {"reboot",   zba_commands_reboot,         "reboot",             "Reboots the device"},
  {"reset",    zba_commands_reset,          "reset",              "Resets the device to factory"},
  {"ssid",     zba_commands_set_ssid,       "ssid SSID",          "Sets the SSID for WiFi"},
  {"wifi_pwd", zba_commands_set_wifi_pwd,   "wifi_pwd PASSWORD",  "Sets the password for WiFi"},
  {"status",   zba_commands_status,         "status",             "Gets the status of subsystems"},
// These are for testing  
  {"start",    zba_commands_start,          "start SUBSYSTEM",    "Start a subsystem"},
  {"stop",     zba_commands_stop,           "stop SUBSYSTEM",     "Stop a subsystem"},
// Blinkies
  {"light",    zba_commands_light,          "light [on|off]",     "Toggles the white led (front)"},
  {"dir",      zba_commands_dir,            "dir",                "Displays files on SD card"}
};
const static int num_command_handlers = sizeof(command_handlers) / sizeof(command_entry_t);


// Subsystem table to simply command-based init/deinit.
typedef zba_err_t(*zba_init_func_t)();
typedef zba_err_t(*zba_deinit_func_t)();
typedef struct 
{
  const char*       name;  
  zba_init_func_t   initFunc;
  zba_deinit_func_t deinitFunc;
  zba_err_t* init_error;
} zba_subsystem_entry_t;

#define DEFINE_ZBA_SUBSYSTEM_ENTRY(x) {#x,zba_##x##_init, zba_##x##_deinit, &ZBA_MODULE_INITIALIZED(zba_##x)}
static const zba_subsystem_entry_t zba_subsystems[] =
{
  DEFINE_ZBA_SUBSYSTEM_ENTRY(web),
  DEFINE_ZBA_SUBSYSTEM_ENTRY(wifi),
  DEFINE_ZBA_SUBSYSTEM_ENTRY(camera),
  DEFINE_ZBA_SUBSYSTEM_ENTRY(led),
  DEFINE_ZBA_SUBSYSTEM_ENTRY(config),
  DEFINE_ZBA_SUBSYSTEM_ENTRY(sd)
};
const static int num_subsystems = sizeof(zba_subsystems) / sizeof(zba_subsystem_entry_t);


static const command_entry_t unauthed_handlers[] = 
{
  // This one has no space, as it may be used w/o password if there's none set.
  {"login",  zba_commands_login,   "login [PASSWORD]", "Logs in"},
  // Allow the status command in either
  {"status", zba_commands_status,  "status", "Gets the status of subsystems"}
};
const static int num_unauthed_handlers = sizeof(unauthed_handlers) / sizeof(command_entry_t);


#define ZBA_CMD_LOG(...) ZBA_LOG(__VA_ARGS__)
// #define ZBA_CMD_LOG(x) {cmd_stream->source->println(x);}
// clang-format on

void zba_commands_stream_init(zba_cmd_stream_t *cmd_stream, int fd)
{
  memset(&cmd_stream->buffer[0], 0, sizeof(cmd_stream->buffer));
  cmd_stream->fd     = fd;
  cmd_stream->bufPos = 0;
  cmd_stream->authed = false;
}

void zba_commands_stream_process(zba_cmd_stream_t *cmd_stream)
{
  if (cmd_stream->fd == ZBA_INVALID_FD)
  {
    return;
  }

  char inByte = 0;
  // Process anything coming in on the stream
  // cmd_stream->source->setTimeout(50);
  // while (cmd_stream->source->available())
  if (false)
  {
    // inByte = cmd_stream->source->read();
    switch (inByte)
    {
      case 0:
      case '\r':
      case '\n':
        cmd_stream->buffer[cmd_stream->bufPos] = 0;
        if (cmd_stream->bufPos > 0)
        {
          zba_commands_process(cmd_stream->buffer, cmd_stream);
          cmd_stream->bufPos = 0;
        }
        break;

      default:
        cmd_stream->buffer[cmd_stream->bufPos] = inByte;
        cmd_stream->bufPos++;
        if (cmd_stream->bufPos >= sizeof(cmd_stream->buffer) - 1)
        {
          ZBA_LOG("Stream buffer overflow! Resetting buffer.");
          // cmd_stream->source->flush();
          cmd_stream->bufPos = 0;
        }
        break;
    }
  }
}

void zba_commands_process(const char *buffer, zba_cmd_stream_t *cmd_stream)
{
  const command_entry_t *handlers = cmd_stream->authed ? command_handlers : unauthed_handlers;
  int num_handlers = cmd_stream->authed ? num_command_handlers : num_unauthed_handlers;

  int i;
  for (i = 0; i < num_handlers; ++i)
  {
    if (0 == strncasecmp(buffer, handlers[i].command, strlen(handlers[i].command)))
    {
      handlers[i].handler(buffer + strlen(handlers[i].command), cmd_stream);
      return;
    }
  }

  ZBA_CMD_LOG("Unknown command.");
  ZBA_CMD_LOG("Zebral ESP32-CAM valid commands:");
  for (i = 0; i < num_handlers; ++i)
  {
    ZBA_CMD_LOG("%24s - %s", handlers[i].usage, handlers[i].description);
  }
}

void zba_commands_login(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  if (arg[0] == 0)
  {
    cmd_stream->authed = (ZBA_OK == zba_config_check_auth(NULL, arg));
    if (cmd_stream->authed)
    {
      ZBA_CMD_LOG("Logged in. Please set a password.");
    }
    else
    {
      ZBA_CMD_LOG("A password is set. Please include password with login command.");
    }
    return;
  }

  if (arg[0] != ' ')
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;

  if (strlen(arg) > kMaxPasswordLen)
  {
    ZBA_CMD_LOG("Password too long - Max length is 64 characters.");
  }
  else
  {
    cmd_stream->authed = (ZBA_OK == zba_config_check_auth(NULL, arg));
    if (cmd_stream->authed)
    {
      ZBA_CMD_LOG("Logged in.");
    }
    else
    {
      ZBA_CMD_LOG("Invalid login.");
    }
  }
}

void zba_commands_logout(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  cmd_stream->authed = false;
  ZBA_CMD_LOG("Logged out.");
}

void zba_commands_status(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  int i;
  (void)arg;

  ZBA_CMD_LOG("Status:");
  ZBA_CMD_LOG("%20s %s", "Subsystem", "Status");

  // We don't init/deinit it from here because it would kill the session, so
  // right now it's not in the subsystem list.
  ZBA_CMD_LOG("%20s 0x%X", "*util", ZBA_MODULE_INITIALIZED(zba_util));
  ZBA_CMD_LOG("%20s 0x%X", "*stream", ZBA_MODULE_INITIALIZED(zba_stream));

  for (i = 0; i < num_subsystems; ++i)
  {
    ZBA_CMD_LOG("%20s 0x%X", zba_subsystems[i].name, *zba_subsystems[i].init_error);
  }
}

void zba_commands_reboot(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  (void)arg;
  ZBA_CMD_LOG("Rebooting.");
  // This just resets processors, not peripherals...
  // hmm.... actually want peripherals reset as well.
  // Abort doesn't appear to do that either though.
  esp_restart();

  // esp_system_abort("What does this button do?");
}

void zba_commands_reset(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  (void)arg;
  ZBA_CMD_LOG("Resetting.");
  zba_config_reset();
}

void zba_commands_set_device_pwd(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  if (*arg != ' ')
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;

  if (strlen(arg) > kMaxPasswordLen)
  {
    ZBA_CMD_LOG("Password too long - Max length is 64 characters.");
  }
  else
  {
    ZBA_LOG("Setting password");
    zba_config_set_device_pwd(arg);
    ZBA_LOG("Writing config.");
    zba_config_write();
    ZBA_CMD_LOG("New Device Password saved.");
  }
}

void zba_commands_set_ssid(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  if (*arg != ' ')
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;

  if (strlen(arg) > kMaxSSIDLen)
  {
    ZBA_CMD_LOG("SSID too long - Max length is 32 characters.");
  }
  else
  {
    zba_config_set_ssid(arg);
    zba_config_write();
    ZBA_CMD_LOG("New SSID saved.");
  }
}

void zba_commands_set_wifi_pwd(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  if (*arg != ' ')
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;

  if (strlen(arg) > kMaxPasswordLen)
  {
    ZBA_CMD_LOG("Password too long - Max length is 64 characters.");
  }
  else
  {
    zba_config_set_wifi_pwd(arg);
    zba_config_write();
    ZBA_CMD_LOG("New WiFi Password saved.");
  }
}

void zba_commands_start(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  zba_err_t result;
  int i;

  if (*arg != ' ')
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;

  for (i = 0; i < num_subsystems; ++i)
  {
    if (0 == strcasecmp(arg, zba_subsystems[i].name))
    {
      if (ZBA_OK != (result = zba_subsystems[i].initFunc()))
      {
        ZBA_ERR("Failed to initialize %s", arg);
        return;
      }
      ZBA_LOG("Initialized %s", arg);
      return;
    }
  }
  ZBA_ERR("Invalid subsystem %s", arg);
}

void zba_commands_stop(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  zba_err_t result;
  int i;

  if (*arg != ' ')
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;

  for (i = 0; i < num_subsystems; ++i)
  {
    if (0 == strcasecmp(arg, zba_subsystems[i].name))
    {
      if (ZBA_OK != (result = zba_subsystems[i].deinitFunc()))
      {
        ZBA_ERR("Failed to deinitialize %s", arg);
        return;
      }
      ZBA_LOG("Deinitialized %s", arg);
      return;
    }
  }
  ZBA_ERR("Invalid subsystem %s", arg);
}

bool arg_means_on(const char *arg)
{
  bool on = false;
  switch (arg[0])
  {
    case '1':
    case 't':
    case 'T':
    case 'y':
    case 'Y':
      on = true;
      break;
    case 'o':
    case 'O':
      switch (arg[1])
      {
        case 'n':
        case 'N':
          on = true;
          break;
      }
      break;
  }
  return on;
}

void zba_commands_light(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  if (*arg != ' ')
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;

  zba_led_light(arg_means_on(arg));
}

void zba_commands_dir(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  (void)arg;
  (void)cmd_stream;
  zba_sd_list_files();
}
