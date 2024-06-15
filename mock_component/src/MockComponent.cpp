#include "../include/MockComponent.hpp"

namespace sdk {
    using Status = Component::Status;
    using res    = Component::res;

    Status MockComponent::getStatus() {
        m_statusReturn.called = true;

        return m_statusReturn.status;
    }

    Status MockComponent::initialize() {
        m_initializeReturn.called = true;

        return m_initializeReturn.status;
    }

    Status MockComponent::run() {
        m_runReturn.called = true;

        return m_runReturn.status;
    }

    Status MockComponent::stop() {
        m_stopReturn.called = true;

        return m_stopReturn.status;
    }

    void MockComponent::reset() {
        m_statusReturn     = {.status = Status::RUNNING, .called = false};
        m_initializeReturn = {.status = Status::RUNNING, .called = false};
        m_runReturn        = {.status = Status::RUNNING, .called = false};
        m_stopReturn       = {.status = Status::RUNNING, .called = false};
    }

} // namespace sdk
