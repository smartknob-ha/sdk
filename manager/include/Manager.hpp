#ifndef MANAGER_HPP
#define MANAGER_HPP

#include <etl/message.h>
#include <etl/message_bus.h>
#include <etl/message_packet.h>
#include <etl/string.h>
#include <etl/utility.h>
#include <etl/vector.h>
#include <result.h>

#include <thread>

#include "Component.hpp"

namespace sdk {

    class Manager {
    public:
        static Manager& instance();

        /**
         * @brief   Use this to add a component to the manager
         * @note    The order in which components are added
         *          determines in what order they will be ran
         */
        void addComponent(Component& ref);

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
        Manager(void){};
        Manager(Manager& other)                     = delete;
        Manager&    operator=(const Manager&)       = delete;
        inline auto operator=(Manager&) -> Manager& = delete;

        /**
         * @brief   Infinitely running function, intended
         *          to keep run all components added
         *          through the addComponent function
         */
        void run();

        /**
         * @brief   Used to couple non-static member function run()
         *          to function pointer for use in FreeRTOS task
         */
        static void startRun(void*);

        using componentEntry = etl::pair<bool, std::reference_wrapper<Component>>;

        /**
         * @brief   restartComponent will attempt to restart
         *          a component after its' run() function
         *          has reported a direct error
         */
        void restartComponent(componentEntry& entry);

        bool                                               m_running = false;
        etl::vector<componentEntry, CONFIG_NUM_COMPONENTS> m_components;
    };

} /* namespace sdk */

#endif /* MANAGER_HPP */