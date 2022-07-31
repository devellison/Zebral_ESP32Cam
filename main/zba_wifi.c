#include "zba_wifi.h"
#include <esp_event.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <string.h>

#include "zba_config.h"
#include "zba_util.h"

DEFINE_ZBA_MODULE(zba_wifi);

#define WIFI_CONNECTED_BIT    0x01
#define WIFI_FAIL_BIT         0x02
#define WIFI_DISCONNECTED_BIT 0x04
#define WIFI_STOPPED_BIT      0x08

/// Flags for what's succeeded in the connections.
/// Teardown with esp_wifi is pretty picky as to what
/// gets called, so track each step and success that doesn't have a handle
/// or other state variable so we can reverse it.
#define NET_INITIALIZED  0x01
#define NET_LOOP_CREATED 0x02
#define WIFI_INITIALIZED 0x04
#define WIFI_STARTED     0x08
#define WIFI_CONNECTED   0x10

typedef struct
{
  SemaphoreHandle_t init_mutex;
  EventGroupHandle_t event_group;
  esp_event_handler_instance_t inst_any_id;
  esp_event_handler_instance_t inst_got_ip;
  esp_netif_t *netif_wifi_sta;
  bool exiting;
  char ip_addr[16];
  unsigned int wifi_status;  /// Bits indicating what parts we've initialized
} zba_wifi_t;

static zba_wifi_t wifi_state = {.init_mutex     = NULL,
                                .event_group    = NULL,
                                .inst_any_id    = NULL,
                                .inst_got_ip    = NULL,
                                .netif_wifi_sta = NULL,
                                .exiting        = false,
                                .wifi_status    = 0,
                                .ip_addr        = {0}};

const char *zba_wifi_get_ip_addr()
{
  return wifi_state.ip_addr;
}

const char *zba_wifi_get_device_name()
{
  typedef struct device_name
  {
    char header[10];
    char suffix[14];
  } device_name_t;
  static device_name_t devName = {"ZebralCam", {0}};
  uint8_t mac[6];

  // If we haven't filled it in yet, fill it in.
  if (devName.suffix[0] == 0)
  {
    if (ESP_OK != esp_efuse_mac_get_default(mac))
    {
      ZBA_ERR("Error getting default base mac address. Try wifi...");
      if (ESP_OK != esp_wifi_get_mac(WIFI_IF_STA, mac))
      {
        ZBA_ERR("WiFi mac failed too.");
      }
    }
    devName.header[9] = '_';
    sprintf(&devName.suffix[0], "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
            mac[5]);
  }
  return &devName.header[0];
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data)
{
  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
      case WIFI_EVENT_STA_START:
        ZBA_SET_BIT(wifi_state.wifi_status, WIFI_STARTED);
        esp_wifi_connect();

        break;
      case WIFI_EVENT_STA_STOP:
        ZBA_UNSET_BIT(wifi_state.wifi_status, WIFI_STARTED);
        xEventGroupSetBits(wifi_state.event_group, WIFI_STOPPED_BIT);

        break;
      case WIFI_EVENT_STA_CONNECTED:
        // Do we need this? Setting it when we get the IP
        // wifi_state.wifi_status.wifi_connected = true;
        break;

      case WIFI_EVENT_STA_DISCONNECTED:
        /// {TODO} add some retry and exit logic here.
        ZBA_LOG("WiFi disconnected.");
        xEventGroupSetBits(wifi_state.event_group, WIFI_DISCONNECTED_BIT);
        ZBA_UNSET_BIT(wifi_state.wifi_status, WIFI_CONNECTED);

        if (!wifi_state.exiting)
        {
          ZBA_LOG("Retrying WiFi connect...");
          esp_wifi_connect();
        }
        break;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    snprintf(wifi_state.ip_addr, 16, IPSTR, IP2STR(&event->ip_info.ip));
    ZBA_LOG("Connected! IP: %s", wifi_state.ip_addr);
    ZBA_SET_BIT(wifi_state.wifi_status, WIFI_CONNECTED);
    xEventGroupSetBits(wifi_state.event_group, WIFI_CONNECTED_BIT);
  }
}

zba_err_t zba_wifi_init()
{
  wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  wifi_config_t wifi_config        = {0};
  zba_err_t result                 = ZBA_OK;

  // Create mutex if it hasn't been yet.
  if (wifi_state.init_mutex == NULL)
  {
    wifi_state.init_mutex = xSemaphoreCreateMutex();
  }
  ZBA_LOCK(wifi_state.init_mutex);

  for (;;)
  {
    if (ZBA_TEST_BIT(wifi_state.wifi_status, WIFI_CONNECTED))
    {
      ZBA_LOG("Already connected to Wifi!");
      break;
    }

    wifi_state.exiting = false;
    // First time only - we're not deinitializing this
    // part currently
    ZBA_LOG("Initializing netIF");
    if (ESP_OK != esp_netif_init())
    {
      ZBA_LOG("esp_netif_init() failed.");
      result = ZBA_WIFI_INIT_FAILED;
      break;
    }

    ZBA_SET_BIT(wifi_state.wifi_status, NET_INITIALIZED);

    wifi_state.event_group = xEventGroupCreate();

    if (ESP_OK != esp_event_loop_create_default())
    {
      ZBA_LOG("esp_event_loop_create_default() failed.");
      result = ZBA_WIFI_INIT_FAILED;
      break;
    }
    ZBA_SET_BIT(wifi_state.wifi_status, NET_LOOP_CREATED);

    wifi_state.netif_wifi_sta = esp_netif_create_default_wifi_sta();

    // From here on gets deinitialized if we turn WiFi off.
    if (ESP_OK != esp_wifi_init(&wifi_init_cfg))
    {
      ZBA_LOG("esp_wifi_init() failed.");
      result = ZBA_WIFI_INIT_FAILED;
      break;
    }
    ZBA_SET_BIT(wifi_state.wifi_status, WIFI_INITIALIZED);

    // Log the device name, while also grabbing the name with the wifi initialized in case
    // we need it after de-initializing it.
    ZBA_LOG("Device name: %s", zba_wifi_get_device_name());

    // Pull wifi config - bail if not yet configured.
    zba_config_get_ssid(wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid));
    zba_config_get_wifi_pwd(wifi_config.sta.password, sizeof(wifi_config.sta.password));
    if (wifi_config.sta.ssid[0] == 0)
    {
      ZBA_LOG("Networking not configured! Please set SSID and Password.");
      result = ZBA_WIFI_NOT_CONFIGURED;
      break;
    }

    if (NULL == wifi_state.inst_any_id)
    {
      if (ESP_OK != esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL,
                                                        &wifi_state.inst_any_id))
      {
        result = ZBA_WIFI_INIT_FAILED;
        break;
      }
    }

    if (NULL == wifi_state.inst_got_ip)
    {
      if (ESP_OK !=
          (esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler,
                                               NULL, &wifi_state.inst_got_ip)))
      {
        result = ZBA_WIFI_INIT_FAILED;
        break;
      }
    }

    if (ESP_OK != esp_wifi_set_mode(WIFI_MODE_STA))
    {
      ZBA_LOG("esp_wifi_set_mode() failed.");
      result = ZBA_WIFI_INIT_FAILED;
      break;
    }

    if (ESP_OK != esp_wifi_set_config(WIFI_IF_STA, &wifi_config))
    {
      ZBA_LOG("esp_wifi_set_config() failed.");
      result = ZBA_WIFI_INIT_FAILED;
      break;
    }

    ZBA_LOG("Starting Wifi");
    if (ESP_OK != esp_wifi_start())
    {
      ZBA_LOG("esp_wifi_start() failed.");
      result = ZBA_WIFI_INIT_FAILED;
      break;
    }
    ZBA_SET_BIT(wifi_state.wifi_status, WIFI_STARTED);

    // Wait for connect or failure
    // EventBits_t bits =
    xEventGroupWaitBits(wifi_state.event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE,
                        pdFALSE, zba_config_get_wifi_timeout_sec() * 1000 * portTICK_PERIOD_MS);

    xEventGroupClearBits(wifi_state.event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT |
                                                     WIFI_DISCONNECTED_BIT | WIFI_STOPPED_BIT);
    break;
  }

  if (!ZBA_TEST_BIT(wifi_state.wifi_status, WIFI_CONNECTED))
  {
    result = ZBA_WIFI_INIT_FAILED;
  }

  ZBA_UNLOCK(wifi_state.init_mutex);

  if (result != ZBA_OK)
  {
    ZBA_LOG("An error occurred initializing wifi - deinitializing");
    zba_wifi_deinit();
    ZBA_SET_INIT(zba_wifi, result);
    return result;
  }

  ZBA_LOG("Connected to %s.", wifi_config.sta.ssid);
  ZBA_SET_INIT(zba_wifi, result);
  return ZBA_OK;
}

zba_err_t zba_wifi_deinit()
{
  zba_err_t deinit_error = ZBA_OK;
  ZBA_LOG("Deinitializing WiFi...");
  ZBA_LOCK(wifi_state.init_mutex);
  wifi_state.exiting = true;

  if (ZBA_TEST_BIT(wifi_state.wifi_status, WIFI_CONNECTED))
  {
    esp_wifi_disconnect();
    /// Wait for disconnect
    xEventGroupWaitBits(wifi_state.event_group, WIFI_DISCONNECTED_BIT, pdFALSE, pdFALSE,
                        zba_config_get_wifi_timeout_sec() * 1000 * portTICK_PERIOD_MS);

    xEventGroupClearBits(wifi_state.event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT |
                                                     WIFI_DISCONNECTED_BIT | WIFI_STOPPED_BIT);
    ZBA_UNSET_BIT(wifi_state.wifi_status, WIFI_CONNECTED);
  }

  if (ZBA_TEST_BIT(wifi_state.wifi_status, WIFI_STARTED))
  {
    esp_wifi_stop();
    xEventGroupWaitBits(wifi_state.event_group, WIFI_STOPPED_BIT, pdFALSE, pdFALSE,
                        zba_config_get_wifi_timeout_sec() * 1000 * portTICK_PERIOD_MS);

    xEventGroupClearBits(wifi_state.event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT |
                                                     WIFI_DISCONNECTED_BIT | WIFI_STOPPED_BIT);
    ZBA_UNSET_BIT(wifi_state.wifi_status, WIFI_STARTED);
  }

  if (wifi_state.inst_any_id)
  {
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_state.inst_any_id);
    wifi_state.inst_any_id = NULL;
  }

  if (wifi_state.inst_got_ip)
  {
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_state.inst_got_ip);
    wifi_state.inst_got_ip = NULL;
  }

  if (ZBA_TEST_BIT(wifi_state.wifi_status, WIFI_INITIALIZED))
  {
    esp_wifi_deinit();
    ZBA_UNSET_BIT(wifi_state.wifi_status, WIFI_INITIALIZED);
  }

  if (wifi_state.netif_wifi_sta != NULL)
  {
    esp_netif_destroy_default_wifi(wifi_state.netif_wifi_sta);
    wifi_state.netif_wifi_sta = NULL;
  }

  if (ZBA_TEST_BIT(wifi_state.wifi_status, NET_LOOP_CREATED))
  {
    esp_event_loop_delete_default();
    ZBA_UNSET_BIT(wifi_state.wifi_status, NET_LOOP_CREATED);
  }

  if (wifi_state.event_group != NULL)
  {
    vEventGroupDelete(wifi_state.event_group);
    wifi_state.event_group = NULL;
  }

  if (ZBA_TEST_BIT(wifi_state.wifi_status, NET_INITIALIZED))
  {
    esp_netif_deinit();
    ZBA_UNSET_BIT(wifi_state.wifi_status, NET_INITIALIZED);
  }

  ZBA_LOG("WiFi deinitialized");
  ZBA_UNLOCK(wifi_state.init_mutex);

  if (wifi_state.init_mutex)
  {
    vSemaphoreDelete(wifi_state.init_mutex);
    wifi_state.init_mutex = NULL;
  }

  ZBA_SET_DEINIT(zba_wifi, deinit_error);

  return deinit_error;
}