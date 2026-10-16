/**
 * @file chat_template.c
 * @brief Chat template implementation for various LLM models
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/chat_template.h"
#include "ethervox/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

// ============================================================================
// Template Definitions
// ============================================================================

// Qwen 2.5 / ChatML template
static const chat_template_t qwen_template = {
    .type = CHAT_TEMPLATE_QWEN,
    .system_start = "<|im_start|>system\n",
    .system_end = "<|im_end|>\n",
    .user_start = "<|im_start|>user\n",
    .user_end = "<|im_end|>\n",
    .assistant_start = "<|im_start|>assistant\n",
    .assistant_end = "<|im_end|>\n",
    .tool_result_start = "<|im_start|>user\n<tool_result>",
    .tool_result_end = "</tool_result><|im_end|>\n<|im_start|>assistant\n",
    .stop_sequences = {
        "<|im_end|>",
        "<|im_start|>",
        "im_end",
        "im_start",
        NULL
    },
    .stop_sequence_count = 4,
    .tool_format = TOOL_FORMAT_XML_ATTR,
    .ignore_eog = false
};

// IBM Granite 3.x template
static const chat_template_t granite3_template = {
    .type = CHAT_TEMPLATE_GRANITE_3,
    .system_start = "<|start_of_role|>system<|end_of_role|>",
    .system_end = "<|end_of_text|>\n",
    .user_start = "<|start_of_role|>user<|end_of_role|>",
    .user_end = "<|end_of_text|>\n",
    .assistant_start = "<|start_of_role|>assistant<|end_of_role|>",
    .assistant_end = "<|end_of_text|>\n",
    .tool_result_start = "<|start_of_role|>user<|end_of_role|><tool_result>",
    .tool_result_end = "</tool_result><|end_of_text|>\n<|start_of_role|>assistant<|end_of_role|>",
    .stop_sequences = {
        "<|end_of_text|>",
        "<|start_of_role|>",
        "<|end_of_role|>",
        "<tool_result",
        NULL
    },
    .stop_sequence_count = 4,
    .tool_format = TOOL_FORMAT_JSON_IN_XML,
    .ignore_eog = false
};

// IBM Granite 4.0/4.1 template
static const chat_template_t granite4_template = {
    .type = CHAT_TEMPLATE_GRANITE_4,
    .system_start = "<|start_of_role|>system<|end_of_role|>",
    .system_end = "<|end_of_text|>\n",
    .user_start = "<|start_of_role|>user<|end_of_role|>",
    .user_end = "<|end_of_text|>\n",
    .assistant_start = "<|start_of_role|>assistant<|end_of_role|>",
    .assistant_end = "<|end_of_text|>\n",
    .tool_result_start = "<|start_of_role|>user<|end_of_role|><tool_result>",
    .tool_result_end = "</tool_result><|end_of_text|>\n<|start_of_role|>assistant<|end_of_role|>",
    .tool_response_start = "<|start_of_role|>user<|end_of_role|>\n<tool_response>\n",
    .tool_response_end = "\n</tool_response><|end_of_text|>\n",
    .stop_sequences = {
        "<|end_of_text|>",
        "<|start_of_role|>",
        "<|end_of_role|>",
        "<tool_result",
        NULL
    },
    .stop_sequence_count = 4,
    .tool_format = TOOL_FORMAT_JSON_IN_XML,
    .ignore_eog = false
};

// IBM Granite 4.2+ template (chat_template.jinja of ibm-granite/granite-4.2-*): ChatML roles,
// <think>...</think> reasoning, and tool calls as <tool_call><function=NAME><parameter=K>V.
// Tool results are a user turn holding <tool_response>.
static const chat_template_t granite42_template = {
    .type = CHAT_TEMPLATE_GRANITE_4_2,
    .system_start = "<|im_start|>system\n",
    .system_end = "<|im_end|>\n",
    .user_start = "<|im_start|>user\n",
    .user_end = "<|im_end|>\n",
    .assistant_start = "<|im_start|>assistant\n",
    .assistant_end = "<|im_end|>\n",
    .tool_result_start = "<|im_start|>user\n<tool_response>\n",
    .tool_result_end = "\n</tool_response>\n<|im_end|>\n<|im_start|>assistant\n",
    .tool_response_start = "<|im_start|>user\n<tool_response>\n",
    .tool_response_end = "\n</tool_response>\n<|im_end|>\n",
    .stop_sequences = {
        "<|im_end|>",
        "<|im_start|>",
        NULL
    },
    .stop_sequence_count = 2,
    .tool_format = TOOL_FORMAT_JSON_IN_XML,
    .ignore_eog = false
};

// Microsoft Phi template
static const chat_template_t phi_template = {
    .type = CHAT_TEMPLATE_PHI,
    .system_start = "<|system|>\n",
    .system_end = "<|end|>\n",
    .user_start = "<|user|>\n",
    .user_end = "<|end|>\n",
    .assistant_start = "<|assistant|>\n",
    .assistant_end = "<|end|>\n",
    .tool_result_start = "<|user|>\n<tool_result>",
    .tool_result_end = "</tool_result><|end|>\n<|assistant|>\n",
    .stop_sequences = {
        "<|end|>",
        "<|system|>",
        "<|user|>",
        "<|assistant|>",
        NULL
    },
    .stop_sequence_count = 4,
    .tool_format = TOOL_FORMAT_XML_ATTR,  // Uses XML with attributes
    .ignore_eog = false  // Respect model's EOG decision
};

// Meta Llama 3 template
static const chat_template_t llama3_template = {
    .type = CHAT_TEMPLATE_LLAMA3,
    .system_start = "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n",
    .system_end = "<|eot_id|>",
    .user_start = "<|start_header_id|>user<|end_header_id|>\n\n",
    .user_end = "<|eot_id|>",
    .assistant_start = "<|start_header_id|>assistant<|end_header_id|>\n\n",
    .assistant_end = "<|eot_id|>",
    .tool_result_start = "<|start_header_id|>user<|end_header_id|>\n\n<tool_result>",
    .tool_result_end = "</tool_result><|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n",
    .stop_sequences = {
        "<|eot_id|>",
        "<|end_header_id|>",
        NULL
    },
    .stop_sequence_count = 2,
    .tool_format = TOOL_FORMAT_XML_ATTR,  // Uses XML with attributes
    .ignore_eog = false  // Respect model's EOG decision
};

// Liquid AI LFM2.5 template (ChatML with native tool calling)
static const chat_template_t lfm_template = {
    .type = CHAT_TEMPLATE_LFM,
    .system_start = "<|startoftext|><|im_start|>system\n",
    .system_end = "<|im_end|>\n",
    .user_start = "<|im_start|>user\n",
    .user_end = "<|im_end|>\n",
    .assistant_start = "<|im_start|>assistant\n",
    .assistant_end = "<|im_end|>\n",
    .tool_result_start = "<|im_start|>tool\n",
    .tool_result_end = "<|im_end|>\n<|im_start|>assistant\n",
    .stop_sequences = {
        "<|im_end|>",
        "<|im_start|>",
        "<|tool_call_end|>",
        "im_end",
        "im_start",
        NULL
    },
    .stop_sequence_count = 5,
    .tool_format = TOOL_FORMAT_JSON_ONLY,  // LFM2.5 uses pure Pythonic function calls between special tokens
    .ignore_eog = false  // Respect model's EOG decision
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Convert string to lowercase for case-insensitive matching
 */
static void str_tolower(char* dst, const char* src, size_t max_len) {
    size_t i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dst[i] = tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

// ============================================================================
// Public API
// ============================================================================

chat_template_type_t chat_template_detect(const char* model_path) {
    if (!model_path) return CHAT_TEMPLATE_QWEN;  // Default fallback
    
    char lower_path[512];
    str_tolower(lower_path, model_path, sizeof(lower_path));
    
    // Check for LFM (Liquid AI)
    if (strstr(lower_path, "lfm") || strstr(lower_path, "liquid")) {
        return CHAT_TEMPLATE_LFM;
    }
    
    // Check for Granite
    if (strstr(lower_path, "granite")) {
        // Granite 4.2 and later: reasoning models with their own (ChatML) format.
        const char* four = strstr(lower_path, "granite-4.");
        if (!four) four = strstr(lower_path, "granite_4.");
        if (four && atoi(four + strlen("granite-4.")) >= 2) {
            return CHAT_TEMPLATE_GRANITE_4_2;
        }
        // Check for version 3
        if (strstr(lower_path, "granite-3") || strstr(lower_path, "granite_3")) {
            return CHAT_TEMPLATE_GRANITE_3;
        }
        // Default to Granite 4 for unversioned or version 4+
        return CHAT_TEMPLATE_GRANITE_4;
    }
    
    // Check for Qwen
    if (strstr(lower_path, "qwen")) {
        return CHAT_TEMPLATE_QWEN;
    }
    
    // Check for Phi
    if (strstr(lower_path, "phi")) {
        return CHAT_TEMPLATE_PHI;
    }
    
    // Check for Llama
    if (strstr(lower_path, "llama-3") || strstr(lower_path, "llama3")) {
        return CHAT_TEMPLATE_LLAMA3;
    }
    
    // Default to Qwen for unknown models
    return CHAT_TEMPLATE_QWEN;
}

bool chat_template_supports_thinking(const chat_template_t* tmpl) {
    return tmpl && tmpl->type == CHAT_TEMPLATE_GRANITE_4_2;
}

const chat_template_t* chat_template_get(chat_template_type_t type, const char* model_path) {
    // Auto-detect if requested
    if (type == CHAT_TEMPLATE_AUTO && model_path) {
        type = chat_template_detect(model_path);
    }
    
    switch (type) {
        case CHAT_TEMPLATE_LFM:
            return &lfm_template;
        case CHAT_TEMPLATE_GRANITE_3:
            return &granite3_template;
        case CHAT_TEMPLATE_GRANITE_4:  // CHAT_TEMPLATE_GRANITE is an alias for this
            return &granite4_template;
        case CHAT_TEMPLATE_GRANITE_4_2:
            return &granite42_template;
        case CHAT_TEMPLATE_PHI:
            return &phi_template;
        case CHAT_TEMPLATE_LLAMA3:
            return &llama3_template;
        case CHAT_TEMPLATE_QWEN:  // CHAT_TEMPLATE_CHATML is an alias for this
        default:
            return &qwen_template;
    }
}

ethervox_result_t chat_template_format_system(
    const chat_template_t* tmpl,
    const char* content,
    char* output,
    size_t output_size
) {
    if (!tmpl || !content || !output || output_size == 0) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    int written = snprintf(output, output_size, "%s%s%s",
                           tmpl->system_start,
                           content,
                           tmpl->system_end);
    return (written >= 0 && (size_t)written < output_size) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_BUFFER_TOO_SMALL;
}

ethervox_result_t chat_template_format_user(
    const chat_template_t* tmpl,
    const char* content,
    char* output,
    size_t output_size
) {
    if (!tmpl || !content || !output || output_size == 0) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    int written = snprintf(output, output_size, "%s%s%s",
                           tmpl->user_start,
                           content,
                           tmpl->user_end);
    return (written >= 0 && (size_t)written < output_size) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_BUFFER_TOO_SMALL;
}

ethervox_result_t chat_template_format_assistant_start(
    const chat_template_t* tmpl,
    char* output,
    size_t output_size
) {
    if (!tmpl || !output || output_size == 0) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    int written = snprintf(output, output_size, "%s", tmpl->assistant_start);
    return (written >= 0 && (size_t)written < output_size) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_BUFFER_TOO_SMALL;
}

ethervox_result_t chat_template_format_tool_result(
    const chat_template_t* tmpl,
    const char* result,
    char* output,
    size_t output_size
) {
    if (!tmpl || !result || !output || output_size == 0) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    int written = snprintf(output, output_size, "%s%s%s",
                           tmpl->tool_result_start,
                           result,
                           tmpl->tool_result_end);
    return (written >= 0 && (size_t)written < output_size) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_BUFFER_TOO_SMALL;
}

ethervox_result_t chat_template_format_tool_response(
    const chat_template_t* tmpl,
    const char* result,
    char* output,
    size_t output_size
) {
    if (!tmpl || !result || !output || output_size == 0) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    if (!tmpl->tool_response_start || !tmpl->tool_response_end) {
        return chat_template_format_tool_result(tmpl, result, output, output_size);
    }

    int written = snprintf(output, output_size, "%s%s%s",
                           tmpl->tool_response_start,
                           result,
                           tmpl->tool_response_end);
    return (written >= 0 && (size_t)written < output_size) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_BUFFER_TOO_SMALL;
}

bool chat_template_has_stop_sequence(
    const chat_template_t* tmpl,
    const char* text
) {
    if (!tmpl || !text) return false;
    
    // Use the template's stop sequences directly
    for (int i = 0; i < tmpl->stop_sequence_count && tmpl->stop_sequences[i] != NULL; i++) {
        if (strstr(text, tmpl->stop_sequences[i])) {
            return true;
        }
    }
    
    return false;
}

tool_format_type_t chat_template_get_tool_format(
    const chat_template_t* tmpl
) {
    if (!tmpl) {
        return TOOL_FORMAT_XML_ATTR;  // Default fallback
    }
    return tmpl->tool_format;
}
