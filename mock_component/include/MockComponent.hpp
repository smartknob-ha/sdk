#ifndef MOCK_COMPONENT_HPP
#define MOCK_COMPONENT_HPP

#include <etl/message_router.h>
#include <etl/message_router_registry.h>

#include "Component.hpp"

namespace sdk {

    struct MockMessage {
        uint32_t data;
    };

    /***
     * @brief   This class can be used to test the component manager
     */
    class MockComponent : public Component, public HasQueue<10, MockMessage, 0> {
    public:
        MockComponent(){};
        ~MockComponent() = default;

        struct MockResult {
            bool             ok;
            etl::string<128> message;
            bool             called;
        };

        /* Component override functions */
        virtual etl::string<50> getTag() override { return TAG; };
        virtual res             getStatus() override;
        virtual res             initialize() override;
        virtual res             run() override;
        virtual res             stop() override;

        /* Testing functions */
        void set_status(MockResult value) { m_statusReturn = value; };
        void set_initialize_return(MockResult value) { m_initializeReturn = value; };
        void set_run_return(MockResult value) { m_runReturn = value; };
        void set_stop_return(MockResult value) { m_stopReturn = value; };

        bool get_status_called() { return m_statusReturn.called; };
        bool initialize_called() { return m_initializeReturn.called; };
        bool run_called() { return m_runReturn.called; };
        bool stop_called() { return m_stopReturn.called; };

        void reset();

    private:
        static const inline char TAG[] = "MockResult component";

        MockResult m_statusReturn{.ok = true, .message = "", .called = false};
        MockResult m_initializeReturn{.ok = true, .message = "", .called = false};
        MockResult m_runReturn{.ok = true, .message = "", .called = false};
        MockResult m_stopReturn{.ok = true, .message = "", .called = false};
    };

} // namespace sdk

#endif /* MOCK_COMPONENT_HPP */