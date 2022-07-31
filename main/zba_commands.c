#include "zba_commands.h"
#include <esp_system.h>
#include <memory.h>
#include "zba_auth.h"
#include "zba_camera.h"
#include "zba_config.h"
#include "zba_i2c.h"
#include "zba_led.h"
#include "zba_sd.h"
#include "zba_stream.h"
#include "zba_util.h"
#include "zba_vision.h"
#include "zba_web.h"
#include "zba_wifi.h"

DEFINE_ZBA_TAG;

// {TODO} In .cpp because it uses Printable and Stream.

// Function to receive commands and do something with them.
typedef void (*command_handler)(const char *arg, zba_cmd_stream_t *cmd_stream);
typedef void (*command_handler_web)(const char *arg, httpd_req_t *req);

// Entry in our command handler list
typedef struct
{
  const char *command;
  command_handler handler;
  command_handler_web web_handler;
  const char *usage;
  const char *description;
} command_entry_t;

// clang-format off

/// Commands that can be run via serial
static const command_entry_t command_handlers[] =
{
// Setup
  {"logout",   zba_commands_logout,        NULL, "logout",             "Logs out of the camera"},
  {"pwd",      zba_commands_set_device_pwd,NULL, "pwd PASSWORD",       "Sets the device password"},
  {"reboot",   zba_commands_reboot,        NULL, "reboot",             "Reboots the device"},
  {"reset",    zba_commands_reset,         NULL,  "reset",              "Resets the device to factory"},
  {"ssid",     zba_commands_set_ssid,      NULL,  "ssid SSID",          "Sets the SSID for WiFi"},
  {"wifi_pwd", zba_commands_set_wifi_pwd,  NULL,  "wifi_pwd PASSWORD",  "Sets the password for WiFi"},
  {"memory",   zba_commands_memory,        NULL,  "memory",             "Gets the memory usage"},
// These are for testing  
  {"start",    zba_commands_start,         NULL,  "start SUBSYSTEM",    "Start a subsystem"},
  {"stop",     zba_commands_stop,          NULL,  "stop SUBSYSTEM",     "Stop a subsystem"},
// Blinkies
  {"light",    zba_commands_light,         NULL,  "light [on|off]",     "Toggles the white led (front)"},
  {"dir",      zba_commands_dir,           NULL,  "dir",                "Displays files on SD card"},
  {"cam",      zba_commands_camera_status, NULL,  "cam",                "Get camera status"},
  {"res",      zba_commands_camera_res,    NULL,  "res",                "Set camera res (VGA,SVGA,HD,SXGA,UXGA)"},
  {"ledcolor", zba_commands_ledcolor,      NULL,  "ledcolor #000000",   "Sets all LEDs to color"},
  {"gpio",     zba_commands_gpio,          NULL,  "gpio## [on|off]",    "Turns on/off gpio bits"},
  {"autoexpose", zba_commands_autoexpose,  NULL,  "autoexpose [on|off]","Turns on/off autoexposure"},
  // Special commands handled differently for web
  {"status",   zba_commands_status,        
               zba_commands_status_web,           "status",             "Gets the status of subsystems"}

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
  DEFINE_ZBA_SUBSYSTEM_ENTRY(sd),
  DEFINE_ZBA_SUBSYSTEM_ENTRY(vision)
};
const static int num_subsystems = sizeof(zba_subsystems) / sizeof(zba_subsystem_entry_t);


static const command_entry_t unauthed_handlers[] = 
{
  // This one has no space, as it may be used w/o password if there's none set.
  {"login",  zba_commands_login,  NULL,                    "login [PASSWORD]", "Logs in"},
  // Allow the status command in either
  {"status", zba_commands_status, zba_commands_status_web, "status", "Gets the status of subsystems"}
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

void zba_commands_process_web(const char *buffer, httpd_req_t *req)
{
  const command_entry_t *handlers = command_handlers;
  int num_handlers                = num_command_handlers;

  int i;
  for (i = 0; i < num_handlers; ++i)
  {
    if (0 == strncasecmp(buffer, handlers[i].command, strlen(handlers[i].command)))
    {
      if (handlers[i].web_handler)
      {
        handlers[i].web_handler(buffer + strlen(handlers[i].command), req);
      }
      else
      {
        handlers[i].handler(buffer + strlen(handlers[i].command), NULL);
        // Provide response - right now, just success on everything.
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "utf-8");
        httpd_resp_sendstr(req, "{\"status\":\"success\"}");
      }
      ZBA_CMD_LOG("Command processed: %s", handlers[i].command);
      return;
    }
  }
  ZBA_CMD_LOG("Unknown command %s.", buffer);
}

void zba_commands_process(const char *buffer, zba_cmd_stream_t *cmd_stream)
{
  const command_entry_t *handlers = NULL;
  int num_handlers                = 0;

  // Web has no stream, but is already authed to get here.
  bool authed = cmd_stream ? cmd_stream->authed : true;

  handlers     = authed ? command_handlers : unauthed_handlers;
  num_handlers = authed ? num_command_handlers : num_unauthed_handlers;

  int i;
  for (i = 0; i < num_handlers; ++i)
  {
    if (0 == strncasecmp(buffer, handlers[i].command, strlen(handlers[i].command)))
    {
      handlers[i].handler(buffer + strlen(handlers[i].command), cmd_stream);
      ZBA_CMD_LOG("Command processed: %s", handlers[i].command);
      return;
    }
  }

  ZBA_CMD_LOG("Unknown command.");
  ZBA_CMD_LOG("Zebral ESP32-CAM valid commands:");
  for (i = 0; i < num_handlers; ++i)
  {
    ZBA_CMD_LOG("%-24s - %s", handlers[i].usage, handlers[i].description);
  }
}

void zba_commands_login(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  if (!cmd_stream)
  {
    ZBA_CMD_LOG("No command stream, should already be logged in.");
    return;
  }

  if (arg[0] == 0)
  {
    cmd_stream->authed = (ZBA_OK == zba_auth_check(kAdminUser, arg));
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
    cmd_stream->authed = (ZBA_OK == zba_auth_check(kAdminUser, arg));
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
  if (!cmd_stream)
  {
    ZBA_CMD_LOG("No command stream, should already be logged in.");
    return;
  }

  cmd_stream->authed = false;
  ZBA_CMD_LOG("Logged out.");
}

void zba_commands_status(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  int i;
  (void)arg;

  if (ZBA_MODULE_INITIALIZED(zba_wifi) == ZBA_OK)
  {
    ZBA_CMD_LOG("ip: %s", zba_wifi_get_ip_addr());
  }

  if (ZBA_MODULE_INITIALIZED(zba_camera) == ZBA_OK)
  {
    ZBA_CMD_LOG("resolution: %s", zba_camera_get_res_name(zba_camera_get_res()));
  }

  ZBA_CMD_LOG("gpio: %04X",
              ((uint16_t)zba_i2c_aw9523_get_out_high() << 8) + zba_i2c_aw9523_get_out_low());

  // We don't init/deinit it from here because it would kill the session, so
  // right now it's not in the subsystem list.
  ZBA_CMD_LOG("%s: 0x%X", "util", ZBA_MODULE_INITIALIZED(zba_util));
  ZBA_CMD_LOG("%s: 0x%X", "stream", ZBA_MODULE_INITIALIZED(zba_stream));

  for (i = 0; i < num_subsystems; ++i)
  {
    ZBA_CMD_LOG("%s: 0x%X", zba_subsystems[i].name, *zba_subsystems[i].init_error);
  }
}

void zba_commands_status_web(const char *arg, httpd_req_t *req)
{
#define MAX_STAT_SIZE 1024
  int i;
  char buffer[MAX_STAT_SIZE + 1] = {0};
  (void)arg;

  // {TODO} Add more, but this is the one I want right now.
  // LED Strip state? Do we want to transfer that? Probably at least config....
  // Also module status like the normal status does.
  snprintf(buffer, MAX_STAT_SIZE, "{\"resolution\":\"%s\", \"gpio\":\"0x%04X\"",
           zba_camera_get_res_name(zba_camera_get_res()),
           ((uint16_t)zba_i2c_aw9523_get_out_high() << 8) + zba_i2c_aw9523_get_out_low());

  // Subsystem status
  snprintf(buffer + strlen(buffer), MAX_STAT_SIZE - strlen(buffer), ",\"util\": \"0x%X\"",
           ZBA_MODULE_INITIALIZED(zba_util));
  snprintf(buffer + strlen(buffer), MAX_STAT_SIZE - strlen(buffer), ",\"stream\": \"0x%X\"",
           ZBA_MODULE_INITIALIZED(zba_stream));

  for (i = 0; i < num_subsystems; ++i)
  {
    snprintf(buffer + strlen(buffer), MAX_STAT_SIZE - strlen(buffer), ",\"%s\": \"0x%X\"",
             zba_subsystems[i].name, *zba_subsystems[i].init_error);
  }
  strncat(buffer, "}", MAX_STAT_SIZE);

  // Go ahead and dump status to our logs when this is called.
  zba_commands_status(arg, NULL);

  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "utf-8");
  httpd_resp_sendstr(req, buffer);
}

void zba_commands_memory(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  (void)arg;
  (void)cmd_stream;
  multi_heap_info_t heapInfo = {0};
  heap_caps_get_info(&heapInfo, MALLOC_CAP_SPIRAM);
  ZBA_CMD_LOG("SPIRAM: free: %u allocated: %u largest: %u", heapInfo.total_free_bytes,
              heapInfo.total_allocated_bytes, heapInfo.largest_free_block);
  heap_caps_get_info(&heapInfo, MALLOC_CAP_8BIT);
  ZBA_CMD_LOG("8BIT: free: %u allocated: %u largest: %u", heapInfo.total_free_bytes,
              heapInfo.total_allocated_bytes, heapInfo.largest_free_block);
  heap_caps_get_info(&heapInfo, MALLOC_CAP_DMA);
  ZBA_CMD_LOG("DMA: free: %u allocated: %u largest: %u", heapInfo.total_free_bytes,
              heapInfo.total_allocated_bytes, heapInfo.largest_free_block);
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
  if ((*arg != ' ') && (*arg != '='))
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
  if ((*arg != ' ') && (*arg != '='))
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
  if ((*arg != ' ') && (*arg != '='))
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

  if ((*arg != ' ') && (*arg != '='))
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

  if ((*arg != ' ') && (*arg != '='))
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
  if ((*arg != ' ') && (*arg != '='))
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;
  zba_led_light(arg_means_on(arg));
}

void zba_commands_autoexpose(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  if ((*arg != ' ') && (*arg != '='))
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;
  zba_camera_set_autoexposure(arg_means_on(arg));
}

void zba_commands_ledcolor(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  uint8_t colors[4] = {0};
  bool gotValue     = false;

  int i = 0;

  if ((*arg != ' ') && (*arg != '='))
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;

  if (*arg == '#')
  {
    gotValue = true;
    arg++;
  }

  if (!gotValue)
  {
    if ((arg[0] == '%') && (arg[1] == '2') && (arg[2] == '3'))
    {
      gotValue = true;
      arg += 3;
    }
    else
    {
      ZBA_CMD_LOG("invalid led color. Should be in format #rrggbb or #rrggbbww");
      return;
    }
  }

  while ((*arg != 0) && (i < 4))
  {
    colors[i] = zba_hex_to_byte(arg);
    ++i;
    arg += 2;
  }
  ZBA_CMD_LOG("Colors: %02X %02X %02X %02X", colors[0], colors[1], colors[2], colors[3]);

  // {HACK} Right now, we're externally using a javascript color picker that gives us
  // RGB.  If the length is 3 (RGB) and not 4 (RGBW) for the command, and r=g=b, then let's use
  // white instead and set RGB to 0.
  if ((i == 3) && (colors[0] == colors[1]) && (colors[0] == colors[2]))
  {
    colors[3] = colors[0];
    colors[0] = colors[1] = colors[2] = 0;
  }

  zba_led_strip_set_led(0, -1, colors[0], colors[1], colors[2], colors[3]);
  zba_led_strip_flip();
}

void zba_commands_dir(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  (void)arg;
  (void)cmd_stream;
  zba_sd_list_files();
}

void zba_commands_camera_status(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  (void)arg;
  (void)cmd_stream;
  zba_camera_dump_status();
}

void zba_commands_gpio(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  int pin     = 0;
  bool pin_on = false;

  if ((*arg >= '0') && (*arg <= '9'))
  {
    pin = (*arg) - '0';
    arg++;
    if ((*arg >= '0') && (*arg <= '9'))
    {
      pin = (pin * 10) + (*arg) - '0';
      arg++;
    }
  }
  else
  {
    ZBA_CMD_LOG("GPIO command needs pin as part of the command. e.g. gpio12=on");
    return;
  }

  if ((*arg != ' ') && (*arg != '='))
  {
    ZBA_CMD_LOG("Command requires an argument.");
    return;
  }
  arg++;

  pin_on = arg_means_on(arg);

  zba_i2c_aw9523_set_pin(pin, pin_on);
}

void zba_commands_camera_res(const char *arg, zba_cmd_stream_t *cmd_stream)
{
  zba_resolution_t res = ZBA_VGA;
  arg++;
  const zba_res_info_t *resInfo = zba_camera_get_res_from_name(arg);
  if (resInfo)
  {
    res = resInfo->res;
    zba_camera_set_res(res);
  }
}
