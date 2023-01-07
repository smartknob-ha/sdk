#ifndef MANAGER_HPP
#define MANAGER_HPP

#include <etl/message.h>
#include <etl/message_packet.h>
#include <etl/message_bus.h>
#include <etl/string.h>
#include <thread>
#include <result.h>

#include "../../messages/include/messages.h"

// TODO: turn this into a Kconfig option?
#define NUM_COMPONENTS 4

namespace sdk {

enum MESSAGE_TYPES {
    STATUS,
    DATA
};

struct status_message : public etl::message<MESSAGE_TYPES::STATUS> {
    Result<COMPONENT_STATUS, etl::string<128>> result;
};

typedef etl::message_packet<status_message> message_packet;

class manager {
    public:
        static manager& instance();

        /**
         * @brief   Use this to add a component to the runtime
         * @note    If your component can handle it, its' safe
         *          to add it after the runtime has started
        */
        void add_component(component& ref);

        /**
         * @brief   Starts a thread on the run function
        */
        void start();
    private:
        static const inline char TAG[] = "Manager";

        /**
         * Make sure this class is atomic 
         * and non-copyable
        */
        manager(void) {};
        manager(manager& other) {};

        /**
         * @brief   Infinitely running function, intended
         *          to keep run all components added
         *          through the add_component function
        */
        void run();

        /**
         * @brief   This function will attempt to restart
         *          a component after its' run() function  
         *          has reported a direct error
        */
        void restartComponent(component& c);

        messages m_messages{};
        bool m_running = false;
        etl::vector<std::reference_wrapper<component>, NUM_COMPONENTS> m_components;
};

} /* namespace sdk */

#endif /* MANAGER_HPP */