#ifndef CONFIG_PROVIDER_HPP
#define CONFIG_PROVIDER_HPP

#include <esp_log.h>
#include <etl/map.h>
#include <etl/string.h>
#include <nlohmann/json.hpp>

#include <any>
#include <cstring>
#include <typeindex>

#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_handle.hpp"

// todo fix
namespace sdk {

    using ConfigKey = etl::string<NVS_NS_NAME_MAX_SIZE>;
    //    using ConfigKey = etl::string<NVS_NS_NAME_MAX_SIZE>;

    enum class RestartType {
        NONE,
        COMPONENT,
        DEVICE
    };

    template<typename T>
    struct ConfigField {
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

        RestartType restartType() const {
            return m_restartType;
        }

        bool componentRestartRequired() const {
            return m_restartType == RestartType::COMPONENT;
        }

        bool rebootRequired() const {
            return m_restartType == RestartType::DEVICE;
        }

        ConfigKey key() const {
            return m_key;
        }

        std::type_index type() const {
            return m_type;
        }
    };

    template<size_t BUFFER_SIZE = 1024>
    class ConfigObject {
    private:
        char            buffer[BUFFER_SIZE]{0};
        nlohmann::json* m_json = reinterpret_cast<nlohmann::json*>(buffer);

    public:
        virtual constexpr std::string_view getKey() = 0;

        explicit ConfigObject(nlohmann::json& data) {
                std::copy(data.begin(), data.end(), m_json->begin());
        };

        template<typename T>
        ConfigField<T> allocate(ConfigField<T>& field) {
            if (!m_json->contains(field.key().c_str())) {
                assert(m_json->size() + sizeof(field) < BUFFER_SIZE);
                m_json->at(field.key().c_str()) = field.value();

                // ConfigField is immutable, so return a new object
                return ConfigField<T>{field.value(), field.key(), field.restartType()};
            } else {
                return get(field);
            }
        };

        void updateConfig(nlohmann::json& json) {
            for (auto field = json.begin(); field != json.end(); ++field) {
                m_json->at(field.key()) = field.value();
            }
        }

        /**
         * @brief Gets value for field from internal json buffer
         * @tparam  T Type of ConfigField
         * @param   field Fully initialised ConfigField with default value
         * @return  New ConfigField<T> with value from ConfigObject json storage
         * @warning If the field is not present yet the returned ConfigField will have
         *          an identical value to the input ConfigField
         */
        template<typename T>
        ConfigField<T> get(ConfigField<T>& field) {
            T retrieved_value = m_json->value(field.key().c_str(), field.value());
            return ConfigField<T>{retrieved_value, field.key(), field.restartType()};
        }

        template<typename T>
        std::error_code put(ConfigKey& key, T& value) {
        }
    };

    class RandomConfig : public ConfigObject<500> {
        using Base = ConfigObject<500>;

        ConfigField<int> fieldA{1, "fieldA", RestartType::NONE};

        explicit RandomConfig(nlohmann::json& data) : Base(data) {
            fieldA = allocate(fieldA);
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

        esp_err_t initialize() {

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
            return err;
        }

        template<typename T>
        esp_err_t getItemSize(const ConfigKey key, T& item, size_t& size) {
            assert(m_handle != nullptr && "Call initialize() first");
            return m_handle->get_item_size(item, key.c_str(), size);
        }

        template<typename T>
        esp_err_t loadItem(const ConfigKey key, T& item) {
            assert(m_handle != nullptr && "Call initialize() first");
            return m_handle->get_item(key.c_str(), item);
        }

        template<typename T>
        esp_err_t saveItem(const ConfigKey key, T& item, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to save if NVS is opened in READONLY mode");
            esp_err_t err = m_handle->set_item(key.c_str(), item);
            if (err != ESP_OK) {
                return err;
            }
            if (commit) {
                return this->commit();
            }
            return err;
        }

        template<size_t BUFFER_SIZE = 1024>
        esp_err_t loadJSON(ConfigKey key, nlohmann::json& json) {
            etl::string<BUFFER_SIZE> buffer;
            auto err = loadItem(key, buffer);
            if (err != ESP_OK) {
                return err;
            }

            if (nlohmann::json::accept(buffer.c_str())) {
                json = nlohmann::json::parse(buffer.c_str(), nullptr, false);
            } else {
                return ESP_ERR_INVALID_ARG;
            }
        }

        template<size_t BUFFER_SIZE = 1024>
        esp_err_t saveJSON(ConfigKey key, nlohmann::json& json) {
            etl::string<BUFFER_SIZE> buffer(json.dump());
            auto err = saveItem(key, buffer.c_str());
        }

        template<typename T>
        esp_err_t saveConfig(T& config, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to save if NVS is opened in READONLY mode");
            static_assert(std::is_convertible<T*, ConfigObject*>::value, "Config object must inherit sdk::ConfigObject as public");

            esp_err_t err = m_handle->set_blob(config.getKey().data(), static_cast<const void*>(&config), sizeof(config));
            if (err != ESP_OK) {
                return err;
            }
            ESP_LOGD(TAG, "Saved config: %s with size %d", config.getKey().data(), sizeof(config));
            if (commit) {
                return this->commit();
            }
            return err;
        }

        template<typename T>
        esp_err_t loadConfig(T& config) {
            assert(m_handle != nullptr && "Call initialize() first");
            static_assert(std::is_convertible<T*, ConfigObject*>::value, "Config object must inherit sdk::ConfigObject as public");

            size_t storedConfigSize = 0;

            esp_err_t err = m_handle->get_item_size(nvs::ItemType::BLOB, config.getKey().data(), storedConfigSize);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error retrieving size for %s: %s", config.getKey().data(), esp_err_to_name(err));
                return err;
            }
            assert(!(sizeof(T) < storedConfigSize) && "Removing struct members is illegal");
            return m_handle->get_blob(config.getKey().data(), static_cast<void*>(&config), storedConfigSize);
        }

        esp_err_t eraseItem(const etl::string<NVS_KEY_NAME_MAX_SIZE>& key, bool commit = true) {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to erase if NVS is opened in READONLY mode");
            auto err = m_handle->erase_item(key.c_str());
            if (err != ESP_OK) {
                return err;
            }
            if (commit) {
                return m_handle->commit();
            }
            return err;
        }

        esp_err_t erase() {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to erase if NVS is opened in READONLY mode");
            return m_handle->erase_all();
        }
        esp_err_t commit() {
            assert(m_handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to commit if NVS is opened in READONLY mode");
            return m_handle->commit();
        }
    };

} // namespace sdk

#endif // CONFIG_PROVIDER_HPP
