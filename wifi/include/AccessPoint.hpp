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
            class Config final : public ConfigObject<3, 512, "WIFI AP"> {
                using Base = ConfigObject<3, 512, "WIFI AP">;

            public:
                sdk::ConfigField<etl::string<31>>  ssid{"", "ssid", sdk::RestartType::COMPONENT};
                sdk::ConfigField<etl::string<63>>  password{"", "password", sdk::RestartType::COMPONENT};
                sdk::ConfigField<wifi_auth_mode_t> authMode{WIFI_AUTH_WPA2_PSK, "hostname", sdk::RestartType::COMPONENT};

                void allocateFields() {
                    ssid     = allocate(ssid);
                    password = allocate(password);
                    authMode = allocate(authMode);
                }

                Config(const nlohmann::json& data) : Base(data) {
                    allocateFields();
                }

                Config() : Base() {
                    allocateFields();
                }
            };

            typedef struct Client {
                etl::string<50> hostname;
                esp_ip4_addr_t  ip;
            } Client;

            AccessPoint() = default;

            /* Component override functions */
            virtual etl::string<50> getTag() override { return TAG; };

            virtual Status initialize() override;
            virtual Status run() override;
            virtual Status stop() override;

            // Initializes AP but returns while it may not be finished starting up
            res  initialize_non_blocking();
            void setConfig(const nlohmann::json& config, bool saveConfig = true);

            // TODO: Implement function
            // etl::vector<ap_client_t, CONFIG_AP_MAX_CONNECTIONS> get_connectedClients();

        private:
            static const inline char TAG[] = "Wifi AP";

            Config                                         m_config;
            etl::vector<Client, CONFIG_AP_MAX_CONNECTIONS> m_connectedClients;
            esp_netif_t*                                   m_espNetif;
            static inline bool                             m_wifiInitialized = false;
            RestartType                                    m_restartType     = RestartType::NONE;

            static void eventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);

            res error_check(esp_err_t err);

            static inline EventGroupHandle_t eventGroup = xEventGroupCreate();
        };

    } /* namespace wifi */

} /* namespace sdk */

#endif /* WIFI_AP_HPP */