#include "../include/mock_component.hpp"

namespace sdk {

using res = Result<esp_err_t, etl::string<128>>;

res mock_component::get_status() {
    if (m_status_return.status) {
        return Err(m_status_return.message);
    }

    return Ok(m_status_return.status);
}

res mock_component::initialize() {
    if (m_initialize_return.status) {
        return Err(m_initialize_return.message);
    }

    return Ok(m_initialize_return.status);
}

res mock_component::run() {
    if (m_run_return.status) {
        return Err(m_initialize_return.message);
    }

    return Ok(m_run_return.status);
}

res mock_component::stop() {
    if (m_stop_return.status) {
        return Err(m_stop_return.message);
    }

    return Ok(m_stop_return.status);
}

} /* namespace sdk */
