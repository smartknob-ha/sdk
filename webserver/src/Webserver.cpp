#include "Webserver.hpp"

#include <esp_sntp.h>
#include <esp_system_error.hpp>
#include <freertos/FreeRTOS.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

#include <regex>

namespace sdk {
    using Status = Component::Status;
    using res    = Component::res;

    Status Webserver::run() {
        return Status::RUNNING;
    }

    Status Webserver::stop() {
        if (m_server == NULL) {
            ESP_LOGD(TAG, "Webserver already stopped");
            return Status::STOPPED;
        }
        ESP_LOGD(TAG, "Stopping webserver");
        if (httpd_stop(m_server) == ESP_OK) {
            // Explicitly clear the server handle
            m_server = NULL;
            return Status::STOPPED;
        }
        // Should be impossible, as `httpd_stop` should always return ESP_OK if server is not NULL
        return Status::ERROR;
    }

    Status Webserver::initialize() {
        if (m_server != NULL) {
            ESP_LOGD(TAG, "Webserver already initialized");
            return Status::RUNNING;
        }

        ESP_LOGD(TAG, "Initializing webserver");

        auto ret = httpd_start(&m_server, &m_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error starting webserver: %s", esp_err_to_name(ret));
            return Status::ERROR;
        }

        return Status::RUNNING;
    }

    std::error_code Webserver::registerUri(const etl::string<128>& path, const httpd_method_t& method, UriHandler handler, void* userContext) {
        if (m_server == NULL) {
            ESP_LOGE(TAG, "Webserver not initialized");
            return std::make_error_code(ESP_ERR_INVALID_STATE);
        }

        ESP_LOGD(TAG, "Registering URI handler for %s", path.c_str());

        if (m_handlerCount >= m_config.max_uri_handlers) {
            ESP_LOGE(TAG, "Max URI handlers reached");
            return std::make_error_code(ESP_ERR_HTTPD_HANDLERS_FULL);
        }

        httpd_uri uri = {
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
        if (auto ret = httpd_register_uri_handler(m_server, &uri) != ESP_OK) {
            ESP_LOGE(TAG, "Error registering URI handler: %s", esp_err_to_name(ret));
            return std::make_error_code(esp_err_t(ret));
        }
        return {};
    }

    std::error_code Webserver::registerAsyncUri(const etl::string<128>& path, const httpd_method_t& method, UriHandler handler, void* userContext) {
        if (m_server == NULL) {
            ESP_LOGE(TAG, "Webserver not initialized");
            return std::make_error_code(ESP_ERR_INVALID_STATE);
        }

        ESP_LOGD(TAG, "Registering Async URI handler for %s", path.c_str());

        if (m_handlerCount >= m_config.max_uri_handlers) {
            ESP_LOGE(TAG, "Max URI handlers reached");
            return std::make_error_code(ESP_ERR_HTTPD_HANDLERS_FULL);
        }

        // Store actual handler and user context in wrapper struct to pass to the async handler
        AsyncData data = {
                .handler     = handler,
                .userContext = userContext};

        httpd_uri uri = {
                .uri      = path.c_str(),
                .method   = method,
                .handler  = asyncHandler,
                .user_ctx = &data,
#ifdef CONFIG_HTTPD_WS_SUPPORT
                .is_websocket             = false,
                .handle_ws_control_frames = false,
                .supported_subprotocol    = nullptr,
#endif
        };
        if (auto ret = httpd_register_uri_handler(m_server, &uri) != ESP_OK) {
            ESP_LOGE(TAG, "Error registering Async URI handler: %s", esp_err_to_name(ret));
            return std::make_error_code(esp_err_t(ret));
        }
        return {};
    }


} // namespace sdk