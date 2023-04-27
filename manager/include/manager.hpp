#ifndef MANAGER_HPP
#define MANAGER_HPP

#include <etl/message.h>
#include <etl/message_packet.h>
#include <etl/message_bus.h>
#include <etl/string.h>
#include <etl/utility.h>
#include <etl/vector.h>
#include <thread>
#include <result.h>

#include "component.hpp"

namespace sdk {

class manager {
    public:
        static manager& instance();

        /**
         * @brief   Use this to add a component to the manager
         * @note    The order in which components are added
         *          determines in what order they will be ran
        */
        void add_component(component& ref);

        /**
         * @brief   Starts a thread on the run function
        */
        void start();

        /**
         * @brief   Stops the managers' thread
        */
        void stop();
    private:
        static const inline char TAG[] = "Manager";

        /**
         * Make sure this class is atomic
         * and non-copyable
        */
        manager(void) {};
        manager(manager& other) = delete;
        manager& operator=(const manager&) = delete;
        manager& operator=(manager&) = delete;

        /**
         * @brief   Infinitely running function, intended
         *          to keep run all components added
         *          through the add_component function
        */
        void run();

        /**
         * @brief   Used to couple non-static member function run()
         *          to function pointer for use in FreeRTOS task
        */
        static void start_run(void*);

        using c_entry = etl::pair<bool, std::reference_wrapper<component>>;

        /**
         * @brief   restart_component will attempt to restart
         *          a component after its' run() function
         *          has reported a direct error
        */
        void restart_component(c_entry& entry);

        bool m_running = false;
        etl::vector<c_entry, CONFIG_NUM_COMPONENTS> m_components;
};

} /* namespace sdk */

#endif /* MANAGER_HPP */