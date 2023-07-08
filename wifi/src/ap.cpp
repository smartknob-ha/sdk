#include "ap.hpp"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <etl/type_traits.h>
#include <etl/container.h>

#define AP_INITIALIZED_BIT  BIT0

namespace sdk
{
	namespace wifi {

    ap::ap(ap_config_t config) : m_config(config)
    {
        event_group = xEventGroupCreate();
    }

    res ap::get_status()
    {
        return Ok(RUNNING);
    }

    res ap::run()
    {
        return Ok(RUNNING);
    }

    res ap::stop()
    {
        wifi_mode_t current_mode = WIFI_MODE_NULL;
        esp_err_t err = esp_wifi_get_mode(&current_mode);
        if(err == ESP_ERR_WIFI_NOT_INIT)
            ESP_LOGW(TAG, "Wifi is not running, returning");
        else if(err == ESP_OK && current_mode == WIFI_MODE_AP) {
            RETURN_ERR_MSG(esp_wifi_stop(), "esp_wifi_stop: ");
        }
        else if(err == ESP_OK && current_mode == WIFI_MODE_APSTA)
            RETURN_ERR_MSG(esp_wifi_set_mode(WIFI_MODE_STA), "esp_wifi_set_mode: ");
        return Ok(STOPPED);
    }

    res ap::initialize() {
        auto ret = initialize_non_blocking();
        if (ret.isErr())
            return ret;

        // Wait for AP_INITIALIZED_BIT to be set by the event handler
        xEventGroupWaitBits(event_group, AP_INITIALIZED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        return Ok(RUNNING);
    }

    res ap::initialize_non_blocking()
    {
        wifi_mode_t current_mode = WIFI_MODE_NULL, new_mode = WIFI_MODE_AP;
        esp_err_t err = esp_wifi_get_mode(&current_mode);

        if (err == ESP_OK && current_mode == WIFI_MODE_AP)
        {
            m_wifi_initialized = true;
            return Err(etl::string<128>("Soft AP is already initialized, returning"));
        }
        else if (err == ESP_OK && current_mode == WIFI_MODE_STA)
        {
            ESP_LOGW(TAG, "Wifi already initialized as STA, attempting to add Soft AP");
            new_mode = WIFI_MODE_APSTA;
            m_wifi_initialized = true;
        }

        if (!m_wifi_initialized)
        {
            // Don't care about return value as it either initializes, or it's already initialized
            esp_netif_init();
            RETURN_ERR_MSG(esp_event_loop_create_default(), "esp_event_loop_create_default: ");
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            RETURN_ERR_MSG(esp_wifi_init(&cfg), "esp_wifi_init: ");
        }

        m_esp_netif = esp_netif_create_default_wifi_ap();

        RETURN_ERR_MSG(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler, NULL, NULL), "esp_event_handler_instance_register: ");

        wifi_config_t wifi_config = {
            .ap = {
                .channel = m_config.channel,
                .authmode = m_config.authmode,
                .max_connection = CONFIG_AP_MAX_CONNECTIONS
            }
        };

        // memcopy to avoid casting errors
        memcpy(wifi_config.ap.ssid, m_config.ssid.data(), m_config.ssid.size());
        memcpy(wifi_config.ap.password, m_config.pass.data(), m_config.pass.size());

        RETURN_ERR_MSG(esp_wifi_set_mode(new_mode), "esp_wifi_set_mode: ");
        RETURN_ERR_MSG(esp_wifi_set_config(WIFI_IF_AP, &wifi_config), "esp_wifi_set_config: ");

        if(!m_wifi_initialized){
            RETURN_ERR_MSG(esp_wifi_start(), "esp_wifi_start: ");
            m_wifi_initialized = true;
        }

        ESP_LOGD(TAG, "AP initialized. SSID:%s password:%s channel:%d", wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);
        return Ok(INITIALIZING);
    }

    void ap::ap_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
    {
        switch (event_id)
        {
        case WIFI_EVENT_AP_START: {
            xEventGroupSetBits(event_group, AP_INITIALIZED_BIT);
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
            ESP_LOGE(TAG, "Perhaps the archives were incomplete, %ld", event_id);
            break;
        }
        }
    }

	} /* namespace wifi */

} /* namespace sdk */