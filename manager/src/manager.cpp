#include "../include/manager.hpp"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace sdk {

manager& manager::instance() {
    static manager* self;
    if (self == nullptr) {
        self = new manager();
    }
    return *self;
}

void manager::add_component(component& ref) {
    assert(!m_components.full());
    m_components.emplace_back(etl::pair{false, std::reference_wrapper<component>(ref)});
}

void manager::start() {
    if (!m_running) {
        m_running = true;
        xTaskCreate(start_run, "manager", 4096, this, 1, NULL);
    }
}

void manager::start_run(void* _this) {
    manager* m = (manager*) (_this);
    m->run();
}

void manager::stop() {
    m_running = false;
}

void manager::run() {
    ESP_LOGD(TAG, "Starting manager::run()");

    for (auto& entry : m_components) {
        auto& c = entry.second.get();
        auto res = c.initialize();
        if (res.isErr()) {
            ESP_LOGE(TAG, "Component %s failed to start: %s",
                c.get_tag().c_str(), res.unwrapErr().c_str());
            entry.first = false;
        } else {
            ESP_LOGD(TAG, "Initialized component: %s", c.get_tag().c_str());
            entry.first = true;
        }
    }

    while (m_running) {
        for (auto& entry : m_components) {
            if (entry.first) {
                component& c = entry.second; 
                auto res = c.run();
                if (res.isErr()) {
                    ESP_LOGW(TAG, "Component %s reported an error: %s", 
                        c.get_tag().c_str(), res.unwrapErr().c_str());
                    restart_component(entry);
                } else if (res.unwrap() == COMPONENT_STATUS::DEINITIALIZED) {
                        entry.first = false;
                }
            }
        }

        // Prevent watchdog trigger
        vTaskDelay(1);
    }

    for (auto& entry : m_components) {
        auto res = entry.second.get().stop();
        if (res.isErr()) {
            ESP_LOGE(TAG, "Failed to stop component %s on shutdown of manager", 
                entry.second.get().get_tag().c_str());
        }
    }

    vTaskDelete(NULL);
}

void manager::restart_component(c_entry& entry) {
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
        c.stop();
        return;
    }

    entry.first = true;
}

} /* namespace sdk */
