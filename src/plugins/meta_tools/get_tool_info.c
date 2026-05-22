/**
#include "ethervox/error.h"
 * Meta-tool: get_tool_info
 * 
 * Returns detailed schema and documentation for any tool.
 * Enables just-in-time tool schema loading to minimize system prompt size.
 */

#include "ethervox/governor.h"
#include "ethervox/tool_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global manifest registry (set by governor)
static tool_manifest_registry_t* g_manifest_registry = NULL;

void ethervox_get_tool_info_set_manifest(tool_manifest_registry_t* manifest) {
    g_manifest_registry = manifest;
}

/**
 * Build JSON schema for a single tool
 */
static char* build_tool_schema_json(const char* tool_name) {
    if (!g_manifest_registry) {
        return strdup("{\"error\": \"Manifest not initialized\"}");
    }
    
    // Get tool detail from manifest
    tool_detail_header_t detail;
    tool_param_t params[MAX_PARAMETERS];
    uint8_t param_count;
    
    if (ethervox_tool_get_detail(g_manifest_registry, tool_name, &detail, params, &param_count) != 0) {
        char error[256];
        snprintf(error, sizeof(error), "{\"error\": \"Tool '%s' not found\"}", tool_name);
        return strdup(error);
    }
    
    // Build JSON response manually
    char* json = malloc(4096);
    if (!json) return strdup("{\"error\": \"Memory allocation failed\"}");
    
    // Get optimized prompt if available
    const char* optimized_desc = ethervox_tool_get_optimized_prompt(g_manifest_registry, tool_name);
    if (!optimized_desc || optimized_desc[0] == '\0') {
        optimized_desc = detail.description;  // Fallback to full description
    }
    
    int off = 0;
    off += snprintf(json + off, 4096 - off, "{\"type\":\"function\",\"function\":{");
    off += snprintf(json + off, 4096 - off, "\"name\":\"%s\",", tool_name);
    off += snprintf(json + off, 4096 - off, "\"description\":\"%s\",", optimized_desc);
    
    // Add parameters in JSON Schema format
    off += snprintf(json + off, 4096 - off, "\"parameters\":{\"type\":\"object\",\"properties\":{");
    for (uint8_t i = 0; i < param_count; i++) {
        if (i > 0) off += snprintf(json + off, 4096 - off, ",");
        off += snprintf(json + off, 4096 - off, "\"%s\":{", params[i].name);
        off += snprintf(json + off, 4096 - off, "\"type\":\"%s\",", params[i].type);
        off += snprintf(json + off, 4096 - off, "\"description\":\"%s\"", params[i].description);
        off += snprintf(json + off, 4096 - off, "}");
    }
    off += snprintf(json + off, 4096 - off, "}");
    
    // Add required array
    off += snprintf(json + off, 4096 - off, ",\"required\":[");
    bool first_required = true;
    for (uint8_t i = 0; i < param_count; i++) {
        if (params[i].required) {
            if (!first_required) off += snprintf(json + off, 4096 - off, ",");
            off += snprintf(json + off, 4096 - off, "\"%s\"", params[i].name);
            first_required = false;
        }
    }
    off += snprintf(json + off, 4096 - off, "]");
    
    // Close parameters and function
    off += snprintf(json + off, 4096 - off, "}");  // Close parameters
    off += snprintf(json + off, 4096 - off, "}}");  // Close function and root
    
    return json;
}

/**
 * Build catalog of all available tools (summary only)
 */
static char* build_tool_catalog_json(void) {
    if (!g_manifest_registry) {
        return strdup("{\"error\": \"Manifest not initialized\"}");
    }
    
    // Build JSON manually
    char* json = malloc(16384);  // Larger buffer for catalog
    if (!json) return strdup("{\"error\": \"Memory allocation failed\"}");
    
    int off = 0;
    off += snprintf(json + off, 16384 - off, "{");
    off += snprintf(json + off, 16384 - off, "\"tool_count\":%u,", g_manifest_registry->header.tool_count);
    off += snprintf(json + off, 16384 - off, "\"tools\":[");
    
    bool first = true;
    for (uint32_t i = 0; i < g_manifest_registry->header.tool_count; i++) {
        const tool_index_entry_t* tool_idx = &g_manifest_registry->index[i];
        
        if (!tool_idx->enabled) {
            continue;  // Skip disabled tools
        }
        
        if (!first) off += snprintf(json + off, 16384 - off, ",");
        first = false;
        
        off += snprintf(json + off, 16384 - off, "{");
        off += snprintf(json + off, 16384 - off, "\"name\":\"%s\",", tool_idx->name);
        off += snprintf(json + off, 16384 - off, "\"category\":\"%s\"", tool_idx->category);
        
        // Add one-liner summary
        tool_detail_header_t detail;
        if (ethervox_tool_get_detail(g_manifest_registry, tool_idx->name, &detail, NULL, NULL) == 0) {
            // Extract first sentence as summary
            char summary[128];
            const char* period = strchr(detail.description, '.');
            if (period && (period - detail.description) < 120) {
                size_t len = period - detail.description + 1;
                memcpy(summary, detail.description, len);
                summary[len] = '\0';
            } else {
                snprintf(summary, sizeof(summary), "%.120s", detail.description);
            }
            off += snprintf(json + off, 16384 - off, ",\"summary\":\"%s\"", summary);
        }
        
        off += snprintf(json + off, 16384 - off, "}");
        
        if (off >= 15800) break;  // Prevent buffer overflow
    }
    
    off += snprintf(json + off, 16384 - off, "]}");
    
    return json;
}

/**
 * Tool wrapper: get_tool_info
 * 
 * Parameters:
 *   tool_name (string, required): Name of tool to get info for, or "*" for all tools
 * 
 * Returns: JSON with tool schema and parameters
 */
static int tool_get_tool_info_wrapper(const char* params_json, char** result, char** error) {
    if (!params_json) {
        *result = NULL;
        *error = strdup("No parameters provided");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Simple JSON parsing for tool_name parameter
    const char* tool_name_start = strstr(params_json, "\"tool_name\"");
    if (!tool_name_start) {
        *result = NULL;
        *error = strdup("Missing 'tool_name' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Find the value after the colon
    const char* colon = strchr(tool_name_start, ':');
    if (!colon) {
        *result = NULL;
        *error = strdup("Invalid JSON format");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Skip whitespace and opening quote
    const char* value_start = colon + 1;
    while (*value_start == ' ' || *value_start == '\t' || *value_start == '\n') value_start++;
    if (*value_start != '"') {
        *result = NULL;
        *error = strdup("Tool name must be a string");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    value_start++;  // Skip opening quote
    
    // Find closing quote
    const char* value_end = strchr(value_start, '"');
    if (!value_end) {
        *result = NULL;
        *error = strdup("Invalid JSON string");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Extract tool name
    size_t len = value_end - value_start;
    char tool_name[128];
    if (len >= sizeof(tool_name)) {
        *result = NULL;
        *error = strdup("Tool name too long");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    memcpy(tool_name, value_start, len);
    tool_name[len] = '\0';
    
    // Handle wildcard or specific tool
    if (strcmp(tool_name, "*") == 0) {
        // Return catalog of all tools
        *result = build_tool_catalog_json();
    } else {
        // Return detailed schema for specific tool
        *result = build_tool_schema_json(tool_name);
    }
    
    *error = NULL;
    return ETHERVOX_SUCCESS;
}

/**
 * Register get_tool_info meta-tool
 */
ethervox_result_t ethervox_get_tool_info_register(ethervox_tool_registry_t* registry) {
    if (!registry) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ethervox_tool_t tool = {
        .name = "get_tool_info",
        .description = "Get detailed schema and documentation for any tool. "
                      "Use tool_name=\"*\" to list all available tools, or specify a tool name "
                      "to get its parameters, types, and usage examples. "
                      "Call this before using unfamiliar tools to understand their parameters.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"tool_name\":{\"type\":\"string\",\"description\":\"Name of tool to get info for, or '*' for all tools\"}"
            "},\"required\":[\"tool_name\"]}",
        .execute = tool_get_tool_info_wrapper,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 1.0f
    };
    
    return ethervox_tool_registry_add(registry, &tool);
}
