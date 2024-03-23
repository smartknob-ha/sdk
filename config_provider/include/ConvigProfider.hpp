#ifndef FIRMWARE_LIB_SDK_CONvIG_PROVIDER_INCLUDE_CONVIGPROFIDER_HPP
#define FIRMWARE_LIB_SDK_CONvIG_PROVIDER_INCLUDE_CONVIGPROFIDER_HPP

#include "etl/map.h"
#include "etl/string.h"
#include "nvs.h"
#include "nlohmann/json.hpp"

namespace sdk {

    using ConfigKey = etl::string<NVS_NS_NAME_MAX_SIZE>;
    using json = nlohmann::json;

    template<typename T>
    struct ConfigField {
    private:
        const ConfigKey m_key;

        T m_data;

        const bool m_componentRestartRequired{false};
        const bool m_rebootRequired{false};
    public:

        ConfigField(T value, const ConfigKey key, bool componentRestartRequired = false, bool rebootRequired = false)
            : m_data(value),
              m_key(key),
              m_componentRestartRequired(componentRestartRequired),
              m_rebootRequired(rebootRequired){};
    };


    template <typename T>
    struct adl_serializer {
        static void to_json(json& j, const T& value) {
            // calls the "to_json" method in T's namespace
        }

        static void from_json(const json& j, T& value) {
            // same thing, but with the "from_json" method
        }
    };

    struct randomConfig {
        struct subConfig {
            int a;
        };
        ConfigField<int> test{1, "bruh"};
        ConfigField<float> test2{1.3F, "asdasd"};
        ConfigField<subConfig> test3{{.a = 1}, "bruh2"};
    };

    void function() {

    };

}


#endif // FIRMWARE_LIB_SDK_CONvIG_PROVIDER_INCLUDE_CONVIGPROFIDER_HPP
