#include "NetworkManager.hpp"

#include <regex>

#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

namespace sdk {
    res NetworkManager::getStatus() {
        return Ok(ComponentStatus::RUNNING);
    }

    res NetworkManager::run() {
        return Ok(ComponentStatus::RUNNING);
    }

    res NetworkManager::stop() {
        auto stationRet     = sta.stop();
        auto accessPointRet = ap.stop();
        if (stationRet.isErr() && accessPointRet.isErr()) {
            auto string = etl::make_string_with_capacity<128>("Error stopping Wifi. STA: ");
            string += stationRet.unwrapErr();
            string += etl::make_string(" AP: ");
            string += accessPointRet.unwrapErr();
            return Err(string);
        } else if (stationRet.isErr()) {
            auto string = etl::make_string_with_capacity<128>("Error stopping STA: ");
            string += stationRet.unwrapErr();
            return Err(string);
        } else if (accessPointRet.isErr()) {
            auto string = etl::make_string_with_capacity<128>("Error stopping AP: ");
            string += accessPointRet.unwrapErr();
            return Err(string);
        }
        return Ok(ComponentStatus::STOPPED);
    }

    res NetworkManager::initialize() {
        return Ok(ComponentStatus::RUNNING);
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
            if (ret.isOk()) {
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
        esp_err_t ret;
        // State == true means DHCP mode
        if (state) {
            ret = esp_netif_dhcpc_start(sta.getNetif());
            if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)
                RETURN_ON_ERR(ret);
            else
                return Ok(ComponentStatus::RUNNING);
        } else {
            ret = esp_netif_dhcpc_stop(sta.getNetif());
            if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
                RETURN_ON_ERR(ret);
            else {
                esp_netif_ip_info_t ip4Conf;
                inet_pton(AF_INET, m_config.ipv4Address.c_str(), &ip4Conf.ip);
                inet_pton(AF_INET, m_config.ipv4Gateway.c_str(), &ip4Conf.gw);
                inet_pton(AF_INET, m_config.ipv4Netmask.c_str(), &ip4Conf.netmask);

                RETURN_ON_ERR(esp_netif_set_ip_info(sta.getNetif(), &ip4Conf));

                esp_netif_dns_info_t dnsInfo;
                inet_pton(AF_INET, m_config.ipv4DnsMain.c_str(), &dnsInfo.ip);
                RETURN_ON_ERR(esp_netif_set_dns_info(sta.getNetif(), ESP_NETIF_DNS_MAIN, &dnsInfo));

                // Only set secondary if it has been set
                if (!m_config.ipv4DnsSecondary.empty()) {
                    inet_pton(AF_INET, m_config.ipv4DnsSecondary.c_str(), &dnsInfo.ip);
                    esp_netif_set_dns_info(sta.getNetif(), ESP_NETIF_DNS_BACKUP, &dnsInfo);
                }
            }
        }
        return Ok(ComponentStatus::RUNNING);
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