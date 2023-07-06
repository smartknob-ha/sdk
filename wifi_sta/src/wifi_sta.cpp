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
#include "lwip/sockets.h"

namespace sdk
{

	namespace wifi {

	res sta::get_status()
	{
		return Ok(RUNNING);
	}

	res sta::run()
	{
		return Ok(RUNNING);
	}

	res sta::stop()
	{
		wifi_mode_t current_mode = WIFI_MODE_NULL;
		esp_err_t err = esp_wifi_get_mode(&current_mode);
		if (err == ESP_ERR_WIFI_NOT_INIT)
			ESP_LOGW(TAG, "Wifi is not running, returning");
		else if (err == ESP_OK && current_mode == WIFI_MODE_STA)
		{
			RETURN_ERR_MSG(esp_wifi_stop(), "esp_wifi_stop: ");
		}
		else if (err == ESP_OK && current_mode == WIFI_MODE_APSTA)
			RETURN_ERR_MSG(esp_wifi_set_mode(WIFI_MODE_AP), "esp_wifi_set_mode: ");
		return Ok(STOPPED);
	}

	res sta::initialize()
	{
		vTaskDelay(3000 / portTICK_PERIOD_MS);
		printf("SSID: %s ; pass: %s\n", m_config.ssid.c_str(), m_config.pass.c_str());
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
			RETURN_ERR_MSG(esp_event_loop_create_default(), "esp_event_loop_create_default");
			wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
			RETURN_ERR_MSG(esp_wifi_init(&cfg), "esp_wifi_init: ");
		}
		m_esp_netif = esp_netif_create_default_wifi_sta();

		RETURN_ERR_MSG(esp_event_handler_instance_register(WIFI_EVENT,
																ESP_EVENT_ANY_ID,
																&sta_event_handler,
																NULL,
																NULL), "esp_event_handler_instance_register: ");
		RETURN_ERR_MSG(esp_event_handler_instance_register(IP_EVENT,
																IP_EVENT_STA_GOT_IP,
																&sta_event_handler,
																NULL,
																NULL), "esp_event_handler_instance_register: ");
		wifi_config_t wifi_config = {
			.sta = {
				.bssid_set = false,
				.pmf_cfg  = {
					.required = true
				}
			}
		};

		memcpy(wifi_config.sta.ssid, m_config.ssid.data(), m_config.ssid.size());
		memcpy(wifi_config.sta.password, m_config.pass.data(), m_config.pass.size());

		RETURN_ERR_MSG(esp_wifi_set_mode(new_mode), "esp_wifi_set_mode: ");
		RETURN_ERR_MSG(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), "esp_wifi_set_config: ");

		if(!m_wifi_initialized) {
			RETURN_ERR_MSG(esp_wifi_start(), "esp_wifi_start: ");
			m_wifi_initialized = true;
		}
		RETURN_ERR_MSG(esp_wifi_connect(), "esp_wifi_connect: ");

		ESP_LOGI(TAG, "Attempting to connect to: \"%s\"", wifi_config.sta.ssid);

		return Ok(RUNNING);
	}

	void sta::sta_event_handler(void *arg, esp_event_base_t event_base,
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

	} /* namespace wifi */

} /* namespace sdk */