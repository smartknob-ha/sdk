#ifndef NETWORK_MANAGER_HPP
#define NETWORK_MANAGER_HPP

#include <string>

#include "AccessPoint.hpp"
#include "Component.hpp"
#include "Station.hpp"
#include "esp_err.h"
#include "esp_log.h"

namespace sdk {

    class NetworkManager : public Component {
    public:
        typedef struct Config {
            bool                      dhcpEnable       = true;
            etl::string<15>           ipv4Address      = CONFIG_DEFAULT_IP4_ADDRESS;
            etl::string<15>           ipv4Netmask      = CONFIG_DEFAULT_IP4_NETMASK;
            etl::string<15>           ipv4Gateway      = CONFIG_DEFAULT_IP4_GATEWAY;
            etl::string<15>           ipv4DnsMain      = CONFIG_DEFAULT_IP4_DNS;
            etl::string<15>           ipv4DnsSecondary = "";
            bool                      useDhcpDns       = true;
            etl::string<50>           sntpHost         = CONFIG_DEFAULT_SNTP;
            wifi::Station::Config     sta;
            wifi::AccessPoint::Config ap;
        } Config;

        NetworkManager(Config config) : m_config(config), m_accessPoint(config.ap), m_station(config.sta) {};

        /* Component override functions */
        virtual etl::string<50>                 getTag() override { return TAG; };
        virtual Status                          initialize() override;
        virtual Status                          run() override;
        virtual Status                          stop() override;

        /* state = true for DHCP, false for static IP */
        res  setIpMode(bool state);
        res  setStationState(bool state);
        void setStationConfig(wifi::Station::Config conf);
        res  setAccessPointState(bool state);
        void setAccessPointConfig(wifi::AccessPoint::Config conf);

        void initSNTP();

        static bool validateIP(etl::string<50> ip_addr);
        static bool validateHostname(etl::string<50> ip_addr);

    private:
        static const inline char TAG[] = "Network Manager";

        Config m_config;

        wifi::AccessPoint m_accessPoint;
        wifi::Station     m_station;
    };

} // namespace sdk

#endif /* NETWORK_MANAGER_HPP */