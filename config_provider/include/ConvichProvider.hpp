#ifndef FIRMWARE_LIB_SDK_CONFIG_PROVIDER_INCLUDE_CONVICHPROVIDER_HPP
#define FIRMWARE_LIB_SDK_CONFIG_PROVIDER_INCLUDE_CONVICHPROVIDER_HPP

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
#include <iostream>

enum class RestartType {
    NONE,
    COMPONENT,
    DEVICE
};

using ConfigKey = etl::string<NVS_NS_NAME_MAX_SIZE>;
using json = nlohmann::json;

struct SettingBase {};

template<typename T>
struct Setting {
    T value;
    RestartType restartType {RestartType::NONE};
};

template<size_t NUM_FIELDS>
class ConfigObject {
private:
    etl::map<ConfigKey, json, NUM_FIELDS> m_settings;
//    etl::map<ConfigKey, SettingBase*, NUM_FIELDS> m_settings;

public:

//    template<typename T>
//    const Setting<T>& addSetting(ConfigKey key, const Setting<T> &setting) {
//        m_settings.assign(key, dynamic_cast<SettingBase>(setting));
//        return setting;
//    }

    template<typename T>
    const Setting<T>& addSetting(ConfigKey key, Setting<T> ) {
        m_settings.assign(key, dynamic_cast<SettingBase>(setting));
        return setting;
    }

    json toJson() const {
        json j;
        for (const auto& setting : m_settings) {
            j[setting.first.c_str()] = setting.second;
        }
        return j;
    }

    void fromJson(const json& j) {

    }
};

class RandomSettings : public ConfigObject<4> {



    Setting<int> a = addSetting("a", Setting<int>{3, RestartType::NONE});
    Setting<float> b = addSetting("b", Setting<float>{0.3f, RestartType::DEVICE});
    Setting<bool> c = addSetting("c", Setting<bool>{true, RestartType::COMPONENT});
    Setting<etl::string<10>> d = addSetting("d", Setting<etl::string<10>>{"0123456789", RestartType::COMPONENT});
};


void functie() {
    RandomSettings testSettings;


    json test = testSettings.toJson();









}

#endif // FIRMWARE_LIB_SDK_CONFIG_PROVIDER_INCLUDE_CONVICHPROVIDER_HPP
