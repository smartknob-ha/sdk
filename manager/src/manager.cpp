#include "../include/manager.hpp"

#include "esp_log.h"
#include <thread>

namespace sdk {

manager& manager::instance() {
    static manager* self;
    if (self == nullptr) {
        self = new manager();
    }
    return *self;
}

void manager::add_component(component& ref) {
    m_components.emplace_back(ref);
}

void manager::start() {
    if (!m_running) {
        std::thread(&manager::run, this).detach();
        m_running = true;
    }
}

void manager::run() {
    for (component& c : m_components) {
        c.initialize();
    }

    while (true) {
        for (component& c : m_components) {
            auto res = c.run();
            if (res.isErr()) {
                ESP_LOGW(TAG, "Component %s reported an error: %s", 
                    c.get_tag().c_str(), res.unwrapErr().c_str());
                restartComponent(c);
            }
        }
    }
}

void manager::restartComponent(component& c) {
    auto stop_res = c.stop();
    if (stop_res.isErr()) {
        ESP_LOGE(TAG, "Failed to stop component %s: %s", 
            c.get_tag().c_str(), stop_res.unwrapErr().c_str());
        return;
    }

    auto init_res = c.initialize();
    if (init_res.isErr()) {
        ESP_LOGE(TAG, "Failed to re-initialize component %s: %s",
            c.get_tag().c_str(), init_res.unwrapErr().c_str());
        return;
    }
}

} /* namespace sdk */
