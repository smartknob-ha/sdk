#include "wifi_station.hpp"
#include "../../manager/include/manager.hpp"

#include <result.h>

extern "C" {
    #include "freertos/FreeRTOS.h"
    #include "sdkconfig.h"
    #include "freertos/task.h"
    #include "freertos/event_groups.h"
    #include "esp_system.h"
    #include "esp_wifi.h"
    #include "esp_event.h"
    #include "esp_log.h"

    #include "lwip/err.h"
    #include "lwip/sys.h"
};
#define STATION_CREDENTIALS_FROM_KCONFIG
#ifdef STATION_CREDENTIALS_FROM_KCONFIG
#define WIFI_SSID           CONFIG_STATION_SSID
#define WIFI_PASS           CONFIG_STATION_PASSWORD
#define WIFI_MAXIMUM_RETRY  CONFIG_STATION_MAXIMUM_RETRY
#endif

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

using namespace n_wifi_station;
using res = Result<esp_err_t, etl::string<128>>;
using res_str = etl::string<128>;

res wifi_station::initialize() {
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    if (!m_wifi_station_bus.subscribe(*this) ||
        !m_wifi_driver_bus.subscribe(*this)) {
        return Err(res_str("Failed to connect to WiFi station bus"));
    }

    esp_wifi_init(&config);

    esp_wifi_set_mode(wifi_mode_t::WIFI_MODE_STA);
    esp_wifi_start();
    //return esp_wifi_connect();


    return Ok(ESP_OK);
}

void wifi_station::event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    static int s_retry_num = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            //xEventGroupSetBits(m_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        //xEventGroupSetBits(m_wifi_event_group, WIFI_CONNECTED_BIT);
   }
}
