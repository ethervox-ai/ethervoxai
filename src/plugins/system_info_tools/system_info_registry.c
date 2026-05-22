/**
 * @file system_info_registry.c
 * @brief System information tools for EthervoxAI
 *
 * Provides tools for querying version, build info, and system capabilities
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/system_info_tools.h"
#include "ethervox/config.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYSINFO_LOG(...) ETHERVOX_LOG_INFO(__VA_ARGS__)

// Tool: system_version - Get version and build information
static int tool_system_version(const char* args_json, char** result, char** error) {
    (void)args_json; // No parameters needed
    
    if (!result || !error) {
        *error = strdup("Invalid arguments");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build JSON response with version info
    char response[1024];
    snprintf(response, sizeof(response),
             "{"
             "\"version\":\"%s\","
             "\"major\":%d,"
             "\"minor\":%d,"
             "\"patch\":%d,"
             "\"build_type\":\"%s\","
             "\"git_commit\":\"%s\","
             "\"git_branch\":\"%s\","
             "\"backend_version\":\"%s\","
             "\"platform\":\"%s\""
             "}",
             ETHERVOX_VERSION_STRING,
             ETHERVOX_VERSION_MAJOR,
             ETHERVOX_VERSION_MINOR,
             ETHERVOX_VERSION_PATCH,
             ETHERVOX_BUILD_TYPE,
             ETHERVOX_GIT_COMMIT,
             ETHERVOX_GIT_BRANCH,
             ETHERVOX_BACKEND_VERSION,
#if defined(ETHERVOX_PLATFORM_MACOS)
             "macOS"
#elif defined(ETHERVOX_PLATFORM_LINUX)
             "Linux"
#elif defined(ETHERVOX_PLATFORM_WINDOWS)
             "Windows"
#elif defined(ETHERVOX_PLATFORM_RPI)
             "Raspberry Pi"
#elif defined(ETHERVOX_PLATFORM_ESP32)
             "ESP32"
#elif defined(ETHERVOX_PLATFORM_ANDROID)
             "Android"
#else
             "Unknown"
#endif
    );
    
    *result = strdup(response);
    SYSINFO_LOG("Retrieved system version: %s (commit: %s, branch: %s, backend: %s)", 
                ETHERVOX_VERSION_STRING,
                ETHERVOX_GIT_COMMIT,
                ETHERVOX_GIT_BRANCH,
                ETHERVOX_BACKEND_VERSION
    );
    
    return ETHERVOX_SUCCESS;
}

// Tool: system_capabilities - Get system capabilities
static int tool_system_capabilities(const char* args_json, char** result, char** error) {
    (void)args_json; // No parameters needed
    
    if (!result || !error) {
        *error = strdup("Invalid arguments");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build JSON response with capability info
    char response[2048];
    snprintf(response, sizeof(response),
             "{"
             "\"max_languages\":%d,"
             "\"max_plugins\":%d,"
             "\"audio_sample_rate\":%d,"
             "\"audio_buffer_size\":%d,"
             "\"llm_max_tokens_default\":%u,"
             "\"is_embedded\":%s,"
             "\"is_desktop\":%s,"
             "\"is_mobile\":%s,"
             "\"runtime_dir\":\"%s\""
             "}",
             ETHERVOX_MAX_LANGUAGES,
             ETHERVOX_MAX_PLUGINS,
             ETHERVOX_AUDIO_SAMPLE_RATE,
             ETHERVOX_AUDIO_BUFFER_SIZE,
             ETHERVOX_LLM_MAX_TOKENS_DEFAULT,
#ifdef ETHERVOX_PLATFORM_EMBEDDED
             "true",
#else
             "false",
#endif
#ifdef ETHERVOX_PLATFORM_DESKTOP
             "true",
#else
             "false",
#endif
#ifdef ETHERVOX_PLATFORM_MOBILE
             "true",
#else
             "false",
#endif
             ETHERVOX_RUNTIME_DIR_BASE ? ETHERVOX_RUNTIME_DIR_BASE : "not set"
    );
    
    *result = strdup(response);
    SYSINFO_LOG("Retrieved system capabilities");
    
    return ETHERVOX_SUCCESS;
}

// Register system info tools with the tool registry
ethervox_result_t ethervox_system_info_tools_register(ethervox_tool_registry_t* registry) {
    if (!registry) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int ret = 0;
    
    // Register system_version tool
    ethervox_tool_t version_tool = {
        .name = "system_version",
        .description = "Get EthervoxAI version and build information including version number, git commit hash, build type, and platform.",
        .parameters_json_schema = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        .execute = tool_system_version,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 1.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &version_tool);
    
    // Register system_capabilities tool
    ethervox_tool_t capabilities_tool = {
        .name = "system_capabilities",
        .description = "Get system capabilities and configuration limits including max languages, plugins, audio settings, and platform type.",
        .parameters_json_schema = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        .execute = tool_system_capabilities,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 1.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &capabilities_tool);
    
    if (ethervox_is_success(ret)) {
        SYSINFO_LOG("System info tools registered successfully");
    } else {
        ETHERVOX_LOG_ERROR("Failed to register system info tools");
    }
    
    return ret;
}
