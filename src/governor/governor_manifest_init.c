/**
 * @file governor_manifest_init.c
 * @brief Governor initialization with Tool Manifest System integration
 *
 * Implements the 4-level fallback chain and minimal system prompt generation.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/governor.h"
#include "ethervox/tool_manifest.h"
#include "ethervox/config.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

/**
 * Initialize governor with Tool Manifest System
 * 
 * This implements the complete initialization sequence:
 * 1. Export manifest from runtime tool registry
 * 2. Load binary manifest
 * 3. Detect model name
 * 4. Load optimized JSON prompts
 * 5. Build minimal system prompt
 * 6. Graceful 4-level fallback
 * 
 * @param governor Governor instance
 * @param model_path Path to GGUF model file
 * @param manifest_registry Output: Tool manifest registry
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_governor_init_with_manifest(
    ethervox_governor_t* governor,
    const char* model_path,
    tool_manifest_registry_t* manifest_registry
) {
    if (!governor || !model_path || !manifest_registry) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Get runtime tool registry (from governor)
    const ethervox_tool_registry_t* runtime_registry = 
        ethervox_governor_get_registry(governor);
    
    if (!runtime_registry) {
        ETHERVOX_LOGE("No runtime tool registry available");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Get Android files directory once for all path operations (used in STEP 1 and STEP 4)
#ifdef ETHERVOX_PLATFORM_ANDROID
    const char* android_files_dir = ethervox_android_get_files_dir();
    if (!android_files_dir || android_files_dir[0] == '\0') {
        ETHERVOX_LOGE("Android files directory not set!");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
#endif
    
    // === STEP 1: Export manifest from runtime registry ===
    char manifest_path[512];
    char tools_dir[512];
    
#ifdef ETHERVOX_PLATFORM_ANDROID
    // On Android, use app files directory (already retrieved and validated above)
    snprintf(manifest_path, sizeof(manifest_path),
            "%s/tools/tools.bin", android_files_dir);
    snprintf(tools_dir, sizeof(tools_dir), "%s/tools", android_files_dir);
#else
    // On desktop, use HOME environment variable
    const char* home = getenv("HOME");
    if (home) {
        snprintf(manifest_path, sizeof(manifest_path),
                "%s/.ethervox/tools/tools.bin", home);
        snprintf(tools_dir, sizeof(tools_dir), "%s/.ethervox/tools", home);
    } else {
        snprintf(manifest_path, sizeof(manifest_path),
                "./.ethervox/tools/tools.bin");
        snprintf(tools_dir, sizeof(tools_dir), "./.ethervox/tools");
    }
#endif
    
#ifdef _WIN32
    _mkdir(tools_dir);
#else
    mkdir(tools_dir, 0755);
#endif
    
    ETHERVOX_LOGI("Exporting tool manifest to: %s", manifest_path);
    
    if (ethervox_tool_registry_export_manifest(runtime_registry, manifest_path) == 0) {
        ETHERVOX_LOGI("Manifest exported: %u tools", runtime_registry->tool_count);
    } else {
        ETHERVOX_LOGW("Failed to export manifest (will try to load existing)");
    }
    
    // === STEP 2: Load binary manifest ===
    // Initialize guard flags
    manifest_registry->tools_detected = false;
    manifest_registry->optimization_loaded = false;
    manifest_registry->tools_loaded_count = 0;
    
    if (ethervox_tool_manifest_init(manifest_registry, manifest_path) != 0) {
        // Level 2 fallback: LLM-only mode
        ETHERVOX_LOGW("═══════════════════════════════════════════════════════");
        ETHERVOX_LOGW("WARNING: Tool manifest unavailable");
        ETHERVOX_LOGW("Running in LLM-only mode with basic commands");
        ETHERVOX_LOGW("═══════════════════════════════════════════════════════");
        
        manifest_registry->tools_available = false;
        manifest_registry->tools_detected = false;
        manifest_registry->fallback_level = 2;
        
        // Still functional - deterministic toolkit always available
        return ETHERVOX_SUCCESS;
    }
    
    // Tools binary manifest was found and loaded
    manifest_registry->tools_detected = true;
    ETHERVOX_LOGI("[OK] Tools detected: %u tools in manifest", runtime_registry->tool_count);
    
    // === STEP 3: Detect model name ===
    // Extract model name from path (e.g., "granite-4.0-h-1b-Q4_K_M.gguf" -> "granite-4.0-h-1b")
    const char* filename = strrchr(model_path, '/');
    if (!filename) filename = model_path;
    else filename++;
    
    char model_name[128];
    size_t i = 0;
    while (i < sizeof(model_name) - 1 && filename[i]) {
        if (filename[i] == '.' && strncmp(&filename[i], ".gguf", 5) == 0) break;
        // Stop at quantization markers like -Q4, -Q5, -Q6, -Q8
        if (filename[i] == '-' && i + 1 < strlen(filename) && filename[i+1] == 'Q') break;
        
        model_name[i] = filename[i];
        i++;
    }
    model_name[i] = '\0';
    
    // Remove trailing dash
    if (i > 0 && model_name[i-1] == '-') {
        model_name[i-1] = '\0';
    }
    
    ETHERVOX_LOGI("Detected model: %s", model_name);
    
    // === STEP 4: Load optimized JSON prompts ===
    char optimized_path[512];
    
#ifdef ETHERVOX_PLATFORM_ANDROID
    // On Android, use app files directory (already retrieved and validated at function start)
    snprintf(optimized_path, sizeof(optimized_path),
             "%s/tools/optimized/%s.json", 
             android_files_dir, model_name);
#else
    snprintf(optimized_path, sizeof(optimized_path),
             "%s/.ethervox/tools/optimized/%s.json", 
             home ? home : ".", model_name);
#endif
    
    if (ethervox_tool_manifest_load_optimized(manifest_registry, optimized_path) == 0) {
        // Level 0: Optimal - using optimized JSON prompts
        manifest_registry->optimization_loaded = true;
        manifest_registry->fallback_level = 0;
        manifest_registry->tools_loaded_count = manifest_registry->header.tool_count;
        manifest_registry->tools_available = true;
        
        ETHERVOX_LOGI("[OK] Loaded optimized prompts: %s",
                      ethervox_tool_fallback_level_name(0));
        ETHERVOX_LOGI("  Tools will be available for use");
        ETHERVOX_LOGI("  DEBUG: Set tools_loaded_count=%u from header.tool_count=%u",
                      manifest_registry->tools_loaded_count, manifest_registry->header.tool_count);
    } else {
        // GUARD: Optimization file missing - do NOT load tools
        manifest_registry->optimization_loaded = false;
        manifest_registry->fallback_level = 3;  // Emergency mode - no tools loaded
        
        ETHERVOX_LOGW("═══════════════════════════════════════════════════════");
        ETHERVOX_LOGW("⚠️  OPTIMIZATION FILE NOT FOUND");
        ETHERVOX_LOGW("Tools detected but NOT loaded into system prompt");
        ETHERVOX_LOGW("Run optimization to enable %u tools", runtime_registry->tool_count);
        ETHERVOX_LOGW("Path: %s", optimized_path);
        ETHERVOX_LOGW("═══════════════════════════════════════════════════════");
        
        // Mark tools as unavailable for system prompt generation
        manifest_registry->tools_available = false;
    }
    
    // === STEP 5: Validate manifest ===
    if (!ethervox_tool_manifest_validate(manifest_registry)) {
        ETHERVOX_LOGW("Manifest validation failed, but continuing...");
    }
    
    ETHERVOX_LOGI("Tool Manifest System initialized");
    ETHERVOX_LOGI("  Tools detected: %s (%u)", 
                  manifest_registry->tools_detected ? "YES" : "NO",
                  manifest_registry->tools_detected ? manifest_registry->header.tool_count : 0);
    ETHERVOX_LOGI("  Optimization loaded: %s", 
                  manifest_registry->optimization_loaded ? "YES" : "NO");
    ETHERVOX_LOGI("  Tools available for use: %s",
                  manifest_registry->tools_available ? "YES" : "NO");
    ETHERVOX_LOGI("  Fallback level: %u (%s)",
                  manifest_registry->fallback_level,
                  ethervox_tool_fallback_level_name(manifest_registry->fallback_level));
    
    return ETHERVOX_SUCCESS;
}

/**
 * Build complete system prompt using Tool Manifest System
 * 
 * Generates minimal prompt with tool index, then appends base instructions.
 * Wraps the entire prompt with chat template markers for proper model recognition.
 * 
 * @param manifest_registry Tool manifest registry
 * @param chat_template Chat template for wrapping (system_start/system_end markers)
 * @param output Output buffer
 * @param output_size Buffer size
 * @return Number of bytes written, or negative on error
 */
ethervox_result_t ethervox_governor_build_system_prompt_with_manifest(
    const tool_manifest_registry_t* manifest_registry,
    const chat_template_t* chat_template,
    char* output,
    size_t output_size
) {
    if (!output || output_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int offset = 0;
    
    // Start with chat template system marker (e.g., "<|system|>\n")
    if (chat_template && chat_template->system_start) {
        offset = snprintf(output, output_size, "%s", chat_template->system_start);
        if (offset < 0 || (size_t)offset >= output_size) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    // Add tool section - the manifest builder will add the Granite-formatted tools
    if (manifest_registry && manifest_registry->tools_available) {
        int tool_prompt_len = ethervox_tool_build_minimal_system_prompt(
            manifest_registry,
            output + offset,
            output_size - offset,
            255  // Include all tools
        );
        
        if (tool_prompt_len > 0) {
            offset += tool_prompt_len;
        }
    } else {
        // Level 2/3: No dynamic tools - simple assistant prompt
        int written = snprintf(output + offset, output_size - offset,
            "You are Ethervox, a helpful AI assistant. Respond naturally and conversationally.\n");
        
        if (written > 0) {
            offset += written;
        }
    }
    
    // End with chat template system end marker (e.g., "<|end|>\n")
    if (chat_template && chat_template->system_end) {
        int written = snprintf(output + offset, output_size - offset, "%s", chat_template->system_end);
        if (written < 0 || (size_t)(offset + written) >= output_size) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
        offset += written;
    }
    
    ETHERVOX_LOGI("System prompt generated: %d bytes (with chat template markers)", offset);
    
    return offset;
}

/**
 * Initialize manifest registry for a loaded governor model
 * 
 * This centralizes the manifest initialization pattern used by both
 * desktop (main.c) and Android (ethervox_android_core.c) to eliminate
 * code duplication.
 * 
 * @param governor Governor instance (must have model loaded)
 * @param model_path Path to model file (for finding manifest)
 * @param manifest_out Receives allocated manifest on success (caller must free on error)
 * @return 0=success, negative=error
 */
ethervox_result_t ethervox_governor_setup_manifest(
    ethervox_governor_t* governor,
    const char* model_path,
    tool_manifest_registry_t** manifest_out
) {
    if (!governor || !model_path || !manifest_out) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Invalid parameters to manifest setup");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    tool_manifest_registry_t* manifest = malloc(sizeof(tool_manifest_registry_t));
    if (!manifest) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to allocate manifest registry");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    memset(manifest, 0, sizeof(tool_manifest_registry_t));
    
    ethervox_result_t result = ethervox_governor_init_with_manifest(governor, model_path, manifest);
    
    if (ethervox_is_success(result)) {
        *manifest_out = manifest;
        
        // Log manifest level for diagnostic purposes
        if (manifest->tools_available) {
            if (manifest->optimized_cache) {
                ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                            "Manifest ready: Level 0 (optimized prompts loaded)");
            } else {
                ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                            "Manifest ready: Level 1 (binary one-liners)");
            }
        } else {
            ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                        "Manifest fallback: Level 2 (LLM-only, consider optimization)");
        }
        
        return ETHERVOX_SUCCESS;
    } else {
        free(manifest);
        *manifest_out = NULL;
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Manifest initialization failed - using runtime registry only");
        return result;
    }
}
