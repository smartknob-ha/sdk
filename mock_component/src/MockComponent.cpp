#include "../include/MockComponent.hpp"

namespace sdk {

    res MockComponent::getStatus() {
        m_statusReturn.called = true;

        if (m_statusReturn.ok) {
            return Err(m_statusReturn.message);
        }

        return Ok(ComponentStatus::RUNNING);
    }

    res MockComponent::initialize() {
        m_initializeReturn.called = true;

        if (m_initializeReturn.ok) {
            return Err(m_initializeReturn.message);
        }

        return Ok(ComponentStatus::RUNNING);
    }

    res MockComponent::run() {
        m_runReturn.called = true;

        if (m_runReturn.ok) {
            return Err(m_initializeReturn.message);
        }

        return Ok(ComponentStatus::RUNNING);
    }

    res MockComponent::stop() {
        m_stopReturn.called = true;

        if (m_stopReturn.ok) {
            return Err(m_stopReturn.message);
        }

        return Ok(ComponentStatus::RUNNING);
    }

    void MockComponent::reset() {
        m_statusReturn     = {.ok = true, .message = "", .called = false};
        m_initializeReturn = {.ok = true, .message = "", .called = false};
        m_runReturn        = {.ok = true, .message = "", .called = false};
        m_stopReturn       = {.ok = true, .message = "", .called = false};
    }

} // namespace sdk
