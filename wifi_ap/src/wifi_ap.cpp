#include "wifi_ap.hpp"

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

namespace sdk
{
    wifi_ap::wifi_ap(wifi_ap_config_t config)
    {
        m_config = config;
    }

    res wifi_ap::get_status()
    {
        return Ok(RUNNING);
    }

    res wifi_ap::run()
    {
        return Ok(RUNNING);
    }

    res wifi_ap::stop()
    {
        wifi_mode_t current_mode = WIFI_MODE_NULL;
        esp_err_t err = esp_wifi_get_mode(&current_mode);
        if(err == ESP_ERR_WIFI_NOT_INIT)
            ESP_LOGW(TAG, "Wifi is not running, returning");
        else if(err == ESP_OK && current_mode == WIFI_MODE_AP) {
            RES_RETURN_ON_ERROR(esp_wifi_stop());
            m_wifi_initialized = false;
        }
        else if(err == ESP_OK && current_mode == WIFI_MODE_APSTA)
            RES_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA));
        return Ok(STOPPED);
    }

    res wifi_ap::initialize()
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
            RES_RETURN_ON_ERROR(esp_event_loop_create_default());
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            RES_RETURN_ON_ERROR(esp_wifi_init(&cfg));
        }

        m_esp_netif = esp_netif_create_default_wifi_ap();

        RES_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler, NULL, NULL));

        wifi_config_t wifi_config = {
            .ap = {
                .ssid_len = (uint8_t)sizeof(m_config.ssid),
                .channel = m_config.channel,
                .authmode = m_config.authmode,
                .max_connection = CONFIG_AP_MAX_CONNECTIONS}};
        // std copy to avoid casting errors
        memcpy(wifi_config.ap.ssid, m_config.ssid.data(), sizeof(m_config.ssid));
        memcpy(wifi_config.ap.password, m_config.pass.data(), sizeof(m_config.pass));

        RES_RETURN_ON_ERROR(esp_wifi_set_mode(new_mode));
        RES_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        RES_RETURN_ON_ERROR(esp_wifi_start());

        ESP_LOGI(TAG, "AP initialized. SSID:%s password:%s channel:%d", wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);
        return Ok(RUNNING);
    }

    void wifi_ap::ap_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
    {
        switch (event_id)
        {
        case WIFI_EVENT_AP_START: {
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

} /* namespace sdk */