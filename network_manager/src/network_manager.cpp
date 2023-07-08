#include "network_manager.hpp"

#include <string.h>
#include <regex>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "esp_sntp.h"

namespace sdk
{
    res network_manager::get_status()
    {
        return Ok(RUNNING);
    }

    res network_manager::run()
    {
        return Ok(RUNNING);
    }

    res network_manager::stop()
    {
        auto sta_ret = sta.stop();
        auto ap_ret = ap.stop();
        if (sta_ret.isErr() && ap_ret.isErr())
        {
            auto string = etl::make_string_with_capacity<128>("Error stopping Wifi. STA: ");
            string += sta_ret.unwrapErr();
            string += etl::make_string(" AP: ");
            string += ap_ret.unwrapErr();
            return Err(string);
        }
        else if (sta_ret.isErr())
        {
            auto string = etl::make_string_with_capacity<128>("Error stopping STA: ");
            string += sta_ret.unwrapErr();
            return Err(string);
        }
        else if (ap_ret.isErr())
        {
            auto string = etl::make_string_with_capacity<128>("Error stopping AP: ");
            string += ap_ret.unwrapErr();
            return Err(string);
        }
        return Ok(STOPPED);
    }

    res network_manager::initialize()
    {
        return Ok(RUNNING);
    }

    res network_manager::set_ap_state(bool state)
    {
        if (state)
            return ap.initialize();
        else
            return ap.stop();
    }

    void network_manager::set_ap_conf(wifi::ap_config_t conf)
    {
        m_config.ap = conf;
    }

    res network_manager::set_sta_state(bool state)
    {
        if (state)
        {
            auto ret = sta.initialize();
            if (ret.isOk())
            {
                return set_ip_mode(m_config.dhcp_enable);
            }
            return ret;
        }
        else
            return sta.stop();
    }

    void network_manager::set_sta_conf(wifi::sta_config_t conf)
    {
        m_config.sta = conf;
    }

    res network_manager::set_ip_mode(bool state)
    {
        esp_err_t ret;
        // State == true means DHCP mode
        if (state)
        {
            ret = esp_netif_dhcpc_start(sta.get_netif());
            if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)
                RETURN_ON_ERR(ret);
            else
                return Ok(RUNNING);
        }
        else
        {
            ret = esp_netif_dhcpc_stop(sta.get_netif());
            if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
                RETURN_ON_ERR(ret);
            else
            {
                esp_netif_ip_info_t ip4_conf;
                inet_pton(AF_INET, m_config.ipv4_address.c_str(), &ip4_conf.ip);
                inet_pton(AF_INET, m_config.ipv4_gateway.c_str(), &ip4_conf.gw);
                inet_pton(AF_INET, m_config.ipv4_netmask.c_str(), &ip4_conf.netmask);

                RETURN_ON_ERR(esp_netif_set_ip_info(sta.get_netif(), &ip4_conf));

                esp_netif_dns_info_t dns_info;
                inet_pton(AF_INET, m_config.ipv4_dns_main.c_str(), &dns_info.ip);
                RETURN_ON_ERR(esp_netif_set_dns_info(sta.get_netif(), ESP_NETIF_DNS_MAIN, &dns_info));

                // Only set secondary if it has been set
                if (!m_config.ipv4_dns_secondary.empty())
                {
                    inet_pton(AF_INET, m_config.ipv4_dns_secondary.c_str(), &dns_info.ip);
                    esp_netif_set_dns_info(sta.get_netif(), ESP_NETIF_DNS_BACKUP, &dns_info);
                }
            }
        }
        return Ok(RUNNING);
    }

    void network_manager::init_sntp()
    {
        sntp_setservername(0, m_config.sntp_host.c_str());
        sntp_init();
    }

    bool network_manager::is_ipaddress(etl::string<50> ip_addr)
    {
        ip_addr_t ip4;
        return ipaddr_aton(ip_addr.c_str(), &ip4);
    }

    bool network_manager::is_hostname(etl::string<50> hostname)
    {
        // Source: https://www.regextester.com/23
        static const std::regex pattern(
            R"(^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])$)",
            std::regex_constants::ECMAScript);
        return std::regex_match(hostname.c_str(), pattern);
    }

} /* namespace sdk */