#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

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