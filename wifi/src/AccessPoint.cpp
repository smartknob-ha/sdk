#include "AccessPoint.hpp"

#include <etl/container.h>
#include <etl/type_traits.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#define AP_INITIALIZED_BIT BIT0

namespace sdk::wifi {

    AccessPoint::AccessPoint(Config config) : m_config(config) {
        eventGroup = xEventGroupCreate();
    }

    res AccessPoint::getStatus() {
        return Ok(ComponentStatus::RUNNING);
    }

    res AccessPoint::run() {
        return Ok(ComponentStatus::RUNNING);
    }

    res AccessPoint::stop() {
        wifi_mode_t currentMode = WIFI_MODE_NULL;

        esp_err_t err = esp_wifi_get_mode(&currentMode);
        if (err == ESP_ERR_WIFI_NOT_INIT)
            ESP_LOGW(TAG, "Wifi is not running, returning");
        else if (err == ESP_OK && currentMode == WIFI_MODE_AP) {
            RETURN_ON_ERR_MSG(esp_wifi_stop(), "esp_wifi_stop: ");
        } else if (err == ESP_OK && currentMode == WIFI_MODE_APSTA)
            RETURN_ON_ERR_MSG(esp_wifi_set_mode(WIFI_MODE_STA), "esp_wifi_set_mode: ");
        return Ok(ComponentStatus::STOPPED);
    }

    res AccessPoint::initialize() {
        auto ret = initialize_non_blocking();
        if (ret.isErr())
            return ret;

        // Wait for AP_INITIALIZED_BIT to be set by the event handler
        xEventGroupWaitBits(eventGroup, AP_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        return Ok(ComponentStatus::RUNNING);
    }

    res AccessPoint::initialize_non_blocking() {
        wifi_mode_t current_mode = WIFI_MODE_NULL, new_mode = WIFI_MODE_AP;
        esp_err_t   err = esp_wifi_get_mode(&current_mode);

        if (err == ESP_OK && current_mode == WIFI_MODE_AP) {
            m_wifiInitialized = true;
            return Err(etl::string<128>("Soft AP is already initialized, returning"));
        } else if (err == ESP_OK && current_mode == WIFI_MODE_STA) {
            ESP_LOGW(TAG, "Wifi already initialized as STA, attempting to add Soft AP");
            new_mode          = WIFI_MODE_APSTA;
            m_wifiInitialized = true;
        }

        if (!m_wifiInitialized) {
            // Don't care about return value as it either initializes, or it's already initialized
            esp_netif_init();
            RETURN_ON_ERR_MSG(esp_event_loop_create_default(), "esp_event_loop_create_default: ");
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            RETURN_ON_ERR_MSG(esp_wifi_init(&cfg), "esp_wifi_init: ");
        }

        m_espNetif = esp_netif_create_default_wifi_ap();

        RETURN_ON_ERR_MSG(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, NULL, NULL), "esp_event_handler_instance_register: ");

        wifi_config_t wifiConfig = {
                .ap = {
                        .channel        = m_config.channel,
                        .authmode       = m_config.authmode,
                        .max_connection = CONFIG_AP_MAX_CONNECTIONS}};

        // memcopy to avoid casting errors
        memcpy(wifiConfig.ap.ssid, m_config.ssid.data(), m_config.ssid.size());
        memcpy(wifiConfig.ap.password, m_config.pass.data(), m_config.pass.size());

        RETURN_ON_ERR_MSG(esp_wifi_set_mode(new_mode), "esp_wifi_set_mode: ");
        RETURN_ON_ERR_MSG(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig), "esp_wifi_set_config: ");

        if (!m_wifiInitialized) {
            RETURN_ON_ERR_MSG(esp_wifi_start(), "esp_wifi_start: ");
            m_wifiInitialized = true;
        }

        ESP_LOGD(TAG, "AP initialized. SSID:%s password:%s channel:%d", wifiConfig.ap.ssid, wifiConfig.ap.password, wifiConfig.ap.channel);
        return Ok(ComponentStatus::INITIALIZING);
    }

    void AccessPoint::eventHandler(void* arg, esp_event_base_t eventBase,
                                   int32_t eventId, void* eventData) {
        switch (eventId) {
            case WIFI_EVENT_AP_START: {
                xEventGroupSetBits(eventGroup, AP_INITIALIZED_BIT);
                ESP_LOGI(TAG, "AP has started");
                break;
            }
            case WIFI_EVENT_AP_STOP: {
                ESP_LOGI(TAG, "AP has stopped");
                break;
            }
            case WIFI_EVENT_AP_STACONNECTED: {
                ESP_LOGI(TAG, "A client has connected");
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                ESP_LOGI(TAG, "A client has disconected");
                break;
            }
            case WIFI_EVENT_AP_PROBEREQRECVED: {
                ESP_LOGI(TAG, "There's some sus probing happening right now");
                break;
            }
            case WIFI_EVENT_MAX: {
                ESP_LOGE(TAG, "Perhaps the archives were incomplete, %ld", eventId);
                break;
            }
        }
    }


} // namespace sdk::wifi