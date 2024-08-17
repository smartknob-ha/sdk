#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <Component.hpp>
#include <esp_err.h>
#include <esp_http_server.h>
#include <esp_log.h>

#include <string>

namespace sdk {

    class Webserver : public Component {
    public:
        typedef esp_err_t (*UriHandler)(httpd_req_t*);

        Webserver(const httpd_config_t& config = HTTPD_DEFAULT_CONFIG()) : m_config(config) {}

        /* Component override functions */
        etl::string<50> getTag() override { return TAG; };
        Status          initialize() override;
        Status          run() override;
        Status          stop() override;

        std::error_code registerUri(const etl::string<128>& path, const httpd_method_t& method, UriHandler handler, void* userContext = nullptr);
        std::error_code registerAsyncUri(const etl::string<128>& path, const httpd_method_t& method, UriHandler handler, void* userContext = nullptr);
        // TODO Add register websocket functions

    private:
        struct AsyncData {
            UriHandler handler;
            void*      userContext = nullptr;
        };

        static const inline char TAG[] = "Webserver";

        httpd_handle_t m_server = NULL;
        httpd_config_t m_config;
        uint16_t       m_handlerCount = 0;

        static esp_err_t asyncHandler(httpd_req_t* req);
    };

} // namespace sdk

#endif /* WEBSERVER_HPP */