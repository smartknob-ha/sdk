#include "esp_system_error.hpp"

namespace std {
    std::error_code make_error_code(esp_err_t e) {
        static sdk::EspErrorCategory category;
        return {static_cast<int>(e), category};
    }
} // namespace std