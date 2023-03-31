#ifndef LED_STRIP_HPP
#define LED_STRIP_HPP

#include <FastLED.h>

#include "../../manager/include/component.hpp"

namespace sdk {

#define NUM_LEDS CONFIG_LED_STRIP_NUM
#define DATA_PIN CONFIG_LED_STRIP_GPIO
#define CLOCK_PIN -1

template<class led_type>
class led_strip_n : public component {
    public:
        led_strip_n();

        using res = Result<esp_err_t, etl::string<128>>;

        /* Component override functions */
        virtual etl::string<50> get_tag() override { return TAG; };
        virtual res get_status() override;
        virtual res initialize() override;
        virtual res run() override;
        virtual res stop() override;

    private:
        static const inline char TAG[] = "LED Strip";
        esp_err_t m_status;
};

} /* namespace sdk */


#endif /* LED_STRIP_HPP */