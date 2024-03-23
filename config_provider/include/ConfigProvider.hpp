#ifndef CONFIG_PROVIDER_HPP
#define CONFIG_PROVIDER_HPP

#include <esp_log.h>
#include <etl/string.h>
#include <etl/map.h>

#include <any>
#include <cstring>

#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_handle.hpp"

#include <any>
#include <typeindex>
#include <nlohmann/json.hpp>

// todo fix
namespace sdk {

    using ConfigKey = etl::string<NVS_NS_NAME_MAX_SIZE>;

    template<typename T>
    class NvsField {
    public:
        T m_data;
    };

    template<typename T>
    struct ConfigField : public NvsField<T> {
    private:
        const ConfigKey m_key;

        const bool m_componentRestartRequired{false};
        const bool m_rebootRequired{false};

        const std::type_index m_type{std::type_identity<T>()};

    public:
        T& value() { return NvsField<T>::m_data; };

        ConfigField(std::any defaultValue, const ConfigKey key, bool componentRestartRequired = false, bool rebootRequired = false)
            : NvsField<T>(std::any_cast<T>(defaultValue)),
              m_key(key),
              m_componentRestartRequired(componentRestartRequired),
              m_rebootRequired(rebootRequired),
              m_type(typeid(T)){};

        ConfigField<T>& operator=(NvsField<T> const& in) {
            this->m_data = in.m_data;
            return *this;


            //            if (this != &in) {
            //                std::destroy_at(NvsField<T>::m_data);
            //                std::construct_at(NvsField<T>::m_data, in);
            //            }
            //            return *this;
        }

        ConfigField() = delete;

        ConfigField<T>& operator=(ConfigField<T> const& in) {
            if (this != &in) {
                std::destroy_at(this);
                std::construct_at(this, in);
            }
            return *this;
        }

        bool get_componentRestartRequired() const {
            return m_componentRestartRequired;
        }

        bool get_rebootRequired() const {
            return m_rebootRequired;
        }

        ConfigKey get_key() const {
            return m_key;
        }

        std::type_index get_type() const {
            return m_type;
        }
    };

    template<size_t NUM_FIELDS>
    class ConfigObject {
    private:
        etl::map<ConfigKey, void*, NUM_FIELDS> m_storedFields;

    public:
        virtual constexpr std::string_view getKey() = 0;

        template<typename T>
        ConfigField<T>& allocate(ConfigField<T>& field) {
            m_storedFields[field.get_key()] = static_cast<void*>(&field);
            return field;
        };

        void fromJson(nlohmann::json& data) {
//            fileContets >> data;

            for (auto& a : m_storedFields) {

                static_cast<NvsField<std::any>>(a.second).m_data = data.at(a.first);
            }

        }
    };

    template<typename T>
    NvsField<T> load(const ConfigKey key) {
        return NvsField<T>{};
    };

    template<typename T>
    void store(NvsField<T>) {
    };


    class RandomConfig : public ConfigObject<5> {
        using Base = ConfigObject<5>;

        inline static int test = 1;

        ConfigField<int> fieldA {test, "fieldA"};

        RandomConfig() :
            fieldA(Base::allocate(fieldA)) {
                fieldA = load<int>(fieldA.get_key());
        };

//        static auto fieldB = allocate(ConfigField<SubConfig>{SubConfig, "fieldA"});
    };



    class ConfigProvider {
    private:
        static inline const char* TAG = "CONFIG";

        static inline bool initialized = false;

        const ConfigKey nvsNamespace;

        const bool readOnly;

        std::unique_ptr<nvs::NVSHandle> handle;

    public:
        ConfigProvider(const ConfigKey nvsNamespace, const bool readOnly = true) : nvsNamespace(nvsNamespace), readOnly(readOnly){};

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

            handle = nvs::open_nvs_handle(nvsNamespace.c_str(), nvsMode, &err);
            return err;
        }

        template<typename T>
        esp_err_t getItemSize(const ConfigKey key, T& item, size_t& size) {
            assert(handle != nullptr && "Call initialize() first");
            return handle->get_item_size(item, key.c_str(), size);
        }

        template<typename T>
        esp_err_t loadItem(const ConfigKey key, T& item) {
            assert(handle != nullptr && "Call initialize() first");
            return handle->get_item(key.c_str(), item);
        }

        template<typename T>
        esp_err_t saveItem(const ConfigKey key, T& item, bool commit = true) {
            assert(handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to save if NVS is opened in READONLY mode");
            esp_err_t err = handle->set_item(key.c_str(), item);
            if (err != ESP_OK) {
                return err;
            }
            if (commit) {
                return this->commit();
            }
            return err;
        }

        template<typename T>
        esp_err_t saveConfig(T& config, bool commit = true) {
            assert(handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to save if NVS is opened in READONLY mode");
            static_assert(std::is_convertible<T*, ConfigObject*>::value, "Config object must inherit sdk::ConfigObject as public");

            esp_err_t err = handle->set_blob(config.getKey().data(), static_cast<const void*>(&config), sizeof(config));
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
            assert(handle != nullptr && "Call initialize() first");
            static_assert(std::is_convertible<T*, ConfigObject*>::value, "Config object must inherit sdk::ConfigObject as public");

            size_t storedConfigSize = 0;

            esp_err_t err = handle->get_item_size(nvs::ItemType::BLOB, config.getKey().data(), storedConfigSize);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error retrieving size for %s: %s", config.getKey().data(), esp_err_to_name(err));
                return err;
            }
            assert(!(sizeof(T) < storedConfigSize) && "Removing struct members is illegal");
            return handle->get_blob(config.getKey().data(), static_cast<void*>(&config), storedConfigSize);
        }

        esp_err_t eraseItem(const etl::string<NVS_KEY_NAME_MAX_SIZE>& key, bool commit = true) {
            assert(handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to erase if NVS is opened in READONLY mode");
            auto err = handle->erase_item(key.c_str());
            if (err != ESP_OK) {
                return err;
            }
            if (commit) {
                return handle->commit();
            }
            return err;
        }

        esp_err_t erase() {
            assert(handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to erase if NVS is opened in READONLY mode");
            return handle->erase_all();
        }
        esp_err_t commit() {
            assert(handle != nullptr && "Call initialize() first");
            assert(!readOnly && "Unable to commit if NVS is opened in READONLY mode");
            return handle->commit();
        }
    };

} // namespace sdk

#endif // CONFIG_PROVIDER_HPP
