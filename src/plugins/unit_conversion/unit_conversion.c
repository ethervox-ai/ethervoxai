// SPDX-License-Identifier: MIT
#include "ethervox/error.h"
// Copyright (c) 2025 Tim Kos

/**
 * @file unit_conversion.c
 * @brief Unit conversion implementation for scientific and engineering units
 * 
 * Supports conversion between various unit systems including:
 * - Temperature (Celsius, Fahrenheit, Kelvin)
 * - Length (metric, imperial, nautical)
 * - Mass (metric, imperial, troy)
 * - Volume (metric, imperial, US)
 * - Speed (m/s, km/h, mph, knots)
 * - Pressure (Pa, bar, psi, atm, mmHg)
 * - Energy (J, cal, kWh, BTU)
 * - Power (W, hp, BTU/h)
 * - Area (m², ft², acre, hectare)
 * - Data (bytes, bits)
 */

#include "ethervox/unit_conversion.h"
#include "ethervox/logging.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

// ============================================================================
// Unit Conversion Tables
// ============================================================================

// Temperature conversions (special case - non-linear)
static double convert_temperature(double value, const char* from, const char* to) {
    double celsius = 0.0;
    
    // Convert to Celsius first
    if (strcasecmp(from, "celsius") == 0 || strcasecmp(from, "c") == 0) {
        celsius = value;
    } else if (strcasecmp(from, "fahrenheit") == 0 || strcasecmp(from, "f") == 0) {
        celsius = (value - 32.0) * 5.0 / 9.0;
    } else if (strcasecmp(from, "kelvin") == 0 || strcasecmp(from, "k") == 0) {
        celsius = value - 273.15;
    } else if (strcasecmp(from, "rankine") == 0 || strcasecmp(from, "r") == 0) {
        celsius = (value - 491.67) * 5.0 / 9.0;
    } else {
        return NAN;
    }
    
    // Convert from Celsius to target
    if (strcasecmp(to, "celsius") == 0 || strcasecmp(to, "c") == 0) {
        return celsius;
    } else if (strcasecmp(to, "fahrenheit") == 0 || strcasecmp(to, "f") == 0) {
        return celsius * 9.0 / 5.0 + 32.0;
    } else if (strcasecmp(to, "kelvin") == 0 || strcasecmp(to, "k") == 0) {
        return celsius + 273.15;
    } else if (strcasecmp(to, "rankine") == 0 || strcasecmp(to, "r") == 0) {
        return (celsius + 273.15) * 9.0 / 5.0;
    }
    
    return NAN;
}

// Length conversion factors (to meters)
typedef struct {
    const char* unit;
    double to_meters;
} length_conversion_t;

static const length_conversion_t LENGTH_CONVERSIONS[] = {
    // Metric
    {"meter", 1.0}, {"m", 1.0}, {"meters", 1.0},
    {"kilometer", 1000.0}, {"km", 1000.0}, {"kilometers", 1000.0},
    {"centimeter", 0.01}, {"cm", 0.01}, {"centimeters", 0.01},
    {"millimeter", 0.001}, {"mm", 0.001}, {"millimeters", 0.001},
    {"micrometer", 1e-6}, {"µm", 1e-6}, {"micrometers", 1e-6}, {"um", 1e-6},
    {"nanometer", 1e-9}, {"nm", 1e-9}, {"nanometers", 1e-9},
    
    // Imperial
    {"inch", 0.0254}, {"in", 0.0254}, {"inches", 0.0254},
    {"foot", 0.3048}, {"ft", 0.3048}, {"feet", 0.3048},
    {"yard", 0.9144}, {"yd", 0.9144}, {"yards", 0.9144},
    {"mile", 1609.344}, {"mi", 1609.344}, {"miles", 1609.344},
    
    // Nautical
    {"nautical mile", 1852.0}, {"nmi", 1852.0}, {"nm", 1852.0},
    
    // Astronomy
    {"astronomical unit", 1.496e11}, {"au", 1.496e11},
    {"light year", 9.461e15}, {"ly", 9.461e15},
    {"parsec", 3.086e16}, {"pc", 3.086e16},
    
    {NULL, 0.0}
};

// Mass conversion factors (to kilograms)
typedef struct {
    const char* unit;
    double to_kg;
} mass_conversion_t;

static const mass_conversion_t MASS_CONVERSIONS[] = {
    // Metric
    {"kilogram", 1.0}, {"kg", 1.0}, {"kilograms", 1.0},
    {"gram", 0.001}, {"g", 0.001}, {"grams", 0.001},
    {"milligram", 1e-6}, {"mg", 1e-6}, {"milligrams", 1e-6},
    {"microgram", 1e-9}, {"µg", 1e-9}, {"micrograms", 1e-9}, {"ug", 1e-9},
    {"tonne", 1000.0}, {"t", 1000.0}, {"metric ton", 1000.0},
    
    // Imperial/US
    {"pound", 0.45359237}, {"lb", 0.45359237}, {"lbs", 0.45359237}, {"pounds", 0.45359237},
    {"ounce", 0.028349523125}, {"oz", 0.028349523125}, {"ounces", 0.028349523125},
    {"ton", 907.18474}, {"short ton", 907.18474},
    {"long ton", 1016.0469088}, {"imperial ton", 1016.0469088},
    {"stone", 6.35029318}, {"st", 6.35029318},
    
    // Troy
    {"troy ounce", 0.0311034768}, {"ozt", 0.0311034768},
    {"troy pound", 0.3732417216}, {"tlb", 0.3732417216},
    
    {NULL, 0.0}
};

// Volume conversion factors (to liters)
typedef struct {
    const char* unit;
    double to_liters;
} volume_conversion_t;

static const volume_conversion_t VOLUME_CONVERSIONS[] = {
    // Metric
    {"liter", 1.0}, {"l", 1.0}, {"liters", 1.0}, {"litre", 1.0}, {"litres", 1.0},
    {"milliliter", 0.001}, {"ml", 0.001}, {"milliliters", 0.001},
    {"cubic meter", 1000.0}, {"m3", 1000.0}, {"m³", 1000.0},
    {"cubic centimeter", 0.001}, {"cm3", 0.001}, {"cm³", 0.001}, {"cc", 0.001},
    
    // Imperial
    {"gallon", 4.54609}, {"gal", 4.54609}, {"imperial gallon", 4.54609},
    {"quart", 1.1365225}, {"qt", 1.1365225}, {"imperial quart", 1.1365225},
    {"pint", 0.56826125}, {"pt", 0.56826125}, {"imperial pint", 0.56826125},
    {"fluid ounce", 0.0284130625}, {"fl oz", 0.0284130625}, {"imperial fl oz", 0.0284130625},
    
    // US
    {"us gallon", 3.785411784}, {"us gal", 3.785411784},
    {"us quart", 0.946352946}, {"us qt", 0.946352946},
    {"us pint", 0.473176473}, {"us pt", 0.473176473},
    {"us fluid ounce", 0.0295735295625}, {"us fl oz", 0.0295735295625},
    {"cup", 0.2365882365}, {"cups", 0.2365882365},
    {"tablespoon", 0.01478676478125}, {"tbsp", 0.01478676478125},
    {"teaspoon", 0.00492892159375}, {"tsp", 0.00492892159375},
    
    // Other
    {"barrel", 158.987294928}, {"bbl", 158.987294928}, {"oil barrel", 158.987294928},
    
    {NULL, 0.0}
};

// Speed conversion factors (to m/s)
typedef struct {
    const char* unit;
    double to_mps;
} speed_conversion_t;

static const speed_conversion_t SPEED_CONVERSIONS[] = {
    {"meter per second", 1.0}, {"m/s", 1.0}, {"mps", 1.0},
    {"kilometer per hour", 0.277777778}, {"km/h", 0.277777778}, {"kph", 0.277777778}, {"kmh", 0.277777778},
    {"mile per hour", 0.44704}, {"mph", 0.44704}, {"mi/h", 0.44704},
    {"foot per second", 0.3048}, {"ft/s", 0.3048}, {"fps", 0.3048},
    {"knot", 0.514444444}, {"knots", 0.514444444}, {"kt", 0.514444444},
    {"speed of light", 299792458.0}, {"c", 299792458.0},
    {"mach", 343.0}, {"mach 1", 343.0},
    
    {NULL, 0.0}
};

// Pressure conversion factors (to pascals)
typedef struct {
    const char* unit;
    double to_pascals;
} pressure_conversion_t;

static const pressure_conversion_t PRESSURE_CONVERSIONS[] = {
    {"pascal", 1.0}, {"pa", 1.0}, {"pascals", 1.0},
    {"kilopascal", 1000.0}, {"kpa", 1000.0}, {"kilopascals", 1000.0},
    {"megapascal", 1e6}, {"mpa", 1e6}, {"megapascals", 1e6},
    {"bar", 100000.0}, {"bars", 100000.0},
    {"millibar", 100.0}, {"mbar", 100.0}, {"millibars", 100.0},
    {"atmosphere", 101325.0}, {"atm", 101325.0}, {"atmospheres", 101325.0},
    {"psi", 6894.757293168}, {"pound per square inch", 6894.757293168},
    {"torr", 133.322368421}, {"mmhg", 133.322368421}, {"mm hg", 133.322368421},
    {"inch of mercury", 3386.389}, {"inhg", 3386.389}, {"in hg", 3386.389},
    
    {NULL, 0.0}
};

// Energy conversion factors (to joules)
typedef struct {
    const char* unit;
    double to_joules;
} energy_conversion_t;

static const energy_conversion_t ENERGY_CONVERSIONS[] = {
    {"joule", 1.0}, {"j", 1.0}, {"joules", 1.0},
    {"kilojoule", 1000.0}, {"kj", 1000.0}, {"kilojoules", 1000.0},
    {"megajoule", 1e6}, {"mj", 1e6}, {"megajoules", 1e6},
    {"calorie", 4.184}, {"cal", 4.184}, {"calories", 4.184},
    {"kilocalorie", 4184.0}, {"kcal", 4184.0}, {"kilocalories", 4184.0}, {"food calorie", 4184.0},
    {"watt hour", 3600.0}, {"wh", 3600.0}, {"watt hours", 3600.0},
    {"kilowatt hour", 3.6e6}, {"kwh", 3.6e6}, {"kilowatt hours", 3.6e6},
    {"electron volt", 1.602176634e-19}, {"ev", 1.602176634e-19},
    {"british thermal unit", 1055.05585262}, {"btu", 1055.05585262},
    {"therm", 1.05505585262e8}, {"therms", 1.05505585262e8},
    {"foot-pound", 1.3558179483314004}, {"ft-lb", 1.3558179483314004}, {"ft·lb", 1.3558179483314004},
    
    {NULL, 0.0}
};

// Power conversion factors (to watts)
typedef struct {
    const char* unit;
    double to_watts;
} power_conversion_t;

static const power_conversion_t POWER_CONVERSIONS[] = {
    {"watt", 1.0}, {"w", 1.0}, {"watts", 1.0},
    {"kilowatt", 1000.0}, {"kw", 1000.0}, {"kilowatts", 1000.0},
    {"megawatt", 1e6}, {"mw", 1e6}, {"megawatts", 1e6},
    {"horsepower", 745.699872}, {"hp", 745.699872},
    {"metric horsepower", 735.49875}, {"ps", 735.49875},
    {"btu per hour", 0.29307107017}, {"btu/h", 0.29307107017},
    {"ton of refrigeration", 3516.853}, {"tr", 3516.853},
    
    {NULL, 0.0}
};

// Area conversion factors (to square meters)
typedef struct {
    const char* unit;
    double to_sq_meters;
} area_conversion_t;

static const area_conversion_t AREA_CONVERSIONS[] = {
    {"square meter", 1.0}, {"m2", 1.0}, {"m²", 1.0}, {"sq m", 1.0},
    {"square kilometer", 1e6}, {"km2", 1e6}, {"km²", 1e6}, {"sq km", 1e6},
    {"square centimeter", 1e-4}, {"cm2", 1e-4}, {"cm²", 1e-4}, {"sq cm", 1e-4},
    {"square millimeter", 1e-6}, {"mm2", 1e-6}, {"mm²", 1e-6}, {"sq mm", 1e-6},
    {"hectare", 10000.0}, {"ha", 10000.0}, {"hectares", 10000.0},
    {"are", 100.0}, {"ares", 100.0},
    
    {"square inch", 0.00064516}, {"in2", 0.00064516}, {"in²", 0.00064516}, {"sq in", 0.00064516},
    {"square foot", 0.09290304}, {"ft2", 0.09290304}, {"ft²", 0.09290304}, {"sq ft", 0.09290304},
    {"square yard", 0.83612736}, {"yd2", 0.83612736}, {"yd²", 0.83612736}, {"sq yd", 0.83612736},
    {"square mile", 2589988.110336}, {"mi2", 2589988.110336}, {"mi²", 2589988.110336}, {"sq mi", 2589988.110336},
    {"acre", 4046.8564224}, {"acres", 4046.8564224},
    
    {NULL, 0.0}
};

// Data conversion factors (to bytes)
typedef struct {
    const char* unit;
    double to_bytes;
} data_conversion_t;

static const data_conversion_t DATA_CONVERSIONS[] = {
    // Bytes (decimal)
    {"byte", 1.0}, {"b", 1.0}, {"bytes", 1.0},
    {"kilobyte", 1000.0}, {"kb", 1000.0}, {"kilobytes", 1000.0},
    {"megabyte", 1e6}, {"mb", 1e6}, {"megabytes", 1e6},
    {"gigabyte", 1e9}, {"gb", 1e9}, {"gigabytes", 1e9},
    {"terabyte", 1e12}, {"tb", 1e12}, {"terabytes", 1e12},
    {"petabyte", 1e15}, {"pb", 1e15}, {"petabytes", 1e15},
    
    // Bytes (binary)
    {"kibibyte", 1024.0}, {"kib", 1024.0}, {"kibibytes", 1024.0},
    {"mebibyte", 1048576.0}, {"mib", 1048576.0}, {"mebibytes", 1048576.0},
    {"gibibyte", 1073741824.0}, {"gib", 1073741824.0}, {"gibibytes", 1073741824.0},
    {"tebibyte", 1099511627776.0}, {"tib", 1099511627776.0}, {"tebibytes", 1099511627776.0},
    {"pebibyte", 1125899906842624.0}, {"pib", 1125899906842624.0}, {"pebibytes", 1125899906842624.0},
    
    // Bits
    {"bit", 0.125}, {"bits", 0.125},
    {"kilobit", 125.0}, {"kbit", 125.0}, {"kb (bit)", 125.0},
    {"megabit", 125000.0}, {"mbit", 125000.0}, {"mb (bit)", 125000.0},
    {"gigabit", 125000000.0}, {"gbit", 125000000.0}, {"gb (bit)", 125000000.0},
    
    {NULL, 0.0}
};

// ============================================================================
// Generic Linear Conversion Helper
// ============================================================================

static double linear_convert(double value, const char* from, const char* to, 
                            const void* conversions, size_t entry_size) {
    const char* from_unit = NULL;
    const char* to_unit = NULL;
    double from_factor = 0.0;
    double to_factor = 0.0;
    
    // Find conversion factors
    for (size_t i = 0; ; i++) {
        const char* unit = *(const char**)((const char*)conversions + i * entry_size);
        if (!unit) break;
        
        double factor = *(const double*)((const char*)conversions + i * entry_size + sizeof(char*));
        
        if (strcasecmp(unit, from) == 0) {
            from_factor = factor;
            from_unit = unit;
        }
        if (strcasecmp(unit, to) == 0) {
            to_factor = factor;
            to_unit = unit;
        }
    }
    
    if (!from_unit || !to_unit) {
        return NAN;
    }
    
    // Convert: value * from_factor / to_factor
    return value * from_factor / to_factor;
}

// ============================================================================
// Main Conversion Function
// ============================================================================

ethervox_result_t ethervox_unit_convert(
    double value,
    const char* from_unit,
    const char* to_unit,
    double* result,
    char** error_message
) {
    if (!from_unit || !to_unit || !result) {
        if (error_message) {
            *error_message = strdup("Missing required parameters");
        }
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Try each conversion category
    double converted = NAN;
    
    // Temperature (special case)
    converted = convert_temperature(value, from_unit, to_unit);
    if (!isnan(converted)) {
        *result = converted;
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Converted %.6f %s to %.6f %s (temperature)", 
                    value, from_unit, converted, to_unit);
        return ETHERVOX_SUCCESS;
    }
    
    // Length
    converted = linear_convert(value, from_unit, to_unit, 
                              LENGTH_CONVERSIONS, sizeof(length_conversion_t));
    if (!isnan(converted)) {
        *result = converted;
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Converted %.6f %s to %.6f %s (length)", 
                    value, from_unit, converted, to_unit);
        return ETHERVOX_SUCCESS;
    }
    
    // Mass
    converted = linear_convert(value, from_unit, to_unit, 
                              MASS_CONVERSIONS, sizeof(mass_conversion_t));
    if (!isnan(converted)) {
        *result = converted;
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Converted %.6f %s to %.6f %s (mass)", 
                    value, from_unit, converted, to_unit);
        return ETHERVOX_SUCCESS;
    }
    
    // Volume
    converted = linear_convert(value, from_unit, to_unit, 
                              VOLUME_CONVERSIONS, sizeof(volume_conversion_t));
    if (!isnan(converted)) {
        *result = converted;
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Converted %.6f %s to %.6f %s (volume)", 
                    value, from_unit, converted, to_unit);
        return ETHERVOX_SUCCESS;
    }
    
    // Speed
    converted = linear_convert(value, from_unit, to_unit, 
                              SPEED_CONVERSIONS, sizeof(speed_conversion_t));
    if (!isnan(converted)) {
        *result = converted;
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Converted %.6f %s to %.6f %s (speed)", 
                    value, from_unit, converted, to_unit);
        return ETHERVOX_SUCCESS;
    }
    
    // Pressure
    converted = linear_convert(value, from_unit, to_unit, 
                              PRESSURE_CONVERSIONS, sizeof(pressure_conversion_t));
    if (!isnan(converted)) {
        *result = converted;
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Converted %.6f %s to %.6f %s (pressure)", 
                    value, from_unit, converted, to_unit);
        return ETHERVOX_SUCCESS;
    }
    
    // Energy
    converted = linear_convert(value, from_unit, to_unit, 
                              ENERGY_CONVERSIONS, sizeof(energy_conversion_t));
    if (!isnan(converted)) {
        *result = converted;
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Converted %.6f %s to %.6f %s (energy)", 
                    value, from_unit, converted, to_unit);
        return ETHERVOX_SUCCESS;
    }
    
    // Power
    converted = linear_convert(value, from_unit, to_unit, 
                              POWER_CONVERSIONS, sizeof(power_conversion_t));
    if (!isnan(converted)) {
        *result = converted;
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Converted %.6f %s to %.6f %s (power)", 
                    value, from_unit, converted, to_unit);
        return ETHERVOX_SUCCESS;
    }
    
    // Area
    converted = linear_convert(value, from_unit, to_unit, 
                              AREA_CONVERSIONS, sizeof(area_conversion_t));
    if (!isnan(converted)) {
        *result = converted;
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Converted %.6f %s to %.6f %s (area)", 
                    value, from_unit, converted, to_unit);
        return ETHERVOX_SUCCESS;
    }
    
    // Data
    converted = linear_convert(value, from_unit, to_unit, 
                              DATA_CONVERSIONS, sizeof(data_conversion_t));
    if (!isnan(converted)) {
        *result = converted;
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Converted %.6f %s to %.6f %s (data)", 
                    value, from_unit, converted, to_unit);
        return ETHERVOX_SUCCESS;
    }
    
    // No conversion found
    if (error_message) {
        char buf[512];
        snprintf(buf, sizeof(buf), 
                "Cannot convert from '%s' to '%s': incompatible or unknown units", 
                from_unit, to_unit);
        *error_message = strdup(buf);
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                "Failed to convert %s to %s: no matching conversion", from_unit, to_unit);
    
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
}
