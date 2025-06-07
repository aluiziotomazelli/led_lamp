#include "unity.h"
#include "led_effects.h"
#include <string.h> // For strcmp
#include <stdlib.h> // For NULL
#include "esp_log.h" // For logging in tests

static const char* TAG = "test_led_effects";

// setUp and tearDown are optional, not strictly needed for these tests
// as they operate on const data.
void setUp(void) {
    // ESP_LOGI(TAG, "Setting up test...");
}

void tearDown(void) {
    // ESP_LOGI(TAG, "Tearing down test...");
}

TEST_CASE("led_effects_get_all_retrieves_effects", "[led_effects]")
{
    ESP_LOGI(TAG, "Running test: led_effects_get_all_retrieves_effects");
    uint8_t count = 0;
    const led_effect_t* effects_array = led_effects_get_all(&count);

    TEST_ASSERT_NOT_NULL(effects_array);
    TEST_ASSERT_GREATER_THAN(0, count); // Expect at least one effect defined

    // Assuming at least 2 effects are defined as per led_effects.c (Static Color, Candle)
    if (count >= 2) {
        // Test properties of the first known effect (Static Color)
        TEST_ASSERT_EQUAL_STRING("Static Color", effects_array[0].name);
        TEST_ASSERT_EQUAL(0, effects_array[0].id); // Assuming ID 0 for Static Color
        TEST_ASSERT_EQUAL(2, effects_array[0].param_count); // Hue, Saturation
        TEST_ASSERT_NOT_NULL(effects_array[0].params);
        if (effects_array[0].param_count > 0 && effects_array[0].params != NULL) {
            TEST_ASSERT_EQUAL_STRING("Hue", effects_array[0].params[0].name);
        }

        // Test properties of the second known effect (Candle)
        TEST_ASSERT_EQUAL_STRING("Candle", effects_array[1].name);
        TEST_ASSERT_EQUAL(1, effects_array[1].id); // Assuming ID 1 for Candle
        TEST_ASSERT_EQUAL(3, effects_array[1].param_count); // Base Hue, Flicker Speed, Intensity
        TEST_ASSERT_NOT_NULL(effects_array[1].params);
         if (effects_array[1].param_count > 0 && effects_array[1].params != NULL) {
            TEST_ASSERT_EQUAL_STRING("Base Hue", effects_array[1].params[0].name);
        }
    } else {
        TEST_FAIL_MESSAGE("Expected at least 2 effects to be defined for detailed testing.");
    }
}

TEST_CASE("led_effects_get_by_id_valid_ids", "[led_effects]")
{
    ESP_LOGI(TAG, "Running test: led_effects_get_by_id_valid_ids");
    const led_effect_t* effect0 = led_effects_get_by_id(0); // Assuming ID 0 is Static Color
    TEST_ASSERT_NOT_NULL(effect0);
    if (effect0) { // Guard against dereferencing NULL
        TEST_ASSERT_EQUAL_STRING("Static Color", effect0->name);
    }

    const led_effect_t* effect1 = led_effects_get_by_id(1); // Assuming ID 1 is Candle
    TEST_ASSERT_NOT_NULL(effect1);
    if (effect1) { // Guard against dereferencing NULL
        TEST_ASSERT_EQUAL_STRING("Candle", effect1->name);
    }
}

TEST_CASE("led_effects_get_by_id_invalid_id", "[led_effects]")
{
    ESP_LOGI(TAG, "Running test: led_effects_get_by_id_invalid_id");
    const led_effect_t* effect = led_effects_get_by_id(99); // Assuming 99 is not a valid ID
    TEST_ASSERT_NULL(effect);
}

TEST_CASE("led_effects_verify_parameter_details_static_color", "[led_effects]")
{
    ESP_LOGI(TAG, "Running test: led_effects_verify_parameter_details_static_color");
    const led_effect_t* effect = led_effects_get_by_id(0); // Static Color
    TEST_ASSERT_NOT_NULL(effect);

    if (effect && effect->params && effect->param_count == 2) {
        effect_param_t* params = effect->params;

        // Parameter 0: Hue
        TEST_ASSERT_EQUAL_STRING("Hue", params[0].name);
        TEST_ASSERT_EQUAL_INT32(0, params[0].value);   // Default value
        TEST_ASSERT_EQUAL_INT32(0, params[0].min);
        TEST_ASSERT_EQUAL_INT32(359, params[0].max);
        TEST_ASSERT_EQUAL_INT32(1, params[0].step);

        // Parameter 1: Saturation
        TEST_ASSERT_EQUAL_STRING("Saturation", params[1].name);
        TEST_ASSERT_EQUAL_INT32(255, params[1].value); // Default value
        TEST_ASSERT_EQUAL_INT32(0, params[1].min);
        TEST_ASSERT_EQUAL_INT32(255, params[1].max);
        TEST_ASSERT_EQUAL_INT32(1, params[1].step);
    } else {
        TEST_FAIL_MESSAGE("Static Color effect or its parameters are not as expected.");
    }
}

TEST_CASE("led_effects_verify_parameter_details_candle", "[led_effects]")
{
    ESP_LOGI(TAG, "Running test: led_effects_verify_parameter_details_candle");
    const led_effect_t* effect = led_effects_get_by_id(1); // Candle
    TEST_ASSERT_NOT_NULL(effect);

    if (effect && effect->params && effect->param_count == 3) {
        effect_param_t* params = effect->params;

        // Parameter 0: Base Hue
        TEST_ASSERT_EQUAL_STRING("Base Hue", params[0].name);
        TEST_ASSERT_EQUAL_INT32(30, params[0].value);  // Default value
        TEST_ASSERT_EQUAL_INT32(0, params[0].min);
        TEST_ASSERT_EQUAL_INT32(359, params[0].max);
        TEST_ASSERT_EQUAL_INT32(1, params[0].step);

        // Parameter 1: Flicker Speed (name check from original code: "Flicker Spd")
        TEST_ASSERT_EQUAL_STRING("Flicker Spd", params[1].name);
        TEST_ASSERT_EQUAL_INT32(50, params[1].value);  // Default value
        TEST_ASSERT_EQUAL_INT32(10, params[1].min);
        TEST_ASSERT_EQUAL_INT32(100, params[1].max);
        TEST_ASSERT_EQUAL_INT32(5, params[1].step);

        // Parameter 2: Intensity
        TEST_ASSERT_EQUAL_STRING("Intensity", params[2].name);
        TEST_ASSERT_EQUAL_INT32(200, params[2].value); // Default value
        TEST_ASSERT_EQUAL_INT32(0, params[2].min);
        TEST_ASSERT_EQUAL_INT32(255, params[2].max);
        TEST_ASSERT_EQUAL_INT32(5, params[2].step);
    } else {
        TEST_FAIL_MESSAGE("Candle effect or its parameters are not as expected.");
    }
}

// The ESP-IDF test framework will call UNITY_BEGIN(), run all tests, and UNITY_END().
// No app_main is needed here when using TEST_SRCS in idf_component_register.
// Example of how it's typically run by the test runner:
void app_main(void) {
    ESP_LOGI(TAG, "Starting LED Effects Tests...");
    UNITY_BEGIN();
    unity_run_test_by_name("led_effects_get_all_retrieves_effects");
    unity_run_test_by_name("led_effects_get_by_id_valid_ids");
    unity_run_test_by_name("led_effects_get_by_id_invalid_id");
    unity_run_test_by_name("led_effects_verify_parameter_details_static_color");
    unity_run_test_by_name("led_effects_verify_parameter_details_candle");
    UNITY_END();
    ESP_LOGI(TAG, "LED Effects Tests Finished.");
}
