#include "unity.h"
#include "fsm.h"
#include "input_integrator.h" // For integrated_event_t
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "project_config.h" // For FSM_STACK_SIZE etc.

static const char* TAG = "test_fsm";
static QueueHandle_t test_integrated_event_queue;

// Test setup: create the event queue
void setUp(void) {
    // Create a new queue for each test to ensure independence
    // Max 10 events for test purposes
    test_integrated_event_queue = xQueueCreate(10, sizeof(integrated_event_t));
    TEST_ASSERT_NOT_NULL(test_integrated_event_queue);
    ESP_LOGI(TAG, "Test event queue created.");
    // Ensure FSM is de-initialized before each test if it's stateful across tests
    // (fsm_init checks fsm_ctx.initialized, so this should be fine)
    // However, fsm_deinit also cleans up global/static things like led_controller,
    // which might be important for re-initialization.
    fsm_deinit(); // Clean up from any previous test, especially LED controller
}

// Test teardown: delete the event queue
void tearDown(void) {
    if (fsm_is_running()) { // Deinit FSM if the test didn't explicitly do it
        ESP_LOGW(TAG, "FSM was still running at teardown. Deinitializing.");
        TEST_ASSERT_EQUAL(ESP_OK, fsm_deinit());
    }
    if (test_integrated_event_queue != NULL) {
        vQueueDelete(test_integrated_event_queue);
        test_integrated_event_queue = NULL; // Prevent double deletion
        ESP_LOGI(TAG, "Test event queue deleted.");
    } else {
        ESP_LOGW(TAG, "Test event queue was already null at teardown.");
    }
     // A small delay to allow tasks cleaned up by fsm_deinit (like fsm_task, led_controller resources)
    // to fully release, especially if tests run very quickly back-to-back.
    vTaskDelay(pdMS_TO_TICKS(10));
}

TEST_CASE("fsm_init_deinit_basic_state_check", "[fsm]")
{
    ESP_LOGI(TAG, "Running test: fsm_init_deinit_basic_state_check");

    // Use default FSM config (NULL)
    esp_err_t ret = fsm_init(test_integrated_event_queue, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ret, "fsm_init failed");

    TEST_ASSERT_TRUE_MESSAGE(fsm_is_running(), "FSM should be running after init");
    TEST_ASSERT_EQUAL_MESSAGE(MODE_DISPLAY, fsm_get_current_state(), "Default initial state should be MODE_DISPLAY");

    // Check default effect and properties
    // These depend on led_effects and led_controller initialization within fsm_init
    TEST_ASSERT_EQUAL_MESSAGE(0, fsm_get_current_effect(), "Default effect ID should be 0"); // "Static Color"
    TEST_ASSERT_TRUE_MESSAGE(fsm_is_led_strip_on(), "LED strip should be ON by default");

    // Default brightness is initialized to 128 in fsm_ctx,
    // then fsm_apply_system_settings() is called which reads system_params_array[1] (Def Brightness = 128)
    // and applies it via led_controller_set_brightness().
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(128, fsm_get_global_brightness(), "Default global brightness should be 128");

    ret = fsm_deinit();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ret, "fsm_deinit failed");
    TEST_ASSERT_FALSE_MESSAGE(fsm_is_running(), "FSM should not be running after deinit");
}

TEST_CASE("fsm_init_with_null_queue_fails", "[fsm]")
{
    ESP_LOGI(TAG, "Running test: fsm_init_with_null_queue_fails");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, fsm_init(NULL, NULL));
}

TEST_CASE("fsm_force_state_transitions", "[fsm]")
{
    ESP_LOGI(TAG, "Running test: fsm_force_state_transitions");

    // Initialize FSM with default config
    esp_err_t init_ret = fsm_init(test_integrated_event_queue, NULL);
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, init_ret, "fsm_init failed for force_state tests");
    TEST_ASSERT_TRUE_MESSAGE(fsm_is_running(), "FSM should be running for force_state tests");

    // Test valid state transitions
    TEST_ASSERT_EQUAL(ESP_OK, fsm_force_state(MODE_EFFECT_SELECT));
    TEST_ASSERT_EQUAL(MODE_EFFECT_SELECT, fsm_get_current_state());

    TEST_ASSERT_EQUAL(ESP_OK, fsm_force_state(MODE_EFFECT_SETUP));
    TEST_ASSERT_EQUAL(MODE_EFFECT_SETUP, fsm_get_current_state());

    TEST_ASSERT_EQUAL(ESP_OK, fsm_force_state(MODE_SYSTEM_SETUP));
    TEST_ASSERT_EQUAL(MODE_SYSTEM_SETUP, fsm_get_current_state());

    TEST_ASSERT_EQUAL(ESP_OK, fsm_force_state(MODE_DISPLAY));
    TEST_ASSERT_EQUAL(MODE_DISPLAY, fsm_get_current_state());

    // Test transition to an invalid state value
    // Assuming MODE_SYSTEM_SETUP is the highest valid enum value before any potential future additions
    int invalid_state_value = (int)MODE_SYSTEM_SETUP + 10;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, fsm_force_state((fsm_state_t)invalid_state_value));
    TEST_ASSERT_EQUAL(MODE_DISPLAY, fsm_get_current_state()); // Should remain in previous valid state

    // Deinitialize FSM
    esp_err_t deinit_ret = fsm_deinit();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, deinit_ret, "fsm_deinit failed after force_state tests");
}


// This app_main is the entry point for the test application.
void app_main(void)
{
    ESP_LOGI(TAG, "Starting FSM Unit Tests...");
    UNITY_BEGIN();

    // Running tests one by one allows for better isolation if setUp/tearDown are complex
    // or if a test failure in one case might impact subsequent tests.
    // For these specific tests, unity_run_all_tests() would also likely work.
    RUN_TEST(fsm_init_deinit_basic_state_check);
    RUN_TEST(fsm_init_with_null_queue_fails);
    RUN_TEST(fsm_force_state_transitions);

    UNITY_END();
    ESP_LOGI(TAG, "FSM Unit Tests Finished.");
}

// Notes for on-target testing:
// 1. `fsm_init` calls `led_controller_init` which tries to initialize SPI for the LED strip.
//    If the GPIOs defined in `project_config.h` (used by `led_controller_config_t` within `fsm_init`)
//    are not valid or available on the test hardware, `led_controller_init` (and thus `fsm_init`) might fail.
//    This would cause `fsm_init_deinit_basic_state_check` and `fsm_force_state_transitions` to fail their init assertions.
//    Consider using dummy GPIOs for tests if actual hardware interaction is not desired,
//    though this would require `led_controller` to be tolerant of this (e.g. by not crashing).
//    Currently, `led_strip_new_spi_device` would likely return an error if GPIOs are invalid.
// 2. These tests do not send events to the FSM queue, so the `fsm_task` itself is not directly tested for event processing.
//    They focus on the synchronous API functions of the FSM and its initialization/deinitialization lifecycle.
// 3. `fsm_deinit` deletes the `fsm_task`. `vTaskDelay` in `tearDown` gives a moment for FreeRTOS to clean up.
// 4. `project_config.h` must define `FSM_STACK_SIZE`, `FSM_PRIORITY`, `FSM_TIMEOUT_MS`, `FSM_MODE_TIMEOUT_MS`
//    and LED strip related GPIOs/configs for `fsm_init` to use default config correctly.
//    If these are not defined, compilation might fail or default values in fsm.c might be used.
//    The test uses `fsm_init(..., NULL)` which relies on these defaults.
//
// To make these tests more robust unit tests (rather than integration tests):
// - Mock `led_controller` functions.
// - Mock `led_effects` functions (though they are simple getters, less critical to mock).
// - Provide a way to inject events into the FSM and check resulting states/actions without relying on real hardware.
// This is beyond the current scope of "basic unit test".
