#include "unity.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../../manager/include/manager.hpp"
#include "../../mock_component/include/mock_component.hpp"

sdk::manager& m = sdk::manager::instance();
sdk::mock_component test_component;

// Repeated for each test
void setUp() {
    m.start();
}

// Repeated after each test
void tearDown() {
    sdk::manager::instance().stop();
    usleep(200);
    test_component.reset();
}

void test_instance_should_always_return_same_reference() {
    sdk::manager& ref_first = sdk::manager::instance();
    sdk::manager& ref_second = sdk::manager::instance();

    TEST_ASSERT_TRUE(&ref_first == &ref_second);
}

void test_added_component_should_be_initialized_and_ran() {
    usleep(200);

    TEST_ASSERT_TRUE(test_component.initialize_called());
    TEST_ASSERT_TRUE(test_component.run_called());
}

void test_component_should_be_restarted_after_run_error() {
    // Make sure thread is running
    while (!test_component.initialize_called()) {};
    while (!test_component.run_called()) {};

    // We need to be able to make sure a restart was done
    test_component.set_initialize_return({
        .status = ESP_OK,
        .message = "",
        .called = false
    });

    test_component.set_run_return({
        .status = ESP_ERR_INVALID_STATE,
        .message = "forced error", 
        .called = false
    });

    sleep(1);

    while (!test_component.run_called()) {}

    test_component.set_run_return({
        .status = ESP_OK,
        .message = "", 
        .called = false
    });

    sleep(1);

    TEST_ASSERT_TRUE_MESSAGE(test_component.initialize_called(), "initialize not called");
    TEST_ASSERT_TRUE_MESSAGE(test_component.run_called(), "run not called");
}

void test_component_should_be_disabled_after_stop_error() {
    // Trigger call to restart_component
    test_component.set_run_return({
        .status = ESP_ERR_INVALID_STATE,
        .message = "forced error", 
        .called = false
    });

    test_component.set_stop_return({
        .status = ESP_ERR_INVALID_STATE,
        .message = "forced error",
        .called = false
    });

    // Make sure we're inside restart_component() and reset run status
    while (!test_component.stop_called()) {}

    test_component.set_run_return({
        .status = ESP_OK,
        .message = "", 
        .called = false
    });

    usleep(200);

    TEST_ASSERT_FALSE_MESSAGE(test_component.run_called(), "didn't expect run to get called");
}

void test_component_should_be_disabled_after_initialize_error() {
    m.stop();
    sleep(1);
    
    test_component.set_initialize_return({
        .status = ESP_ERR_INVALID_STATE,
        .message = "forced error",
        .called = false
    });

    m.start();
    sleep(1);

    TEST_ASSERT_FALSE(test_component.run_called());
}

void test_component_should_be_disabled_after_initialize_error_in_restart() {
    // Trigger call to restart_component
    test_component.set_run_return({
        .status = ESP_ERR_INVALID_STATE,
        .message = "forced error", 
        .called = false
    });

    test_component.set_initialize_return({
        .status = ESP_ERR_INVALID_STATE,
        .message = "forced error",
        .called = false
    });

    // Make sure we're inside restart_component() and reset run status
    while (!test_component.stop_called()) {}

    test_component.set_run_return({
        .status = ESP_OK,
        .message = "", 
        .called = false
    });

    sleep(1);

    TEST_ASSERT_FALSE(test_component.run_called());
}

extern "C" {

int app_main(void) {
    m.add_component(test_component);

    UNITY_BEGIN();

    RUN_TEST(test_instance_should_always_return_same_reference);
    RUN_TEST(test_added_component_should_be_initialized_and_ran);
    RUN_TEST(test_component_should_be_restarted_after_run_error);
    RUN_TEST(test_component_should_be_disabled_after_stop_error);
    RUN_TEST(test_component_should_be_disabled_after_initialize_error);
    RUN_TEST(test_component_should_be_disabled_after_initialize_error_in_restart);

    return UNITY_END();
}

} /* Extern "C" */