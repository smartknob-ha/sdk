#ifndef NETWORK_MANAGER_HPP
#define NETWORK_MANAGER_HPP

#include <AccessPoint.hpp>
#include <Component.hpp>
#include <Station.hpp>
#include <esp_err.h>
#include <esp_log.h>

#include <string>

namespace sdk {

    class NetworkManager : public Component {
    public:
        class Config final : public ConfigObject<8, 512, "Network Manager"> {
            using Base = ConfigObject;

        public:
            sdk::ConfigField<etl::string<15>> ipv4Address{CONFIG_DEFAULT_IP4_ADDRESS, "address_v4", sdk::RestartType::NONE};
            sdk::ConfigField<etl::string<15>> ipv4Netmask{CONFIG_DEFAULT_IP4_NETMASK, "netmask_v4", sdk::RestartType::NONE};
            sdk::ConfigField<etl::string<15>> ipv4Gateway{CONFIG_DEFAULT_IP4_GATEWAY, "gateway_v4", sdk::RestartType::NONE};
            sdk::ConfigField<etl::string<15>> ipv4DnsMain{CONFIG_DEFAULT_IP4_DNS, "dns_main_v4", sdk::RestartType::NONE};
            sdk::ConfigField<etl::string<15>> ipv4DnsSecondary{CONFIG_DEFAULT_IP4_DNS_SECONDARY, "dns_secondary_v4", sdk::RestartType::NONE};
            sdk::ConfigField<bool>            useDhcpDns{true, "use_dhcp_dns", sdk::RestartType::NONE};
            sdk::ConfigField<bool>            dhcpEnable{true, "dhcp_enable", sdk::RestartType::NONE};
            sdk::ConfigField<etl::string<50>> sntpHost{CONFIG_DEFAULT_SNTP, "sntp_host", sdk::RestartType::COMPONENT};

            void allocateFields() {
                ipv4Address      = allocate(ipv4Address);
                ipv4Netmask      = allocate(ipv4Netmask);
                ipv4Gateway      = allocate(ipv4Gateway);
                ipv4DnsMain      = allocate(ipv4DnsMain);
                ipv4DnsSecondary = allocate(ipv4DnsSecondary);
                useDhcpDns       = allocate(useDhcpDns);
                dhcpEnable       = allocate(dhcpEnable);
                sntpHost         = allocate(sntpHost);
            }

            Config(const nlohmann::json& data) : Base(data) {
                allocateFields();
            }

            Config() : Base() {
                allocateFields();
            }
        };

        NetworkManager() = default;

        /* Component override functions */
        etl::string<50> getTag() override { return TAG; };
        Status          initialize() override;
        Status          run() override;
        Status          stop() override;

        /* state = true for DHCP, false for static IP */
        res  setIpMode(bool state);
        res  setStationState(bool state);
        void setStationConfig(const nlohmann::json& config);
        res  setAccessPointState(bool state);
        void setAccessPointConfig(const nlohmann::json& config);

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