#ifndef NETWORK_MANAGER_HPP
#define NETWORK_MANAGER_HPP

#include <string>
#include "esp_log.h"
#include "esp_err.h"
#include "component.hpp"
#include "ap.hpp"
#include "sta.hpp"

namespace sdk
{

    typedef struct
    {
        bool dhcp_enable = true;
        etl::string<15> ipv4_address = CONFIG_DEFAULT_IP4_ADDRESS;
        etl::string<15> ipv4_netmask = CONFIG_DEFAULT_IP4_NETMASK;
        etl::string<15> ipv4_gateway = CONFIG_DEFAULT_IP4_GATEWAY;
        etl::string<15> ipv4_dns_main = CONFIG_DEFAULT_IP4_DNS;
        etl::string<15> ipv4_dns_secondary = "";
        bool use_dhcp_dns = true;
        wifi::sta_config_t sta;
        wifi::ap_config_t ap;
        etl::string<50> sntp_host = CONFIG_DEFAULT_SNTP;
    } network_config_t;

    class network_manager : public component
    {
    public:
        network_manager(network_config_t config) : m_config(config), ap(config.ap), sta(config.sta){};

        /* Component override functions */
        virtual etl::string<50> get_tag() override { return TAG; };
        virtual res get_status() override;
        virtual res initialize() override;
        virtual res run() override;
        virtual res stop() override;

        /* state = true for DHCP, false for static IP */
        res set_ip_mode(bool state);
        res set_sta(bool state);
        res set_sta(bool state, wifi::sta_config_t conf);
        res set_ap(bool state);
        res set_ap(bool state, wifi::ap_config_t conf);

        void init_sntp();

        static bool is_ipaddress(etl::string<50> ip_addr);
        static bool is_hostname(etl::string<50> ip_addr);

    private:
        static const inline char TAG[] = "Network Manager";

        network_config_t m_config;

        wifi::ap ap;
        wifi::sta sta;
    };

} /* namespace sdk */

#endif /* NETWORK_MANAGER_HPP */