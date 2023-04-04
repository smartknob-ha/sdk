#include "wifi_sta.hpp"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

namespace sdk
{
    wifi_sta::wifi_sta(wifi_sta_config_t config)
    {
        m_config = config;
    }

    res wifi_sta::get_status()
    {
        return Ok(RUNNING);
    }

    res wifi_sta::run()
    {
        return Ok(RUNNING);
    }

    res wifi_sta::stop()
    {
        wifi_mode_t current_mode = WIFI_MODE_NULL;
        esp_err_t err = esp_wifi_get_mode(&current_mode);
        if (err == ESP_ERR_WIFI_NOT_INIT)
            ESP_LOGW(TAG, "Wifi is not running, returning");
        else if (err == ESP_OK && current_mode == WIFI_MODE_STA)
        {
            RES_RETURN_ON_ERROR(esp_wifi_stop());
            m_wifi_initialized = false;
        }
        else if (err == ESP_OK && current_mode == WIFI_MODE_APSTA)
            RES_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP));
        return Ok(STOPPED);
    }

    res wifi_sta::initialize()
    {

        vTaskDelay(7000 / portTICK_PERIOD_MS);
        wifi_mode_t current_mode = WIFI_MODE_NULL, new_mode = WIFI_MODE_STA;
        esp_err_t err = esp_wifi_get_mode(&current_mode);

        if (err == ESP_OK && current_mode == WIFI_MODE_STA)
        {
            m_wifi_initialized = true;
            return Err(etl::string<128>("Wifi STA is already initialized, returning"));
        }
        else if (err == ESP_OK && current_mode == WIFI_MODE_AP)
        {
            ESP_LOGW(TAG, "Wifi already initialized as AP, attempting to add Soft STA");
            new_mode = WIFI_MODE_APSTA;
            m_wifi_initialized = true;
        }

        if (!m_wifi_initialized)
        {
            ESP_LOGI(TAG, "Initializing wifi STA");
            // Don't care about return value as it either initializes, or it's already initialized
            esp_netif_init();
            RES_RETURN_ON_ERROR(esp_event_loop_create_default());
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            RES_RETURN_ON_ERROR(esp_wifi_init(&cfg));
        }

        m_esp_netif = esp_netif_create_default_wifi_sta();

        RES_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &sta_event_handler,
                                                            NULL,
                                                            NULL));
        RES_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &sta_event_handler,
                                                            NULL,
                                                            NULL));

        wifi_config_t wifi_config;
        memcpy(wifi_config.sta.ssid, m_config.ssid.data(), sizeof(m_config.ssid));
        memcpy(wifi_config.sta.password, m_config.pass.data(), sizeof(m_config.pass));

        // Disable AP mac verification
        wifi_config.sta.bssid_set = false;
        // Disable PMF requirement
        wifi_config.sta.pmf_cfg.required = false;

        RES_RETURN_ON_ERROR(esp_wifi_set_mode(new_mode));
        RES_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        RES_RETURN_ON_ERROR(esp_wifi_start());
        RES_RETURN_ON_ERROR(esp_wifi_connect());
        
        ESP_LOGI(TAG, "Attempting to connect to: \"%s\"", wifi_config.sta.ssid);

        return Ok(RUNNING);
    }

    void wifi_sta::sta_event_handler(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
        {
            ESP_LOGI(TAG, "STA has started");
            break;
        }
        case WIFI_EVENT_STA_STOP:
        {
            ESP_LOGI(TAG, "STA has stopped");
            break;
        }
        case WIFI_EVENT_STA_CONNECTED:
        {
            ESP_LOGI(TAG, "Connected to a network");
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGI(TAG, "Disconnected from a network, reason: %u", event->reason);
            break;
        }
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
        {
            ESP_LOGI(TAG, "Not sure?¿");
            break;
        }
        case IP_EVENT_STA_GOT_IP:
        {
            ESP_LOGI(TAG, "Got an IP");
            break;
        }
        case IP_EVENT_STA_LOST_IP:
        {
            ESP_LOGI(TAG, "Lost an IP");
            break;
        }
        case WIFI_EVENT_MAX:
        {
            ESP_LOGE(TAG, "Perhaps the archives were incomplete");
            break;
        }
        }
    }

} /* namespace sdk */