#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include <etl/message.h>
#include <etl/string.h>
#include <etl/unordered_map.h>

#include "../../manager/include/component.hpp"

#define MAX_ROUTERS_PER_BUS 4

typedef enum {
    BUS_STATUS,
    BUS_WIFI_DRIVER,
    BUS_WIFI_STATION,
    BUS_WIFI_AP,
    // Expand for bus types
    BUS_MAX // Leave this at the bottom of the list
} MSGBUS;

typedef etl::message_bus<MAX_ROUTERS_PER_BUS> mbus;

/**
 * @brief Simple class to hold the message busses
*/
class messages {
    public:
        /**
         * @brief Returns a reference to a message bus
         * 
         * @param msgbus    Bus selector, has to be of MSGBUS enum
        */
        mbus& get_bus(MSGBUS msgbus);
    private:
        static const inline char TAG[] = "Messages";
        etl::array<mbus, static_cast<unsigned int>(MSGBUS::BUS_MAX)> m_busses;
};

#endif /* MESSAGES_HPP */