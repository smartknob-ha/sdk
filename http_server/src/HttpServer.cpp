#include "HttpServer.hpp"

#include <esp_system_error.hpp>
#include <etl/string.h>
#include <etl/string_stream.h>
#include <etl/unordered_map.h>
#include <etl/vector.h>
#include <freertos/FreeRTOS.h>
#include <lwip/sockets.h>

#include <regex>

namespace sdk::Http {
    using Status = Component::Status;
    using res    = Component::res;

    Status Server::run() {
        return Status::RUNNING;
    }

    Status Server::stop() {
        if (m_server == nullptr) {
            ESP_LOGD(TAG, "Webserver already stopped");
            return Status::STOPPED;
        }
        ESP_LOGD(TAG, "Stopping webserver");
        if (httpd_stop(m_server) == ESP_OK) {
            // Explicitly clear the server handle
            m_server = nullptr;
            return Status::STOPPED;
        }
        // Should be impossible, as `httpd_stop` should always return ESP_OK if server is not NULL
        return Status::ERROR;
    }

    Status Server::initialize() {
        if (m_server != nullptr) {
            ESP_LOGD(TAG, "Webserver already initialized");
            return Status::RUNNING;
        }

        ESP_LOGD(TAG, "Initializing webserver");

        if (const auto ret = httpd_start(&m_server, &m_config); ret != ESP_OK) {
            ESP_LOGE(TAG, "Error starting webserver: %s", esp_err_to_name(ret));
            return Status::ERROR;
        }

        return Status::RUNNING;
    }

    std::error_code Server::registerRawUri(const Uri& path, const httpd_method_t& method, const RawHandler handler, void* userContext) const {
        ESP_LOGD(TAG, "Registering URI handler for %s", path.c_str());

        if (m_server == nullptr) {
            ESP_LOGE(TAG, "Webserver not initialized");
            return std::make_error_code(ESP_ERR_INVALID_STATE);
        }

        const httpd_uri uri = {
                .uri      = path.c_str(),
                .method   = method,
                .handler  = handler,
                .user_ctx = userContext,
#ifdef CONFIG_HTTPD_WS_SUPPORT
                .is_websocket             = false,
                .handle_ws_control_frames = false,
                .supported_subprotocol    = nullptr,
#endif
        };
        if (const auto ret = httpd_register_uri_handler(m_server, &uri); ret != ESP_OK) {
            assert(ret == ESP_ERR_HTTPD_HANDLER_EXISTS && "Don't register the same URI twice");
            ESP_LOGE(TAG, "Error registering URI handler: %s", esp_err_to_name(ret));
            return std::make_error_code(ret);
        }
        return {};
    }

} // namespace sdk::Http
