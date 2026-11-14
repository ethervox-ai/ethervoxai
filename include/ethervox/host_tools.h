/**
 * @file host_tools.h
 * @brief Host-registered tools API for shell/engine integration
 *
 * Allows shells and the engine layer to register custom tools with the backend.
 * Enforces the `is_mutating` refusal rule (ADR-0007) at the C level: tools marked
 * `is_mutating=true` surface as ETHERVOX_EVENT_TOOL_CALL_REQUESTED instead of being
 * auto-invoked, giving the host control over file operations and preventing prompt
 * injection from modifying user data.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_HOST_TOOLS_H
#define ETHERVOX_HOST_TOOLS_H

#include "ethervox/error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Forward declaration of tool manifest registry
 */
typedef struct tool_manifest_registry tool_manifest_registry_t;

/**
 * Host tool callback function signature
 *
 * The host implements this function to execute the tool. It receives JSON arguments
 * and must return JSON result or an error message.
 *
 * @param arguments_json JSON string with tool arguments (null-terminated)
 * @param user_data User-provided context (set in ethervox_host_tool_t)
 * @param out_result_json Output: JSON result string (allocated by callee, freed by caller via ethervox_string_free)
 * @param out_error_message Output: Error message if failed (allocated by callee, freed by caller via ethervox_string_free)
 * @return ETHERVOX_SUCCESS on success, error code on failure
 *
 * @note Exactly ONE of out_result_json or out_error_message must be set (not both, not neither)
 * @note The callback must NOT call back into the same model handle (reentrancy check enforced)
 * @note The callback should complete within the configured timeout (default: no timeout)
 */
typedef ethervox_result_t (*ethervox_host_tool_fn)(
    const char* arguments_json,
    void* user_data,
    char** out_result_json,
    char** out_error_message
);

/**
 * Host tool descriptor
 *
 * Describes a tool registered by the host (shell or engine). Combined with built-in
 * tools to form the complete manifest sent to the LLM.
 */
typedef struct {
    /** Tool name (must be unique across built-in and host tools) */
    const char* name;
    
    /** Human-readable description (shown to the LLM) */
    const char* description;
    
    /** JSON Schema for parameters (must be valid JSON Schema object)
     *  Example: {"type":"object","properties":{"query":{"type":"string"}},
     *           "required":["query"]} */
    const char* parameters_schema_json;
    
    /** If true, this tool is NEVER auto-invoked by the backend. Instead,
     *  when the LLM requests it, the backend emits ETHERVOX_EVENT_TOOL_CALL_REQUESTED
     *  and waits for the host to decide whether to execute it.
     *  
     *  This enforces ADR-0007 (Preview → Approve → Apply) at the C level:
     *  tools that mutate user data (create_plan, apply_plan, delete_file, etc.)
     *  must go through explicit user approval, preventing prompt injection attacks. */
    bool is_mutating;
    
    /** Callback function to invoke the tool */
    ethervox_host_tool_fn invoke;
    
    /** User-provided context passed to invoke callback */
    void* user_data;
} ethervox_host_tool_t;

/**
 * Register a host-provided tool
 *
 * Adds a tool to the registry, making it available to the LLM. The tool's JSON Schema
 * will be included in the system prompt, and requests for this tool will be routed to
 * the provided callback.
 *
 * @param registry Tool manifest registry
 * @param tool Tool descriptor (strings are copied, callbacks are stored)
 * @return ETHERVOX_SUCCESS, ETHERVOX_ERROR_INVALID_ARGUMENT if tool is NULL/invalid,
 *         ETHERVOX_ERROR_ALREADY_EXISTS if tool name conflicts
 *
 * @note Thread-safe: can be called before or during inference
 * @note The tool descriptor strings (name, description, schema) are copied; the caller
 *       retains ownership of the original ethervox_host_tool_t struct
 */
ethervox_result_t ethervox_tool_registry_register_host_tool(
    tool_manifest_registry_t* registry,
    const ethervox_host_tool_t* tool
);

/**
 * Set timeout for tool invocations
 *
 * If a tool takes longer than this timeout, the invocation is cancelled and an error
 * is returned. Default: 0 (no timeout).
 *
 * @param registry Tool manifest registry
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return ETHERVOX_SUCCESS or error code
 *
 * @note Timeout enforcement is best-effort; actual cancellation depends on the tool
 *       implementation checking cancellation tokens
 */
ethervox_result_t ethervox_tool_registry_set_timeout(
    tool_manifest_registry_t* registry,
    uint32_t timeout_ms
);

/**
 * Clear all host-registered tools
 *
 * Removes all host tools from the registry. Built-in tools are NOT affected.
 * Useful when rebuilding the tool set or shutting down.
 *
 * @param registry Tool manifest registry
 * @return ETHERVOX_SUCCESS or error code
 *
 * @note Thread-safe: can be called during inference (tools in-flight will complete)
 */
ethervox_result_t ethervox_tool_registry_clear_host_tools(
    tool_manifest_registry_t* registry
);

/**
 * Free a string allocated by the backend
 *
 * Use this to free strings returned in out_result_json or out_error_message from
 * tool invocations, or any other string allocated by ethervox_core and returned to
 * the caller.
 *
 * @param str String to free (NULL is safe)
 *
 * @note This is a thin wrapper around free() for ABI stability across compilers
 */
void ethervox_string_free(char* str);

/**
 * Check if a tool is marked as mutating (internal API for governor)
 *
 * @param registry Tool manifest registry
 * @param name Tool name
 * @return true if tool is mutating, false otherwise
 */
bool ethervox_host_tool_is_mutating(
    const tool_manifest_registry_t* registry,
    const char* name
);

/**
 * Check whether a host tool with this name is registered (internal API for governor).
 *
 * `ethervox_host_tool_is_mutating()` alone cannot distinguish "not registered" from
 * "registered, not mutating" - both return false. Callers that need to fall back to a
 * built-in tool registry when a host tool doesn't exist must check this first.
 *
 * @param registry Tool manifest registry
 * @param name Tool name
 * @return true if a host tool with this name is registered
 */
bool ethervox_host_tool_exists(
    const tool_manifest_registry_t* registry,
    const char* name
);

/**
 * Invoke a host tool (internal API for governor)
 *
 * @param registry Tool manifest registry
 * @param name Tool name
 * @param arguments_json JSON arguments string
 * @param out_result_json Output: JSON result (caller must free via ethervox_string_free)
 * @param out_error_message Output: Error message (caller must free via ethervox_string_free)
 * @return ETHERVOX_SUCCESS, ETHERVOX_ERROR_NOT_FOUND, ETHERVOX_ERROR_PERMISSION_DENIED (if mutating), etc.
 */
ethervox_result_t ethervox_host_tool_invoke(
    const tool_manifest_registry_t* registry,
    const char* name,
    const char* arguments_json,
    char** out_result_json,
    char** out_error_message
);

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_HOST_TOOLS_H */
