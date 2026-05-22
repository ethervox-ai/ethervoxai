/**
 * @file tool_manifest.c
 * @brief Tool Manifest System implementation
 *
 * Portable binary manifest loading with optimized prompt caching.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/tool_manifest.h"
#include "ethervox/config.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// CRC32 polynomial (IEEE 802.3)
#define CRC32_POLYNOMIAL 0xEDB88320

// ============================================================================
// CRC32 Implementation
// ============================================================================

static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void init_crc32_table(void) {
    if (crc32_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    
    crc32_table_initialized = true;
}

uint32_t ethervox_tool_crc32(const uint8_t* data, size_t length) {
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }
    
    return ~crc;
}

// ============================================================================
// Endianness Detection
// ============================================================================

static bool is_little_endian(void) {
    uint32_t test = 1;
    return *(uint8_t*)&test == 1;
}

static uint32_t swap_uint32(uint32_t val) {
    return ((val >> 24) & 0xFF) |
           ((val >> 8) & 0xFF00) |
           ((val << 8) & 0xFF0000) |
           ((val << 24) & 0xFF000000);
}

static uint16_t swap_uint16(uint16_t val) {
    return ((val >> 8) & 0xFF) | ((val << 8) & 0xFF00);
}

// ============================================================================
// Tool Manifest Initialization
// ============================================================================

ethervox_result_t ethervox_tool_manifest_init(
    tool_manifest_registry_t* registry,
    const char* binary_path
) {
    if (!registry || !binary_path) {
        ETHERVOX_LOGE("Invalid arguments to tool_manifest_init");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    memset(registry, 0, sizeof(tool_manifest_registry_t));
    
    // Open binary manifest file
    FILE* fp = fopen(binary_path, "rb");
    if (!fp) {
        ETHERVOX_LOGW("Failed to open tool manifest: %s (tools will be unavailable)", binary_path);
        registry->tools_available = false;
        registry->fallback_level = 2;  // LLM-only mode
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Read header
    size_t read = fread(&registry->header, sizeof(tool_manifest_header_t), 1, fp);
    if (read != 1) {
        ETHERVOX_LOGE("Failed to read manifest header");
        fclose(fp);
        registry->tools_available = false;
        registry->fallback_level = 2;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check magic number (detects endianness)
    if (registry->header.magic != TOOL_MANIFEST_MAGIC) {
        if (swap_uint32(registry->header.magic) == TOOL_MANIFEST_MAGIC) {
            registry->needs_byte_swap = true;
            ETHERVOX_LOGI("Manifest uses different endianness, will swap bytes");
        } else {
            ETHERVOX_LOGE("Invalid manifest magic: 0x%08X", registry->header.magic);
            fclose(fp);
            registry->tools_available = false;
            registry->fallback_level = 2;
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    // Byte swap header if needed
    if (registry->needs_byte_swap) {
        registry->header.version = swap_uint32(registry->header.version);
        registry->header.tool_count = swap_uint32(registry->header.tool_count);
        registry->header.index_offset = swap_uint32(registry->header.index_offset);
        registry->header.detail_offset = swap_uint32(registry->header.detail_offset);
    }
    
    // Validate version
    if (registry->header.version != TOOL_MANIFEST_VERSION) {
        ETHERVOX_LOGE("Unsupported manifest version: %u", registry->header.version);
        fclose(fp);
        registry->tools_available = false;
        registry->fallback_level = 2;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Allocate index array
    registry->index = (tool_index_entry_t*)calloc(
        registry->header.tool_count,
        sizeof(tool_index_entry_t)
    );
    if (!registry->index) {
        ETHERVOX_LOGE("Failed to allocate index array");
        fclose(fp);
        registry->tools_available = false;
        registry->fallback_level = 2;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Read index
    fseek(fp, registry->header.index_offset, SEEK_SET);
    read = fread(registry->index, sizeof(tool_index_entry_t), 
                 registry->header.tool_count, fp);
    if (read != registry->header.tool_count) {
        ETHERVOX_LOGE("Failed to read index entries");
        free(registry->index);
        fclose(fp);
        registry->tools_available = false;
        registry->fallback_level = 2;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Byte swap index if needed
    if (registry->needs_byte_swap) {
        for (uint32_t i = 0; i < registry->header.tool_count; i++) {
            registry->index[i].detail_size = swap_uint16(registry->index[i].detail_size);
            registry->index[i].detail_offset = swap_uint32(registry->index[i].detail_offset);
        }
    }
    
    // Keep file open for on-demand detail loading
    registry->manifest_file = fp;
    registry->tools_available = true;
    registry->fallback_level = 1;  // Using binary one-liners (optimized prompts not loaded yet)
    
    ETHERVOX_LOGI("Tool manifest loaded: %u tools from %s", 
                  registry->header.tool_count, binary_path);
    
    return ETHERVOX_SUCCESS;
}

// ============================================================================
// Cleanup
// ============================================================================

void ethervox_tool_manifest_cleanup(tool_manifest_registry_t* registry) {
    if (!registry) return;
    
    if (registry->manifest_file) {
        fclose(registry->manifest_file);
        registry->manifest_file = NULL;
    }
    
    if (registry->index) {
        free(registry->index);
        registry->index = NULL;
    }
    
    if (registry->optimized_cache) {
        // Free optimized prompts
        for (uint32_t i = 0; i < registry->optimized_cache->prompt_count; i++) {
            free(registry->optimized_cache->prompts[i].tool_name);
            free(registry->optimized_cache->prompts[i].optimized_prompt);
        }
        free(registry->optimized_cache->prompts);
        free(registry->optimized_cache->model_name);
        free(registry->optimized_cache);
        registry->optimized_cache = NULL;
    }
    
    memset(registry, 0, sizeof(tool_manifest_registry_t));
}

// ============================================================================
// Tool Lookup
// ============================================================================

const tool_index_entry_t* ethervox_tool_get_index(
    const tool_manifest_registry_t* registry,
    const char* name
) {
    if (!registry || !name || !registry->tools_available) {
        return NULL;
    }
    
    // Linear scan (could be optimized with hash table if needed)
    for (uint32_t i = 0; i < registry->header.tool_count; i++) {
        if (strcmp(registry->index[i].name, name) == 0) {
            return &registry->index[i];
        }
    }
    
    return NULL;
}

ethervox_result_t ethervox_tool_get_detail(
    const tool_manifest_registry_t* registry,
    const char* name,
    tool_detail_header_t* detail,
    tool_param_t* params,
    uint8_t* param_count
) {
    if (!registry || !name || !detail || !params || !param_count) {
        ETHERVOX_LOGE("ethervox_tool_get_detail: Invalid parameters (registry=%p, name=%p, detail=%p, params=%p, param_count=%p)",
                     (void*)registry, (void*)name, (void*)detail, (void*)params, (void*)param_count);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if manifest file is available (not tools_available flag)
    // The tools_available flag may be false during optimization when
    // the optimization file doesn't exist yet
    if (!registry->manifest_file) {
        ETHERVOX_LOGE("ethervox_tool_get_detail: Manifest file is NULL for tool '%s'", name);
        ETHERVOX_LOGE("  registry=%p, header.tool_count=%u, tools_available=%d",
                     (void*)registry, registry->header.tool_count, registry->tools_available);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Find in index
    const tool_index_entry_t* index_entry = ethervox_tool_get_index(registry, name);
    if (!index_entry) {
        ETHERVOX_LOGE("ethervox_tool_get_detail: Tool '%s' not found in index", name);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Seek to detail
    fseek(registry->manifest_file, index_entry->detail_offset, SEEK_SET);
    
    // Read detail header
    size_t read = fread(detail, sizeof(tool_detail_header_t), 1, registry->manifest_file);
    if (read != 1) {
        ETHERVOX_LOGE("Failed to read detail for tool: %s", name);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Read parameters
    *param_count = detail->param_count;
    if (detail->param_count > 0) {
        read = fread(params, sizeof(tool_param_t), detail->param_count, registry->manifest_file);
        if (read != detail->param_count) {
            ETHERVOX_LOGE("Failed to read parameters for tool: %s", name);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    return ETHERVOX_SUCCESS;
}

const char* ethervox_tool_get_optimized_prompt(
    const tool_manifest_registry_t* registry,
    const char* name
) {
    if (!registry || !name || !registry->optimized_cache) {
        return NULL;
    }
    
    // Linear search through optimized prompts
    for (uint32_t i = 0; i < registry->optimized_cache->prompt_count; i++) {
        if (strcmp(registry->optimized_cache->prompts[i].tool_name, name) == 0) {
            return registry->optimized_cache->prompts[i].optimized_prompt;
        }
    }
    
    return NULL;
}

// ============================================================================
// System Prompt Generation
// ============================================================================

void ethervox_tool_foreach(
    const tool_manifest_registry_t* registry,
    uint8_t min_priority,
    tool_index_callback_t callback,
    void* user_data
) {
    if (!registry || !callback || !registry->tools_available) {
        return;
    }
    
    // Iterate through index (already sorted by priority during manifest build)
    for (uint32_t i = 0; i < registry->header.tool_count; i++) {
        const tool_index_entry_t* entry = &registry->index[i];
        
        // Skip disabled tools
        if (!entry->enabled) continue;
        
        // Skip low priority tools
        if (entry->priority > min_priority) continue;
        
        // Get optimized prompt if available
        const char* optimized = ethervox_tool_get_optimized_prompt(registry, entry->name);
        
        callback(entry, optimized, user_data);
    }
}

ethervox_result_t ethervox_tool_build_index_prompt(
    const tool_manifest_registry_t* registry,
    char* output,
    size_t output_size,
    uint8_t min_priority
) {
    if (!registry || !output || output_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (!registry->tools_available) {
        // No tools available - return empty
        output[0] = '\0';
        return ETHERVOX_SUCCESS;
    }
    
    int offset = 0;
    
    // Header
    offset += snprintf(output + offset, output_size - offset,
                      "Available tools:\n\n");
    
    // Categorize tools by priority
    bool has_high_priority = false;
    bool has_normal_priority = false;
    
    // High priority tools (0-1)
    for (uint32_t i = 0; i < registry->header.tool_count && offset < (int)output_size - 100; i++) {
        const tool_index_entry_t* entry = &registry->index[i];
        if (!entry->enabled || entry->priority > min_priority || entry->priority > 1) continue;
        
        if (!has_high_priority) {
            offset += snprintf(output + offset, output_size - offset,
                             "HIGH PRIORITY:\n");
            has_high_priority = true;
        }
        
        const char* desc = ethervox_tool_get_optimized_prompt(registry, entry->name);
        if (!desc) desc = entry->one_line;
        
        offset += snprintf(output + offset, output_size - offset,
                         "• %s - %s\n", entry->name, desc);
    }
    
    if (has_high_priority) {
        offset += snprintf(output + offset, output_size - offset, "\n");
    }
    
    // Normal priority tools (2+)
    for (uint32_t i = 0; i < registry->header.tool_count && offset < (int)output_size - 100; i++) {
        const tool_index_entry_t* entry = &registry->index[i];
        if (!entry->enabled || entry->priority > min_priority || entry->priority <= 1) continue;
        
        if (!has_normal_priority) {
            offset += snprintf(output + offset, output_size - offset,
                             "NORMAL PRIORITY:\n");
            has_normal_priority = true;
        }
        
        const char* desc = ethervox_tool_get_optimized_prompt(registry, entry->name);
        if (!desc) desc = entry->one_line;
        
        offset += snprintf(output + offset, output_size - offset,
                         "• %s - %s\n", entry->name, desc);
    }
    
    // Usage hint
    if (has_high_priority || has_normal_priority) {
        offset += snprintf(output + offset, output_size - offset,
                         "\nCall tools using: <tool_call name=\"TOOL_NAME\" param=\"value\" />\n");
    }
    
    return offset;
}

// ============================================================================
// Runtime Management
// ============================================================================

ethervox_result_t ethervox_tool_manifest_enable(
    tool_manifest_registry_t* registry,
    const char* name
) {
    if (!registry || !name || !registry->tools_available) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    for (uint32_t i = 0; i < registry->header.tool_count; i++) {
        if (strcmp(registry->index[i].name, name) == 0) {
            registry->index[i].enabled = 1;
            return ETHERVOX_SUCCESS;
        }
    }
    
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
}

ethervox_result_t ethervox_tool_manifest_disable(
    tool_manifest_registry_t* registry,
    const char* name
) {
    if (!registry || !name || !registry->tools_available) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    for (uint32_t i = 0; i < registry->header.tool_count; i++) {
        if (strcmp(registry->index[i].name, name) == 0) {
            registry->index[i].enabled = 0;
            return ETHERVOX_SUCCESS;
        }
    }
    
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
}

ethervox_result_t ethervox_tool_manifest_set_priority(
    tool_manifest_registry_t* registry,
    const char* name,
    uint8_t priority
) {
    if (!registry || !name || !registry->tools_available) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    for (uint32_t i = 0; i < registry->header.tool_count; i++) {
        if (strcmp(registry->index[i].name, name) == 0) {
            registry->index[i].priority = priority;
            return ETHERVOX_SUCCESS;
        }
    }
    
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
}

// ============================================================================
// Validation
// ============================================================================

bool ethervox_tool_manifest_validate(const tool_manifest_registry_t* registry) {
    if (!registry || !registry->tools_available || !registry->manifest_file) {
        return false;
    }
    
    // Basic validation
    if (registry->header.magic != TOOL_MANIFEST_MAGIC) {
        return false;
    }
    
    if (registry->header.version != TOOL_MANIFEST_VERSION) {
        return false;
    }
    
    if (registry->header.tool_count == 0) {
        return false;
    }
    
    // TODO: Implement CRC32 checksum validation
    
    return true;
}

// ============================================================================
// Utilities
// ============================================================================

const char* ethervox_tool_fallback_level_name(uint8_t level) {
    switch (level) {
        case 0: return "Optimal (JSON optimized prompts)";
        case 1: return "Good (Binary one-liners)";
        case 2: return "LLM-only (No dynamic tools)";
        case 3: return "Emergency (/quit, /help only)";
        default: return "Unknown";
    }
}
