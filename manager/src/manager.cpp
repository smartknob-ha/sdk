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
    m_components.emplace_back(etl::pair{false, std::reference_wrapper<component>(ref)});
}

void manager::start() {
    if (!m_running) {
        std::thread(&manager::run, this).detach();
        m_running = true;
    }
}

void manager::run() {
    for (auto& entry : m_components) {
        auto& c = entry.second.get();
        auto res = c.initialize();
        if (res.isErr()) {
            ESP_LOGE(TAG, "Component %s failed to start: %s",
                c.get_tag().c_str(), res.unwrapErr().c_str());
            entry.first = false;
        } else {
            entry.first = true;
        }
    }

    while (true) {
        for (auto& entry : m_components) {
            if (entry.first) {
                component& c = entry.second; 
                auto res = c.run();
                if (res.isErr()) {
                    ESP_LOGW(TAG, "Component %s reported an error: %s", 
                        c.get_tag().c_str(), res.unwrapErr().c_str());
                    restartComponent(entry);
                }
            }
        }
    }
}

void manager::restartComponent(etl::pair<bool, std::reference_wrapper<component>>& entry) {
    component& c = entry.second.get();

    auto stop_res = c.stop();
    if (stop_res.isErr()) {
        ESP_LOGE(TAG, "Failed to stop component %s: %s", 
            c.get_tag().c_str(), stop_res.unwrapErr().c_str());
        entry.first = false;
        return;
    }

    auto init_res = c.initialize();
    if (init_res.isErr()) {
        ESP_LOGE(TAG, "Failed to re-initialize component %s: %s",
            c.get_tag().c_str(), init_res.unwrapErr().c_str());
        entry.first = false;
        return;
    }

    entry.first = true;
}

} /* namespace sdk */
