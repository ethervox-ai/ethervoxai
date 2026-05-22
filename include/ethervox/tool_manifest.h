/**
 * @file tool_manifest.h
 * @brief Tool Manifest System - Binary manifest format and API
 *
 * Scalable tool management using binary manifests with optimized prompts.
 * Reduces KV cache bloat by storing tools externally and loading on-demand.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_TOOL_MANIFEST_H
#define ETHERVOX_TOOL_MANIFEST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define TOOL_MANIFEST_MAGIC 0x45544F4C  // "ETOL" in little-endian
#define TOOL_MANIFEST_VERSION 1

#define TOOL_NAME_MAX 64
#define TOOL_DESC_MAX 256
#define TOOL_CATEGORY_MAX 32
#define TOOL_VERSION_MAX 32
#define MAX_PARAMETERS 32
#define MAX_TRIGGERS 16

// ============================================================================
// Binary Manifest Structures (Portable Format)
// ============================================================================

/**
 * File header (64 bytes, naturally aligned)
 */
typedef struct {
    uint32_t magic;              // 0x45544F4C ("ETOL")
    uint32_t version;            // Format version (currently 1)
    uint32_t tool_count;         // Number of tools
    uint32_t index_offset;       // Byte offset to index section
    uint32_t detail_offset;      // Byte offset to detail section
    uint8_t checksum_type;       // 1=CRC32, 2=SHA256
    uint8_t reserved1[3];        // Padding for alignment
    uint64_t timestamp;          // Unix timestamp of creation
    uint64_t file_size;          // Total file size for validation
    uint8_t reserved2[28];       // Future expansion
} tool_manifest_header_t;

/**
 * Tool index entry (for quick lookup and system prompt generation)
 */
typedef struct {
    char name[TOOL_NAME_MAX];          // Tool name (null-terminated)
    char one_line[TOOL_DESC_MAX];      // One-line description (fallback)
    char category[TOOL_CATEGORY_MAX];  // Category (e.g., "memory", "compute")
    uint8_t priority;                  // 0=highest, 255=lowest
    uint8_t enabled;                   // 1=enabled, 0=disabled
    uint16_t detail_size;              // Size of detail record in bytes
    uint32_t detail_offset;            // Offset to full detail record
    uint8_t reserved[10];              // Future expansion
} tool_index_entry_t;

/**
 * Full tool detail (complete schema for validation)
 */
typedef struct {
    char name[TOOL_NAME_MAX];
    char description[TOOL_DESC_MAX];
    char category[TOOL_CATEGORY_MAX];
    char version[TOOL_VERSION_MAX];
    
    uint8_t priority;
    uint8_t requires_confirmation;
    uint8_t param_count;
    uint8_t trigger_count;
    
    char triggers[MAX_TRIGGERS][64];  // Trigger phrases
    
    // Parameters stored as variable-length structs
    // Actual struct defined below
    uint8_t reserved[32];             // Future expansion
} tool_detail_header_t;

/**
 * Parameter definition (follows tool_detail_header_t)
 */
typedef struct {
    char name[32];
    char type[16];                // "string", "number", "boolean", "array"
    char description[128];
    uint8_t required;
    uint8_t reserved[3];
    char default_value[64];
} tool_param_t;

/**
 * File footer (checksum)
 */
typedef struct {
    uint8_t checksum_type;            // 1=CRC32, 2=SHA256
    union {
        uint32_t crc32;               // CRC32 checksum
        uint8_t sha256[32];           // SHA-256 hash
    } checksum;
} tool_manifest_footer_t;

// ============================================================================
// Optimized Prompts (JSON Cache)
// ============================================================================

/**
 * Optimized prompt for a single tool (loaded from JSON at startup)
 */
typedef struct {
    char* tool_name;                  // Tool name (reference to binary manifest)
    char* optimized_prompt;           // Model-specific optimized prompt (heap)
    uint32_t token_count;             // Estimated token count
} tool_optimized_prompt_t;

/**
 * Model-specific optimized prompts cache (loaded from JSON)
 */
typedef struct {
    char* model_name;                 // Model identifier
    uint32_t prompt_count;            // Number of optimized prompts
    tool_optimized_prompt_t* prompts; // Array of optimized prompts
} tool_optimized_cache_t;

// ============================================================================
// Tool Manifest Registry (Runtime State)
// ============================================================================

/**
 * Tool manifest registry (manages binary manifest + optimized prompts)
 */
typedef struct tool_manifest_registry {
    // Binary manifest (loaded via fread, stays in memory)
    FILE* manifest_file;              // File handle (stays open for fast access)
    tool_manifest_header_t header;    // Header (cached in memory)
    tool_index_entry_t* index;        // Index array (cached in memory)
    
    // Optimized prompts (loaded from JSON at startup)
    tool_optimized_cache_t* optimized_cache; // NULL if not loaded
    
    // Runtime state
    bool needs_byte_swap;             // True if file endianness differs
    bool tools_available;             // True if manifest loaded successfully
    uint8_t fallback_level;           // 0=optimal, 1=one-liners, 2=LLM-only, 3=emergency
    bool name_only_mode;              // True to list ONLY names (model uses get_tool_info for schemas)
    
    // Guard system - tracks whether tools are actually usable
    bool tools_detected;              // True if tools.bin was found (tools exist)
    bool optimization_loaded;         // True if optimized JSON prompts loaded
    uint16_t tools_loaded_count;      // Number of tools actually loaded into system prompt
} tool_manifest_registry_t;

// ============================================================================
// Core API
// ============================================================================

/**
 * Initialize tool manifest registry
 * 
 * @param registry Registry to initialize
 * @param binary_path Path to tools.bin file
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_tool_manifest_init(
    tool_manifest_registry_t* registry,
    const char* binary_path
);

/**
 * Cleanup and free resources
 */
void ethervox_tool_manifest_cleanup(tool_manifest_registry_t* registry);

/**
 * Load optimized prompts from JSON (optional, for performance)
 * 
 * @param registry Registry
 * @param json_path Path to optimized/<model>.json
 * @return 0 on success, negative on error (non-fatal)
 */
ethervox_result_t ethervox_tool_manifest_load_optimized(
    tool_manifest_registry_t* registry,
    const char* json_path
);

/**
 * Get tool index entry (fast, already in memory)
 */
const tool_index_entry_t* ethervox_tool_get_index(
    const tool_manifest_registry_t* registry,
    const char* name
);

/**
 * Get full tool detail (loads from file on-demand)
 * 
 * @param registry Registry
 * @param name Tool name
 * @param detail Output buffer for detail header
 * @param params Output array for parameters (must be sized MAX_PARAMETERS)
 * @param param_count Output: actual parameter count
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_tool_get_detail(
    const tool_manifest_registry_t* registry,
    const char* name,
    tool_detail_header_t* detail,
    tool_param_t* params,
    uint8_t* param_count
);

/**
 * Get optimized prompt for tool (if available)
 * 
 * @param registry Registry
 * @param name Tool name
 * @return Optimized prompt string, or NULL if not available
 */
const char* ethervox_tool_get_optimized_prompt(
    const tool_manifest_registry_t* registry,
    const char* name
);

// ============================================================================
// System Prompt Generation
// ============================================================================

/**
 * Callback for iterating tools
 */
typedef void (*tool_index_callback_t)(
    const tool_index_entry_t* entry,
    const char* optimized_prompt,  // NULL if not available
    void* user_data
);

/**
 * Iterate over all tools (sorted by priority)
 */
void ethervox_tool_foreach(
    const tool_manifest_registry_t* registry,
    uint8_t min_priority,
    tool_index_callback_t callback,
    void* user_data
);

/**
 * Build minimal system prompt index
 * 
 * Generates compact tool list for system prompt:
 * - Uses optimized prompts if available
 * - Falls back to one-line descriptions
 * - Skips disabled tools
 * 
 * @param registry Registry
 * @param output Output buffer
 * @param output_size Buffer size
 * @param min_priority Minimum priority (0=all, 255=none)
 * @return Number of bytes written, or negative on error
 */
ethervox_result_t ethervox_tool_build_index_prompt(
    const tool_manifest_registry_t* registry,
    char* output,
    size_t output_size,
    uint8_t min_priority
);

// ============================================================================
// Runtime Management (Dynamic Enable/Disable)
// ============================================================================

/**
 * Enable tool at runtime
 */
ethervox_result_t ethervox_tool_manifest_enable(
    tool_manifest_registry_t* registry,
    const char* name
);

/**
 * Disable tool at runtime
 */
ethervox_result_t ethervox_tool_manifest_disable(
    tool_manifest_registry_t* registry,
    const char* name
);

/**
 * Set tool priority (affects system prompt ordering)
 */
ethervox_result_t ethervox_tool_manifest_set_priority(
    tool_manifest_registry_t* registry,
    const char* name,
    uint8_t priority
);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Calculate CRC32 checksum
 */
uint32_t ethervox_tool_crc32(const uint8_t* data, size_t length);

/**
 * Validate manifest integrity
 */
bool ethervox_tool_manifest_validate(const tool_manifest_registry_t* registry);

/**
 * Get fallback level name (for logging)
 */
const char* ethervox_tool_fallback_level_name(uint8_t level);

// ============================================================================
// Minimal System Prompt Generation (NEW)
// ============================================================================

/**
 * Build minimal system prompt using optimized prompts or one-liners
 * 
 * Generates ~150 token system prompts instead of ~15K by using:
 * - Level 0: Optimized JSON prompts (~15 tokens/tool)
 * - Level 1: Binary manifest one-liners (~30 tokens/tool)
 * - Level 2: LLM-only mode (0 tokens, no dynamic tools)
 * 
 * @param registry Tool manifest registry
 * @param output Output buffer for system prompt
 * @param output_size Size of output buffer
 * @param min_priority Minimum priority (0=all, 255=none)
 * @return Number of bytes written, or -1 on error
 */
ethervox_result_t ethervox_tool_build_minimal_system_prompt(
    const tool_manifest_registry_t* registry,
    char* output,
    size_t output_size,
    uint8_t min_priority
);

/**
 * Build detailed tool schema for contextual injection
 * 
 * When LLM generates <tool_call>, inject ONLY that tool's full schema
 * into the NEXT prompt temporarily. Remove after tool execution.
 * This prevents permanent KV cache bloat.
 * 
 * @param registry Tool manifest registry
 * @param tool_name Name of tool to get schema for
 * @param output Output buffer for schema
 * @param output_size Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
ethervox_result_t ethervox_tool_build_schema_injection(
    const tool_manifest_registry_t* registry,
    const char* tool_name,
    char* output,
    size_t output_size
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_TOOL_MANIFEST_H
