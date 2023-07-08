#include "../include/mock_component.hpp"

namespace sdk {

res mock_component::get_status() {
    m_status_return.called = true;

    if (m_status_return.ok) {
        return Err(m_status_return.message);
    }

    return Ok(COMPONENT_STATUS::RUNNING);
}

res mock_component::initialize() {
    m_initialize_return.called = true;

    if (m_initialize_return.ok) {
        return Err(m_initialize_return.message);
    }

    return Ok(COMPONENT_STATUS::RUNNING);
}

res mock_component::run() {
    m_run_return.called = true;

    if (m_run_return.ok) {
        return Err(m_initialize_return.message);
    }

    return Ok(COMPONENT_STATUS::RUNNING);
}

res mock_component::stop() {
    m_stop_return.called = true;

    if (m_stop_return.ok) {
        return Err(m_stop_return.message);
    }

    return Ok(COMPONENT_STATUS::RUNNING);
}

void mock_component::reset() {
    m_status_return = { .ok = true, .message = "", .called = false };
    m_initialize_return = { .ok = true, .message = "", .called = false };
    m_run_return = { .ok = true, .message = "", .called = false };
    m_stop_return = { .ok = true, .message = "", .called = false };
}

} /* namespace sdk */
