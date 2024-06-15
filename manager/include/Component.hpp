#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

#include <etl/string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <result.h>

#include <expected>

#include "esp_err.h"

#include "ConfigProvider.hpp"

namespace sdk {

    template<UBaseType_t LEN, typename QUEUETYPE, TickType_t ENQUEUE_TIMEOUT>
    class HasQueue {
    public:
        HasQueue() : m_queue(xQueueCreateStatic(LEN, sizeof(QUEUETYPE), m_queueStorage, &m_queueData)){};

        /**
         * @brief Enqueues new message
         * @param item Reference to message
         * @attention When you inherit multiple has_queue classes with different types, overload this function for each type
         * 	like this: `void enqueue(your_type& message) override { has_queue<1, your_type, 0>::enqueue(message); }`
         */
        virtual void enqueue(QUEUETYPE& item) {
            xQueueSend(m_queue, static_cast<void*>(&item), ENQUEUE_TIMEOUT);
        }

        ~HasQueue() {
            vQueueUnregisterQueue(m_queue)
        }

    protected:
        /**
         * @brief Pops a message from the queue
         * @param item Reference to variable which will be filled with data
         * @param xTicksToWait Maximum ticks to wait, if 0 this function is non-blocking
         * @return pdTRUE if a message was read, pdFALSE if not
         */
        BaseType_t dequeue(QUEUETYPE& item, TickType_t xTicksToWait) {
            return xQueueReceive(m_queue, static_cast<void*>(&item), xTicksToWait);
        }

    private:
        uint8_t       m_queueStorage[LEN * sizeof(QUEUETYPE)]{};
        StaticQueue_t m_queueData{};
        QueueHandle_t m_queue{};
    };

    /**
     * @brief   Base class for all components created in the
     *          SmartKnob-HA SDK
     */
    class Component {
    public:
        /**
        * @brief Component status order
        */
        enum class Status {
            /**
             * @brief Like new, `initialize()` will be called by manager
             */
            UNINITIALIZED,
            /**
             * @brief Working on initialization
             */
            INITIALIZING,
            /**
             * @brief Nominal running state
             */
            RUNNING,
            /**
             * @brief Something went wrong during any lower state, manager will try to restart the component
             */
            ERROR,
            /**
             * @brief State during stopping
             */
            STOPPING,
            /**
             * @brief State when manually stopped, will not be reinitialized by manager
             */
            STOPPED
        };

        using res = std::expected<Status, std::error_code>;

        /***
         * @brief Returns name of the component, for use in logging
         */
        virtual etl::string<50> getTag() = 0;

        /**
         * @brief Sets the log level for the component
         * @param level The log level to set
         */
        void setLogLevel(esp_log_level_t level) {
            esp_log_level_set(getTag().c_str(), level);
        }

        /**
         * @brief Returns component status, may include error
         */
        virtual Status getStatus() { return m_status; };

        /**
         * @brief Gets component error
         * @return  Nothing when status isn't ERROR, error message when it is
         */
        virtual std::optional<etl::string<128>> getError() {
            if (m_status == Status::ERROR && m_err != ESP_OK) {
                return esp_err_to_name(m_err);
            }
            return {};
        }

        /**
         * @brief Check if a component or device restart is required
         */
        virtual RestartType getRestartType() {
            return RestartType::NONE;
        };

        /**
         * @brief Gets called by the manager before startup
         */
        virtual Status initialize() = 0;

        /**
         * @brief Gets called by the manager in its' main loop
         * @note  Any error message returned will be treated
         *          as an error. The manager will try to restart
         *          the component by calling stop(), initialize()
         *          and run().
         */
        virtual Status run() = 0;


        /**
         * @brief May be called after the component has returned
         *        an error status, or when the main thread is stopped
         */
        virtual Status stop() = 0;
    protected:
        Status                   m_status{Status::UNINITIALIZED};
        esp_err_t                m_err{ESP_OK};
    };

} /* namespace sdk */

#endif /* COMPONENTS_HPP */