#ifndef ESP_SYSTEM_ERROR_HPP
#define ESP_SYSTEM_ERROR_HPP

#include <system_error>

#include "esp_err.h"

namespace sdk {

    class EspErrorCategory : public std::error_category {
    public:
        const char* name() const noexcept override {
            return "esp_err_t";
        }

        std::string message(int ev) const override {
            return {esp_err_to_name(static_cast<esp_err_t>(ev))};
        }
    };

} // namespace sdk


namespace std {
    template<>
    struct is_error_code_enum<sdk::EspErrorCategory> : true_type {};

    std::error_code make_error_code(esp_err_t e);
} // namespace std

#endif // ESP_SYSTEM_ERROR_HPP
