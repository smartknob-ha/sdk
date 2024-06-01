#include "../include/MockComponent.hpp"

namespace sdk {

    res MockComponent::getStatus() {
        m_statusReturn.called = true;

        if (!m_statusReturn.ok) {
            return std::unexpected(m_statusReturn.error);
        }

        return ComponentStatus::RUNNING;
    }

    res MockComponent::initialize() {
        m_initializeReturn.called = true;

        if (!m_initializeReturn.ok) {
            return std::unexpected(m_initializeReturn.error);
        }

        return ComponentStatus::RUNNING;
    }

    res MockComponent::run() {
        m_runReturn.called = true;

        if (!m_runReturn.ok) {
            return std::unexpected(m_initializeReturn.error);
        }

        return ComponentStatus::RUNNING;
    }

    res MockComponent::stop() {
        m_stopReturn.called = true;

        if (!m_stopReturn.ok) {
            return std::unexpected(m_stopReturn.error);
        }

        return ComponentStatus::RUNNING;
    }

    void MockComponent::reset() {
        m_statusReturn     = {.ok = true, .error = {}, .called = false};
        m_initializeReturn = {.ok = true, .error = {}, .called = false};
        m_runReturn        = {.ok = true, .error = {}, .called = false};
        m_stopReturn       = {.ok = true, .error = {}, .called = false};
    }

} // namespace sdk
