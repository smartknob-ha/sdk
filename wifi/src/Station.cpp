#include "Station.hpp"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_system_error.hpp"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define STA_CONNECTED_BIT BIT0
#define STA_DISCONNECTED_BIT BIT1

#define INIT_RETURN_ON_ERROR(err)                                           \
    do {                                                                    \
        if (err != ESP_OK) {                                                \
            ESP_LOGE(TAG, "Failed to init STA: %s", esp_err_to_name(err)); \
            return std::unexpected(std::make_error_code(err));              \
        }                                                                   \
    } while (0)

namespace sdk {
    namespace wifi {

        Station::Station(Config config) : m_config(config) {
            eventGroup = xEventGroupCreate();
        }

        res Station::getStatus() {
            return Status::RUNNING;
        }

        res Station::run() {
            return Status::RUNNING;
        }

        res Station::stop() {
            wifi_mode_t currentMode = WIFI_MODE_NULL;
            esp_err_t   err         = esp_wifi_get_mode(&currentMode);
            if (err == ESP_ERR_WIFI_NOT_INIT) {
                ESP_LOGW(TAG, "Wifi is not running, returning");
            } else if (err == ESP_OK && currentMode == WIFI_MODE_STA) {
                err = esp_wifi_stop();
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to stop STA: %s", esp_err_to_name(err));
                    return std::unexpected(std::make_error_code(err));
                }
            } else if (err == ESP_OK && currentMode == WIFI_MODE_APSTA) {
                ESP_LOGD(TAG, "Wifi is in APSTA mode, stopping STA only");
                err = esp_wifi_set_mode(WIFI_MODE_AP);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to set wifi mode to AP: %s", esp_err_to_name(err));
                    return std::unexpected(std::make_error_code(err));
                }
            }
            return Status::STOPPED;
        }

        res Station::initialize() {
            auto ret = initializeNonBlocking();
            if (!ret.has_value())
                return ret;

            // Wait for one of the bits to be set by the event handler
            EventBits_t bits = xEventGroupWaitBits(eventGroup,
                                                   STA_CONNECTED_BIT | STA_DISCONNECTED_BIT,
                                                   pdFALSE,
                                                   pdFALSE,
                                                   30000 / portTICK_PERIOD_MS); // 30 seconds

            if (bits == STA_CONNECTED_BIT) {
                return Status::RUNNING;
            } else if (bits == STA_DISCONNECTED_BIT) {
                ESP_LOGE(TAG, "Failed to connect to wifi");
                return std::unexpected(std::make_error_code(ESP_ERR_WIFI_NOT_CONNECT));
            } else {
                ESP_LOGE(TAG, "Wifi connection timed out after 30 seconds");
                return std::unexpected(std::make_error_code(ESP_ERR_WIFI_TIMEOUT));
            }
        }

        res Station::initializeNonBlocking() {
            wifi_mode_t current_mode = WIFI_MODE_NULL, new_mode = WIFI_MODE_STA;
            esp_err_t   err = esp_wifi_get_mode(&current_mode);
            if (err == ESP_OK && current_mode == WIFI_MODE_STA) {
                m_wifiInitialized = true;
                return Status::RUNNING;
            } else if (err == ESP_OK && current_mode == WIFI_MODE_AP) {
                ESP_LOGW(TAG, "Wifi already initialized as AP, attempting to add Soft STA");
                new_mode          = WIFI_MODE_APSTA;
                m_wifiInitialized = true;
            }
            if (!m_wifiInitialized) {
                ESP_LOGI(TAG, "Initializing wifi STA");
                // Don't care about return value as it either initializes, or it's already initialized
                esp_netif_init();
                INIT_RETURN_ON_ERROR(esp_event_loop_create_default());
                wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
                INIT_RETURN_ON_ERROR(esp_wifi_init(&cfg));
            }

            esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
            // Default config has DHCP client enabled on startup, we will do this manually later (if configured)
            esp_netif_config.flags = (esp_netif_flags_t) (ESP_NETIF_FLAG_GARP | ESP_NETIF_FLAG_EVENT_IP_MODIFIED);
            m_networkInterface     = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);

            esp_wifi_set_default_wifi_sta_handlers();

            esp_netif_set_hostname(m_networkInterface, m_config.hostname.c_str());

            INIT_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT,
                                                                     ESP_EVENT_ANY_ID,
                                                                     &eventHandler,
                                                                     NULL,
                                                                     NULL));
            INIT_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT,
                                                                     IP_EVENT_STA_GOT_IP,
                                                                     &eventHandler,
                                                                     NULL,
                                                                     NULL));

            wifi_config_t wifiConfig = {
                    .sta = {
                            .bssid_set = false,
                            .pmf_cfg   = {
                                      .required = true}}};

            memcpy(wifiConfig.sta.ssid, m_config.ssid.data(), m_config.ssid.size());
            memcpy(wifiConfig.sta.password, m_config.pass.data(), m_config.pass.size());

            INIT_RETURN_ON_ERROR(esp_wifi_set_mode(new_mode));
            INIT_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));

            if (!m_wifiInitialized) {
                INIT_RETURN_ON_ERROR(esp_wifi_start());
                m_wifiInitialized = true;
            }
            INIT_RETURN_ON_ERROR(esp_wifi_connect());

            ESP_LOGI(TAG, "Attempting to connect to: \"%s\"", wifiConfig.sta.ssid);

            return Status::RUNNING;
        }

        void Station::eventHandler(void* arg, esp_event_base_t eventBase,
                                   int32_t eventId, void* eventData) {
            switch (eventId) {
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
                    wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) eventData;
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