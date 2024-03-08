#ifndef WIFI_AP_HPP
#define WIFI_AP_HPP

#include <etl/vector.h>

#include "Component.hpp"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "freertos/event_groups.h"

namespace sdk {

    namespace wifi {

        class AccessPoint : public Component {
        public:
            typedef struct Config {
                etl::string<31>  ssid;
                etl::string<63>  pass;
                wifi_auth_mode_t authmode = WIFI_AUTH_WPA2_PSK;
                uint8_t          channel  = CONFIG_AP_CHANNEL;
            } Config;

            typedef struct Client {
                etl::string<50> hostname;
                esp_ip4_addr_t  ip;
            } Client;

            AccessPoint(Config config);

            /* Component override functions */
            virtual etl::string<50> getTag() override { return TAG; };

            virtual res getStatus() override;
            virtual res initialize() override;
            virtual res run() override;
            virtual res stop() override;

            // Initializes AP but returns while it may not be finished starting up
            res  initialize_non_blocking();
            void setConfig(Config config) { m_config = config; };

            // TODO: Implement function
            // etl::vector<ap_client_t, CONFIG_AP_MAX_CONNECTIONS> get_connected_clients();

        private:
            static const inline char TAG[] = "Wifi AP";

            Config                                         m_config;
            etl::vector<Client, CONFIG_AP_MAX_CONNECTIONS> m_connected_clients;
            esp_netif_t*                                   m_espNetif;
            static inline bool                             m_wifiInitialized = false;

            static void eventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);

            res error_check(esp_err_t err);

            static inline EventGroupHandle_t eventGroup;
        };

    } /* namespace wifi */

} /* namespace sdk */

#endif /* WIFI_AP_HPP */