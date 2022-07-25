#include "zba_sd.h"

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "zba_led.h"
#include "zba_pins.h"
#include "zba_util.h"

DEFINE_ZBA_MODULE(zba_sd);

// Maximum depth in directories.
#define SD_MAX_DIR_DEPTH 2

typedef struct
{
  bool active;
  const char* root;
  sdmmc_card_t* card;
} zba_sd_state_t;

static zba_sd_state_t sd_state = {.active = false, .root = "/sd", .card = NULL};

const char* zba_sd_get_root()
{
  return sd_state.root;
}

zba_err_t zba_sd_init()
{
  zba_err_t result    = ZBA_OK;
  esp_err_t esp_error = ESP_OK;

  zba_pin_mode(PIN_MODULE_3, PIN_MODE_DIGITAL_IN_PULLUP);
  zba_pin_mode(PIN_MODULE_2, PIN_MODE_DIGITAL_IN_PULLUP);

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = true, .max_files = 6, .allocation_unit_size = 16 * 1024};
  sdmmc_host_t host               = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  // Narrow bus so it conflicts with less.
  host.flags        = SDMMC_HOST_FLAG_1BIT;
  slot_config.width = 1;

  zba_led_light(false);

  ZBA_LOG("Mounting SD at %s", sd_state.root);

  esp_error =
      esp_vfs_fat_sdmmc_mount(sd_state.root, &host, &slot_config, &mount_config, &sd_state.card);

  if (esp_error != ESP_OK)
  {
    if (esp_error == ESP_FAIL)
    {
      ZBA_ERR("Failed to mount SDMMC card");
    }
    else
    {
      ZBA_ERR("Failed to initialize the card (%s). ", esp_err_to_name(esp_error));
    }

    result = ZBA_SD_ERROR;
  }
  else
  {
    ZBA_LOG("SD Card mounted to %s", sd_state.root);
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, sd_state.card);
    sd_state.active = true;
  }

  ZBA_MODULE_INITIALIZED(zba_sd) = result;
  return result;
}

zba_err_t zba_sd_deinit()
{
  zba_err_t deinit_error = ZBA_OK;
  bool light_off         = false;
  if (sd_state.active)
  {
    // {TODO} Unfortunately this doesn't fully de-init.
    // After using a SDMMC card, you cannot flash
    // the device until the card has been removed.
    // It can be reinserted immediately afterwards.
    //
    // Tried a few things to fix this - reinitializing pins,
    // etc - but have not had success yet.
    esp_vfs_fat_sdcard_unmount(sd_state.root, sd_state.card);
    sdmmc_host_deinit();
    sd_state.active = false;
    light_off       = true;
    // Check if these fix the programming issues after using an SD
    zba_pin_mode(PIN_MODULE_3, PIN_MODE_DIGITAL_IN_PULLUP);
    zba_pin_mode(PIN_MODULE_2, PIN_MODE_DIGITAL_IN_PULLUP);
  }

  ZBA_MODULE_INITIALIZED(zba_sd) =
      (ZBA_OK == deinit_error) ? ZBA_MODULE_NOT_INITIALIZED : deinit_error;

  if (light_off)
  {
    zba_led_light(false);
  }

  return deinit_error;
}

bool zba_print_filename(const char* path, void* context, int depth)
{
  (void)context;
  ZBA_LOG("FILE Depth(%d): %s", depth, path);
  return true;
}

zba_err_t zba_sd_list_files()
{
  return zba_sd_enum_files(sd_state.root, zba_print_filename, NULL, true, 0);
}

zba_err_t zba_sd_enum_files(const char* path, zba_file_callback cb, void* context, bool recurse,
                            int curDepth)
{
  zba_err_t zba_err       = ZBA_OK;
  DIR* curDir             = NULL;
  struct dirent* curEntry = NULL;

  // This'll put a hard limit on depth... but
  // we also have 5 files open max, so...
  char tmpPath[514] = {0};

  ZBA_LOG("DIR: %s", path);
  curDir = opendir(path);
  if (curDir == NULL)
  {
    ZBA_LOG("Could not enumerate path: %s", path);
    return ZBA_SD_INVALID_PATH;
  }

  while (NULL != (curEntry = readdir(curDir)))
  {
    if (recurse && (curEntry->d_type == DT_DIR))
    {
      if (curDepth >= SD_MAX_DIR_DEPTH)
      {
        ZBA_LOG("At max directory depth of %d. Skipping dir %s/%s", SD_MAX_DIR_DEPTH, path,
                curEntry->d_name);
        continue;
      }
      snprintf(tmpPath, sizeof(tmpPath) - 1, "%s/%s", path, curEntry->d_name);
      zba_sd_enum_files(tmpPath, cb, context, recurse, curDepth + 1);
    }
    else
    {
      snprintf(tmpPath, sizeof(tmpPath) - 1, "%s/%s", path, curEntry->d_name);
      cb(tmpPath, context, curDepth);
    }
  }
  closedir(curDir);
  return zba_err;
}
