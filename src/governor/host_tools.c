/**
 * @file host_tools.c
 * @brief Host-registered tools implementation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/host_tools.h"
#include "ethervox/tool_manifest.h"
#include "ethervox/logging.h"
#include <stdlib.h>
#include <string.h>

/**
 * Internal host tool entry (linked list)
 */
typedef struct host_tool_entry {
    char* name;
    char* description;
    char* parameters_schema_json;
    bool is_mutating;
    ethervox_host_tool_fn invoke;
    void* user_data;
    struct host_tool_entry* next;
} host_tool_entry_t;

/**
 * Get host tools list from registry
 */
static host_tool_entry_t** get_host_tools_list(tool_manifest_registry_t* registry) {
    return (host_tool_entry_t**)&registry->host_tools;
}

ethervox_result_t ethervox_tool_registry_register_host_tool(
    tool_manifest_registry_t* registry,
    const ethervox_host_tool_t* tool
) {
    if (!registry || !tool) {
        ETHERVOX_LOG_ERROR("[HostTools] NULL registry or tool");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (!tool->name || !tool->description || !tool->parameters_schema_json || !tool->invoke) {
        ETHERVOX_LOG_ERROR("[HostTools] Invalid tool: missing required fields");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check for duplicate names in host tools
    host_tool_entry_t** list_head = get_host_tools_list(registry);
    host_tool_entry_t* entry = *list_head;
    while (entry) {
        if (strcmp(entry->name, tool->name) == 0) {
            ETHERVOX_LOG_ERROR("[HostTools] Tool '%s' already registered", tool->name);
            return ETHERVOX_ERROR_ALREADY_EXISTS;
        }
        entry = entry->next;
    }
    
    // TODO: Also check built-in tools for conflicts
    
    // Allocate new entry
    host_tool_entry_t* new_entry = (host_tool_entry_t*)calloc(1, sizeof(host_tool_entry_t));
    if (!new_entry) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy strings
    new_entry->name = strdup(tool->name);
    new_entry->description = strdup(tool->description);
    new_entry->parameters_schema_json = strdup(tool->parameters_schema_json);
    
    if (!new_entry->name || !new_entry->description || !new_entry->parameters_schema_json) {
        free(new_entry->name);
        free(new_entry->description);
        free(new_entry->parameters_schema_json);
        free(new_entry);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // Store callback and metadata
    new_entry->is_mutating = tool->is_mutating;
    new_entry->invoke = tool->invoke;
    new_entry->user_data = tool->user_data;
    
    // Add to linked list
    new_entry->next = *list_head;
    *list_head = new_entry;
    
    ETHERVOX_LOG_INFO("[HostTools] Registered tool '%s' (mutating=%d)", 
                     tool->name, tool->is_mutating);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_tool_registry_set_timeout(
    tool_manifest_registry_t* registry,
    uint32_t timeout_ms
) {
    if (!registry) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    registry->host_tool_timeout_ms = timeout_ms;
    
    ETHERVOX_LOG_INFO("[HostTools] Set timeout: %u ms", timeout_ms);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_tool_registry_clear_host_tools(
    tool_manifest_registry_t* registry
) {
    if (!registry) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    host_tool_entry_t** list_head = get_host_tools_list(registry);
    host_tool_entry_t* entry = *list_head;
    
    int count = 0;
    while (entry) {
        host_tool_entry_t* next = entry->next;
        
        free(entry->name);
        free(entry->description);
        free(entry->parameters_schema_json);
        free(entry);
        
        entry = next;
        count++;
    }
    
    *list_head = NULL;
    
    ETHERVOX_LOG_INFO("[HostTools] Cleared %d host tools", count);
    
    return ETHERVOX_SUCCESS;
}

void ethervox_string_free(char* str) {
    free(str);
}

/**
 * Find a host tool by name
 */
host_tool_entry_t* ethervox_host_tool_find(
    const tool_manifest_registry_t* registry,
    const char* name
) {
    if (!registry || !name) {
        return NULL;
    }
    
    host_tool_entry_t** list_head = (host_tool_entry_t**)get_host_tools_list((tool_manifest_registry_t*)registry);
    host_tool_entry_t* entry = *list_head;
    
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/**
 * Check if a tool is mutating
 */
bool ethervox_host_tool_is_mutating(
    const tool_manifest_registry_t* registry,
    const char* name
) {
    host_tool_entry_t* tool = ethervox_host_tool_find(registry, name);
    return tool ? tool->is_mutating : false;
}

bool ethervox_host_tool_exists(
    const tool_manifest_registry_t* registry,
    const char* name
) {
    return ethervox_host_tool_find(registry, name) != NULL;
}

/**
 * Invoke a host tool
 */
ethervox_result_t ethervox_host_tool_invoke(
    const tool_manifest_registry_t* registry,
    const char* name,
    const char* arguments_json,
    char** out_result_json,
    char** out_error_message
) {
    if (!registry || !name || !arguments_json || !out_result_json || !out_error_message) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    host_tool_entry_t* tool = ethervox_host_tool_find(registry, name);
    if (!tool) {
        ETHERVOX_LOG_ERROR("[HostTools] Tool '%s' not found", name);
        return ETHERVOX_ERROR_NOT_FOUND;
    }
    
    if (tool->is_mutating) {
        ETHERVOX_LOG_ERROR("[HostTools] Attempted to auto-invoke mutating tool '%s' (ADR-0007 violation)", name);
        return ETHERVOX_ERROR_PERMISSION_DENIED;
    }
    
    // TODO: Reentrancy check - verify caller isn't holding model lock
    
    // Invoke callback
    ETHERVOX_LOG_DEBUG("[HostTools] Invoking tool '%s'", name);
    ethervox_result_t result = tool->invoke(arguments_json, tool->user_data, 
                                           out_result_json, out_error_message);
    
    // Verify exactly one output is set
    if (*out_result_json && *out_error_message) {
        ETHERVOX_LOG_ERROR("[HostTools] Tool '%s' returned both result and error", name);
        ethervox_string_free(*out_result_json);
        ethervox_string_free(*out_error_message);
        *out_result_json = NULL;
        *out_error_message = NULL;
        return ETHERVOX_ERROR_INVALID_STATE;
    }
    
    if (!*out_result_json && !*out_error_message) {
        ETHERVOX_LOG_ERROR("[HostTools] Tool '%s' returned neither result nor error", name);
        return ETHERVOX_ERROR_INVALID_STATE;
    }
    
    return result;
}
