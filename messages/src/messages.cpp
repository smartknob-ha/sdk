#include "../include/messages.h"

#include "esp_log.h"

#include <etl/message_router.h> 
#include <etl/message_bus.h>

typedef etl::message_bus<MAX_ROUTERS_PER_BUS> mbus;

mbus& messages::get_bus(MSGBUS msgbus) {
    return m_busses[msgbus];
}
