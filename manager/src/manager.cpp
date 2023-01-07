#include "../include/manager.hpp"

#include "esp-log.h"

#include <thread>

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
    std::thread(&manager::run, this).detach();
}

void manager::run() {
    for (component& c : m_components) {
        c.initialize();
    }

    while (true) {
        for (component& c : m_components) {
            auto res = c.run();
            if (res.isErr()) {
                ESP_LOGW("Component %s reported an error: %s", 
                    c.get_tag().c_str(), res.unwrapErr().c_str());
                restartComponent(c);
            }
        }
    }
}

void manager::restartComponent(component& c) {
    auto res = c.stop();
    if (res.isErr()) {
        return ESP_LOGE("Failed to stop component %s: %s", 
            c.get_tag().c_str(), res.unwrapErr().c_str());
    }

    res = c.initialize();
    if (res.isErr()) {
        return ESP_LOGE("Failed to re-initialize component %s: %s",
            c.get_tag().c_str(), res.unwrapErr().c_str());
    }
}
