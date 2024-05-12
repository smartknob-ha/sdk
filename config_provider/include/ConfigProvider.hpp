#ifndef CONFIG_PROVIDER_HPP
#define CONFIG_PROVIDER_HPP

#include <esp_log.h>
#include <etl/string.h>
#include <nlohmann/json.hpp>

#include <any>
#include <cstring>
#include <typeindex>

#include "esp_err.h"
#include "esp_system_error.hpp"
#include "etl/unordered_map.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_handle.hpp"

namespace sdk {

    using ConfigKey = etl::string<NVS_NS_NAME_MAX_SIZE>;

    enum class RestartType {
        NONE,
        COMPONENT,
        DEVICE
    };

    template<typename T>
    class ConfigField {
    private:
        const ConfigKey m_key;

        const RestartType m_restartType{RestartType::NONE};

        const std::type_index m_type{std::type_identity<T>()};

        const T m_value;

    public:
        ConfigField(T defaultValue, const ConfigKey key, RestartType restartType = RestartType::NONE)
            : m_key(key),
              m_restartType(restartType),
              m_type(typeid(T)),
              m_value(defaultValue){};

        ConfigField<T>& operator=(ConfigField<T> const& in) {
            if (this != &in) {
                std::destroy_at(this);
                std::construct_at(this, in);
            }
            return *this;
        }

        ConfigField() = delete;

        const T& value() const {
            return m_value;
        };

        [[nodiscard]] RestartType restartType() const {
            return m_restartType;
        }

        [[nodiscard]] bool componentRestartRequired() const {
            return m_restartType == RestartType::COMPONENT;
        }

        [[nodiscard]] bool rebootRequired() const {
            return m_restartType == RestartType::DEVICE;
        }

        [[nodiscard]] ConfigKey key() const {
            return m_key;
        }

        [[nodiscard]] std::type_index type() const {
            return m_type;
        }

        operator T() const {
            return m_value;
        }
    };

    class ConfigProvider {
    private:
        static inline const char* TAG = "CONFIG";

        static inline bool initialized = false;

        const ConfigKey nvsNamespace;

        const bool readOnly;

        std::unique_ptr<nvs::NVSHandle> m_handle;

    public:
        explicit ConfigProvider(const ConfigKey nvsNamespace, const bool readOnly = true) : nvsNamespace(nvsNamespace), readOnly(readOnly){};

        std::error_code initialize() {

            esp_err_t err;

            if (!initialized) {
                err = nvs_flash_init();
                if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                    // NVS partition was truncated and needs to be erased
                    // Retry nvs_flash_init
                    ESP_ERROR_CHECK(nvs_flash_erase());
                    err = nvs_flash_init();
                }
                ESP_ERROR_CHECK(err);
                initialized = true;
            }
            nvs_open_mode_t nvsMode = readOnly ? NVS_READONLY : NVS_READWRITE;

            m_handle = nvs::open_nvs_handle(nvsNamespace.c_str(), nvsMode, &err);
            ESP_ERROR_CHECK_WITHOUT_ABORT(err);
            return std::make_error_code(err);
        }

        template<typename T>
        std::error_code getItemSize(const ConfigKey key, T& item, size_t& size) {
            assert(m_handle != nullptr && "Call initialize() first");
            return std::make_error_code(m_handle->get_item_size(item, key.c_str(), size));
        }

        template<size_t LENGTH>
        std::error_code getItemSize(const ConfigKey key, etl::string<LENGTH>& str, size_t& size) {
            assert(m_handle != nullptr && "Call initialize() first");
            return std::make_error_code(m_handle->get_item_size(nvs::ItemType::SZ, key.c_str(), size));
        }

        template<size_t LENGTH>
        std::error_code loadItem(const ConfigKey key, etl::string<LENGTH>& item) {
            assert(m_handle != nullptr && "Call initialize() first");
            size_t required_size = 0;

            auto err = getItemSize(key, item, required_size);
            if (err) {
                return err;
            }
            assert(LENGTH >= required_size && "LENGTH too small");
            char buffer[LENGTH];
            err = std::make_error_code(m_handle->get_string(key.c_str(), &buffer[0], required_size));
            if (err) {
                ESP_LOGE(TAG, "Error loading item: %s, err: %s", key.c_str(), err.message().c_str());
            } else {
                item.assign(buffer);
            }
            return err;
        }

        template<size_t BUFFER_SIZE>
        std::error_code loadJson(const ConfigKey key, nlohmann::json& item) {
            assert(m_handle != nullptr && "Call initialize() first");
            etl::string<BUFFER_SIZE> buffer;

            auto err = loadItem(key, buffer);
            if (err) {
                return err;
            }
            item = nlohmann::json::parse(buffer);
            return err;
        }

        template<typename T>
        std::error_code loadItem(const ConfigKey key, T& item) {
            assert(m_handle != nullptr && "Call initialize() first");
            return m_handle->get_item(key.c_str(), item);
        }

        template<typename T>
        std::error_code saveItem(const ConfigKey key, T& item, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to save if NVS is opened in READONLY mode");
            esp_err_t err = m_handle->set_item(key.c_str(), item);
            if (err != ESP_OK) {
                return std::make_error_code(err);
            }
            if (commit) {
                return this->commit();
            }
            return std::make_error_code(err);
        }

        template<size_t LENGTH>
        std::error_code saveItem(const ConfigKey key, const etl::string<LENGTH>& item, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to save if NVS is opened in READONLY mode");
            esp_err_t err = m_handle->set_string(key.c_str(), item.c_str());
            if (err != ESP_OK) {
                return std::make_error_code(err);
            }
            if (commit) {
                return this->commit();
            }
            return std::make_error_code(err);
        }

        template<size_t BUFFER_SIZE>
        std::error_code saveJson(const ConfigKey key, nlohmann::json& json, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            etl::string<BUFFER_SIZE> buffer(json.dump().c_str());
            auto                     err = std::make_error_code(m_handle->set_string(key.c_str(), buffer.c_str()));
            if (err) {
                ESP_LOGE(TAG, "Error saving json %s: %s", key.c_str(), err.message().c_str());
                return err;
            }
            if (commit) {
                return this->commit();
            }
            return err;
        }

        std::error_code eraseItem(const etl::string<NVS_KEY_NAME_MAX_SIZE>& key, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to erase if NVS is opened in READONLY mode");
            auto err = m_handle->erase_item(key.c_str());
            if (err != ESP_OK) {
                return std::make_error_code(err);
            }
            if (commit) {
                return std::make_error_code(m_handle->commit());
            }
            return std::make_error_code(err);
        }

        std::error_code erase() {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to erase if NVS is opened in READONLY mode");
            return std::make_error_code(m_handle->erase_all());
        }

        std::error_code commit() {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to commit if NVS is opened in READONLY mode");
            auto err = std::make_error_code(m_handle->commit());
            if (err) {
                ESP_LOGE(TAG, "Error committing changes: %s", err.message().c_str());
            }
            return err;
        }
    };

    template<size_t N>
    struct StringLiteral {
        constexpr StringLiteral(const char (&str)[N]) {
            std::copy_n(str, N, value);
        }

        char value[N];

        operator const char*() const {
            return value[0];
        }

        operator char*() const {
            return value[0];
        }

        const char* c_str() const {
            return value;
        }
    };

    template<size_t NUM_ITEMS, size_t BUFFER_SIZE, StringLiteral KEY>
    class ConfigObject {
    private:
        char m_buffer[BUFFER_SIZE]{0};

        std::error_code load() {
            ConfigProvider provider("config", true);
            auto           err = provider.initialize();
            if (err) {
                ESP_LOGE(KEY.c_str(), "Error initializing config: %s", err.message().c_str());
                return err;
            }
            err = provider.loadJson<BUFFER_SIZE>(KEY.c_str(), *m_json);
            return err;
        }

        size_t hash(ConfigKey key) {
            etl::hash<etl::string<NVS_KEY_NAME_MAX_SIZE>> hasher;
            return hasher(key);
        }

    public:
        etl::unordered_map<size_t, RestartType, NUM_ITEMS> m_restartRequiredMap{};

        nlohmann::json* m_json = reinterpret_cast<nlohmann::json*>(m_buffer);

        explicit ConfigObject(nlohmann::json& data) {
            for (const auto& object: data.items()) {
                (*m_json)[object.key()] = object.value();
            }
        };

        ConfigObject() {
            load();
        }

        template<typename T>
        ConfigField<T> allocate(ConfigField<T>& field) {
            m_restartRequiredMap[hash(field.key())] = field.restartType();

            if (!m_json->contains(field.key().c_str())) {
                assert(m_json->size() + sizeof(field) < BUFFER_SIZE);
                m_json->emplace(field.key().c_str(), field.value());

                // ConfigField is immutable, so return a new object
                return ConfigField<T>{field.value(), field.key(), field.restartType()};
            } else {
                T retrieved_value = m_json->value(static_cast<const char*>(field.key().c_str()), field.value());
                return ConfigField<T>{retrieved_value, field.key(), field.restartType()};
            }
        };

        std::error_code save() {
            ConfigProvider provider("config", false);
            auto           err = provider.initialize();
            if (err) {
                return err;
            }
            err = provider.saveJson<BUFFER_SIZE>(KEY.c_str(), *m_json, true);
            if (err) {
                ESP_LOGE(KEY.c_str(), "Error saving config: %s", err.message().c_str());
            }
            return err;
        }

        RestartType checkForRestartRequired(nlohmann::json& json) {
            auto temp = RestartType::NONE;
            for (const auto& object: json.items()) {
                auto element = m_restartRequiredMap.find(hash(object.key().c_str()));
                if (element == m_restartRequiredMap.end()) {
                    ESP_LOGD(KEY.c_str(), "No restart required found for %s", object.key().c_str());
                    continue;
                }
                // If device restart is found, nothing else matters
                if (element->second == sdk::RestartType::COMPONENT) {
                    temp = sdk::RestartType::COMPONENT;
                } else if (element->second == sdk::RestartType::DEVICE) {
                    return sdk::RestartType::DEVICE;
                }
            }
            return temp;
        }

        RestartType checkForRestartRequired(etl::string<NVS_KEY_NAME_MAX_SIZE>& key) {
            auto element = m_restartRequiredMap.find(hash(key));
            if (element == m_restartRequiredMap.end()) {
                ESP_LOGD(KEY.c_str(), "No restart required found for %s", key.c_str());
                RestartType::NONE;
            }
            return element->second;
        }
    };
} // namespace sdk

#endif // CONFIG_PROVIDER_HPP
