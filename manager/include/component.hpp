#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <result.h>
#include <etl/string.h>
#include "esp_err.h"

/**
 * @brief   This is supposed to be a list of all the components
 *          used in the SmartKnob-ha SDK.
 * @details When adding a component, add it between the last
 *          component and the MAX element.
 **/
typedef enum {
    WIFI_STATION,
    WIFI_AP,

    MAX, // reserved spot at the end of the list
} COMPONENTS;

typedef enum {
    UNINITIALIZED,
    INITIALIZING,
    RUNNING,
    STOPPING,
    STOPPED,
    DEINITIALIZED
} COMPONENT_STATUS;

typedef Result<COMPONENT_STATUS, etl::string<128>> res;
#define ERR_MSG(esp_err, err_msg)                                                                                       \
    do {                                                                                                            \
        Err(etl::string<128>(etl::string<128>(err_msg).substr(0, 104).append(esp_err_to_name(esp_err))));           \
    } while (0)

#define ERR(esp_err)                                                                                                \
    do {                                                                                                            \
        Err(etl::string<128>(esp_err_to_name(esp_err)));                                                            \
    } while(0)

#define RETURN_ERR_MSG(esp_err, err_msg)                                                                                \
    do {                                                                                                            \
        return Err(etl::string<128>(etl::string<128>(err_msg).substr(0, 104).append(esp_err_to_name(esp_err))));    \
    } while (0)

#define RETURN_ERR(esp_err)                                                                                         \
    do {                                                                                                            \
        return Err(etl::string<128>(esp_err_to_name(esp_err)));                                                     \
    } while (0)

#define MAX_ENQUEUE_WAIT_TICKS 0


template<UBaseType_t LEN, typename QUEUETYPE, char* ... NAME>
class has_queue {
    public:
        has_queue() : m_queue(xQueueCreateStatic(LEN, sizeof(QUEUETYPE), m_queue_storage, &m_queue_data)) {
            vQueueAddToRegistry(m_queue, NAME);
        };

        void enqueue(QUEUETYPE& item) { xQueueSend(m_queue, (void *) item, 0); }

        ~has_queue() {
            vQueueUnregisterQueue(m_queue);
        }
    protected:
        QueueHandle_t m_queue;

    private:
        uint8_t m_queue_storage [ LEN * sizeof(QUEUETYPE) ];
        StaticQueue_t m_queue_data;
};

/**
 * @brief   Base class for all components created in the
 *          SmartKnob-HA SDK
*/
class component {
    public:
        /***
         * @brief Returns name of the component, for use in logging
        */
        virtual etl::string<50> get_tag() = 0;

        /**
         * @brief Returns component status, may include error
        */
        virtual res get_status() = 0;

        /**
         * @brief Gets called by the manager before startup
        */
        virtual res initialize() {  return Ok(UNINITIALIZED);  };

        /**
         * @brief Gets called by the manager in its' main loop
         * @note  Any error message returned will be treated
         *          as an error. The manager will try to restart
         *          the component by calling stop(), initialize()
         *          and run().
        */
        virtual res run() { return Ok(UNINITIALIZED);  };

        /**
         * @brief May be called after the component has returned
         *        an error status, or when the main thread is stopped
        */
        virtual res stop() = 0;
};

#endif /* COMPONENTS_HPP */