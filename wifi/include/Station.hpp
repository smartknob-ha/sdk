#ifndef WIFI_STA_HPP
#define WIFI_STA_HPP

#include <etl/vector.h>

#include "Component.hpp"
#include "ConfigProvider.hpp"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "freertos/event_groups.h"

namespace sdk::wifi {

    class Station final : public Component {
    public:
        class Config final : public ConfigObject<3, 512, "WIFI Station"> {
            using Base = ConfigObject<3, 512, "WIFI Station">;

        public:
            sdk::ConfigField<etl::string<31>> ssid{"", "ssid", sdk::RestartType::COMPONENT};
            sdk::ConfigField<etl::string<63>> password{"", "password", sdk::RestartType::COMPONENT};
            sdk::ConfigField<etl::string<31>> hostname{CONFIG_DEFAULT_HOSTNAME, "hostname", sdk::RestartType::COMPONENT};

            void allocateFields() {
                ssid     = allocate(ssid);
                password = allocate(password);
                hostname = allocate(hostname);
            }

            Config(const nlohmann::json& data) : Base(data) {
                allocateFields();
            }

            Config() : Base() {
                allocateFields();
            }
        };

        enum class STATUS {
            NOT_RUNNING,
            EMPTY_CONFIG,
            CONNECTING,
            CONNECTED,
            TIMED_OUT,
            DISCONNECTED,
            INVALID_PASSWORD,
            AP_NOT_FOUND,
            RECONNECTING,
            ERROR
        };

        Station() = default;

        /* Component override functions */
        etl::string<50> getTag() override { return TAG; };

        Status initialize() override;
        Status run() override;
        Status stop() override;

        STATUS               connect(bool blocking = true);
        static inline STATUS getWifiStatus() { return m_wifiStatus; };

        esp_netif_t * const getNetif() { return m_networkInterface; };
        void                     setConfig(const nlohmann::json& config);
        etl::string<15>          getIpInfo();

    private:
        static const inline char TAG[] = "Wifi STA";

        Config               m_config;
        esp_netif_t*         m_networkInterface;
        etl::string<15>      m_assignedIp;
        static inline bool   m_wifiInitialized = false;
        RestartType          m_restartType     = RestartType::NONE;
        static inline STATUS m_wifiStatus      = STATUS::DISCONNECTED;

        static etl::string<90> reasonToString(wifi_err_reason_t reason);

        static void eventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);

        res errorCheck(esp_err_t err);

        static inline EventGroupHandle_t eventGroup = xEventGroupCreate();
    };


} // namespace sdk::wifi

#endif /* WIFI_STA_HPP */