#include "../../manager/include/Manager.hpp"
#include "MockComponent.hpp"
#include "unity.h"

sdk::Manager& m = sdk::Manager::instance();

sdk::MockComponent testComponent;

// Repeated for each test
void setUp() {
    m.start();
}

// Repeated after each test
void tearDown() {
    sdk::Manager::instance().stop();
    usleep(200);
    testComponent.reset();
}

void testInstanceShouldAlwaysReturnSameReference() {
    sdk::Manager& refFirst  = sdk::Manager::instance();
    sdk::Manager& refSecond = sdk::Manager::instance();

    TEST_ASSERT_TRUE(&refFirst == &refSecond);
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
    testComponent.set_initialize_return({.ok      = true,
                                         .message = "",
                                         .called  = false});

    testComponent.set_run_return({.ok      = false,
                                  .message = "forced error",
                                  .called  = false});

    sleep(1);

    while (!testComponent.run_called()) {}

    testComponent.set_run_return({.ok      = true,
                                  .message = "",
                                  .called  = false});

    sleep(1);

    TEST_ASSERT_TRUE_MESSAGE(testComponent.initialize_called(), "initialize not called");
    TEST_ASSERT_TRUE_MESSAGE(testComponent.run_called(), "run not called");
}

void testComponentShouldBeDisabledAfterStopError() {
    // Trigger call to restart_component
    testComponent.set_run_return({.ok      = false,
                                  .message = "forced error",
                                  .called  = false});

    testComponent.set_stop_return({.ok      = false,
                                   .message = "forced error",
                                   .called  = false});

    // Make sure we're inside restart_component() and reset run status
    while (!testComponent.stop_called()) {}

    testComponent.set_run_return({.ok      = true,
                                  .message = "",
                                  .called  = false});

    usleep(200);

    TEST_ASSERT_FALSE_MESSAGE(testComponent.run_called(), "didn't expect run to get called");
}

void testComponentShouldBeDisabledAfterInitializeError() {
    m.stop();
    sleep(1);

    testComponent.set_initialize_return({.ok      = false,
                                         .message = "forced error",
                                         .called  = false});

    m.start();
    sleep(1);

    TEST_ASSERT_FALSE(testComponent.run_called());
}

void testComponentShouldBeDisabledAfterInitializeErrorInRestart() {
    // Trigger call to restart_component
    testComponent.set_run_return({.ok      = false,
                                  .message = "forced error",
                                  .called  = false});

    testComponent.set_initialize_return({.ok      = false,
                                         .message = "forced error",
                                         .called  = false});

    // Make sure we're inside restart_component() and reset run status
    while (!testComponent.stop_called()) {}

    testComponent.set_run_return({.ok      = true,
                                  .message = "",
                                  .called  = false});

    sleep(1);

    TEST_ASSERT_FALSE(testComponent.run_called());
}

extern "C" {

auto app_main(void) -> int {
    m.addComponent(testComponent);

    UNITY_BEGIN();

    RUN_TEST(testInstanceShouldAlwaysReturnSameReference);
    RUN_TEST(testAddedComponentShouldBeInitializedAndRan);
    RUN_TEST(testComponentShouldBeRestartedAfterRunError);
    RUN_TEST(testComponentShouldBeDisabledAfterStopError);
    RUN_TEST(testComponentShouldBeDisabledAfterInitializeError);
    RUN_TEST(testComponentShouldBeDisabledAfterInitializeErrorInRestart);

    return UNITY_END();
}

} /* Extern "C" */