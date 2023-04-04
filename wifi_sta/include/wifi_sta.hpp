#ifndef WIFI_STA_HPP
#define WIFI_STA_HPP

#include <string>
#include <vector>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi_types.h"
#include "esp_wifi.h"

#include <etl/vector.h>

#include "../../manager/include/component.hpp"

namespace sdk {

class wifi_sta : public component
{
public:
    typedef struct
    {
        etl::string<50> ssid;
        etl::string<50> pass;
        wifi_auth_mode_t authmode = WIFI_AUTH_WPA2_PSK;
    } wifi_sta_config_t;

    wifi_sta(wifi_sta_config_t config);

    /* Component override functions */
    virtual etl::string<50> get_tag() override { return TAG; };
    virtual res get_status() override;
    virtual res initialize() override;
    virtual res run() override;
    virtual res stop() override;


private:
    static const inline char TAG[] = "Wifi STA";
    
    wifi_sta_config_t m_config;
    esp_netif_t *m_esp_netif;
    static inline bool m_wifi_initialized = false;

    static void sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    res error_check(esp_err_t err);
};

} /* namespace sdk */

#endif /* WIFI_STA_HPP */