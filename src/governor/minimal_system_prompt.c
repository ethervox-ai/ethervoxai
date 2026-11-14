/**
 * @file minimal_system_prompt.c
 * @brief Minimal system prompt generation using Tool Manifest System
 *
 * Generates ~150 token system prompts instead of ~15K tokens by using
 * optimized prompts or binary one-liners.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
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
        ETHERVOX_LOGE("Invalid arguments: registry=%p, output=%p, size=%zu", 
                     (void*)registry, (void*)output, output_size);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ETHERVOX_LOGI("Building tool prompt: registry=%p, tools_available=%d, tool_count=%u, min_priority=%u",
                 (void*)registry, registry->tools_available, registry->header.tool_count, min_priority);
    
    int offset = 0;
    
    // Check if tools are available
    if (!registry->tools_available) {
        // Level 2: LLM-only mode - no dynamic tools
        offset = snprintf(output, output_size,
            "You are Ethervox, a helpful assistant. Respond naturally and conversationally.\n");
        
        ETHERVOX_LOGI("System prompt: LLM-only mode (%d bytes)", offset);
        return offset;
    }
    
    // IBM Granite 4.0 preamble - Clear guidance on tool usage and assistant behavior
    int preamble_len = snprintf(output, output_size,
        "You are Ethervox, a helpful AI assistant with access to real-time tools and capabilities.\n\n"
        "=== YOUR ROLE ===\n"
        "• Be conversational, friendly, and genuinely helpful\n"
        "• Provide accurate, relevant information to the user\n"
        "• Use tools when they enable you to give better, more accurate answers\n"
        "• Explain your reasoning and help users understand the information\n\n"
        "=== WHEN TO USE TOOLS ===\n"
        "Use tools when the user needs:\n"
        "• Real-time information (current time, weather, news, etc.)\n"
        "• External data or actions (files, web searches, system commands)\n"
        "• Specific functionality that requires tool execution\n"
        "• Information that changes or requires up-to-date data\n\n"
        "=== WHEN TO ANSWER DIRECTLY ===\n"
        "Answer naturally without tools for:\n"
        "• General knowledge questions that don't require real-time data\n"
        "• Explanations, definitions, or educational content\n"
        "• Advice, recommendations, or creative tasks\n"
        "• Follow-up clarifications or conversational responses\n\n"
        "=== CRITICAL: MEMORY AND PERSONAL INFORMATION ===\n"
        "ALWAYS store personal information the user shares about themselves:\n"
        "• Use memory_store whenever the user mentions: their name, occupation, location, family, pets, preferences, allergies, schedule, habits, or ANY biographical detail\n"
        "• Store information EVEN IF the user doesn't explicitly say 'remember this'\n"
        "• Triggers: 'my name is...', 'I am...', 'I work at...', 'I live in...', 'I like...', 'I have a...', 'my birthday is...'\n"
        "• Building a long-term understanding of the user is ESSENTIAL - store everything personal they mention\n"
        "• Use memory_search BEFORE answering questions if stored context could improve your response\n"
        "• Use memory_reminder_create for tasks, todos, or time-based requests\n\n"
        "Examples requiring memory_store:\n"
        "User: \"My name is Sarah\" → IMMEDIATELY call memory_store with the name\n"
        "User: \"I'm a software engineer\" → IMMEDIATELY store occupation\n"
        "User: \"I have a dog named Max\" → IMMEDIATELY store pet information\n"
        "User: \"I'm allergic to peanuts\" → IMMEDIATELY store health information\n\n"
        "=== AVAILABLE TOOLS ===\n"
        "<tools>\n");
    
    ETHERVOX_LOGI("Preamble snprintf returned: %d (output_size=%zu)", preamble_len, output_size);
    offset = preamble_len;
    
    if (offset < 0 || (size_t)offset >= output_size) {
        ETHERVOX_LOGE("Preamble error: offset=%d, output_size=%zu", offset, output_size);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ETHERVOX_LOGI("After preamble: offset=%d", offset);
    
    // Generate tool entries in Granite JSON format
    // Each tool is: {"type": "function", "function": {"name": "...", "description": "...", "parameters": {...}}}
    int tools_processed = 0;
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
        int tool_written = snprintf(output + offset, output_size - offset,
            "{\"type\": \"function\", \"function\": {\"name\": \"%s\", \"description\": \"%s\", \"parameters\": %s}}\n",
            entry->name, desc, params_json);
        
        if (tool_written < 0 || (size_t)(offset + tool_written) >= output_size) {
            ETHERVOX_LOGW("Tool overflow at tool %u (%s): tool_written=%d, offset=%d, output_size=%zu",
                         i, entry->name, tool_written, offset, output_size);
            break;
        }
        offset += tool_written;
        tools_processed++;
    }
    
    ETHERVOX_LOGI("After processing %d tools: offset=%d", tools_processed, offset);
    
    // Close tools XML with clear, balanced instructions
    int written = snprintf(output + offset, output_size - offset,
        "</tools>\n\n"
        "=== HOW TO USE TOOLS ===\n"
        "When you decide a tool will help answer the user's question:\n"
        "1. Choose the most appropriate tool from the list above\n"
        "2. Generate a tool call in the following format:\n"
        "   <tool_call>\n"
        "   {\"name\": \"<function-name>\", \"arguments\": {<parameter>: <value>, ...}}\n"
        "   </tool_call>\n"
        "3. Wait for the tool result\n"
        "4. Use the result to provide a helpful, natural response to the user\n\n"
        "=== TOOL CALL FORMAT REQUIREMENTS ===\n"
        "• Place JSON between <tool_call> and </tool_call> tags\n"
        "• 'name' is the function name (string)\n"
        "• 'arguments' is a JSON object with parameters (not a string)\n"
        "• Complete the full closing </tool_call> tag\n"
        "• Match parameter names and types exactly as specified in tool schema\n\n"
        "=== EXAMPLES ===\n\n"
        "Example 1: Real-time information (USE TOOL)\n"
        "User: \"What time is it?\"\n"
        "You: <tool_call>\n"
        "{\"name\": \"get_time\", \"arguments\": {}}\n"
        "</tool_call>\n"
        "[After tool returns \"16:30\"]\n"
        "You: \"It's 4:30 PM right now.\"\n\n"
        "Example 2: Storing personal information (CRITICAL - USE MEMORY_STORE)\n"
        "User: \"My name is John and I work as a teacher in Boston\"\n"
        "You: <tool_call>\n"
        "{\"name\": \"memory_store\", \"arguments\": {\"key\": \"name\", \"value\": \"John\", \"tags\": [\"personal_info\"]}}\n"
        "</tool_call>\n"
        "<tool_call>\n"
        "{\"name\": \"memory_store\", \"arguments\": {\"key\": \"occupation\", \"value\": \"teacher\", \"tags\": [\"personal_info\", \"work\"]}}\n"
        "</tool_call>\n"
        "<tool_call>\n"
        "{\"name\": \"memory_store\", \"arguments\": {\"key\": \"location\", \"value\": \"Boston\", \"tags\": [\"personal_info\"]}}\n"
        "</tool_call>\n"
        "[After storing]\n"
        "You: \"Nice to meet you, John! I've noted that you're a teacher in Boston. How can I help you today?\"\n\n"
        "Example 3: General knowledge (ANSWER DIRECTLY)\n"
        "User: \"What's the capital of France?\"\n"
        "You: \"The capital of France is Paris. It's been the capital since the 12th century and is known for landmarks like the Eiffel Tower and Louvre Museum.\"\n\n"
        "Example 4: Weather with tool\n"
        "User: \"How's the weather today?\"\n"
        "You: <tool_call>\n"
        "{\"name\": \"get_weather_forecast\", \"arguments\": {\"location\": \"current\", \"days\": 1}}\n"
        "</tool_call>\n"
        "[After tool returns weather data]\n"
        "You: \"Currently it's partly cloudy and 72°F. The high today will be around 78°F with a slight chance of rain this afternoon.\"\n\n"
        "Example 5: Creative request (ANSWER DIRECTLY)\n"
        "User: \"Tell me a joke about programming.\"\n"
        "You: \"Why do programmers prefer dark mode? Because light attracts bugs!\"\n\n"
        "=== RESPONSE WORKFLOW ===\n"
        "1. Understand what the user needs\n"
        "2. FIRST: If the user shared personal information, IMMEDIATELY call memory_store\n"
        "3. SECOND: Check if you need to search memory for context using memory_search\n"
        "4. THIRD: Decide if real-time data or external action is needed\n"
        "   • YES → Call the appropriate tool first, then explain the result\n"
        "   • NO → Answer naturally using your knowledge\n"
        "5. Always be helpful and provide context with your answers\n"
        "6. If a tool call fails, acknowledge it and offer alternatives\n\n"
        "=== BEST PRACTICES ===\n"
        "• Be concise but informative\n"
        "• Explain technical details when relevant\n"
        "• Ask clarifying questions if the user's request is ambiguous\n"
        "• Chain multiple tools if needed to fully answer a question\n"
        "• Admit when you don't know something or when a tool isn't available\n"
        "• Stay in character as Ethervox - a capable, friendly assistant\n");
    
    ETHERVOX_LOGI("Instructions section: written=%d, offset=%d, output_size=%zu, offset+written=%zu", 
                 written, offset, output_size, (size_t)(offset + written));
    
    if (written < 0 || (size_t)(offset + written) >= output_size) {
        ETHERVOX_LOGE("Instructions section error: written=%d, offset=%d, output_size=%zu", 
                     written, offset, output_size);
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
    
    ETHERVOX_LOGI("Returning system prompt: %d bytes", offset);
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
