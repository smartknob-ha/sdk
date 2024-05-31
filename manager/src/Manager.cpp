#include "../include/Manager.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace sdk {

    Manager& Manager::instance() {
        static Manager* self;
        if (self == nullptr) {
            self = new Manager();
        }
        return *self;
    }

    void Manager::addComponent(Component& ref) {
        assert(!m_components.full());
        m_components.emplace_back(etl::pair{false, std::reference_wrapper<Component>(ref)});
    }

    void Manager::start() {
        if (!m_running) {
            m_running = true;
            xTaskCreate(startRun, "manager", 4096, this, 2, nullptr);
        }
    }

    void Manager::startRun(void* _this) {
        Manager* m = static_cast<Manager*>(_this);
        m->run();
    }

    void Manager::stop() {
        m_running = false;
    }

    void Manager::run() {
        ESP_LOGD(TAG, "Starting manager::run()");

        for (auto& entry: m_components) {
            auto& c   = entry.second.get();
            auto  res = c.initialize();
            if (res.has_value()) {
                ESP_LOGD(TAG, "Initialized component: %s", c.getTag().c_str());
                entry.first = true;
            } else {
                ESP_LOGE(TAG, "Component %s failed to start: %s",
                         c.getTag().c_str(), res.error().message().c_str());
                entry.first = false;
            }
        }

        while (m_running) {
            for (auto& entry: m_components) {
                if (entry.first) {
                    Component& c = entry.second;

                    auto res = c.run();
                    if (!res.has_value()) {
                        ESP_LOGW(TAG, "Component %s reported an error: %s",
                                 c.getTag().c_str(), res.error().message().c_str());
                        restartComponent(entry);
                    } else if (res.value() == ComponentStatus::DEINITIALIZED) {
                        entry.first = false;
                    }
                }
            }

            // Prevent watchdog trigger
            vTaskDelay(1);
        }

        for (auto& entry: m_components) {
            auto res = entry.second.get().stop();
            if (!res.has_value()) {
                ESP_LOGE(TAG, "Failed to stop component %s on shutdown of manager",
                         entry.second.get().getTag().c_str());
            }
        }

        vTaskDelete(nullptr);
    }

    void Manager::restartComponent(componentEntry& entry) {
        Component& c = entry.second.get();

        auto stopRes = c.stop();
        if (!stopRes.has_value()) {
            ESP_LOGE(TAG, "Failed to stop component %s: %s",
                     c.getTag().c_str(), stopRes.error().message().c_str());
            entry.first = false;
            return;
        }

        auto initRes = c.initialize();
        if (!initRes.has_value()) {
            ESP_LOGE(TAG, "Failed to re-initialize component %s: %s",
                     c.getTag().c_str(), initRes.error().message().c_str());
            entry.first = false;
            c.stop();
            return;
        }

        entry.first = true;
    }

} // namespace sdk
