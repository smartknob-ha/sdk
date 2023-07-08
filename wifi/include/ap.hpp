#ifndef WIFI_AP_HPP
#define WIFI_AP_HPP

#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi_types.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"

#include <etl/vector.h>

#include "component.hpp"

namespace sdk {

namespace wifi {

    typedef struct
    {
        etl::string<31> ssid;
        etl::string<63> pass;
        wifi_auth_mode_t authmode = WIFI_AUTH_WPA2_PSK;
        uint8_t channel = CONFIG_AP_CHANNEL;
    } ap_config_t;

    typedef struct {
        etl::string<50> hostname;
        esp_ip4_addr_t ip;
    } ap_client_t;

class ap : public component
{
public:
    ap(ap_config_t config);

    /* Component override functions */
    virtual etl::string<50> get_tag() override { return TAG; };
    virtual res get_status() override;
    virtual res initialize() override;
    virtual res run() override;
    virtual res stop() override;
    
    // Initializes AP but returns while it may not be finished starting up
    res initialize_non_blocking();
    void set_config(ap_config_t config) { m_config = config; };

    //TODO: Implement function
    //etl::vector<ap_client_t, CONFIG_AP_MAX_CONNECTIONS> get_connected_clients();

private:
    static const inline char TAG[] = "Wifi AP";
    
    ap_config_t m_config;
    etl::vector<ap_client_t, CONFIG_AP_MAX_CONNECTIONS> m_connected_clients;
    esp_netif_t *m_esp_netif;
    static inline bool m_wifi_initialized = false;

    static void ap_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    res error_check(esp_err_t err);

    static inline EventGroupHandle_t event_group;
};

} /* namespace wifi */

} /* namespace sdk */

#endif /* WIFI_AP_HPP */