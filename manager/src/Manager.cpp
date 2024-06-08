#include "../include/Manager.hpp"

#include "esp_log.h"
#include "esp_system_error.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace sdk {

    static etl::vector<componentEntry, CONFIG_NUM_COMPONENTS> m_components{};
    static bool                                               m_running{false};

    void Manager::addComponent(Component& ref) {
        assert(!m_components.full());
        m_components.emplace_back(etl::pair{false, std::reference_wrapper<Component>(ref)});
    }

    void Manager::start() {
        if (!m_running) {
            m_running = true;
            auto res = xTaskCreate(Manager::run, "manager", CONFIG_RUN_TASK_STACK_SIZE, nullptr, 1, &m_taskHandle);
            if (res != pdPASS) {
                ESP_LOGE(TAG, "Failed to create manager thread, error code: %d", res);
            }
        }
    }

    void Manager::stop() {
        m_running = false;
        TaskStatus_t task_status;

        // Wait for the manager thread to be deleted (deletion happens from inside the tread)
        do {
            vTaskDelay(1);
            vTaskGetInfo(m_taskHandle, &task_status, pdFALSE, eInvalid);
        } while (task_status.eCurrentState == eDeleted);

        stopAll();
    }

    void Manager::stopAll() {
        for (auto& entry: m_components) {
            // entry.first is a bool that describes whether the component is active
            bool&      componentActive = entry.first;
            Component& component       = entry.second;

            // If component is not active, don't attempt to stop it
            if (!componentActive) {
                break;
            }

            if (component.stop() != Component::Status::STOPPED) {
                ESP_LOGE(TAG, "Failed to stop component %s on shutdown of manager",
                         component.getTag().c_str());
            }
        }
    }

    bool Manager::getInitialized() {
        return std::ranges::all_of(m_components.begin(), m_components.end(),
                                   [&](const componentEntry& entry) {
                                       return entry.first;
                                   });
    }

    std::expected<bool, esp_err_t> Manager::getInitialized(const char* tag) {
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
        ESP_LOGD(TAG, "Starting Manager::run()");

        while (m_running) {
            for (auto& entry: m_components) {
                // entry.first is a bool that describes whether the component is active
                bool&      componentActive = entry.first;
                Component& component       = entry.second;
                if (componentActive) {
                    auto res = component.run();
                    if (!res.has_value()) {
                        ESP_LOGW(TAG, "Component %s reported an error: %s, attempting to restart",
                                 component.getTag().c_str(), res.error().message().c_str());
                        restartComponent(entry);
                    } else if (res.value() == Component::Status::ERROR) {
                        componentActive = false;
                    }
                } else {
                    auto status = component.getStatus();
                    if (status.has_value() && status.value() == Component::Status::UNINITIALIZED) {
                        initComponent(entry);
                    }
                }
            }

            // Prevent watchdog trigger
            vTaskDelay(1);
        }

        ESP_LOGD(TAG, "Finished Manager::run()");
        vTaskDelete(m_taskHandle);
    }

    bool Manager::initComponent(componentEntry& entry) {
        auto& component = entry.second.get();
        auto  res       = component.initialize();
        if (res.has_value()) {
            ESP_LOGD(TAG, "Initialized component: %s", component.getTag().c_str());
            entry.first = true;
        } else {
            ESP_LOGE(TAG, "Component %s failed to start: %s",
                     component.getTag().c_str(), res.error().message().c_str());
            entry.first = false;
        }

        bool done = false;
        while (!done) {
            auto status = component.getStatus();
            if (status.has_value() && status.value() > Component::Status::INITIALIZING) {
                done = true;
            }
            vTaskDelay(1);
        }
    }

    void Manager::restartComponent(componentEntry& entry) {
        Component& component = entry.second.get();

        auto stopStatus = component.stop();
        if (stopStatus == Component::Status::ERROR) {
            ESP_LOGE(TAG, "Failed to stop component %s: %s",
                     component.getTag().c_str(), component.getError()->c_str());
            entry.first = false;
            return;
        }

        auto initStatus = component.initialize();
        if (initStatus == Component::Status::ERROR) {
            ESP_LOGE(TAG, "Failed to re-initialize component %s: %s",
                     component.getTag().c_str(), component.getError()->c_str());
            entry.first = false;
            return;
        }

        entry.first = true;
    }


} // namespace sdk
