#ifndef WIFI_STA_HPP
#define WIFI_STA_HPP

#include <etl/string.h>
#include <etl/message_bus.h>
#include <esp_err.h>
#include <esp_wifi_types.h>

#include "../../messages/include/messages.h"
#include "../../manager/include/component.hpp"

namespace n_wifi_station {
    enum status {
        UNINITIALIZED,
        SCAN_START,
        SCAN_STOP,
        CONNECTING,
        CONNECTED,
        CONNECT_FAILED,
        DISCONNECTED
    };

    struct uninitialized : public etl::message<status::UNINITIALIZED> {};
    struct scan_start : public etl::message<status::SCAN_START> {};
    struct scan_stop : public etl::message<status::SCAN_STOP> {};
    struct connecting : public etl::message<status::CONNECTING> {};
    struct connected : public etl::message<status::CONNECTED> {};
    struct connect_failed : public etl::message<status::CONNECT_FAILED> {};
    struct disconnected : public etl::message<status::DISCONNECTED> {};
};

using namespace n_wifi_station;
class wifi_station : 
        public etl::message_router<wifi_station>, 
        public component {
    public:
        wifi_station(mbus& status_bus,
                     mbus& wifi_driver_bus, 
                     mbus& wifi_station_bus) :
            message_router(COMPONENTS::WIFI_STATION),
            m_status_bus(status_bus),
            m_wifi_driver_bus(wifi_driver_bus),
            m_wifi_station_bus(wifi_station_bus) {};

        using res = Result<esp_err_t, etl::string<128>>;

        etl::string<50> get_tag() override { return TAG; };
        res get_status() override;
        res initialize() override;
        res stop() override;

        // Messages we can receive
        void on_receive_unknown(const etl::imessage& msg) {};

    private:
        void event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data);

        static const inline char TAG[] = "Wifi Station Mode";

        status m_status = status::UNINITIALIZED;
        mbus &m_status_bus;
        mbus &m_wifi_driver_bus;
        mbus &m_wifi_station_bus;
};

#endif /* WIFI_STA_HPP */