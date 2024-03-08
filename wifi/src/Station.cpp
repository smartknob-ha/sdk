#include "Station.hpp"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#define STA_CONNECTED_BIT BIT0
#define STA_DISCONNECTED_BIT BIT1

namespace sdk {
    namespace wifi {

        Station::Station(Config config) : m_config(config) {
            eventGroup = xEventGroupCreate();
        }

        res Station::getStatus() {
            return Ok(ComponentStatus::RUNNING);
        }

        res Station::run() {
            return Ok(ComponentStatus::RUNNING);
        }

        res Station::stop() {
            wifi_mode_t current_mode = WIFI_MODE_NULL;
            esp_err_t   err          = esp_wifi_get_mode(&current_mode);
            if (err == ESP_ERR_WIFI_NOT_INIT)
                ESP_LOGW(TAG, "Wifi is not running, returning");
            else if (err == ESP_OK && current_mode == WIFI_MODE_STA) {
                RETURN_ON_ERR_MSG(esp_wifi_stop(), "esp_wifi_stop: ");
            } else if (err == ESP_OK && current_mode == WIFI_MODE_APSTA)
                RETURN_ON_ERR_MSG(esp_wifi_set_mode(WIFI_MODE_AP), "esp_wifi_set_mode: ");
            return Ok(ComponentStatus::STOPPED);
        }

        res Station::initialize() {
            auto ret = initializeNonBlocking();
            if (ret.isErr())
                return ret;

            // Wait for one of the bits to be set by the event handler
            EventBits_t bits = xEventGroupWaitBits(eventGroup,
                                                   STA_CONNECTED_BIT | STA_DISCONNECTED_BIT,
                                                   pdFALSE,
                                                   pdFALSE,
                                                   30000 / portTICK_PERIOD_MS); // 30 seconds

            if (bits == STA_DISCONNECTED_BIT)
                return Err(etl::string<128>("STA Disconnected"));
            else if (bits == STA_CONNECTED_BIT)
                return Ok(ComponentStatus::RUNNING);
            else
                return Err(etl::string<128>("Wifi connection timed out after 30 seconds"));
        }

        res Station::initializeNonBlocking() {
            wifi_mode_t current_mode = WIFI_MODE_NULL, new_mode = WIFI_MODE_STA;
            esp_err_t   err = esp_wifi_get_mode(&current_mode);
            if (err == ESP_OK && current_mode == WIFI_MODE_STA) {
                m_wifiInitialized = true;
                return Err(etl::string<128>("Wifi STA is already initialized, returning"));
            } else if (err == ESP_OK && current_mode == WIFI_MODE_AP) {
                ESP_LOGW(TAG, "Wifi already initialized as AP, attempting to add Soft STA");
                new_mode          = WIFI_MODE_APSTA;
                m_wifiInitialized = true;
            }
            if (!m_wifiInitialized) {
                ESP_LOGI(TAG, "Initializing wifi STA");
                // Don't care about return value as it either initializes, or it's already initialized
                esp_netif_init();
                RETURN_ON_ERR_MSG(esp_event_loop_create_default(), "esp_event_loop_create_default");
                wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
                RETURN_ON_ERR_MSG(esp_wifi_init(&cfg), "esp_wifi_init: ");
            }

            esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
            // Default config has DHCP client enabled on startup, we will do this manually later (if configured)
            esp_netif_config.flags = (esp_netif_flags_t) (ESP_NETIF_FLAG_GARP | ESP_NETIF_FLAG_EVENT_IP_MODIFIED);
            m_esp_netif            = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);

            esp_wifi_set_default_wifi_sta_handlers();

            esp_netif_set_hostname(m_esp_netif, m_config.hostname.c_str());

            RETURN_ON_ERR_MSG(esp_event_handler_instance_register(WIFI_EVENT,
                                                                  ESP_EVENT_ANY_ID,
                                                                  &stationEventHandler,
                                                                  NULL,
                                                                  NULL),
                              "esp_event_handler_instance_register: ");
            RETURN_ON_ERR_MSG(esp_event_handler_instance_register(IP_EVENT,
                                                                  IP_EVENT_STA_GOT_IP,
                                                                  &stationEventHandler,
                                                                  NULL,
                                                                  NULL),
                              "esp_event_handler_instance_register: ");

            wifi_config_t wifi_config = {
                    .sta = {
                            .bssid_set = false,
                            .pmf_cfg   = {
                                      .required = true}}};

            memcpy(wifi_config.sta.ssid, m_config.ssid.data(), m_config.ssid.size());
            memcpy(wifi_config.sta.password, m_config.pass.data(), m_config.pass.size());

            RETURN_ON_ERR_MSG(esp_wifi_set_mode(new_mode), "esp_wifi_set_mode: ");
            RETURN_ON_ERR_MSG(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), "esp_wifi_set_config: ");

            if (!m_wifiInitialized) {
                RETURN_ON_ERR_MSG(esp_wifi_start(), "esp_wifi_start: ");
                m_wifiInitialized = true;
            }
            RETURN_ON_ERR_MSG(esp_wifi_connect(), "esp_wifi_connect: ");

            ESP_LOGI(TAG, "Attempting to connect to: \"%s\"", wifi_config.sta.ssid);

            return Ok(ComponentStatus::RUNNING);
        }

        void Station::stationEventHandler(void* arg, esp_event_base_t event_base,
                                          int32_t event_id, void* event_data) {
            switch (event_id) {
                case WIFI_EVENT_STA_START: {
                    ESP_LOGI(TAG, "STA has started");
                    break;
                }
                case WIFI_EVENT_STA_STOP: {
                    ESP_LOGI(TAG, "STA has stopped");
                    break;
                }
                case WIFI_EVENT_STA_CONNECTED: {
                    xEventGroupSetBits(eventGroup, STA_CONNECTED_BIT);
                    ESP_LOGI(TAG, "Connected to a network");
                    break;
                }
                case WIFI_EVENT_STA_DISCONNECTED: {
                    xEventGroupSetBits(eventGroup, STA_DISCONNECTED_BIT);
                    wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                    ESP_LOGI(TAG, "Disconnected from a network, reason: %u", event->reason);
                    break;
                }
                case WIFI_EVENT_STA_AUTHMODE_CHANGE: {
                    ESP_LOGI(TAG, "Not sure?¿");
                    break;
                }
                case IP_EVENT_STA_GOT_IP: {
                    ESP_LOGI(TAG, "Got an IP");
                    break;
                }
                case IP_EVENT_STA_LOST_IP: {
                    ESP_LOGI(TAG, "Lost an IP");
                    break;
                }
                case WIFI_EVENT_MAX: {
                    ESP_LOGE(TAG, "Perhaps the archives are incomplete");
                    break;
                }
            }
        }

    } /* namespace wifi */

} /* namespace sdk */