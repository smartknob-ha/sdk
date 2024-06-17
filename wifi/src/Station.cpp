#include "Station.hpp"

#include <lwip/ip4_addr.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_system_error.hpp"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define STA_CONNECTED_BIT BIT0
#define STA_DISCONNECTED_BIT BIT1

#define INIT_RETURN_ON_ERROR(err)                                      \
    do {                                                               \
        if (err != ESP_OK) {                                           \
            ESP_LOGE(TAG, "Failed to init: %s", esp_err_to_name(err)); \
            m_err           = err;                                     \
            return m_status = Status::ERROR;                           \
        }                                                              \
    } while (0)

namespace sdk {
    namespace wifi {

        using Status = Component::Status;
        using res    = Component::res;

        Status Station::run() {
            return m_status;
        }

        Status Station::stop() {
            wifi_mode_t currentMode = WIFI_MODE_NULL;
            esp_err_t   err         = esp_wifi_get_mode(&currentMode);
            if (err == ESP_ERR_WIFI_NOT_INIT) {
                ESP_LOGW(TAG, "Wifi is not running, returning");
            } else if (err == ESP_OK && currentMode == WIFI_MODE_STA) {
                err = esp_wifi_stop();
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to stop STA: %s", esp_err_to_name(err));
                    return m_status = Status::ERROR;
                }
            } else if (err == ESP_OK && currentMode == WIFI_MODE_APSTA) {
                ESP_LOGD(TAG, "Wifi is in APSTA mode, stopping STA only");
                err = esp_wifi_set_mode(WIFI_MODE_AP);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to set wifi mode to AP: %s", esp_err_to_name(err));
                    return m_status = Status::ERROR;
                }
            }
            return m_status = Status::STOPPED;
        }

        Status Station::initialize() {
            wifi_mode_t current_mode = WIFI_MODE_NULL, new_mode = WIFI_MODE_STA;
            esp_err_t   err = esp_wifi_get_mode(&current_mode);
            if (err == ESP_OK && current_mode == WIFI_MODE_STA) {
                m_wifiInitialized = true;
                return m_status   = Status::RUNNING;
            } else if (err == ESP_OK && current_mode == WIFI_MODE_AP) {
                ESP_LOGW(TAG, "Wifi already initialized as AP, attempting to add Soft STA");
                new_mode          = WIFI_MODE_APSTA;
                m_wifiInitialized = true;
            }
            if (!m_wifiInitialized) {
                ESP_LOGI(TAG, "Initializing wifi STA");
                // Don't care about return value as it either initializes, or it's already initialized
                esp_netif_init();
                INIT_RETURN_ON_ERROR(esp_event_loop_create_default());
                wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
                INIT_RETURN_ON_ERROR(esp_wifi_init(&cfg));
            }

            esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
            // Default config has DHCP client enabled on startup, we will do this manually later (if configured)
            esp_netif_config.flags = (esp_netif_flags_t) (ESP_NETIF_FLAG_GARP | ESP_NETIF_FLAG_EVENT_IP_MODIFIED);
            m_networkInterface     = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);

            esp_wifi_set_default_wifi_sta_handlers();

            esp_netif_set_hostname(m_networkInterface, m_config.hostname.value().c_str());

            INIT_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT,
                                                                     ESP_EVENT_ANY_ID,
                                                                     &eventHandler,
                                                                     NULL,
                                                                     NULL));
            INIT_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT,
                                                                     IP_EVENT_STA_GOT_IP,
                                                                     &eventHandler,
                                                                     NULL,
                                                                     NULL));

            INIT_RETURN_ON_ERROR(esp_wifi_set_mode(new_mode));

            if (!m_wifiInitialized) {
                INIT_RETURN_ON_ERROR(esp_wifi_start());
                m_wifiInitialized = true;
            }

            return m_status = Status::RUNNING;
        }

        Station::STATUS Station::connect(bool blocking) {
            if (m_status != Status::RUNNING) {
                ESP_LOGE(TAG, "Station is not running, returning");
                return STATUS::NOT_RUNNING;
            }

            if (m_config.ssid.value().empty()) {
                ESP_LOGE(TAG, "SSID is empty, returning");
                return STATUS::EMPTY_CONFIG;
            }

            wifi_config_t wifiConfig = {
                    .sta = {
                            .bssid_set = false,
                            .pmf_cfg   = {
                                      .required = true}}};

            memcpy(wifiConfig.sta.ssid, m_config.ssid.value().data(), m_config.ssid.value().size());
            if (m_config.password.value().empty()) {
                ESP_LOGW(TAG, "Password is empty, connecting without password");
            }
            memcpy(wifiConfig.sta.password, m_config.password.value().data(), m_config.password.value().size());

            auto ret = esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set wifi config: %s", esp_err_to_name(ret));
                return m_wifiStatus = STATUS::ERROR;
            }

            ESP_LOGI(TAG, "Attempting to connect to: \"%s\"", wifiConfig.sta.ssid);

            ret = esp_wifi_connect();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to connect to wifi: %s", esp_err_to_name(ret));
                return m_wifiStatus = STATUS::ERROR;
            }

            if (!blocking) {
                return m_wifiStatus = STATUS::CONNECTING;
            }

            // Wait for one of the bits to be set by the event handler
            EventBits_t bits = xEventGroupWaitBits(eventGroup,
                                                   STA_CONNECTED_BIT | STA_DISCONNECTED_BIT,
                                                   pdFALSE,
                                                   pdFALSE,
                                                   CONFIG_STATION_CONNECT_TIMEOUT * 1000 / portTICK_PERIOD_MS);

            if (bits == STA_CONNECTED_BIT) {
                return m_wifiStatus;
            } else if (bits == STA_DISCONNECTED_BIT) {
                ESP_LOGE(TAG, "Failed to connect to wifi");
                return m_wifiStatus;
            } else {
                ESP_LOGE(TAG, "Wifi connection timed out after 30 seconds");
                return STATUS::TIMED_OUT;
            }
        }

        void Station::eventHandler(void* arg, esp_event_base_t eventBase,
                                   int32_t eventId, void* eventData) {
            switch (eventId) {
                case WIFI_EVENT_STA_START: {
                    ESP_LOGI(TAG, "Has started");
                    break;
                }
                case WIFI_EVENT_STA_STOP: {
                    ESP_LOGI(TAG, "Has stopped");
                    break;
                }
                case WIFI_EVENT_STA_CONNECTED: {
                    xEventGroupSetBits(eventGroup, STA_CONNECTED_BIT);
                    wifi_event_sta_connected_t* event = static_cast<wifi_event_sta_connected_t*>(eventData);
                    ESP_LOGI(TAG, "Successfully connected to: %s", event->ssid);
                    m_wifiStatus = STATUS::CONNECTED;
                    break;
                }
                case WIFI_EVENT_STA_DISCONNECTED: {
                    wifi_event_sta_disconnected_t* event  = static_cast<wifi_event_sta_disconnected_t*>(eventData);
                    wifi_err_reason_t              reason = static_cast<wifi_err_reason_t>(event->reason);
                    ESP_LOGW(TAG, "Disconnected from a network, reason: (%u) %s", reason, reasonToString(reason).c_str());

                    switch (reason) {
                        case WIFI_REASON_AUTH_FAIL: {
                            ESP_LOGE(TAG, "Authentication failed, not reconnecting");
                            m_wifiStatus = STATUS::INVALID_PASSWORD;
                            break;
                        }
                        case WIFI_REASON_NO_AP_FOUND: {
                            ESP_LOGE(TAG, "No AP found with that SSID, not reconnecting");
                            m_wifiStatus = STATUS::AP_NOT_FOUND;
                            break;
                        }
                        case WIFI_REASON_ASSOC_LEAVE: {
                            ESP_LOGI(TAG, "Disconnected on purpose, not reconnecting");
                            m_wifiStatus = STATUS::DISCONNECTED;
                            break;
                        }
                        default: {
                            auto ret = esp_wifi_connect();
                            if (ret != ESP_OK) {
                                ESP_LOGE(TAG, "Failed to reconnect to wifi: %s", esp_err_to_name(ret));
                                m_wifiStatus = STATUS::ERROR;
                                break;
                            }
                            m_wifiStatus = STATUS::RECONNECTING;
                            // Return instead of breaking, so we don't set the disconnected bit, as we are retrying
                            return;
                        }
                    }
                    xEventGroupSetBits(eventGroup, STA_DISCONNECTED_BIT);
                }
                case IP_EVENT_STA_GOT_IP: {
                    ip_event_got_ip_t*  event = (ip_event_got_ip_t*) eventData;
                    esp_netif_ip_info_t ip    = event->ip_info;
                    ESP_LOGD(TAG, "Was assigned an IP: " IPSTR, IP2STR(&ip.ip));
                    break;
                }
                case IP_EVENT_STA_LOST_IP: {
                    ESP_LOGD(TAG, "Assigned IP address was lost");
                    break;
                }
            }
        }

        void Station::setConfig(const nlohmann::json& config) {
            Config newConfig(config);
            if (newConfig == m_config) {
                ESP_LOGD(TAG, "Incoming config is the same, returning");
                return;
            }
            m_config = newConfig;
            // All config changes require a restart of the component
            m_restartType = RestartType::COMPONENT;
        }

        etl::string<15> Station::getAssignedIp() {
            if (!esp_netif_is_netif_up(m_networkInterface)) {
                ESP_LOGE(TAG, "Network interface is not up, unable to request IP info");
                return etl::string<15>();
            }
            esp_netif_ip_info_t ipInfo;
            auto                ret = esp_netif_get_ip_info(m_networkInterface, &ipInfo);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to get IP info: %s", esp_err_to_name(ret));
                return etl::string<15>();
            }

            etl::string<15> ip;
            esp_ip4addr_ntoa(&ipInfo.ip, ip.data(), ip.max_size());
            ip.repair();
            return ip;
        }

        etl::vector<Station::WifiRecord, CONFIG_AP_SCAN_NUMBER> Station::scan() {
            if(!m_wifiInitialized){
                ESP_LOGE(TAG, "Wifi is not initialized, unable to perform scan, returning");
                return {};
            }

            ESP_LOGD(TAG, "Starting scan...");
            auto err = esp_wifi_scan_start(NULL, true);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(err));
                return {};
            }

            uint16_t ap_count = 0;
            err = esp_wifi_scan_get_ap_num(&ap_count);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to get scan count: %s", esp_err_to_name(err));
                return {};
            }
            ESP_LOGD(TAG, "Scan finished; found %d APs", ap_count);

            uint16_t scanCount = CONFIG_AP_SCAN_NUMBER;
            std::array<wifi_ap_record_t, CONFIG_AP_SCAN_NUMBER> apRecords;
            err = esp_wifi_scan_get_ap_records(&scanCount, apRecords.data());
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to get scan records: %s", esp_err_to_name(err));
                return {};
            }

            if(scanCount > CONFIG_AP_SCAN_NUMBER){
                ESP_LOGW(TAG, "Scanned more AP than the configured maximum, truncating to %d", CONFIG_AP_SCAN_NUMBER);
                scanCount = CONFIG_AP_SCAN_NUMBER;
            }

            etl::vector<WifiRecord, CONFIG_AP_SCAN_NUMBER> aps;
            ESP_LOGD(TAG, "Found APs:\n");
            for (int i = 0; i < scanCount; i++) {
                ESP_LOGD(TAG, "\tSSID: %s, RSSI: %d", apRecords[i].ssid, apRecords[i].rssi);
                aps.emplace_back(etl::string<32>(reinterpret_cast<char*>(apRecords[i].ssid), sizeof(apRecords[i].ssid)), apRecords[i].rssi);
            }

            return aps;
        }

        etl::string<90> Station::reasonToString(wifi_err_reason_t reason) {
            switch (reason) {
                case WIFI_REASON_UNSPECIFIED:
                    return "Unspecified reason";
                case WIFI_REASON_AUTH_EXPIRE:
                    return "Previous authentication no longer valid";
                case WIFI_REASON_AUTH_LEAVE:
                    return "Station is leaving (or has left) IBSS or ESS";
                case WIFI_REASON_ASSOC_EXPIRE:
                    return "Disassociated due to inactivity";
                case WIFI_REASON_ASSOC_TOOMANY:
                    return "Disassociated because AP is unable to handle all currently associated stations";
                case WIFI_REASON_NOT_AUTHED:
                    return "Class 2 frame received from nonauthenticated station";
                case WIFI_REASON_NOT_ASSOCED:
                    return "Class 3 frame received from nonassociated station";
                case WIFI_REASON_ASSOC_LEAVE:
                    return "Disassociated because sending station is leaving (or has left) basic service set (BSS)";
                case WIFI_REASON_ASSOC_NOT_AUTHED:
                    return "Station requesting association or reassociation not authenticated with responding station";
                case WIFI_REASON_DISASSOC_PWRCAP_BAD:
                    return "Disassociated because of unacceptable information in the power capability element";
                case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
                    return "Disassociated because of unacceptable information in the supported channels element";
                case WIFI_REASON_BSS_TRANSITION_DISASSOC:
                    return "Association denied due to reason outside scope of this standard";
                case WIFI_REASON_IE_INVALID:
                    return "Invalid information (Doesn't follow 802.11 standard)";
                case WIFI_REASON_MIC_FAILURE:
                    return "Message integrity code (MIC) failure";
                case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
                    return "4-way handshake timeout";
                case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
                    return "Group-key handshake timeout";
                case WIFI_REASON_IE_IN_4WAY_DIFFERS:
                    return "Information element in 4-way handshake different from association request";
                case WIFI_REASON_GROUP_CIPHER_INVALID:
                    return "Invalid group cipher";
                case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
                    return "Invalid pairwise cipher";
                case WIFI_REASON_AKMP_INVALID:
                    return "Invalid authentication and key management protocol (AKMP)";
                case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
                    return "Unsupported robust security network (RSN) information element version";
                case WIFI_REASON_INVALID_RSN_IE_CAP:
                    return "Invalid RSN information element capabilities";
                case WIFI_REASON_802_1X_AUTH_FAILED:
                    return "IEEE 802.1X authentication failed";
                case WIFI_REASON_CIPHER_SUITE_REJECTED:
                    return "Cipher suite rejected because of security policy";
                case WIFI_REASON_TDLS_PEER_UNREACHABLE:
                case WIFI_REASON_TDLS_UNSPECIFIED:
                    return "TDLS direct-link setup failed";
                case WIFI_REASON_SSP_REQUESTED_DISASSOC:
                    return "Supported set identification (SSID) requested the station to leave";
                case WIFI_REASON_NO_SSP_ROAMING_AGREEMENT:
                    return "No SSP roaming agreement";
                case WIFI_REASON_BAD_CIPHER_OR_AKM:
                    return "Bad cipher or authentication algorithm";
                case WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION:
                    return "Not authorized for this location";
                case WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS:
                    return "Service change precludes Transport Authentication (TS)";
                case WIFI_REASON_UNSPECIFIED_QOS:
                    return "Unspecified. Quality of service (QoS)-related failure";
                case WIFI_REASON_NOT_ENOUGH_BANDWIDTH:
                    return "Association denied because QoS AP has insufficient bandwidth to handle another QoS station";
                case WIFI_REASON_MISSING_ACKS:
                    return "Association denied due to excessive frame loss rates";
                case WIFI_REASON_EXCEEDED_TXOP:
                    return "Exceeded transmission opportunity";
                case WIFI_REASON_STA_LEAVING:
                    return "Requested from peer station as station is leaving or resetting the BSS";
                case WIFI_REASON_END_BA:
                    return "Requested from peer station as it doesn't want to use the mechanism";
                case WIFI_REASON_UNKNOWN_BA:
                    return "Requested from peer station as station received frames using the mechanism";
                case WIFI_REASON_TIMEOUT:
                    return "Requested from peer station due to timeout";
                case WIFI_REASON_PEER_INITIATED:
                    return "Peer station requested to reset";
                case WIFI_REASON_AP_INITIATED:
                    return "AP requested to reset";
                case WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT:
                    return "Invalid fast transition (FT) action frame count";
                case WIFI_REASON_INVALID_PMKID:
                    return "Invalid shared key (pairwise master key identifier or PMKID)";
                case WIFI_REASON_INVALID_MDE:
                    return "Invalid mobility domain element (MDE)";
                case WIFI_REASON_INVALID_FTE:
                    return "Invalid fast transition element (FTE)";
                case WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED:
                    return "Transmission link establishment failed";
                case WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED:
                    return "Alternative channel is occupied";
                case WIFI_REASON_BEACON_TIMEOUT:
                    return "Beacon timeout";
                case WIFI_REASON_NO_AP_FOUND:
                    return "No AP found";
                case WIFI_REASON_AUTH_FAIL:
                    return "Authentication failure, likely wrong password";
                case WIFI_REASON_ASSOC_FAIL:
                    return "Association failure";
                case WIFI_REASON_HANDSHAKE_TIMEOUT:
                    return "Handshake timeout";
                case WIFI_REASON_CONNECTION_FAIL:
                    return "Connection failure";
                case WIFI_REASON_AP_TSF_RESET:
                    return "AP TSF reset";
                case WIFI_REASON_ROAMING:
                    return "Roaming";
                case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG:
                    return "Association comeback time too long";
                case WIFI_REASON_SA_QUERY_TIMEOUT:
                    return "SA query timeout";
                case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
                    return "No AP found with compatible security";
                case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
                    return "No AP found within acceptable authentication mode threshold";
                case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
                    return "No AP found within acceptable signal strength threshold";
                default:
                    return "Unknown reason";
            }
        }
    } /* namespace wifi */

} /* namespace sdk */