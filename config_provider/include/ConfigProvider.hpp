#ifndef CONFIG_PROVIDER_HPP
#define CONFIG_PROVIDER_HPP

#include <esp_log.h>
#include <etl/string.h>
#include <nlohmann/json.hpp>

#include <any>
#include <cstring>
#include <typeindex>

#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_system_error.hpp"
#include "etl/unordered_map.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_handle.hpp"
#include "semantic_versioning.hpp"

#define CONFIG_NAMESPACE "config"
#define CONFIG_VERSION_KEY "version"

// Serialize and deserialize etl::string
namespace nlohmann {
    template<std::size_t N>
    struct adl_serializer<etl::string<N>> {
        static void to_json(json& j, const etl::string<N>& str) {
            j = std::string(str.begin(), str.end());
        }

        static void from_json(const json& j, etl::string<N>& str) {
            std::string temp = j.get<std::string>();
            str.assign(temp.begin(), temp.end());
        }
    };
} // namespace nlohmann

namespace sdk {

    using ConfigKey = etl::string<NVS_NS_NAME_MAX_SIZE>;

    class ConfigProvider {
    private:
        static inline const char* TAG = "CONFIG";

        static inline bool m_initialized = false;

        const ConfigKey m_namespace;

        const bool m_readOnly;

        std::unique_ptr<nvs::NVSHandle> m_handle;

    public:
        explicit ConfigProvider(const ConfigKey& nvsNamespace, const bool readOnly = true) : m_namespace(nvsNamespace), m_readOnly(readOnly){};

        /**
         * @brief Initialize the NVS handle, call this before any other function
         *
         *        If the NVS partition is truncated, it will be erased
         * @return Error code of type esp_err_t
         */
        std::error_code initialize() {

            esp_err_t err;

            if (!m_initialized) {
                err = nvs_flash_init();
                if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                    // NVS partition was truncated and needs to be erased
                    // Retry nvs_flash_init
                    ESP_ERROR_CHECK(nvs_flash_erase());
                    err = nvs_flash_init();
                }
                ESP_ERROR_CHECK(err);
                m_initialized = true;
            }
            nvs_open_mode_t nvsMode = m_readOnly ? NVS_READONLY : NVS_READWRITE;

            m_handle = nvs::open_nvs_handle(m_namespace.c_str(), nvsMode, &err);
            ESP_ERROR_CHECK_WITHOUT_ABORT(err);
            return std::make_error_code(err);
        }

        /**
         * @brief Get the size of an item in NVS
         * @tparam T Item type
         * @param key Key of the nvs entry, max length is NVS_KEY_NAME_MAX_SIZE
         * @param item Item to get the size of
         * @param size Place to store the size of the item
         * @return Error code of type esp_err_t, will be ESP_ERR_NVS_NOT_FOUND if the entry does not exist
         */
        template<typename T>
        std::error_code getItemSize(const ConfigKey key, T& item, size_t& size) {
            assert(m_handle != nullptr && "Call initialize() first");
            return std::make_error_code(m_handle->get_item_size(item, key.c_str(), size));
        }

        /**
         * @brief Get the size of a string in NVS
         * @param key Key of the nvs entry, max length is NVS_KEY_NAME_MAX_SIZE
         * @param size Place to store the size of the item
         * @return Error code of type esp_err_t, will be ESP_ERR_NVS_NOT_FOUND if the entry does not exist
         */
        std::error_code getStringSize(const ConfigKey key, size_t& size) {
            assert(m_handle != nullptr && "Call initialize() first");
            return std::make_error_code(m_handle->get_item_size(nvs::ItemType::SZ, key.c_str(), size));
        }

        /**
         * @brief Load an item from NVS
         * @tparam T Item type
         * @param key Key of the nvs entry, max length is NVS_KEY_NAME_MAX_SIZE
         * @param item Place to store the item, will be untouched if not found
         * @return Error code of type esp_err_t, will be ESP_ERR_NVS_NOT_FOUND if the entry does not exist
         */
        template<typename T>
        std::error_code loadItem(const ConfigKey key, T& item) {
            assert(m_handle != nullptr && "Call initialize() first");

            auto err = std::make_error_code(m_handle->get_item(key.c_str(), item));
            if (err) {
                ESP_LOGE(TAG, "Error loading item: %s, err: %s", key.c_str(), err.message().c_str());
            }
            return err;
        }

        /**
         * @brief Load an etl::string from NVS
         * @tparam LENGTH Length of the string
         * @param key Key of the nvs entry, max length is NVS_KEY_NAME_MAX_SIZE
         * @param string Place to store the string, will be untouched if not found
         * @return Error code of type esp_err_t, will be ESP_ERR_NVS_NOT_FOUND if the entry does not exist
         */
        template<size_t LENGTH>
        std::error_code loadItem(const ConfigKey key, etl::string<LENGTH>& string) {
            assert(m_handle != nullptr && "Call initialize() first");
            size_t storedSize = 0;

            auto err = getStringSize(key, storedSize);
            if (err) {
                return err;
            }
            assert(LENGTH >= storedSize && "LENGTH too small");
            char buffer[LENGTH];
            err = std::make_error_code(m_handle->get_string(key.c_str(), &buffer[0], storedSize));
            if (err) {
                ESP_LOGE(TAG, "Error loading string: %s, err: %s", key.c_str(), err.message().c_str());
            } else {
                string.assign(buffer);
            }
            return err;
        }

        /**
         * @brief Load a json string from NVS
         * @tparam BUFFER_SIZE Size of the buffer to store the json object
         * @param key Key of the nvs entry, max length is NVS_KEY_NAME_MAX_SIZE
         * @param json Place to store the json, will be untouched if not found
         * @return Error code of type esp_err_t, will be ESP_ERR_NVS_NOT_FOUND if the entry does not exist
         */
        template<size_t BUFFER_SIZE>
        std::error_code loadJson(const ConfigKey key, nlohmann::json& json) {
            assert(m_handle != nullptr && "Call initialize() first");
            etl::string<BUFFER_SIZE> buffer;

            auto err = loadItem(key, buffer);
            if (err) {
                return err;
            }
            json = nlohmann::json::parse(buffer);
            return err;
        }

        /**
         * @brief Save an item to NVS
         * @tparam T Item type
         * @param key Key of the nvs entry, max length is NVS_KEY_NAME_MAX_SIZE
         * @param item Item to save
         * @param commit Commit changes to NVS after saving, if false, call commit() manually
         * @return Error code of type esp_err_t
         */
        template<typename T>
        std::error_code saveItem(const ConfigKey key, T& item, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!m_readOnly && "Unable to save if NVS is opened in READONLY mode");
            auto err = std::make_error_code(m_handle->set_item(key.c_str(), item));
            if (err) {
                ESP_LOGE(TAG, "Error saving item %s: %s", key.c_str(), err.message().c_str());
                return err;
            }
            if (commit) {
                return this->commit();
            }
            return err;
        }

        /**
         * @brief Save an etl::string to NVS
         * @tparam LENGTH Length of the string
         * @param key Key of the nvs entry, max length is NVS_KEY_NAME_MAX_SIZE
         * @param string String to save
         * @param commit Commit changes to NVS after saving, if false, call commit() manually
         * @return Error code of type esp_err_t
         */
        template<size_t LENGTH>
        std::error_code saveItem(const ConfigKey key, const etl::string<LENGTH>& string, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!m_readOnly && "Unable to save if NVS is opened in READONLY mode");
            auto err = std::make_error_code(m_handle->set_string(key.c_str(), string.c_str()));
            if (err) {
                ESP_LOGE(TAG, "Error saving string %s: %s", key.c_str(), err.message().c_str());
                return err;
            }
            if (commit) {
                return this->commit();
            }
            return err;
        }

        /**
         * @brief Save a json object to NVS
         * @param key Key of the nvs entry, max length is NVS_KEY_NAME_MAX_SIZE
         * @param json Json object to save
         * @param commit Commit changes to NVS after saving, if false, call commit() manually
         * @return Error code of type esp_err_t
         */
        std::error_code saveJson(const ConfigKey key, const nlohmann::json& json, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!m_readOnly && "Unable to save if NVS is opened in READONLY mode");
            auto err = std::make_error_code(m_handle->set_string(key.c_str(), json.dump().c_str()));
            if (err) {
                ESP_LOGE(TAG, "Error saving json %s: %s", key.c_str(), err.message().c_str());
                return err;
            }
            if (commit) {
                return this->commit();
            }
            return err;
        }

        /**
         * @brief Erase an item from NVS
         * @param key Key of the nvs entry, max length is NVS_KEY_NAME_MAX_SIZE
         * @param commit Commit changes to NVS after erasing, if false, call commit() manually
         * @return Error code of type esp_err_t
         */
        std::error_code eraseItem(const etl::string<NVS_KEY_NAME_MAX_SIZE>& key, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!m_readOnly && "Unable to erase if NVS is opened in READONLY mode");
            auto err = std::make_error_code(m_handle->erase_item(key.c_str()));
            if (err) {
                ESP_LOGE(TAG, "Error erasing item %s: %s", key.c_str(), err.message().c_str());
                return err;
            }
            if (commit) {
                return this->commit();
            }
            return err;
        }

        /**
         * @brief Erase an entire namespace from NVS
         * @return Error code of type esp_err_t
         */
        std::error_code erase() {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!m_readOnly && "Unable to erase if NVS is opened in READONLY mode");
            auto err = std::make_error_code(m_handle->erase_all());
            if (err) {
                ESP_LOGE(TAG, "Error erasing namespace %s: %s", m_namespace.c_str(), err.message().c_str());
            }
            return err;
        }

        /**
         * @brief Commit changes to NVS
         * @return Error code of type esp_err_t
         */
        std::error_code commit() {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!m_readOnly && "Unable to commit if NVS is opened in READONLY mode");
            auto err = std::make_error_code(m_handle->commit());
            if (err) {
                ESP_LOGE(TAG, "Error committing changes: %s", err.message().c_str());
            }
            return err;
        }
    };

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

        /**
         * Copy constructor
         * @param in ConfigField to copy
         */
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

    // String literal for to support strings as template parameter
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
        // Buffer to store the json object. This is a workaround to avoid dynamic memory allocation
        char m_buffer[BUFFER_SIZE]{0};

        semver::version m_version;

        // Map to store the restart required status of each field
        etl::unordered_map<size_t, RestartType, NUM_ITEMS> m_restartRequiredMap{};

        nlohmann::json* m_json = reinterpret_cast<nlohmann::json*>(m_buffer);

        /**
         * @brief Retrieves itself from NVS
         * @return Error code of type esp_err_t
         */
        std::error_code load() {
            ConfigProvider provider(CONFIG_NAMESPACE, true);
            auto           err = provider.initialize();
            if (err) {
                ESP_LOGE(KEY.c_str(), "Error initializing config: %s", err.message().c_str());
                return err;
            }
            err = provider.loadJson<BUFFER_SIZE>(KEY.c_str(), *m_json);

            // If the version field is not found, set it to the current version
            if (!err && m_json->contains(CONFIG_VERSION_KEY)) {
                m_version = semver::from_string(m_json->at(CONFIG_VERSION_KEY).get<std::string>());
            } else {
                auto app_desc = esp_app_get_description();
                m_version = semver::from_string(app_desc->version);
            }
            return err;
        }

        /**
         * @brief Hash function for the ConfigKey, workaround to use etl::strings in etl::unordered_map
         * @param key Key to hash
         * @return Hash of the key
         */
        size_t hash(ConfigKey key) {
            etl::hash<etl::string<NVS_KEY_NAME_MAX_SIZE>> hasher;
            return hasher(key);
        }

    public:
        /**
         * @brief Constructor to be used for incoming config changes
         * @param data Json object containing changes. Should only contain the fields that need to be updated
         */
        explicit ConfigObject(nlohmann::json& data) {
            for (const auto& object: data.items()) {
                (*m_json)[object.key()] = object.value();
            }
        };

        /**
         * @brief Constructor that attempts to retrieve itself from NVS and load the fields
         */
        ConfigObject() {
            load();
        }

        /**
         * @brief Allocates a field in the json object
         * @tparam T Type of the field
         * @param field Field to allocate, if it already exists in NVS, the default value of field will be overwritten
         * @return ConfigField to be assigned to the passed field
         */
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

        /**
         * @brief Saves itself to NVS in the namespace "config"
         * @return Error code of type esp_err_t
         */
        std::error_code save() {
            ConfigProvider provider(CONFIG_NAMESPACE, false);

            // Update the version field to the current version
            auto app_desc              = esp_app_get_description();
            m_json->at(CONFIG_VERSION_KEY) = app_desc->version;
            m_version                  = semver::from_string(app_desc->version);

            auto err = provider.initialize();
            if (err) {
                return err;
            }
            err = provider.saveJson(KEY.c_str(), *m_json, true);
            if (err) {
                ESP_LOGE(KEY.c_str(), "Error saving config: %s", err.message().c_str());
            }
            return err;
        }

        /**
         * @brief Check if a device restart is required based on the fields in the json object
         * @param json Json object to check
         * @return RestartType::DEVICE if a device restart is required, RestartType::COMPONENT if a component restart is required, RestartType::NONE if no restart is required
         */
        RestartType checkForRestartRequired(const nlohmann::json& json) {
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

        /**
         * @brief Check if a device restart is required
         * @param key Key to check
         * @return RestartType::DEVICE if a device restart is required, RestartType::COMPONENT if a component restart is required, RestartType::NONE if no restart is required
         */
        RestartType checkForRestartRequired(etl::string<NVS_KEY_NAME_MAX_SIZE>& key) {
            auto element = m_restartRequiredMap.find(hash(key));
            if (element == m_restartRequiredMap.end()) {
                ESP_LOGD(KEY.c_str(), "No restart required found for %s", key.c_str());
                return RestartType::NONE;
            }
            return element->second;
        }

        /**
         * @brief Get the firmware version the config was last updated on
         * @return Version the config was last updated on
         */
        const semver::version version() const {
            return m_version;
        }

        /**
         * @brief Get the size of the json string stored in NVS
         * @return Size of the json string stored in NVS. Returns 0 if the entry does not exist
         */
        static size_t getStoredSize() {
            ConfigProvider provider(CONFIG_NAMESPACE, false);
            auto           err = provider.initialize();
            if (err) {
                return 0;
            }
            size_t size = 0;
            provider.getStringSize(KEY.c_str(), size);
            return size;
        }
    };
} // namespace sdk

#endif // CONFIG_PROVIDER_HPP
