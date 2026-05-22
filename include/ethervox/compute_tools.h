/**
 * @file compute_tools.h
 * @brief Deterministic compute tools for the Governor
 *
 * These tools provide instant, stateless computation with no side effects.
 * Perfect for mathematical operations, unit conversions, and data transformations.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_COMPUTE_TOOLS_H
#define ETHERVOX_COMPUTE_TOOLS_H

#include "ethervox/governor.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Calculator Tool
// ============================================================================

/**
 * Get calculator tool definition
 * 
 * Evaluates mathematical expressions:
 * - Basic operations: +, -, *, /, ^
 * - Functions: sqrt, sin, cos, tan, log, abs
 * - Parentheses for order of operations
 * 
 * Example: {"expression": "47.50 * 0.15"} -> {"result": 7.13}
 */
const ethervox_tool_t* ethervox_tool_calculator(void);

// ============================================================================
// Percentage Calculator Tool
// ============================================================================

/**
 * Get percentage calculator tool definition
 * 
 * Operations:
 * - "of": Calculate percentage of value
 * - "increase": Increase value by percentage
 * - "decrease": Decrease value by percentage
 * - "is_what_percent": Calculate what percent value1 is of value2
 * 
 * Example: {"value": 47.50, "percentage": 15, "operation": "of"} -> {"result": 7.13}
 */
const ethervox_tool_t* ethervox_tool_percentage(void);

// ============================================================================
// Unit Converter Tool
// ============================================================================

/**
 * Get unit converter tool definition
 * 
 * Supported categories:
 * - Distance: miles, kilometers, meters, feet, inches, yards
 * - Temperature: celsius, fahrenheit, kelvin
 * - Weight: pounds, kilograms, grams, ounces
 * - Volume: gallons, liters, milliliters, cups, pints, quarts
 * 
 * Example: {"value": 100, "from_unit": "miles", "to_unit": "kilometers"} -> {"result": 160.934}
 */
const ethervox_tool_t* ethervox_tool_unit_converter(void);

// ============================================================================
// Time Query Tools
// ============================================================================

/**
 * Get current time tool definition
 * 
 * Returns ONLY the current time (no date).
 * Use when user asks "what time is it"
 * 
 * Example: {} -> {"time_24hr": "3:45 PM", "time_24hr": "15:45"}
 */
const ethervox_tool_t* ethervox_tool_time_get_current(void);

/**
 * Get current date tool definition
 * 
 * Returns ONLY the current date (no time).
 * Use when user asks "what's the date" or "what's today's date"
 * 
 * Example: {} -> {"date": "Friday, November 22, 2025", "day_name": "Friday", "month": "November", "day": 22, "year": 2025}
 */
const ethervox_tool_t* ethervox_tool_time_get_date(void);

/**
 * Get day of week tool definition
 * 
 * Returns the current day of the week.
 * Use when user asks "what day is it"
 * 
 * Example: {} -> {"day": "Friday", "day_short": "Fri", "day_number": 5}
 */
const ethervox_tool_t* ethervox_tool_time_get_day_of_week(void);

/**
 * Get week number tool definition
 * 
 * Returns the current week number of the year.
 * Use when user asks "what week is it" or "what week number"
 * 
 * Example: {} -> {"week_number": 47, "year": 2025, "day_of_year": 326}
 */
const ethervox_tool_t* ethervox_tool_time_get_week_number(void);

// ============================================================================
// Registration Helper
// ============================================================================

/**
 * Register all compute tools to a registry
 * 
 * @param registry Tool registry to add compute tools to
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_compute_tools_register_all(ethervox_tool_registry_t* registry);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_COMPUTE_TOOLS_H
