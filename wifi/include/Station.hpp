#ifndef WIFI_STA_HPP
#define WIFI_STA_HPP

#include <etl/vector.h>

#include "Component.hpp"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "freertos/event_groups.h"

namespace sdk::wifi {

    class Station : public Component {
    public:
        typedef struct Config {
            etl::string<31>  ssid;
            etl::string<63>  pass;
            wifi_auth_mode_t authmode = WIFI_AUTH_WPA2_PSK;
            etl::string<31>  hostname = CONFIG_DEFAULT_HOSTNAME;
        } Config;

        Station(Config config);
        Station() = default;

        /* Component override functions */
        virtual etl::string<50> getTag() override { return TAG; };

        virtual res getStatus() override;
        virtual res initialize() override;
        virtual res run() override;
        virtual res stop() override;

        // Initializes sta but returns while it may not be finished starting up
        res             initializeNonBlocking();
        esp_netif_t*    getNetif() { return m_esp_netif; };
        void            setConfig(Config config) { m_config = config; };
        etl::string<15> getAssignedIp() { return m_assignedIp; };

    private:
        static const inline char TAG[] = "Wifi STA";

        Config             m_config;
        esp_netif_t*       m_esp_netif;
        etl::string<15>    m_assignedIp;
        static inline bool m_wifiInitialized = false;

        static void stationEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

        res errorCheck(esp_err_t err);

        static inline EventGroupHandle_t eventGroup;
    };


} // namespace sdk::wifi

#endif /* WIFI_STA_HPP */