#ifndef MOCK_COMPONENT_HPP
#define MOCK_COMPONENT_HPP

#include "component.hpp"

#include <etl/message_router.h>
#include <etl/message_router_registry.h>

namespace sdk {

struct mock_message {
    uint32_t data;
};

/***
 * @brief   This class can be used to test the component manager
*/
class mock_component : public component, public has_queue<10, mock_message, 0> {
    public:
        mock_component() {};
        ~mock_component() = default;

        struct mock_result {
            bool ok;
            etl::string<128> message;
            bool called;
        };

        /* Component override functions */
        virtual etl::string<50> get_tag() override { return TAG; };
        virtual res get_status() override;
        virtual res initialize() override;
        virtual res run() override;
        virtual res stop() override;

        /* Testing functions */
        void set_status(mock_result value) { m_status_return = value; };
        void set_initialize_return(mock_result value) { m_initialize_return = value; };
        void set_run_return(mock_result value) { m_run_return = value; };
        void set_stop_return(mock_result value) { m_stop_return = value; };

        bool get_status_called() { return m_status_return.called; };
        bool initialize_called() { return m_initialize_return.called; };
        bool run_called() { return m_run_return.called; };
        bool stop_called() { return m_stop_return.called; };

        void reset();

    private:
        static const inline char TAG[] = "mock_result component";

        mock_result m_status_return { .ok = true, .message = "", .called = false };
        mock_result m_initialize_return { .ok = true, .message = "", .called = false };
        mock_result m_run_return { .ok = true, .message = "", .called = false };
        mock_result m_stop_return { .ok = true, .message = "", .called = false };
};

} /* namespace sdk */

#endif /* MOCK_COMPONENT_HPP */