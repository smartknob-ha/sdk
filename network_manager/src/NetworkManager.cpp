#include "NetworkManager.hpp"

#include <regex>

#include "esp_sntp.h"
#include "esp_system_error.hpp"
#include "freertos/FreeRTOS.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

namespace sdk {
    res NetworkManager::getStatus() {
        return Status::RUNNING;
    }

    res NetworkManager::run() {
        return Status::RUNNING;
    }

    res NetworkManager::stop() {
        auto stationRet = sta.stop();
        if (stationRet.has_value()) {
            auto accessPointRet = ap.stop();
            if (accessPointRet.has_value()) {
                return Status::STOPPED;
            }
            ESP_LOGI(TAG, "Error stopping AP: %s", accessPointRet.error().message().c_str());
            return accessPointRet;
        }
        ESP_LOGI(TAG, "Error stopping STA: %s", stationRet.error().message().c_str());
        return stationRet;
    }

    res NetworkManager::initialize() {
        return Status::RUNNING;
    }

    res NetworkManager::setAccessPointState(bool state) {
        if (state)
            return ap.initialize();
        else
            return ap.stop();
    }

    void NetworkManager::setAccessPointConfig(wifi::AccessPoint::Config conf) {
        m_config.ap = conf;
    }

    res NetworkManager::setStationState(bool state) {
        if (state) {
            auto ret = sta.initialize();
            if (ret.has_value()) {
                return setIpMode(m_config.dhcpEnable);
            }
            return ret;
        } else
            return sta.stop();
    }

    void NetworkManager::setStationConfig(wifi::Station::Config conf) {
        m_config.sta = conf;
    }

    res NetworkManager::setIpMode(bool state) {
        if (state) {
            auto ret = esp_netif_dhcpc_start(sta.getNetif());
            if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
                ESP_LOGE(TAG, "Error starting DHCP client: %s", esp_err_to_name(ret));
                return std::unexpected(std::make_error_code(ret));
            } else {
                return Status::RUNNING;
            }
        } else {
            auto ret = esp_netif_dhcpc_stop(sta.getNetif());
            if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
                ESP_LOGE(TAG, "Error stopping DHCP client: %s", esp_err_to_name(ret));
                return std::unexpected(std::make_error_code(ret));
            } else {
                esp_netif_ip_info_t ip4Conf;
                inet_pton(AF_INET, m_config.ipv4Address.c_str(), &ip4Conf.ip);
                inet_pton(AF_INET, m_config.ipv4Gateway.c_str(), &ip4Conf.gw);
                inet_pton(AF_INET, m_config.ipv4Netmask.c_str(), &ip4Conf.netmask);

                auto err = std::make_error_code(esp_netif_set_ip_info(sta.getNetif(), &ip4Conf));
                if (err) {
                    ESP_LOGE(TAG, "Error setting static IP: %s", err.message().c_str());
                    return std::unexpected(err);
                }

                esp_netif_dns_info_t dnsInfo;
                inet_pton(AF_INET, m_config.ipv4DnsMain.c_str(), &dnsInfo.ip);
                err = std::make_error_code(esp_netif_set_dns_info(sta.getNetif(), ESP_NETIF_DNS_MAIN, &dnsInfo));
                if (err) {
                    ESP_LOGE(TAG, "Error setting static IP: %s", err.message().c_str());
                    return std::unexpected(err);
                }

                // Only set secondary if it has been set
                if (!m_config.ipv4DnsSecondary.empty()) {
                    inet_pton(AF_INET, m_config.ipv4DnsSecondary.c_str(), &dnsInfo.ip);
                    err = std::make_error_code(esp_netif_set_dns_info(sta.getNetif(), ESP_NETIF_DNS_BACKUP, &dnsInfo));
                    if (err) {
                        ESP_LOGE(TAG, "Error setting static IP: %s", err.message().c_str());
                        return std::unexpected(err);
                    }
                }
            }
        }
        return Status::RUNNING;
    }

    void NetworkManager::initSNTP() {
        esp_sntp_setservername(0, m_config.sntpHost.c_str());
        esp_sntp_init();
    }

    bool NetworkManager::validateIP(etl::string<50> ip_addr) {
        ip_addr_t ip4;
        return ipaddr_aton(ip_addr.c_str(), &ip4);
    }

    bool NetworkManager::validateHostname(etl::string<50> ip_addr) {
        // Source: https://www.regextester.com/23
        static const std::regex pattern(
                R"(^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])$)",
                std::regex_constants::ECMAScript);
        return std::regex_match(ip_addr.c_str(), pattern);
    }

} // namespace sdk