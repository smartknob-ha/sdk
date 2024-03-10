#ifndef CONFIG_PROVIDER_HPP
#define CONFIG_PROVIDER_HPP

#include <esp_log.h>
#include <etl/string.h>

#include <cstring>

#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_handle.hpp"

namespace sdk {

    class ConfigObject {
    public:
        virtual constexpr std::string_view getKey() = 0;
    };

    class ConfigProvider {
    private:
        static inline const char* TAG = "CONFIG";

        static inline bool initialized = false;

        const etl::string<NVS_NS_NAME_MAX_SIZE> nvsNamespace;

        const bool readOnly;

        std::unique_ptr<nvs::NVSHandle> handle;

    public:
        ConfigProvider(const etl::string<NVS_NS_NAME_MAX_SIZE>& nvsNamespace, const bool readOnly = true) : nvsNamespace(nvsNamespace), readOnly(readOnly){};

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
        esp_err_t getItemSize(const etl::string<NVS_NS_NAME_MAX_SIZE>& key, T& item, size_t& size) {
            assert(handle != nullptr && "Call initialize() first");
            return handle->get_item_size(item, key.c_str(), size);
        }

        template<typename T>
        esp_err_t loadItem(const etl::string<NVS_NS_NAME_MAX_SIZE>& key, T& item) {
            assert(handle != nullptr && "Call initialize() first");
            return handle->get_item(key.c_str(), item);
        }

        template<typename T>
        esp_err_t saveItem(const etl::string<NVS_NS_NAME_MAX_SIZE>& key, T& item, bool commit = true) {
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
