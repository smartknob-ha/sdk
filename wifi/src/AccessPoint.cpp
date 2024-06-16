#include "AccessPoint.hpp"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system_error.hpp"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

#define AP_INITIALIZED_BIT BIT0

#define INIT_RETURN_ON_ERROR(err)                                      \
    do {                                                               \
        if (err != ESP_OK) {                                           \
            ESP_LOGE(TAG, "Failed to init: %s", esp_err_to_name(err)); \
            m_err           = err;                                     \
            return m_status = Status::ERROR;                           \
        }                                                              \
    } while (0)

namespace sdk::wifi {

    using Status = sdk::Component::Status;
    using res    = sdk::Component::res;

    Status AccessPoint::run() {
        return m_status;
    }

    Status AccessPoint::stop() {
        wifi_mode_t currentMode = WIFI_MODE_NULL;
        esp_err_t   err         = esp_wifi_get_mode(&currentMode);
        if (err == ESP_ERR_WIFI_NOT_INIT) {
            ESP_LOGD(TAG, "Wifi is not running, returning");
        } else if (err == ESP_OK && currentMode == WIFI_MODE_STA) {
            err = esp_wifi_stop();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to stop wifi: %s", esp_err_to_name(err));
                return m_status = Status::ERROR;
            }
        } else if (err == ESP_OK && currentMode == WIFI_MODE_APSTA) {
            ESP_LOGD(TAG, "Wifi is in APSTA mode, stopping AP only");
            err = esp_wifi_set_mode(WIFI_MODE_STA);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set wifi mode to STA: %s", esp_err_to_name(err));
                return m_status = Status::ERROR;
            }
        }
        return Status::STOPPED;
    }

    Status AccessPoint::initialize() {
        auto ret = initialize_non_blocking();
        if (!ret.has_value()) {
            ESP_LOGE(TAG, "Failed to initialize AP: %s", ret.error().message().c_str());
            return m_status = Status::ERROR;
        }

        // Wait for AP_INITIALIZED_BIT to be set by the event handler
        xEventGroupWaitBits(eventGroup, AP_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        return m_status = Status::RUNNING;
    }

    res AccessPoint::initialize_non_blocking() {
        m_status                 = Status::INITIALIZING;
        wifi_mode_t current_mode = WIFI_MODE_NULL, new_mode = WIFI_MODE_AP;
        esp_err_t   err = esp_wifi_get_mode(&current_mode);

        if (err == ESP_OK && current_mode == WIFI_MODE_AP) {
            m_wifiInitialized = true;
            return m_status   = Status::RUNNING;
        } else if (err == ESP_OK && current_mode == WIFI_MODE_STA) {
            ESP_LOGW(TAG, "Wifi already initialized as STA, attempting to add Soft AP");
            new_mode          = WIFI_MODE_APSTA;
            m_wifiInitialized = true;
        }

        if (!m_wifiInitialized) {
            // Don't care about return value as it either initializes, or it's already initialized
            esp_netif_init();
            INIT_RETURN_ON_ERROR(esp_event_loop_create_default());
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            INIT_RETURN_ON_ERROR(esp_wifi_init(&cfg));
        }

        m_espNetif = esp_netif_create_default_wifi_ap();

        INIT_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, NULL, NULL));

        wifi_config_t wifiConfig = {
                .ap = {
                        .channel        = CONFIG_AP_CHANNEL,
                        .authmode       = m_config.authMode,
                        .max_connection = CONFIG_AP_MAX_CONNECTIONS}};

        // memcopy to avoid casting errors
        memcpy(wifiConfig.ap.ssid, m_config.ssid.value().data(), m_config.ssid.value().size());
        memcpy(wifiConfig.ap.password, m_config.password.value().data(), m_config.password.value().size());

        INIT_RETURN_ON_ERROR(esp_wifi_set_mode(new_mode));
        INIT_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig));

        if (!m_wifiInitialized) {
            INIT_RETURN_ON_ERROR(esp_wifi_start());
            m_wifiInitialized = true;
        }

        ESP_LOGD(TAG, "AP initialized. SSID:%s password:%s channel:%d", wifiConfig.ap.ssid, wifiConfig.ap.password, wifiConfig.ap.channel);
        return m_status = Status::RUNNING;
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
                ESP_LOGI(TAG, "A client has disconnected");
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

    void AccessPoint::setConfig(const nlohmann::json& config) {
        Config newConfig(config);
        if (newConfig == m_config) {
            ESP_LOGD(TAG, "Incoming config is the same, returning");
            return;
        }
        m_config = newConfig;
        //All config changes require a restart of the component
        m_restartType = RestartType::COMPONENT;
    }


} // namespace sdk::wifi