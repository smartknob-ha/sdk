#include "../include/mock_component.hpp"

namespace sdk {

res mock_component::get_status() {
    m_status_return.called = true;

    if (m_status_return.status) {
        return Err(m_status_return.message);
    }

    return Ok(m_status_return.status);
}

res mock_component::initialize() {
    m_initialize_return.called = true;

    if (m_initialize_return.status) {
        return Err(m_initialize_return.message);
    }

    return Ok(m_initialize_return.status);
}

res mock_component::run() {
    m_run_return.called = true;

    if (m_run_return.status) {
        return Err(m_initialize_return.message);
    }

    return Ok(m_run_return.status);
}

res mock_component::stop() {
    m_stop_return.called = true;

    if (m_stop_return.status) {
        return Err(m_stop_return.message);
    }

    return Ok(m_stop_return.status);
}

void mock_component::reset() {
    m_status_return = { .status = COMPONENT_STATUS::UNINITIALIZED, .message = "", .called = false };
    m_initialize_return = { .status = COMPONENT_STATUS::UNINITIALIZED, .message = "", .called = false };
    m_run_return = { .status = COMPONENT_STATUS::UNINITIALIZED, .message = "", .called = false };
    m_stop_return = { .status = COMPONENT_STATUS::UNINITIALIZED, .message = "", .called = false };
}

} /* namespace sdk */
