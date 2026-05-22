/**
#include "ethervox/error.h"
 * @file startup_prompt_registry.c
 * @brief Tool registry for startup prompt management
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/startup_prompt_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#endif

#define STARTUP_LOG(...) ETHERVOX_LOG_INFO(__VA_ARGS__)
#define STARTUP_ERROR(...) ETHERVOX_LOG_ERROR(__VA_ARGS__)

static int tool_startup_prompt_update(const char* args_json, char** result, char** error) {
    if (!args_json || !result || !error) {
        *error = strdup("Invalid arguments");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Parse JSON to extract prompt text
    const char* text_start = strstr(args_json, "\"prompt_text\"");
    if (!text_start) {
        *error = strdup("Missing 'prompt_text' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Skip past "prompt_text": to find the value
    text_start = strchr(text_start + 13, ':'); // +13 to skip "prompt_text"
    if (!text_start) {
        *error = strdup("Invalid JSON format - missing colon");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    text_start++; // Skip colon
    
    // Skip whitespace
    while (*text_start && (*text_start == ' ' || *text_start == '\t' || *text_start == '\n')) {
        text_start++;
    }
    
    // Now find the opening quote of the value
    if (*text_start != '"') {
        *error = strdup("Invalid JSON format - value must be a string");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    text_start++; // Skip opening quote
    
    const char* text_end = text_start;
    while (*text_end && *text_end != '"') {
        if (*text_end == '\\' && *(text_end + 1)) {
            text_end++; // Skip escaped character
        }
        text_end++;
    }
    
    if (!*text_end) {
        *error = strdup("Unterminated string in JSON");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Allocate and unescape the prompt text
    size_t len = text_end - text_start;
    char* prompt = malloc(len + 1);
    if (!prompt) {
        *error = strdup("Memory allocation failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Unescape JSON string
    const char* src = text_start;
    char* dst = prompt;
    while (src < text_end) {
        if (*src == '\\' && src + 1 < text_end) {
            src++;
            switch (*src) {
                case 'n': *dst++ = '\n'; break;
                case 't': *dst++ = '\t'; break;
                case 'r': *dst++ = '\r'; break;
                case '"': *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                default: *dst++ = *src; break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    
    // Determine file path - platform-specific
    char prompt_file[512];
    
#ifdef ETHERVOX_PLATFORM_ANDROID
    // Android: use files directory from JNI
    extern const char* ethervox_android_get_files_dir(void);
    const char* android_files_dir = ethervox_android_get_files_dir();
    if (android_files_dir) {
        snprintf(prompt_file, sizeof(prompt_file), "%s/.ethervox_startup_prompt.txt", android_files_dir);
    } else {
        snprintf(prompt_file, sizeof(prompt_file), "./.ethervox_startup_prompt.txt");
    }
#else
    // Desktop: use HOME directory
    const char* home_dir = getenv("HOME");
    if (home_dir) {
        snprintf(prompt_file, sizeof(prompt_file), "%s/.ethervox/startup_prompt.txt", home_dir);
        
        // Create .ethervox directory if it doesn't exist
        char ethervox_dir[512];
        snprintf(ethervox_dir, sizeof(ethervox_dir), "%s/.ethervox", home_dir);
#ifdef _WIN32
        _mkdir(ethervox_dir);
#else
        mkdir(ethervox_dir, 0755);
#endif
    } else {
        snprintf(prompt_file, sizeof(prompt_file), "./.ethervox_startup_prompt.txt");
    }
#endif
    
    // Write prompt to file
    FILE* fp = fopen(prompt_file, "w");
    if (!fp) {
        *error = strdup("Failed to open startup prompt file for writing");
        free(prompt);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    fprintf(fp, "%s", prompt);
    fclose(fp);
    
    STARTUP_LOG("Updated startup prompt: %s", prompt_file);
    
    // Build success result
    char result_buf[1024];
    snprintf(result_buf, sizeof(result_buf),
             "{\"status\":\"success\",\"file\":\"%s\",\"length\":%zu}",
             prompt_file, strlen(prompt));
    *result = strdup(result_buf);
    
    free(prompt);
    return ETHERVOX_SUCCESS;
}

static int tool_startup_prompt_read(const char* args_json, char** result, char** error) {
    (void)args_json; // No parameters needed
    
    if (!result || !error) {
        *error = strdup("Invalid arguments");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Determine file path - platform-specific
    char prompt_file[512];
    
#ifdef ETHERVOX_PLATFORM_ANDROID
    // Android: use files directory from JNI
    extern const char* ethervox_android_get_files_dir(void);
    const char* android_files_dir = ethervox_android_get_files_dir();
    if (android_files_dir) {
        snprintf(prompt_file, sizeof(prompt_file), "%s/.ethervox_startup_prompt.txt", android_files_dir);
    } else {
        snprintf(prompt_file, sizeof(prompt_file), "./.ethervox_startup_prompt.txt");
    }
#else
    // Desktop: use HOME directory
    const char* home_dir = getenv("HOME");
    if (home_dir) {
        snprintf(prompt_file, sizeof(prompt_file), "%s/.ethervox/startup_prompt.txt", home_dir);
    } else {
        snprintf(prompt_file, sizeof(prompt_file), "./.ethervox_startup_prompt.txt");
    }
#endif
    
    // Try to read custom prompt
    FILE* fp = fopen(prompt_file, "r");
    if (!fp) {
        // No custom prompt, return indication
        *result = strdup("{\"status\":\"default\",\"has_custom\":false}");
        return ETHERVOX_SUCCESS;
    }
    
    // Read the file
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size <= 0 || size > 10000) {
        fclose(fp);
        *error = strdup("Invalid startup prompt file size");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* content = malloc(size + 1);
    if (!content) {
        fclose(fp);
        *error = strdup("Memory allocation failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    size_t read = fread(content, 1, size, fp);
    content[read] = '\0';
    fclose(fp);
    
    // Build JSON result with escaped content
    char* result_buf = malloc(size * 2 + 256);
    if (!result_buf) {
        free(content);
        *error = strdup("Memory allocation failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    sprintf(result_buf, "{\"status\":\"custom\",\"has_custom\":true,\"prompt\":\"");
    char* dst = result_buf + strlen(result_buf);
    
    // Escape the content for JSON
    for (const char* src = content; *src; src++) {
        switch (*src) {
            case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
            case '\t': *dst++ = '\\'; *dst++ = 't'; break;
            case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
            case '"': *dst++ = '\\'; *dst++ = '"'; break;
            case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
            default: *dst++ = *src; break;
        }
    }
    strcpy(dst, "\"}");
    
    *result = result_buf;
    free(content);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_startup_prompt_tools_register(void* registry_ptr) {
    ethervox_tool_registry_t* registry = (ethervox_tool_registry_t*)registry_ptr;
    if (!registry) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Register startup_prompt_update tool
    ethervox_tool_t update_tool = {
        .name = "startup_prompt_update",
        .description = "Update the startup prompt that runs when the assistant starts. Use this to customize the initial greeting or behavior. The prompt will be saved and used on next restart.",
        .parameters_json_schema = 
            "{\"type\":\"object\",\"properties\":{"
                "\"prompt_text\":{\"type\":\"string\",\"description\":\"The new startup prompt text. Should be a natural greeting or instruction for the assistant.\"}"
            "},\"required\":[\"prompt_text\"]}",
        .execute = tool_startup_prompt_update,
        .is_deterministic = false
    };
    
    if (ethervox_tool_registry_add(registry, &update_tool) != 0) {
        STARTUP_ERROR("Failed to register startup_prompt_update tool");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Register startup_prompt_read tool
    ethervox_tool_t read_tool = {
        .name = "startup_prompt_read",
        .description = "Read the current custom startup prompt. Returns the prompt text if a custom one exists, or indicates if using the default.",
        .parameters_json_schema = "{\"type\":\"object\",\"properties\":{}}",
        .execute = tool_startup_prompt_read,
        .is_deterministic = true
    };
    
    if (ethervox_tool_registry_add(registry, &read_tool) != 0) {
        STARTUP_ERROR("Failed to register startup_prompt_read tool");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    STARTUP_LOG("Registered 2 startup prompt tools");
    
    return ETHERVOX_SUCCESS;
}
