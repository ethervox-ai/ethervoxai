// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Tim Kos

/**
 * @file test_unit_conversion.c
 * @brief Comprehensive unit tests for unit conversion
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ethervox/unit_conversion.h"

#define EPSILON 0.0001
#define ASSERT_NEAR(expected, actual, tolerance) \
    if (fabs((expected) - (actual)) > (tolerance)) { \
        printf("✗ FAILED: Expected %.10f, got %.10f (diff: %.10f)\n", \
               (double)(expected), (double)(actual), fabs((expected) - (actual))); \
        exit(1); \
    }

static int test_count = 0;
static int passed_count = 0;

static void test_temperature(void) {
    printf("Test Category: Temperature Conversions\n");
    double result;
    char* error = NULL;
    
    // Celsius to Fahrenheit
    ethervox_unit_convert(0.0, "celsius", "fahrenheit", &result, &error);
    ASSERT_NEAR(32.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 0°C = 32°F\n");
    
    ethervox_unit_convert(100.0, "celsius", "fahrenheit", &result, &error);
    ASSERT_NEAR(212.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 100°C = 212°F\n");
    
    // Fahrenheit to Celsius
    ethervox_unit_convert(32.0, "fahrenheit", "celsius", &result, &error);
    ASSERT_NEAR(0.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 32°F = 0°C\n");
    
    ethervox_unit_convert(98.6, "fahrenheit", "celsius", &result, &error);
    ASSERT_NEAR(37.0, result, 0.01);
    test_count++; passed_count++;
    printf("  ✓ 98.6°F ≈ 37°C\n");
    
    // Celsius to Kelvin
    ethervox_unit_convert(0.0, "celsius", "kelvin", &result, &error);
    ASSERT_NEAR(273.15, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 0°C = 273.15K\n");
    
    ethervox_unit_convert(-273.15, "celsius", "kelvin", &result, &error);
    ASSERT_NEAR(0.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ -273.15°C = 0K (absolute zero)\n");
    
    // Kelvin to Fahrenheit
    ethervox_unit_convert(273.15, "kelvin", "fahrenheit", &result, &error);
    ASSERT_NEAR(32.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 273.15K = 32°F\n");
    
    // Rankine
    ethervox_unit_convert(491.67, "rankine", "fahrenheit", &result, &error);
    ASSERT_NEAR(32.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 491.67°R = 32°F\n");
}

static void test_length(void) {
    printf("\nTest Category: Length Conversions\n");
    double result;
    char* error = NULL;
    
    // Metric
    ethervox_unit_convert(1000.0, "meter", "kilometer", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1000m = 1km\n");
    
    ethervox_unit_convert(100.0, "centimeter", "meter", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 100cm = 1m\n");
    
    // Imperial
    ethervox_unit_convert(12.0, "inch", "foot", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 12in = 1ft\n");
    
    ethervox_unit_convert(3.0, "foot", "yard", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 3ft = 1yd\n");
    
    ethervox_unit_convert(5280.0, "foot", "mile", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 5280ft = 1mi\n");
    
    // Mixed
    ethervox_unit_convert(1.0, "mile", "kilometer", &result, &error);
    ASSERT_NEAR(1.609344, result, 0.0001);
    test_count++; passed_count++;
    printf("  ✓ 1mi ≈ 1.609km\n");
    
    ethervox_unit_convert(1.0, "inch", "centimeter", &result, &error);
    ASSERT_NEAR(2.54, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1in = 2.54cm\n");
    
    // Nautical
    ethervox_unit_convert(1.0, "nautical mile", "meter", &result, &error);
    ASSERT_NEAR(1852.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1nmi = 1852m\n");
}

static void test_mass(void) {
    printf("\nTest Category: Mass Conversions\n");
    double result;
    char* error = NULL;
    
    // Metric
    ethervox_unit_convert(1000.0, "gram", "kilogram", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1000g = 1kg\n");
    
    ethervox_unit_convert(1000.0, "kilogram", "tonne", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1000kg = 1t\n");
    
    // Imperial
    ethervox_unit_convert(16.0, "ounce", "pound", &result, &error);
    ASSERT_NEAR(1.0, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 16oz ≈ 1lb\n");
    
    ethervox_unit_convert(2000.0, "pound", "ton", &result, &error);
    ASSERT_NEAR(1.0, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 2000lb ≈ 1 short ton\n");
    
    ethervox_unit_convert(14.0, "pound", "stone", &result, &error);
    ASSERT_NEAR(1.0, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 14lb ≈ 1 stone\n");
    
    // Mixed
    ethervox_unit_convert(1.0, "kilogram", "pound", &result, &error);
    ASSERT_NEAR(2.20462, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 1kg ≈ 2.205lb\n");
}

static void test_volume(void) {
    printf("\nTest Category: Volume Conversions\n");
    double result;
    char* error = NULL;
    
    // Metric
    ethervox_unit_convert(1000.0, "milliliter", "liter", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1000ml = 1L\n");
    
    ethervox_unit_convert(1000.0, "liter", "cubic meter", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1000L = 1m³\n");
    
    // US
    ethervox_unit_convert(16.0, "tablespoon", "cup", &result, &error);
    ASSERT_NEAR(1.0, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 16tbsp = 1 cup\n");
    
    ethervox_unit_convert(4.0, "cup", "us quart", &result, &error);
    ASSERT_NEAR(1.0, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 4 cups = 1 US quart\n");
    
    ethervox_unit_convert(4.0, "us quart", "us gallon", &result, &error);
    ASSERT_NEAR(1.0, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 4 US qt = 1 US gal\n");
    
    // Imperial
    ethervox_unit_convert(8.0, "imperial pint", "imperial gallon", &result, &error);
    ASSERT_NEAR(1.0, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 8 imperial pt = 1 imperial gal\n");
}

static void test_speed(void) {
    printf("\nTest Category: Speed Conversions\n");
    double result;
    char* error = NULL;
    
    ethervox_unit_convert(1.0, "m/s", "km/h", &result, &error);
    ASSERT_NEAR(3.6, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 1m/s = 3.6km/h\n");
    
    ethervox_unit_convert(60.0, "mph", "km/h", &result, &error);
    ASSERT_NEAR(96.56, result, 0.1);
    test_count++; passed_count++;
    printf("  ✓ 60mph ≈ 96.56km/h\n");
    
    ethervox_unit_convert(1.0, "knot", "km/h", &result, &error);
    ASSERT_NEAR(1.852, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 1kt ≈ 1.852km/h\n");
}

static void test_pressure(void) {
    printf("\nTest Category: Pressure Conversions\n");
    double result;
    char* error = NULL;
    
    ethervox_unit_convert(1.0, "bar", "pascal", &result, &error);
    ASSERT_NEAR(100000.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1bar = 100000Pa\n");
    
    ethervox_unit_convert(1.0, "atmosphere", "pascal", &result, &error);
    ASSERT_NEAR(101325.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1atm = 101325Pa\n");
    
    ethervox_unit_convert(1.0, "atmosphere", "psi", &result, &error);
    ASSERT_NEAR(14.696, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 1atm ≈ 14.696psi\n");
    
    ethervox_unit_convert(760.0, "torr", "atmosphere", &result, &error);
    ASSERT_NEAR(1.0, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 760torr = 1atm\n");
}

static void test_energy(void) {
    printf("\nTest Category: Energy Conversions\n");
    double result;
    char* error = NULL;
    
    ethervox_unit_convert(1.0, "kilocalorie", "calorie", &result, &error);
    ASSERT_NEAR(1000.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1kcal = 1000cal\n");
    
    ethervox_unit_convert(1.0, "calorie", "joule", &result, &error);
    ASSERT_NEAR(4.184, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 1cal ≈ 4.184J\n");
    
    ethervox_unit_convert(1.0, "kilowatt hour", "joule", &result, &error);
    ASSERT_NEAR(3.6e6, result, 1.0);
    test_count++; passed_count++;
    printf("  ✓ 1kWh = 3.6MJ\n");
    
    ethervox_unit_convert(1.0, "btu", "joule", &result, &error);
    ASSERT_NEAR(1055.06, result, 0.1);
    test_count++; passed_count++;
    printf("  ✓ 1BTU ≈ 1055J\n");
}

static void test_power(void) {
    printf("\nTest Category: Power Conversions\n");
    double result;
    char* error = NULL;
    
    ethervox_unit_convert(1000.0, "watt", "kilowatt", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1000W = 1kW\n");
    
    ethervox_unit_convert(1.0, "horsepower", "watt", &result, &error);
    ASSERT_NEAR(745.7, result, 0.1);
    test_count++; passed_count++;
    printf("  ✓ 1hp ≈ 745.7W\n");
    
    ethervox_unit_convert(1.0, "kilowatt", "horsepower", &result, &error);
    ASSERT_NEAR(1.341, result, 0.01);
    test_count++; passed_count++;
    printf("  ✓ 1kW ≈ 1.341hp\n");
}

static void test_area(void) {
    printf("\nTest Category: Area Conversions\n");
    double result;
    char* error = NULL;
    
    ethervox_unit_convert(10000.0, "square meter", "hectare", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 10000m² = 1ha\n");
    
    ethervox_unit_convert(1.0, "square kilometer", "square meter", &result, &error);
    ASSERT_NEAR(1e6, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1km² = 1,000,000m²\n");
    
    ethervox_unit_convert(144.0, "square inch", "square foot", &result, &error);
    ASSERT_NEAR(1.0, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 144in² = 1ft²\n");
    
    ethervox_unit_convert(43560.0, "square foot", "acre", &result, &error);
    ASSERT_NEAR(1.0, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 43560ft² = 1 acre\n");
    
    ethervox_unit_convert(1.0, "acre", "hectare", &result, &error);
    ASSERT_NEAR(0.4047, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ 1 acre ≈ 0.4047ha\n");
}

static void test_data(void) {
    printf("\nTest Category: Data Conversions\n");
    double result;
    char* error = NULL;
    
    // Decimal
    ethervox_unit_convert(1000.0, "byte", "kilobyte", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1000B = 1KB (decimal)\n");
    
    ethervox_unit_convert(1000000.0, "byte", "megabyte", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1,000,000B = 1MB (decimal)\n");
    
    // Binary
    ethervox_unit_convert(1024.0, "byte", "kibibyte", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1024B = 1KiB (binary)\n");
    
    ethervox_unit_convert(1048576.0, "byte", "mebibyte", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1,048,576B = 1MiB (binary)\n");
    
    // Bits
    ethervox_unit_convert(8.0, "bit", "byte", &result, &error);
    ASSERT_NEAR(1.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 8bits = 1byte\n");
    
    ethervox_unit_convert(1.0, "megabit", "kilobyte", &result, &error);
    ASSERT_NEAR(125.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ 1Mbit = 125KB\n");
}

static void test_errors(void) {
    printf("\nTest Category: Error Handling\n");
    double result;
    char* error = NULL;
    int ret;
    
    // Incompatible units
    ret = ethervox_unit_convert(10.0, "meter", "kilogram", &result, &error);
    if (ret == 0) {
        printf("✗ Should reject incompatible units (meter to kilogram)\n");
        exit(1);
    }
    free(error);
    error = NULL;
    test_count++; passed_count++;
    printf("  ✓ Rejects incompatible units\n");
    
    // Unknown unit
    ret = ethervox_unit_convert(10.0, "foobar", "meter", &result, &error);
    if (ret == 0) {
        printf("✗ Should reject unknown unit 'foobar'\n");
        exit(1);
    }
    free(error);
    error = NULL;
    test_count++; passed_count++;
    printf("  ✓ Rejects unknown units\n");
    
    // NULL parameters
    ret = ethervox_unit_convert(10.0, NULL, "meter", &result, &error);
    if (ret == 0) {
        printf("✗ Should reject NULL from_unit\n");
        exit(1);
    }
    free(error);
    error = NULL;
    test_count++; passed_count++;
    printf("  ✓ Rejects NULL from_unit\n");
    
    ret = ethervox_unit_convert(10.0, "meter", NULL, &result, &error);
    if (ret == 0) {
        printf("✗ Should reject NULL to_unit\n");
        exit(1);
    }
    free(error);
    error = NULL;
    test_count++; passed_count++;
    printf("  ✓ Rejects NULL to_unit\n");
}

static void test_case_insensitive(void) {
    printf("\nTest Category: Case Insensitivity\n");
    double result;
    char* error = NULL;
    
    ethervox_unit_convert(100.0, "CELSIUS", "fahrenheit", &result, &error);
    ASSERT_NEAR(212.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ CELSIUS (uppercase) works\n");
    
    ethervox_unit_convert(1.0, "KiLoMeTeR", "METER", &result, &error);
    ASSERT_NEAR(1000.0, result, EPSILON);
    test_count++; passed_count++;
    printf("  ✓ Mixed case works\n");
    
    ethervox_unit_convert(1.0, "lb", "KG", &result, &error);
    ASSERT_NEAR(0.4536, result, 0.001);
    test_count++; passed_count++;
    printf("  ✓ Abbreviations with mixed case work\n");
}

int main(void) {
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  Unit Conversion - Comprehensive Test Suite\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    
    test_temperature();
    test_length();
    test_mass();
    test_volume();
    test_speed();
    test_pressure();
    test_energy();
    test_power();
    test_area();
    test_data();
    test_errors();
    test_case_insensitive();
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  Results: %d/%d tests passed\n", passed_count, test_count);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    if (passed_count == test_count) {
        printf("\n✅ All tests PASSED!\n\n");
        return ETHERVOX_SUCCESS;
    } else {
        printf("\n❌ Some tests FAILED!\n\n");
        return ETHERVOX_SUCCESS;
    }
}
