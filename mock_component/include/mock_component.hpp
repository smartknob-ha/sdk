#ifndef MOCK_COMPONENT_HPP
#define MOCK_COMPONENT_HPP

#include "../../manager/include/component.hpp"

namespace sdk {

/***
 * @brief   This class can be used to test the component manager
*/
class mock_component : public component {
    public:
        mock_component() {};
        ~mock_component() = default;

        using res = Result<esp_err_t, etl::string<128>>;

        struct mock_result {
            esp_err_t status;
            etl::string<128> message;
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

    private:
        static const inline char TAG[] = "mock_result component";

        mock_result m_status_return { .status = ESP_OK, .message = "" };
        mock_result m_initialize_return { .status = ESP_OK, .message = "" };
        mock_result m_run_return { .status = ESP_OK, .message = "" };
        mock_result m_stop_return { .status = ESP_OK, .message = "" };


};

} /* namespace sdk */

#endif /* MOCK_COMPONENT_HPP */