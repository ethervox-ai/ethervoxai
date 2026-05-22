/**
 * @file chat_template.h
 * @brief Flexible chat template system for different LLM models
 *
 * Provides model-specific chat formatting for various LLMs including
 * Qwen, Granite, Phi, Llama, and others.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_CHAT_TEMPLATE_H
#define ETHERVOX_CHAT_TEMPLATE_H

#include "ethervox/error.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tool calling format types
 * Defines how models expect tools to be defined and called
 */
typedef enum {
    TOOL_FORMAT_XML_ATTR,       // XML with attributes: <tool_call name="calc" expr="5+5" />
    TOOL_FORMAT_JSON_IN_XML,    // JSON inside XML: <tool_call>{"name":"calc","arguments":{"expr":"5+5"}}</tool_call>
    TOOL_FORMAT_JSON_ONLY,      // Pure JSON: {"name":"calc","arguments":{"expr":"5+5"}}
    TOOL_FORMAT_AUTO            // Auto-detect from template type
} tool_format_type_t;

/**
 * Supported chat template formats
 */
typedef enum {
    CHAT_TEMPLATE_QWEN,      // Qwen 2.5 format: <|im_start|>role\n...<|im_end|>
    CHAT_TEMPLATE_GRANITE,   // IBM Granite format: <|system|>, <|user|>, <|assistant|>
    CHAT_TEMPLATE_PHI,       // Microsoft Phi format: <|system|>...<|end|><|user|>...<|end|>
    CHAT_TEMPLATE_LLAMA3,    // Meta Llama 3 format: <|begin_of_text|><|start_header_id|>role<|end_header_id|>
    CHAT_TEMPLATE_LFM,       // Liquid AI LFM2.5 format: ChatML with tool calling tokens
    CHAT_TEMPLATE_AUTO       // Auto-detect from model name/metadata
} chat_template_type_t;

/**
 * Chat template definition
 * Contains all formatting strings for a specific model family
 */
typedef struct {
    chat_template_type_t type;
    
    // System message
    const char* system_start;
    const char* system_end;
    
    // User message
    const char* user_start;
    const char* user_end;
    
    // Assistant message
    const char* assistant_start;
    const char* assistant_end;
    
    // Tool result wrapper
    const char* tool_result_start;
    const char* tool_result_end;
    
    // Stop sequences for generation
    const char* stop_sequences[8];
    int stop_sequence_count;
    
    // Tool-calling format and instructions (model-specific)
    tool_format_type_t tool_format;         // How this model expects tool definitions and calls
    const char* tool_system_instructions;   // Instructions for tool usage (goes before tool list)
    const char* tool_usage_examples;        // Model-specific examples showing tool call patterns
    bool requires_explicit_directives;      // If true, use MUST/ALWAYS/NEVER language
    bool prefers_tool_first_response;       // If true, examples show tool call before explanation
    
} chat_template_t;

/**
 * Get chat template for a specific model type
 * 
 * @param type Template type (or CHAT_TEMPLATE_AUTO to detect)
 * @param model_path Path to model file (used for auto-detection)
 * @return Chat template structure
 */
const chat_template_t* chat_template_get(chat_template_type_t type, const char* model_path);

/**
 * Auto-detect chat template from model filename
 * 
 * @param model_path Path to model file
 * @return Detected template type
 */
chat_template_type_t chat_template_detect(const char* model_path);

/**
 * Format a system message
 * 
 * @param template Chat template to use
 * @param content System message content
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t chat_template_format_system(
    const chat_template_t* template,
    const char* content,
    char* output,
    size_t output_size
);

/**
 * Format a user message
 * 
 * @param template Chat template to use
 * @param content User message content
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t chat_template_format_user(
    const chat_template_t* template,
    const char* content,
    char* output,
    size_t output_size
);

/**
 * Format an assistant message start (for generation)
 * 
 * @param template Chat template to use
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t chat_template_format_assistant_start(
    const chat_template_t* template,
    char* output,
    size_t output_size
);

/**
 * Format a tool result message
 * 
 * @param template Chat template to use
 * @param result Tool result content
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t chat_template_format_tool_result(
    const chat_template_t* template,
    const char* result,
    char* output,
    size_t output_size
);

/**
 * Check if text contains any stop sequence
 * 
 * @param template Chat template with stop sequences
 * @param text Text to check
 * @return true if stop sequence found, false otherwise
 */
bool chat_template_has_stop_sequence(
    const chat_template_t* template,
    const char* text
);

/**
 * Get the tool calling format for a chat template
 * 
 * @param template Chat template to query
 * @return Tool format type (TOOL_FORMAT_XML_ATTR, TOOL_FORMAT_JSON_IN_XML, etc.)
 */
tool_format_type_t chat_template_get_tool_format(
    const chat_template_t* template
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_CHAT_TEMPLATE_H
