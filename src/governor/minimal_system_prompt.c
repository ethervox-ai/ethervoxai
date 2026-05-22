/**
 * @file minimal_system_prompt.c
 * @brief Minimal system prompt generation using Tool Manifest System
 *
 * Generates ~150 token system prompts instead of ~15K tokens by using
 * optimized prompts or binary one-liners.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/tool_manifest.h"
#include "ethervox/config.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Helper: Build JSON parameters object from manifest tool parameters
 */
static int build_parameters_json(const tool_param_t* params, uint8_t param_count, char* buffer, size_t buffer_size) {
    if (param_count == 0) {
        return snprintf(buffer, buffer_size, "{}");
    }
    
    int offset = snprintf(buffer, buffer_size, "{\"type\": \"object\", \"properties\": {");
    if (offset < 0 || (size_t)offset >= buffer_size) return -1;
    
    for (uint8_t i = 0; i < param_count; i++) {
        const tool_param_t* param = &params[i];
        
        int written = snprintf(buffer + offset, buffer_size - offset,
            "%s\"%s\": {\"type\": \"%s\", \"description\": \"%s\"}",
            (i > 0) ? ", " : "",
            param->name,
            param->type,
            param->description);
        
        if (written < 0 || (size_t)(offset + written) >= buffer_size) return -1;
        offset += written;
    }
    
    // Add required array
    int written = snprintf(buffer + offset, buffer_size - offset, "}, \"required\": [");
    if (written < 0 || (size_t)(offset + written) >= buffer_size) return -1;
    offset += written;
    
    bool first_required = true;
    for (uint8_t i = 0; i < param_count; i++) {
        if (params[i].required) {
            written = snprintf(buffer + offset, buffer_size - offset,
                "%s\"%s\"",
                first_required ? "" : ", ",
                params[i].name);
            
            if (written < 0 || (size_t)(offset + written) >= buffer_size) return -1;
            offset += written;
            first_required = false;
        }
    }
    
    written = snprintf(buffer + offset, buffer_size - offset, "]}");
    if (written < 0 || (size_t)(offset + written) >= buffer_size) return -1;
    offset += written;
    
    return offset;
}

/**
 * Build system prompt from tool manifest following IBM Granite 4.0 format
 * 
 * Follows IBM Granite 4.0 tool-calling best practices:
 * - Standard preamble: "You are a helpful assistant with access to the following tools..."
 * - Tools wrapped in <tools></tools> XML tags
 * - Each tool as JSON: {"type": "function", "function": {...}}
 * - Uses optimized prompts when available, falls back to manifest defaults
 * - Tool call format: <tool_call>{"name": "...", "arguments": {...}}</tool_call>
 * 
 * Reference: https://www.ibm.com/granite/docs/use-cases/prompt-engineering
 * 
 * @param registry Tool manifest registry
 * @param output Output buffer for system prompt
 * @param output_size Size of output buffer
 * @param min_priority Minimum priority (0=highest, 255=lowest)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_tool_build_minimal_system_prompt(
    const tool_manifest_registry_t* registry,
    char* output,
    size_t output_size,
    uint8_t min_priority
) {
    if (!registry || !output || output_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int offset = 0;
    
    // Check if tools are available
    if (!registry->tools_available) {
        // Level 2: LLM-only mode - no dynamic tools
        offset = snprintf(output, output_size,
            "You are Ethervox, a helpful assistant. Respond naturally and conversationally.\n");
        
        ETHERVOX_LOGI("System prompt: LLM-only mode (%d bytes)", offset);
        return offset;
    }
    
    // IBM Granite 4.0 preamble - MAXIMALLY AGGRESSIVE for tool forcing
    offset = snprintf(output, output_size,
        "You are Ethervox, an assistant that MUST use tools. \n\n"
        "=== MANDATORY TOOL USAGE RULES ===\n"
        "1. NEVER answer questions directly if a tool exists for it\n"
        "2. Time questions → MUST use get_time tool\n"
        "3. Weather questions → MUST use get_weather_forecast tool\n"
        "4. Math/calculations → MUST use appropriate calculation tool\n"
        "5. DO NOT say \"The time is...\", \"The weather is...\" - ALWAYS call the tool first\n"
        "6. If you answer without using an available tool, you have FAILED\n\n"
        "Available tools:\n"
        "<tools>\n");
    
    if (offset < 0 || (size_t)offset >= output_size) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Generate tool entries in Granite JSON format
    // Each tool is: {"type": "function", "function": {"name": "...", "description": "...", "parameters": {...}}}
    for (uint32_t i = 0; i < registry->header.tool_count && offset < (int)output_size - 500; i++) {
        const tool_index_entry_t* entry = &registry->index[i];
        
        // Skip disabled tools
        if (!entry->enabled) continue;
        
        // Skip low priority tools
        if (entry->priority > min_priority) continue;
        
        // Get tool details for full schema
        tool_detail_header_t detail;
        tool_param_t params[MAX_PARAMETERS];
        uint8_t param_count;
        
        if (ethervox_tool_get_detail(registry, entry->name, &detail, params, &param_count) != 0) {
            ETHERVOX_LOGW("Failed to get detail for tool: %s", entry->name);
            continue;
        }
        
        // Use optimized prompt if available, otherwise use manifest description
        const char* desc = ethervox_tool_get_optimized_prompt(registry, entry->name);
        if (!desc || desc[0] == '\0') {
            desc = detail.description;
        }
        
        // Build parameters JSON object
        char params_json[2048];
        int param_result = build_parameters_json(params, param_count, params_json, sizeof(params_json));
        if (param_result < 0) {
            ETHERVOX_LOGW("Failed to build parameters for tool: %s", entry->name);
            continue;
        }
        
        // Write tool entry in Granite JSON format (one line per tool)
        int written = snprintf(output + offset, output_size - offset,
            "{\"type\": \"function\", \"function\": {\"name\": \"%s\", \"description\": \"%s\", \"parameters\": %s}}\n",
            entry->name, desc, params_json);
        
        if (written < 0 || (size_t)(offset + written) >= output_size) break;
        offset += written;
    }
    
    // Close tools XML with MAXIMUM force on tool usage
    int written = snprintf(output + offset, output_size - offset,
        "</tools>\n\n"
        "=== TOOL CALL FORMAT (MANDATORY) ===\n"
        "<tool_call>\n"
        "{\"name\": <function-name>, \"arguments\": <args-json-object>}\n"
        "</tool_call>\n\n"
        "=== EXAMPLES (STUDY THESE CAREFULLY) ===\n"
        "User: \"What time is it?\"\n"
        "CORRECT Response: <tool_call>\n{\"name\": \"get_time\", \"arguments\": {}}\n</tool_call>\n"
        "WRONG Response: \"The current time is 4:04 PM\" ← This is FAILURE\n\n"
        "User: \"What's the weather?\"\n"
        "CORRECT Response: <tool_call>\n{\"name\": \"get_weather_forecast\", \"arguments\": {\"location\": \"current\"}}\n</tool_call>\n"
        "WRONG Response: \"The weather is sunny\" ← This is FAILURE\n\n"
        "=== FORMATTING RULES ===\n"
        "- 'arguments' MUST be JSON object, NOT string\n"
        "- Complete the full </tool_call> tag before stopping\n"
        "- After tool returns result, THEN explain to user\n\n"
        "=== YOUR ONLY JOB ===\n"
        "When user asks a question that matches a tool: Generate the <tool_call> tag IMMEDIATELY.\n"
        "DO NOT attempt to answer first. DO NOT explain before calling. CALL THE TOOL.\n");
    
    if (written < 0 || (size_t)(offset + written) >= output_size) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    offset += written;
    
    // Log fallback level
    const char* level_name = ethervox_tool_fallback_level_name(registry->fallback_level);
    ETHERVOX_LOGI("System prompt: %s (Granite 4.0 format, %d bytes, %u tools)", 
                  level_name, offset, registry->header.tool_count);
    ETHERVOX_LOGI("===== FULL SYSTEM PROMPT (%s) =====", level_name);
    
    // Log in chunks to avoid logcat truncation (Android limit ~1KB per line)
    const int chunk_size = 250;
    for (int i = 0; i < offset; i += chunk_size) {
        int remaining = offset - i;
        int current_chunk = remaining < chunk_size ? remaining : chunk_size;
        char chunk[chunk_size + 1];
        strncpy(chunk, output + i, current_chunk);
        chunk[current_chunk] = '\0';
        ETHERVOX_LOGI("PROMPT[%d-%d]: %s", i, i + current_chunk - 1, chunk);
    }
    
    ETHERVOX_LOGI("===== END SYSTEM PROMPT =====");
    
    return offset;
}

/**
 * Build detailed tool schema for contextual injection
 * 
 * When LLM generates <tool_call>, inject ONLY that tool's full schema
 * into the NEXT prompt temporarily. Remove after tool execution.
 * 
 * @param registry Tool manifest registry
 * @param tool_name Name of tool to get schema for
 * @param output Output buffer for schema
 * @param output_size Size of output buffer
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_tool_build_schema_injection(
    const tool_manifest_registry_t* registry,
    const char* tool_name,
    char* output,
    size_t output_size
) {
    if (!registry || !tool_name || !output || output_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Get tool detail from manifest
    tool_detail_header_t detail;
    tool_param_t params[MAX_PARAMETERS];
    uint8_t param_count;
    
    if (ethervox_tool_get_detail(registry, tool_name, &detail, params, &param_count) != 0) {
        ETHERVOX_LOGE("Tool schema not found: %s", tool_name);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int offset = 0;
    
    // Tool header
    offset = snprintf(output, output_size,
        "\n--- Tool Schema for %s ---\n"
        "%s\n\n"
        "Parameters:\n",
        tool_name, detail.description);
    
    if (offset < 0 || (size_t)offset >= output_size) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Parameters
    for (uint8_t i = 0; i < param_count && offset < (int)output_size - 100; i++) {
        const char* req_str = params[i].required ? "REQUIRED" : "optional";
        const char* default_str = params[i].default_value[0] ? params[i].default_value : "none";
        
        offset += snprintf(output + offset, output_size - offset,
            "• %s (%s, %s) - default: %s\n",
            params[i].name, params[i].type, req_str, default_str);
    }
    
    offset += snprintf(output + offset, output_size - offset,
        "--- End Schema ---\n\n");
    
    ETHERVOX_LOGI("Injected schema for %s (%d bytes)", tool_name, offset);
    
    return offset;
}
