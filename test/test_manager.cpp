#include "../../manager/include/Manager.hpp"
#include "MockComponent.hpp"
#include "unity.h"
#include "../../util/include/esp_system_error.hpp"

sdk::MockComponent testComponent;

using Status = sdk::Component::Status;

// Repeated for each test
void setUp() {
    testComponent.reset();
    sdk::Manager::start();
}

// Repeated after each test
void tearDown() {
    sdk::Manager::stop();
    usleep(200);
    testComponent.reset();
}

void testAddedComponentShouldBeInitializedAndRan() {
    usleep(200);

    TEST_ASSERT_TRUE(testComponent.initialize_called());
    TEST_ASSERT_TRUE(testComponent.run_called());
}

void testComponentShouldBeRestartedAfterRunError() {
    // Make sure thread is running
    while (!testComponent.initialize_called()) {};
    while (!testComponent.run_called()) {};

    // We need to be able to make sure a restart was done
    testComponent.set_initialize_return({.status = Status::RUNNING,
                                         .called  = false});

    testComponent.set_run_return({.status = Status::ERROR,
                                  .called  = false});

    sleep(1);

    while (!testComponent.run_called()) {}

    testComponent.set_run_return({.status = Status::ERROR,
                                  .called  = false});

    sleep(1);

    TEST_ASSERT_TRUE_MESSAGE(testComponent.initialize_called(), "initialize not called");
    TEST_ASSERT_TRUE_MESSAGE(testComponent.run_called(), "run not called");
}

void testComponentShouldBeDisabledAfterStopError() {
    // First setup stop error to prevent race condition
    testComponent.set_stop_return({.status = Status::ERROR,
                                   .called  = false});

    // Trigger call to restart_component
    testComponent.set_run_return({.status = Status::ERROR,
                                  .called  = false});

    // Make sure we're inside restart_component() and reset run status
    while (!testComponent.stop_called()) {}

    testComponent.set_run_return({.status = Status::RUNNING,
                                  .called  = false});

    usleep(200);

    TEST_ASSERT_FALSE_MESSAGE(testComponent.run_called(), "didn't expect run to get called");
}

void testComponentShouldBeDisabledAfterInitializeError() {
    sdk::Manager::stop();
    sleep(1);

    testComponent.set_initialize_return({.status = Status::ERROR,
                                         .called  = false});

    // Set called to false as it may have been set to true already in the thread
    testComponent.set_run_return({.status = Status::RUNNING,
                                  .called  = false});

    sdk::Manager::start();
    sleep(1);

    TEST_ASSERT_FALSE(testComponent.run_called());
}

void testComponentShouldBeDisabledAfterInitializeErrorInRestart() {
    // First setup stop error to prevent race condition
    testComponent.set_initialize_return({.status = Status::ERROR,
                                         .called  = false});

    // Trigger call to restart_component
    testComponent.set_run_return({.status = Status::ERROR,
                                  .called  = false});

    // Make sure we're inside restart_component() and reset run status
    while (!testComponent.stop_called()) {}

    testComponent.set_run_return({.status = Status::RUNNING,
                                  .called  = false});

    sleep(1);

    TEST_ASSERT_FALSE(testComponent.run_called());
}

extern "C" {

auto app_main(void) -> int {
    sdk::Manager::addComponent(testComponent);

    UNITY_BEGIN();

    RUN_TEST(testAddedComponentShouldBeInitializedAndRan);
    RUN_TEST(testComponentShouldBeRestartedAfterRunError);
    RUN_TEST(testComponentShouldBeDisabledAfterStopError);
    RUN_TEST(testComponentShouldBeDisabledAfterInitializeError);
    RUN_TEST(testComponentShouldBeDisabledAfterInitializeErrorInRestart);

    return UNITY_END();
}

} /* Extern "C" */