/**
#include "ethervox/error.h"
 * @file calculator_plugin.c
 * @brief Calculator compute tool implementation
 *
 * Provides mathematical expression evaluation with support for:
 * - Basic operations: +, -, *, /, ^
 * - Functions: sqrt, abs
 * - Parentheses for order of operations
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/compute_tools.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple recursive descent parser for mathematical expressions
typedef struct {
    const char* expr;
    size_t pos;
    char error[256];
} parser_t;

static double parse_expression(parser_t* p);
static double parse_term(parser_t* p);
static double parse_factor(parser_t* p);
static double parse_power(parser_t* p);

static void skip_whitespace(parser_t* p) {
    while (p->expr[p->pos] && isspace((unsigned char)p->expr[p->pos])) {
        p->pos++;
    }
}

static double parse_number(parser_t* p) {
    skip_whitespace(p);
    
    const char* start = &p->expr[p->pos];
    char* end;
    double value = strtod(start, &end);
    
    if (start == end) {
        snprintf(p->error, sizeof(p->error), "Expected number at position %zu", p->pos);
        return 0.0;
    }
    
    p->pos += (end - start);
    return value;
}

static double parse_factor(parser_t* p) {
    skip_whitespace(p);
    
    // Handle parentheses
    if (p->expr[p->pos] == '(') {
        p->pos++;
        double result = parse_expression(p);
        skip_whitespace(p);
        if (p->expr[p->pos] != ')') {
            snprintf(p->error, sizeof(p->error), "Expected ')' at position %zu", p->pos);
            return 0.0;
        }
        p->pos++;
        return result;
    }
    
    // Handle sqrt function
    if (strncmp(&p->expr[p->pos], "sqrt", 4) == 0) {
        p->pos += 4;
        skip_whitespace(p);
        if (p->expr[p->pos] != '(') {
            snprintf(p->error, sizeof(p->error), "Expected '(' after sqrt");
            return 0.0;
        }
        p->pos++;
        double arg = parse_expression(p);
        skip_whitespace(p);
        if (p->expr[p->pos] != ')') {
            snprintf(p->error, sizeof(p->error), "Expected ')' after sqrt argument");
            return 0.0;
        }
        p->pos++;
        return sqrt(arg);
    }
    
    // Handle abs function
    if (strncmp(&p->expr[p->pos], "abs", 3) == 0) {
        p->pos += 3;
        skip_whitespace(p);
        if (p->expr[p->pos] != '(') {
            snprintf(p->error, sizeof(p->error), "Expected '(' after abs");
            return 0.0;
        }
        p->pos++;
        double arg = parse_expression(p);
        skip_whitespace(p);
        if (p->expr[p->pos] != ')') {
            snprintf(p->error, sizeof(p->error), "Expected ')' after abs argument");
            return 0.0;
        }
        p->pos++;
        return fabs(arg);
    }
    
    // Handle unary minus
    if (p->expr[p->pos] == '-') {
        p->pos++;
        return -parse_factor(p);
    }
    
    // Handle unary plus
    if (p->expr[p->pos] == '+') {
        p->pos++;
        return parse_factor(p);
    }
    
    return parse_number(p);
}

static double parse_power(parser_t* p) {
    double left = parse_factor(p);
    
    skip_whitespace(p);
    if (p->expr[p->pos] == '^') {
        p->pos++;
        double right = parse_power(p); // Right associative
        return pow(left, right);
    }
    
    return left;
}

static double parse_term(parser_t* p) {
    double left = parse_power(p);
    
    while (1) {
        skip_whitespace(p);
        char op = p->expr[p->pos];
        
        if (op == '*') {
            p->pos++;
            left *= parse_power(p);
        } else if (op == '/') {
            p->pos++;
            double right = parse_power(p);
            if (right == 0.0) {
                snprintf(p->error, sizeof(p->error), "Division by zero");
                return 0.0;
            }
            left /= right;
        } else {
            break;
        }
    }
    
    return left;
}

static double parse_expression(parser_t* p) {
    double left = parse_term(p);
    
    while (1) {
        skip_whitespace(p);
        char op = p->expr[p->pos];
        
        if (op == '+') {
            p->pos++;
            left += parse_term(p);
        } else if (op == '-') {
            p->pos++;
            left -= parse_term(p);
        } else {
            break;
        }
    }
    
    return left;
}

static double evaluate_expression(const char* expr, char** error_msg) {
    parser_t parser = {
        .expr = expr,
        .pos = 0,
        .error = {0}
    };
    
    double result = parse_expression(&parser);
    
    skip_whitespace(&parser);
    if (parser.expr[parser.pos] != '\0' && parser.error[0] == '\0') {
        snprintf(parser.error, sizeof(parser.error), 
                "Unexpected character '%c' at position %zu", 
                parser.expr[parser.pos], parser.pos);
    }
    
    if (parser.error[0] != '\0') {
        if (error_msg) {
            *error_msg = strdup(parser.error);
        }
        return 0.0;
    }
    
    return result;
}

// Extract expression from JSON: {"expression": "5+5"}
static char* extract_json_string(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* key_pos = strstr(json, search);
    if (!key_pos) return NULL;
    
    const char* colon = strchr(key_pos, ':');
    if (!colon) return NULL;
    
    const char* value_start = strchr(colon, '"');
    if (!value_start) return NULL;
    value_start++; // Skip opening quote
    
    const char* value_end = strchr(value_start, '"');
    if (!value_end) return NULL;
    
    size_t len = value_end - value_start;
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, value_start, len);
    result[len] = '\0';
    return result;
}

static int extract_json_int(const char* json, const char* key, int default_value) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* key_pos = strstr(json, search);
    if (!key_pos) return default_value;
    
    const char* colon = strchr(key_pos, ':');
    if (!colon) return default_value;
    
    return atoi(colon + 1);
}

static int calculator_execute(const char* args_json, char** result, char** error) {
    if (!args_json || !result || !error) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Extract expression from JSON
    char* expression = extract_json_string(args_json, "expression");
    if (!expression) {
        *error = strdup("Missing 'expression' parameter in JSON");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Extract optional decimal_places (default: 2)
    int decimal_places = extract_json_int(args_json, "decimal_places", 2);
    if (decimal_places < 0) decimal_places = 0;
    if (decimal_places > 15) decimal_places = 15;  // Limit precision
    
    // Evaluate expression
    char* eval_error = NULL;
    double value = evaluate_expression(expression, &eval_error);
    
    if (eval_error) {
        *error = eval_error;
        free(expression);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Return result as JSON with tool name
    char* result_json = malloc(256);
    if (!result_json) {
        free(expression);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    snprintf(result_json, 256, "{\"result\": %.*f, \"tool\": \"calculator_compute\", \"expression\": \"%s\"}", 
             decimal_places, value, expression);
    *result = result_json;
    
    free(expression);
    return ETHERVOX_SUCCESS;
}

static ethervox_tool_t calculator_tool = {
    .name = "calculator_compute",
    .description = "Compute ANY math calculation - use for all arithmetic, don't calculate mentally. Supports +, -, *, /, ^, sqrt, abs, parentheses.",
    .parameters_json_schema = 
        "{\"type\":\"object\","
        "\"properties\":{"
        "\"expression\":{\"type\":\"string\",\"description\":\"Mathematical expression to evaluate\"},"
        "\"decimal_places\":{\"type\":\"integer\",\"description\":\"Number of decimal places (0-15, default: 2)\",\"default\":2}"
        "},"
        "\"required\":[\"expression\"]}",
    .execute = calculator_execute,
    .is_deterministic = true,
    .requires_confirmation = false,
    .is_stateful = false,
    .estimated_latency_ms = 0.5f
};

const ethervox_tool_t* ethervox_tool_calculator(void) {
    return &calculator_tool;
}
