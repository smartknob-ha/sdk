#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

#include <etl/string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <result.h>

#include "esp_err.h"

namespace sdk {

    /**
     * @brief   This is supposed to be a list of all the components
     *          used in the SmartKnob-ha SDK.
     * @details When adding a component, add it between the last
     *          component and the MAX element.
     **/
    enum class Components {
        WIFI_STATION,
        WIFI_AP,

        MAX, // reserved spot at the end of the list
    };

    enum class ComponentStatus {
        UNINITIALIZED,
        INITIALIZING,
        RUNNING,
        STOPPING,
        STOPPED,
        DEINITIALIZED
    };

    using res = Result<ComponentStatus, etl::string<128>>;
#define ERR_MSG(espError, errorMsg)                                                                         \
    do {                                                                                                    \
        Err(etl::string<128>(etl::string<128>(errorMsg).substr(0, 104).append(esp_err_to_name(espError)))); \
    } while (0)

#define ERR(espError)                                     \
    do {                                                  \
        Err(etl::string<128>(esp_err_to_name(espError))); \
    } while (0)

#define RETURN_ON_ERR_MSG(espError, errorMsg)                                                                      \
    do {                                                                                                           \
        return Err(etl::string<128>(etl::string<128>(errorMsg).substr(0, 104).append(esp_err_to_name(espError)))); \
    } while (0)

#define RETURN_ON_ERR(espError)                                  \
    do {                                                         \
        return Err(etl::string<128>(esp_err_to_name(espError))); \
    } while (0)

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
            xQueueSend(m_queue, (void*) &item, ENQUEUE_TIMEOUT);
        }

        ~HasQueue() {
            vQueueUnregisterQueue(m_queue);
        }

    protected:
        /**
         * @brief Pops a message from the queue
         * @param item Reference to variable which will be filled with data
         * @param xTicksToWait Maximum ticks to wait, if 0 this function is non-blocking
         * @return pdTRUE if a message was read, pdFALSE if not
         */
        BaseType_t dequeue(QUEUETYPE& item, TickType_t xTicksToWait) {
            return xQueueReceive(m_queue, (void*) &item, xTicksToWait);
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
        /***
         * @brief Returns name of the component, for use in logging
         */
        virtual etl::string<50> getTag() = 0;

        /**
         * @brief Returns component status, may include error
         */
        virtual res getStatus() = 0;

        /**
         * @brief Gets called by the manager before startup
         */
        virtual res initialize() { return Ok(ComponentStatus::UNINITIALIZED); };

        /**
         * @brief Gets called by the manager in its' main loop
         * @note  Any error message returned will be treated
         *          as an error. The manager will try to restart
         *          the component by calling stop(), initialize()
         *          and run().
         */
        virtual res run() { return Ok(ComponentStatus::UNINITIALIZED); };


        /**
         * @brief May be called after the component has returned
         *        an error status, or when the main thread is stopped
         */
        virtual res stop() = 0;
    };

}; /* namespace sdk */

#endif /* COMPONENTS_HPP */