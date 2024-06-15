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

    using componentEntry = etl::pair<bool, std::reference_wrapper<Component>>;

    class Manager {
    public:
        /**
         * @brief   Use this to add a component to the manager
         * @note    The order in which components are added
         *          determines in what order they will be ran
         */
        static void addComponent(Component& ref);

        /**
         * @brief   Starts a thread on the run function
         * @note    Only starts once, until `stop()` has been called
         */
        static void start();

        /**
         * @brief   Stops the managers' thread
         */
        static void stop();

        /**
         * @brief Stops the manager and any added components
         */
        static void stopAll();

        /**
         * @brief   Checks initialization status of added components
         * @return  True when all present components are running
         */
        static bool getInitialized();

        static std::expected<bool, esp_err_t> getInitialized(const char* tag);

    private:
        static constexpr inline char TAG[] = "Manager";

        static inline TaskHandle_t m_taskHandle {NULL};

        /**
         * Make sure this class is atomic
         * and non-copyable
         */
        Manager(void) {};
        Manager(Manager& other)                     = delete;
        Manager&    operator=(const Manager&)       = delete;
        inline auto operator=(Manager&) -> Manager& = delete;

        /**
         * @brief   Infinitely running function, intended
         *          to keep run all components added
         *          through the addComponent function
         */
        static void run(void*);

        /**
         * @brief Initializes component
         * @param entry Reference to componentEntry
         * @returns Whether initialisation completed successfully
         */
        static bool initComponent(componentEntry& entry);

        /**
         * @brief   restartComponent will attempt to restart
         *          a component after its' run() function
         *          has reported a direct error
         */
        static void restartComponent(componentEntry& entry);
    };
} /* namespace sdk */

#endif /* MANAGER_HPP */