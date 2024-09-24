#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <Component.hpp>
#include <esp_err.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <etl/string.h>
#include <etl/unordered_map.h>
#include <etl/vector.h>

#include <expected>

#include "ConfigProvider.hpp"
#include "HttpStatusCode.hpp"

namespace sdk::Http {

    class Server final : public Component {
    public:
        static constexpr std::size_t MAX_QUERY_SIZE = 128;

        typedef esp_err_t (*RawHandler)(httpd_req_t*);

        using SendHeader = std::pair<etl::string<32>, etl::string<128>>;
        using Uri        = etl::string<128>;
        using QueryKey   = etl::string<20>;
        using QueryValue = etl::string<50>;

        /**
         * @brief Error type for the server. To be used in the callbacks to return an error.
         */
        struct Error {
            /**
             * @brief The status code of the error.
             */
            StatusCode code;
            /**
             * @brief Any additional information for the client.
             */
            etl::string<32> message;
            /**
             * @brief Get the status message for the status code.
             * @return The status message.
             */
            const char* statusMessage() const { return statusCodePhrase(code); }
        };

        /**
         * @brief Callback type for GET requests.
         * @tparam N_PARAMETERS The number of parameters that the callback expects.
         * @tparam SIZE_RETURN The size of the returned string.
         * @param uri The full URI of the request made.
         * @param parameters The parameters of the request, already decoded.
         * @param userContext Any user context that was passed when registering the callback. Will be nullptr if not provided.
         */
        template<std::size_t N_PARAMETERS, std::size_t SIZE_RETURN>
        using GetCallback = std::function<std::expected<etl::string<SIZE_RETURN>, Error>(const Uri& uri, const etl::unordered_map<QueryKey, QueryValue, N_PARAMETERS>& parameters, void* userContext)>;

        /**
         * @brief Callback type for POST requests.
         * @tparam N_PARAMETERS The number of parameters that the callback expects.
         * @tparam SIZE_RETURN The size of the returned string.
         * @tparam SIZE_BODY The size of the buffer for the request body.
         * @param uri The full URI of the request made.
         * @param body The body of the request.
         * @param parameters The parameters of the request, already decoded.
         * @param userContext Any user context that was passed when registering the callback. Will be nullptr if not provided.
         */
        template<std::size_t N_PARAMETERS, std::size_t SIZE_RETURN, std::size_t SIZE_BODY>
        using PostCallback = std::function<std::expected<etl::string<SIZE_RETURN>, Error>(const Uri& uri, etl::string<SIZE_BODY>& body, const etl::unordered_map<QueryKey, QueryValue, N_PARAMETERS>& parameters, void* userContext)>;

        explicit Server(const httpd_config_t& config = HTTPD_DEFAULT_CONFIG()) : m_config(config) {}

        /* Component override functions */
        etl::string<50> getTag() override { return TAG; };
        Status          initialize() override;
        Status          run() override;
        Status          stop() override;

        /**
         * @brief Register a raw URI handler.
         * @param uri The URI to register the handler for.
         * @param method The HTTP method to register the handler for.
         * @param handler The handler to call when the URI is requested.
         * @param userContext Any user context to pass to the handler.
         * @return An error code if the handler could not be registered.
         */
        std::error_code registerRawUri(const Uri& uri, const httpd_method_t& method, RawHandler handler, void* userContext = nullptr) const;

        /**
         * @brief Register a GET request callback.
         * @tparam PATH The path of the URI to register the handler for.
         * @tparam N_HEADERS The number of headers to send with the response.
         * @tparam N_PARAMETERS The number of parameters that the callback expects.
         * @tparam SIZE_RETURN The size of the returned string by the callback.
         * @param callback The callback to call when the URI is requested.
         * @param parameters List of parameter keys that the callback expects. No additional parameters will be passed to the callback.
         * @param headers List of headers to send with the response.
         * @param userContext Any user context to pass to the handler.
         * @return An error code if the handler could not be registered.
         */
        template<StringLiteral PATH, std::size_t N_HEADERS, std::size_t N_PARAMETERS, std::size_t SIZE_RETURN>
        std::error_code registerGet(GetCallback<N_PARAMETERS, SIZE_RETURN> callback, const std::array<QueryKey, N_PARAMETERS>& parameters, const etl::vector<SendHeader, N_HEADERS>& headers, void* userContext = nullptr) {
            ESP_LOGD(TAG, "Registering GET callback for %s", PATH.c_str());

            if (m_server == nullptr) {
                ESP_LOGE(TAG, "Webserver not initialized");
                return std::make_error_code(ESP_ERR_INVALID_STATE);
            }

            // Static so that the data will not be destroyed after the function returns, as the handler needs to access it
            static GetData<N_HEADERS, N_PARAMETERS, SIZE_RETURN> data{
                    .callback    = callback,
                    .parameters  = parameters,
                    .headers     = headers,
                    .userContext = userContext};

            const httpd_uri uri = {
                    .uri      = PATH.c_str(),
                    .method   = HTTP_GET,
                    .handler  = getHandler<N_HEADERS, N_PARAMETERS, SIZE_RETURN>,
                    .user_ctx = &data,
#ifdef CONFIG_HTTPD_WS_SUPPORT
                    .is_websocket             = false,
                    .handle_ws_control_frames = false,
                    .supported_subprotocol    = nullptr,
#endif
            };
            if (const auto ret = httpd_register_uri_handler(m_server, &uri); ret != ESP_OK) {
                assert(ret == ESP_ERR_HTTPD_HANDLER_EXISTS && "Don't register the same URI twice");
                ESP_LOGE(TAG, "Error registering Get callback: %s", esp_err_to_name(ret));
                return std::make_error_code(ret);
            }
            ESP_LOGD(TAG, "Registered GET callback for %s", PATH.c_str());
            return {};
        }

        /**
         * @brief Register a POST request callback.
         * @tparam PATH The path of the URI to register the handler for.
         * @tparam N_HEADERS The number of headers to send with the response.
         * @tparam N_PARAMETERS The number of parameters that the callback expects.
         * @tparam SIZE_RETURN The size of the returned string by the callback.
         * @tparam SIZE_BODY The size of the buffer for the request body.
         * @param callback The callback to call when the URI is requested.
         * @param parameters List of parameter keys that the callback expects. No additional parameters will be passed to the callback.
         * @param headers List of headers to send with the response.
         * @param userContext Any user context to pass to the handler.
         * @return An error code if the handler could not be registered.
         */
        template<StringLiteral PATH, std::size_t N_HEADERS, std::size_t N_PARAMETERS, std::size_t SIZE_RETURN, std::size_t SIZE_BODY>
        std::error_code registerPost(PostCallback<N_PARAMETERS, SIZE_RETURN, SIZE_BODY> callback, const std::array<QueryKey, N_PARAMETERS>& parameters, const etl::vector<SendHeader, N_HEADERS>& headers, void* userContext = nullptr) {
            ESP_LOGD(TAG, "Registering GET callback for %s", PATH.c_str());

            if (m_server == nullptr) {
                ESP_LOGE(TAG, "Webserver not initialized");
                return std::make_error_code(ESP_ERR_INVALID_STATE);
            }

            // Static so that the data will not be destroyed after the function returns, as the handler needs to access it
            static PostData<N_HEADERS, N_PARAMETERS, SIZE_RETURN, SIZE_BODY> data{
                    .callback    = callback,
                    .parameters  = parameters,
                    .headers     = headers,
                    .userContext = userContext};

            const httpd_uri uri = {
                    .uri      = PATH.c_str(),
                    .method   = HTTP_POST,
                    .handler  = postHandler<N_HEADERS, N_PARAMETERS, SIZE_RETURN, SIZE_BODY>,
                    .user_ctx = &data,
#ifdef CONFIG_HTTPD_WS_SUPPORT
                    .is_websocket             = false,
                    .handle_ws_control_frames = false,
                    .supported_subprotocol    = nullptr,
#endif
            };
            if (const auto ret = httpd_register_uri_handler(m_server, &uri); ret != ESP_OK) {
                assert(ret == ESP_ERR_HTTPD_HANDLER_EXISTS && "Don't register the same URI twice");
                ESP_LOGE(TAG, "Error registering Get callback: %s", esp_err_to_name(ret));
                return std::make_error_code(ret);
            }
            ESP_LOGD(TAG, "Registered GET callback for %s", PATH.c_str());
            return {};
        }

    private:
        /**
         * @brief Data structure for GET request handlers. Wraps the callback and additional data.
         * @tparam N_HEADERS The number of headers to send with the response.
         * @tparam N_PARAMETERS The number of parameters that the callback expects.
         * @tparam SIZE_RETURN The size of the returned string.
         */
        template<std::size_t N_HEADERS, std::size_t N_PARAMETERS, std::size_t SIZE_RETURN>
        struct GetData {
            GetCallback<N_PARAMETERS, SIZE_RETURN>   callback;
            std::array<QueryKey, N_PARAMETERS>       parameters;
            const etl::vector<SendHeader, N_HEADERS> headers;
            void*                                    userContext = nullptr;
        };

        /**
         * @brief Data structure for POST request handlers. Wraps the callback and additional data.
         * @tparam N_HEADERS The number of headers to send with the response.
         * @tparam N_PARAMETERS The number of parameters that the callback expects.
         * @tparam SIZE_RETURN The size of the returned string.
         * @tparam SIZE_BODY The size of the buffer for the request body.
         */
        template<std::size_t N_HEADERS, std::size_t N_PARAMETERS, std::size_t SIZE_RETURN, std::size_t SIZE_BODY>
        struct PostData {
            PostCallback<N_PARAMETERS, SIZE_RETURN, SIZE_BODY> callback;
            std::array<QueryKey, N_PARAMETERS>                 parameters;
            const etl::vector<SendHeader, N_HEADERS>           headers;
            void*                                              userContext = nullptr;
        };

        static constexpr char TAG[] = "HTTP SERVER";

        httpd_handle_t m_server = nullptr;
        httpd_config_t m_config;

        /**
         * @brief Wrapper around GET request callbacks that get registered.
         * @tparam N_HEADERS The number of headers to send with the response.
         * @tparam N_PARAMETERS The number of parameters that the callback expects.
         * @tparam SIZE_RETURN The size of the returned string.
         * @param req Pointer to the request object that was made.
         * @return ESP_OK Whether the request was handled successfully.
         */
        template<std::size_t N_HEADERS, std::size_t N_PARAMETERS, std::size_t SIZE_RETURN>
        static esp_err_t getHandler(httpd_req_t* req) {

            assert(req->user_ctx != nullptr && "User context is null");
            auto* data = static_cast<GetData<N_HEADERS, N_PARAMETERS, SIZE_RETURN>*>(req->user_ctx);

            for (const auto& header: data->headers) {
                auto err = httpd_resp_set_hdr(req, header.first.c_str(), header.second.c_str());
                assert(err == ESP_OK && "Error setting headers");
            }

            auto parameters = getQueryParameters(req, data->parameters);

            auto ret = data->callback(req->uri, parameters, data->userContext);
            if (!ret.has_value()) {
                ESP_LOGD(TAG, "Callback for %s returned an error: %s", req->uri, ret.error().statusMessage());
                httpd_resp_send_custom_err(req, ret.error().statusMessage(), ret.error().message.c_str());
                return ESP_FAIL;
            }

            httpd_resp_send(req, ret.value().c_str(), ret.value().length());
            return ESP_OK;
        }

        /**
         * @brief Wrapper around POST request callbacks that get registered.
         * @tparam N_HEADERS The number of headers to send with the response.
         * @tparam N_PARAMETERS The number of parameters that the callback expects.
         * @tparam SIZE_RETURN The size of the returned string.
         * @tparam SIZE_BODY The size of the buffer for the request body.
         * @param req Pointer to the request object that was made.
         * @return ESP_OK Whether the request was handled successfully.
         */
        template<std::size_t N_HEADERS, std::size_t N_PARAMETERS, std::size_t SIZE_RETURN, std::size_t SIZE_BODY>
        static esp_err_t postHandler(httpd_req_t* req) {

            assert(req->user_ctx != nullptr && "User context is null");
            auto* data = static_cast<PostData<N_HEADERS, N_PARAMETERS, SIZE_RETURN, SIZE_BODY>*>(req->user_ctx);

            auto body = receiveBody<SIZE_BODY>(req);
            if (!body) {
                if (body.error() == ESP_ERR_NO_MEM) {
                    httpd_resp_send_custom_err(req, statusCodePhrase(StatusCode::PayloadTooLarge), statusCodePhrase(StatusCode::PayloadTooLarge));
                } else if (body.error() == HTTPD_SOCK_ERR_TIMEOUT) {
                    httpd_resp_send_408(req);
                }
                return body.error();
            }

            for (const auto& header: data->headers) {
                auto err = httpd_resp_set_hdr(req, header.first.c_str(), header.second.c_str());
                assert(err == ESP_OK && "Error setting headers");
            }

            auto parameters = getQueryParameters(req, data->parameters);

            auto ret = data->callback(req->uri, body.value(), parameters, data->userContext);
            if (!ret.has_value()) {
                ESP_LOGD(TAG, "Callback for %s returned an error: %s", req->uri, ret.error().statusMessage());
                httpd_resp_send_custom_err(req, ret.error().statusMessage(), ret.error().message.c_str());
                return ESP_FAIL;
            }

            httpd_resp_send(req, ret.value().c_str(), ret.value().length());
            return ESP_OK;
        }

        /**
         * @brief Get the query parameters from the request uri. Also decodes the parameters
         * @tparam N_PARAMETERS The number of parameters to get.
         * @param req Pointer to the request object that was made.
         * @param keys The keys to get the parameters for.
         * @return A map of the found query parameters. If a key is not found, it will not be in the map.
         */
        template<std::size_t N_PARAMETERS>
        static etl::unordered_map<QueryKey, QueryValue, N_PARAMETERS> getQueryParameters(httpd_req_t* req, const std::array<QueryKey, N_PARAMETERS>& keys) {
            etl::string<QueryKey::MAX_SIZE + QueryValue::MAX_SIZE> query;
            auto                                           err = httpd_req_get_url_query_str(req, query.data(), query.max_size());
            assert((err == ESP_OK || err == ESP_ERR_NOT_FOUND) && "Error getting URL query string");
            // If there are no query parameters, return an empty map
            if (err == ESP_ERR_NOT_FOUND) {
                return {};
            }

            etl::unordered_map<QueryKey, QueryValue, N_PARAMETERS> parameters;
            for (const auto& key: keys) {
                char buf[QueryValue::MAX_SIZE];
                err = httpd_query_key_value(query.c_str(), key.c_str(), buf, sizeof(buf));
                assert((err == ESP_OK || err == ESP_ERR_NOT_FOUND) && "Error getting query key value");
                // If the key is not found, continue to the next key
                if (err == ESP_ERR_NOT_FOUND) {
                    continue;
                }
                parameters[key] = buf;
            }
            return parameters;
        }

        /**
         * @brief Decode a URL encoded string.
         * @tparam N The size of the string to decode.
         * @param url The URL to decode.
         */
        template<std::size_t N>
        static void urlDecode(etl::string<N>& url) {
            // Replace '+' with ' '
            etl::replace(url.begin(), url.end(), '+', ' ');
            auto pos = url.find('%');
            while (pos != etl::string<N>::npos) {
                if (pos + 2 < url.length()) {
                    int value{};
                    // Convert hex string to int
                    auto [ptr, ec] = std::from_chars(url.data() + pos + 1, url.data() + pos + 3, value, 16);
                    assert(ec == std::errc() && "Illegal characters in URL");
                    // Replace '%<hex string>' with decoded character
                    url.replace(pos, 3, 1, static_cast<char>(value));
                }
                // Find next '%', starting after the previous '%'
                pos = url.find('%', pos + 1);
            }
        }

        /**
         * @brief Receive the body of a request. It will retry receiving the body if a timeout occurs.
         * @tparam N The size of the buffer to receive the body in.
         * @param req Pointer to the request object that was made.
         * @return The received body, or an error code if the body could not be received.
         */
        template<std::size_t N>
        static std::expected<etl::string<N>, esp_err_t> receiveBody(httpd_req_t* req) {
            ESP_LOGD(TAG, "Receiving body for %s, of length %d", req->uri, req->content_len);
            // Check if the body is too large for the buffer
            if (N <= req->content_len) {
                ESP_LOGE(TAG, "The received body is too large for the buffer: %d > %d", req->content_len, N);
                return std::unexpected(ESP_ERR_NO_MEM);
            }

            std::array<char, N> body;
            while (1) {
                auto retrieved = httpd_req_recv(req, body.data(), std::min(req->content_len, body.size()));
                if (retrieved > 0) {
                    if (retrieved < N) {
                        // Zero terminate the buffer
                        body[retrieved] = '\0';
                    }
                    return body.data();
                }
                // If there was a timeout while receiving the body, retry
                if (retrieved == HTTPD_SOCK_ERR_TIMEOUT) {
                    continue;
                }
                return std::unexpected(retrieved);
            }
        }
    };

} // namespace sdk::Http

#endif /* HTTP_SERVER_HPP */
