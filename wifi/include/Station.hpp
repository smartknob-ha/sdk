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

        /**
         * @brief Status codes for the wifi station
         */
        enum class STATUS {
            /**
             * @brief The wifi station is not initialized
             */
            NOT_RUNNING,
            /**
             * @brief To indicate that the configured SSID is empty
             */
            EMPTY_CONFIG,
            /**
             * @brief The wifi station is currently connecting
             */
            CONNECTING,
            /**
             * @brief The wifi station is connected
             */
            CONNECTED,
            /**
             * @brief The wifi station has timed out while connecting
             */
            TIMED_OUT,
            /**
             * @brief The wifi station is disconnected
             */
            DISCONNECTED,
            /**
             * @brief The configured password is invalid
             */
            INVALID_PASSWORD,
            /**
             * @brief The configured SSID is not found
             */
            AP_NOT_FOUND,
            /**
             * @brief The wifi station is currently reconnecting
             */
            RECONNECTING,
            /**
             * @brief The wifi station has encountered an error
             */
            ERROR
        };

        struct WifiRecord {
            etl::string<32> ssid;
            int        rssi;
        };

        Station() = default;

        /* Component override functions */
        etl::string<50> getTag() override { return TAG; };

        Status initialize() override;
        Status run() override;
        Status stop() override;

        /**
         * @brief Connects to the configured SSID
         * @param blocking If true, the function will block until the connection is established, or times out after CONFIG_STATION_CONNECT_TIMEOUT
         * @return STATUS
         */
        STATUS connect(bool blocking = true);

        static inline STATUS getWifiStatus() { return m_wifiStatus; };

        /**
         * @brief Scans for available access points
         * @note This function will block until the scan is complete
         * @note initialize() must be called before this function
         * @return etl::vector<WifiRecord, CONFIG_AP_SCAN_NUMBER> Empty vector on error or no APs found
         */
        etl::vector<WifiRecord, CONFIG_AP_SCAN_NUMBER> scan();

        esp_netif_t* const getNetif() { return m_networkInterface; };
        void               setConfig(const nlohmann::json& config, bool saveConfig = true);

        /**
         * @brief Retrieves current assigned IP address
         * @return etl::string<15> Empty string if no IP assigned
         */
        etl::string<15>    getAssignedIp();

    private:
        static const inline char TAG[] = "Wifi STA";

        Config               m_config;
        esp_netif_t*         m_networkInterface;
        etl::string<15>      m_assignedIp;
        static inline bool   m_wifiInitialized = false;
        RestartType          m_restartType     = RestartType::NONE;
        static inline STATUS m_wifiStatus      = STATUS::NOT_RUNNING;

        static etl::string<90> reasonToString(wifi_err_reason_t reason);

        static void eventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);

        res errorCheck(esp_err_t err);

        static inline EventGroupHandle_t eventGroup = xEventGroupCreate();
    };


} // namespace sdk::wifi

#endif /* WIFI_STA_HPP */