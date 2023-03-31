#include "../include/led_strip.hpp"

namespace sdk {

using res = Result<esp_err_t, etl::string<128>>;

res led_strip::get_status() {
    if (m_status != ESP_OK) {
        return Err(m_status);
    }

    return Ok(m_status);
}

template<class led_type>
res led_strip::initialize() {
    FastLED.addLeds<led_type, DATA_PIN, RGB>(m_leds, NUM_LEDS);
    FastLED.clearLeds(NUM_LEDS);
}

res led_strip::run() {
    FastLED.showLeds();
}

} /* namespace sdk */
