#include "../include/Manager.hpp"

#include "esp_log.h"
#include "esp_system_error.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace sdk {

    static etl::vector<componentEntry, CONFIG_NUM_COMPONENTS> m_components {};
    static bool m_running {false};

    void Manager::addComponent(Component& ref) {
        assert(!m_components.full());
        m_components.emplace_back(etl::pair{false, std::reference_wrapper<Component>(ref)});
    }

    void Manager::start() {
        if (!m_running) {
            m_running = true;
            xTaskCreate(Manager::run, "manager", 4096, nullptr, 1, nullptr);
        }
    }

    void Manager::stop() {
        m_running = false;
    }

    bool Manager::getInitialized() {
        return std::ranges::all_of(m_components.begin(), m_components.end(),
                [&](const componentEntry& entry) {
                    return entry.first;
                });
    }

    std::expected<bool, esp_err_t > Manager::getInitialized(const char* tag) {
        auto it = std::find_if(m_components.begin(), m_components.end(), [&](componentEntry& entry) {
            return entry.second.get().getTag() == tag;
        });

        if (it != m_components.end()) {
            return it->first;
        } else {
            return std::unexpected(ESP_ERR_NOT_FOUND);
        }
    }

    void Manager::run(void*) {
        ESP_LOGD(TAG, "Starting manager::run()");

        while (m_running) {
            for (auto& entry: m_components) {
                // entry.first is a bool that describes whether the component is active
                if (entry.first) {
                    Component& component = entry.second;

                    auto res = component.run();
                    if (!res.has_value()) {
                        ESP_LOGW(TAG, "Component %s reported an error: %s",
                                 component.getTag().c_str(), res.error().message().c_str());
                        restartComponent(entry);
                    } else if (res.value() == ComponentStatus::DEINITIALIZED) {
                        entry.first = false;
                    }
                } else {
                    auto status = entry.second.get().getStatus();
                    // Only initialise components if they
                    if (status.has_value() && status.value() < ComponentStatus::DEINITIALIZED) {
                        initComponent(entry);
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

    void Manager::initComponent(componentEntry& entry) {
        auto& component   = entry.second.get();
        auto  res = component.initialize();
        if (res.has_value()) {
            ESP_LOGD(TAG, "Initialized component: %s", component.getTag().c_str());
            entry.first = true;
        } else {
            ESP_LOGE(TAG, "Component %s failed to start: %s",
                     component.getTag().c_str(), res.error().message().c_str());
            entry.first = false;
        }
    }

    void Manager::restartComponent(componentEntry& entry) {
        Component& component = entry.second.get();

        auto stopRes = component.stop();
        if (!stopRes.has_value()) {
            ESP_LOGE(TAG, "Failed to stop component %s: %s",
                     component.getTag().c_str(), stopRes.error().message().c_str());
            entry.first = false;
            return;
        }

        auto initRes = component.initialize();
        if (!initRes.has_value()) {
            ESP_LOGE(TAG, "Failed to re-initialize component %s: %s",
                     component.getTag().c_str(), initRes.error().message().c_str());
            entry.first = false;
            component.stop();
            return;
        }

        entry.first = true;
    }


} // namespace sdk
