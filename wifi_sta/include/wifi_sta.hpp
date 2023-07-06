#ifndef WIFI_STA_HPP
#define WIFI_STA_HPP

#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi_types.h"
#include "esp_wifi.h"

#include <etl/vector.h>

#include "component.hpp"

namespace sdk {

namespace wifi {

typedef struct
{
    etl::string<31> ssid;
    etl::string<63> pass;
    wifi_auth_mode_t authmode = WIFI_AUTH_WPA2_PSK;
    etl::string<31> hostname = CONFIG_DEFAULT_HOSTNAME;
} sta_config_t;

class sta : public component
{
public:
    
    sta(sta_config_t config) { m_config = config; };
    ~sta() = default;

    /* Component override functions */
    virtual etl::string<50> get_tag() override { return TAG; };
    virtual res get_status() override;
    virtual res initialize() override;
    virtual res run() override;
    virtual res stop() override;

    esp_netif_t* get_netif() { return m_esp_netif; };
    void set_config(sta_config_t config) { m_config = config; };
    etl::string<15> get_assigned_ip() { return m_assigned_ip; };

private:
    static const inline char TAG[] = "Wifi STA";
    
    sta_config_t m_config;
    esp_netif_t *m_esp_netif;
    etl::string<15> m_assigned_ip;
    static inline bool m_wifi_initialized = false;

    static void sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    res error_check(esp_err_t err);
};

} /* namespace wifi */

} /* namespace sdk */

#endif /* WIFI_STA_HPP */