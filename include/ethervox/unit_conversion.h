// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Tim Kos

/**
 * @file unit_conversion.h
 * @brief Public API for scientific unit conversion
 * 
 * Provides comprehensive unit conversion capabilities for the LLM Governor.
 * Supports multiple unit systems and categories.
 */

#ifndef ETHERVOX_UNIT_CONVERSION_H
#define ETHERVOX_UNIT_CONVERSION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert a value from one unit to another
 * 
 * Supports the following categories:
 * - Temperature: celsius, fahrenheit, kelvin, rankine
 * - Length: meter, km, mile, foot, inch, yard, nautical mile, etc.
 * - Mass: kilogram, gram, pound, ounce, ton, stone, etc.
 * - Volume: liter, gallon, quart, pint, cup, ml, etc.
 * - Speed: m/s, km/h, mph, knot, mach, etc.
 * - Pressure: pascal, bar, psi, atm, torr, mmHg, etc.
 * - Energy: joule, calorie, kWh, BTU, eV, etc.
 * - Power: watt, horsepower, BTU/h, etc.
 * - Area: m², km², acre, hectare, sq ft, etc.
 * - Data: byte, kilobyte, megabyte, bit, etc.
 * 
 * @param value The numeric value to convert
 * @param from_unit The source unit (case-insensitive)
 * @param to_unit The target unit (case-insensitive)
 * @param result Output parameter for the converted value
 * @param error_message Optional output parameter for error details (caller must free)
 * 
 * @return 0 on success, -1 on error
 * 
 * @example
 * double celsius;
 * char* error = NULL;
 * if (ethervox_unit_convert(32.0, "fahrenheit", "celsius", &celsius, &error) == 0) {
 *     printf("32°F = %.2f°C\n", celsius);  // Output: 32°F = 0.00°C
 * } else {
 *     printf("Error: %s\n", error);
 *     free(error);
 * }
 */
ethervox_result_t ethervox_unit_convert(
    double value,
    const char* from_unit,
    const char* to_unit,
    double* result,
    char** error_message
);

/**
 * @brief Register unit conversion tool with the tool registry
 * 
 * @param registry_ptr Pointer to ethervox_tool_registry_t
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_unit_conversion_register(void* registry_ptr);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_UNIT_CONVERSION_H
