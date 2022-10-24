#include "wifi_ap.hpp"
#include "esp_log.h"
#include "esp_wifi.h"
#include "config.h"

void WifiAP::wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

esp_err_t WifiAP::initialize_ap()
{
    // Don't care about return value as it either initializes, or it's already initialized
    esp_netif_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *wifi_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
#ifdef CONFIG_AP_CREDENTIALS_FROM_KCONFIG
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_AP_SSID,
            .password = CONFIG_AP_PASSWORD,
            .ssid_len = strlen(CONFIG_AP_SSID),
            .channel = CONFIG_AP_CHANNEL,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = CONFIG_AP_MAX_CONNECTIONS}
    };
#else
    wifi_config_t wifi_config = {
        .ap = {
            // .ssid = reinterpret_cast<uint8_t>(&ap_ssid[0]),
            // .password = reinterpret_cast<uint8_t>(ap_ssid.c_str()),
            // .ssid_len = ap_ssid.length(),
            .channel = CONFIG_AP_CHANNEL,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = CONFIG_AP_MAX_CONNECTIONS}
    };
    // std copy to avoid casting errors
    std::copy(ap_ssid.begin(), ap_ssid.end(), std::begin(wifi_config.ap.ssid));
    std::copy(ap_pass.begin(), ap_pass.end(), std::begin(wifi_config.ap.password));

#endif

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP initialized. SSID:%s password:%s channel:%d", wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);
    return ESP_OK;
}

WifiAP::WifiAP(std::string ssid, std::string password) {
    ap_ssid = ssid;
    ap_pass = password;
}