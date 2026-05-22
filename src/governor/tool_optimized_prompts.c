/**
 * @file tool_optimized_prompts.c
 * @brief Optimized prompt JSON cache loader
 *
 * Loads model-specific optimized tool descriptions from JSON cache.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/tool_manifest.h"
#include "ethervox/config.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple JSON parsing (minimal implementation)
// For production, consider using a JSON library like cJSON

#define JSON_MAX_TOKEN_SIZE 4096

typedef struct {
    const char* json;
    size_t pos;
    size_t length;
} json_parser_t;

static void skip_whitespace(json_parser_t* parser) {
    while (parser->pos < parser->length) {
        char c = parser->json[parser->pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
        parser->pos++;
    }
}

static bool expect_char(json_parser_t* parser, char expected) {
    skip_whitespace(parser);
    if (parser->pos >= parser->length || parser->json[parser->pos] != expected) {
        return false;
    }
    parser->pos++;
    return true;
}

static bool parse_string(json_parser_t* parser, char* output, size_t output_size) {
    skip_whitespace(parser);
    
    if (parser->pos >= parser->length || parser->json[parser->pos] != '"') {
        return false;
    }
    parser->pos++;  // Skip opening quote
    
    size_t i = 0;
    while (parser->pos < parser->length && i < output_size - 1) {
        char c = parser->json[parser->pos++];
        
        if (c == '"') {
            output[i] = '\0';
            return true;
        }
        
        if (c == '\\' && parser->pos < parser->length) {
            // Handle escape sequences
            char next = parser->json[parser->pos++];
            switch (next) {
                case 'n': output[i++] = '\n'; break;
                case 't': output[i++] = '\t'; break;
                case 'r': output[i++] = '\r'; break;
                case '"': output[i++] = '"'; break;
                case '\\': output[i++] = '\\'; break;
                default: output[i++] = next; break;
            }
        } else {
            output[i++] = c;
        }
    }
    
    return false;  // Missing closing quote
}

static bool find_key(json_parser_t* parser, const char* key) {
    size_t start_pos = parser->pos;
    char found_key[256];
    
    while (parser->pos < parser->length) {
        skip_whitespace(parser);
        
        if (parser->json[parser->pos] == '}') {
            // End of object, key not found
            parser->pos = start_pos;
            return false;
        }
        
        if (parser->json[parser->pos] == ',') {
            parser->pos++;
            continue;
        }
        
        // Try to parse key
        if (!parse_string(parser, found_key, sizeof(found_key))) {
            parser->pos = start_pos;
            return false;
        }
        
        skip_whitespace(parser);
        if (parser->json[parser->pos] != ':') {
            parser->pos = start_pos;
            return false;
        }
        parser->pos++;  // Skip colon
        
        if (strcmp(found_key, key) == 0) {
            return true;  // Found the key, parser->pos now points to value
        }
        
        // Skip the value (simple implementation - just skip to next comma or brace)
        int depth = 0;
        bool in_string = false;
        while (parser->pos < parser->length) {
            char c = parser->json[parser->pos++];
            
            if (c == '"' && (parser->pos == 1 || parser->json[parser->pos - 2] != '\\')) {
                in_string = !in_string;
            }
            
            if (!in_string) {
                if (c == '{' || c == '[') depth++;
                if (c == '}' || c == ']') depth--;
                if (depth == 0 && (c == ',' || c == '}')) {
                    if (c == '}') parser->pos--;
                    break;
                }
            }
        }
    }
    
    parser->pos = start_pos;
    return false;
}

ethervox_result_t ethervox_tool_manifest_load_optimized(
    tool_manifest_registry_t* registry,
    const char* json_path
) {
    if (!registry || !json_path) {
        ETHERVOX_LOGE("Invalid arguments to load_optimized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Open JSON file
    FILE* fp = fopen(json_path, "rb");
    if (!fp) {
        ETHERVOX_LOGW("Optimized prompts not found: %s (fallback to level 1)", json_path);
        registry->fallback_level = 1;  // Use binary one-liners
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {  // Max 10MB
        ETHERVOX_LOGE("Invalid JSON file size: %ld", file_size);
        fclose(fp);
        registry->fallback_level = 1;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Read entire file
    char* json_data = (char*)malloc(file_size + 1);
    if (!json_data) {
        ETHERVOX_LOGE("Failed to allocate JSON buffer");
        fclose(fp);
        registry->fallback_level = 1;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    size_t read = fread(json_data, 1, file_size, fp);
    fclose(fp);
    
    if (read != (size_t)file_size) {
        ETHERVOX_LOGE("Failed to read JSON file");
        free(json_data);
        registry->fallback_level = 1;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    json_data[file_size] = '\0';
    
    // Parse JSON
    json_parser_t parser = { json_data, 0, (size_t)file_size };
    
    // Allocate cache
    tool_optimized_cache_t* cache = (tool_optimized_cache_t*)calloc(1, sizeof(tool_optimized_cache_t));
    if (!cache) {
        ETHERVOX_LOGE("Failed to allocate cache");
        free(json_data);
        registry->fallback_level = 1;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Expect opening brace
    if (!expect_char(&parser, '{')) {
        ETHERVOX_LOGE("Invalid JSON: expected opening brace");
        free(cache);
        free(json_data);
        registry->fallback_level = 1;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Parse model_name field
    char parsed_model[256];
    if (!find_key(&parser, "model_name") || !parse_string(&parser, parsed_model, sizeof(parsed_model))) {
        ETHERVOX_LOGW("JSON missing model_name, using 'unknown'");
        strncpy(parsed_model, "unknown", sizeof(parsed_model) - 1);
    }
    
    cache->model_name = strdup(parsed_model);
    
    // Find tools array
    parser.pos = 0;
    expect_char(&parser, '{');
    
    if (!find_key(&parser, "tools")) {
        ETHERVOX_LOGE("JSON missing 'tools' array");
        free(cache->model_name);
        free(cache);
        free(json_data);
        registry->fallback_level = 1;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (!expect_char(&parser, '[')) {
        ETHERVOX_LOGE("Invalid JSON: 'tools' is not an array");
        free(cache->model_name);
        free(cache);
        free(json_data);
        registry->fallback_level = 1;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Count tools first
    size_t array_start = parser.pos;
    uint32_t tool_count = 0;
    int depth = 1;
    bool in_string = false;
    
    while (parser.pos < parser.length && depth > 0) {
        char c = json_data[parser.pos++];
        
        if (c == '"' && (parser.pos == 1 || json_data[parser.pos - 2] != '\\')) {
            in_string = !in_string;
        }
        
        if (!in_string) {
            if (c == '{') tool_count++;
            if (c == '[') depth++;
            if (c == ']') depth--;
        }
    }
    
    if (tool_count == 0) {
        ETHERVOX_LOGW("No tools in optimized prompts JSON");
        free(cache->model_name);
        free(cache);
        free(json_data);
        registry->fallback_level = 1;
        return ETHERVOX_SUCCESS;
    }
    
    // Allocate prompts array
    cache->prompts = (tool_optimized_prompt_t*)calloc(tool_count, sizeof(tool_optimized_prompt_t));
    if (!cache->prompts) {
        ETHERVOX_LOGE("Failed to allocate prompts array");
        free(cache->model_name);
        free(cache);
        free(json_data);
        registry->fallback_level = 1;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Parse each tool
    parser.pos = array_start;
    cache->prompt_count = 0;
    
    while (cache->prompt_count < tool_count) {
        skip_whitespace(&parser);
        
        if (parser.pos >= parser.length || json_data[parser.pos] == ']') {
            break;
        }
        
        if (json_data[parser.pos] == ',') {
            parser.pos++;
            continue;
        }
        
        if (!expect_char(&parser, '{')) {
            break;
        }
        
        char tool_name[TOOL_NAME_MAX];
        char optimized_prompt[TOOL_DESC_MAX * 4];  // Allow larger optimized descriptions
        
        // Parse name
        if (!find_key(&parser, "name") || !parse_string(&parser, tool_name, sizeof(tool_name))) {
            ETHERVOX_LOGW("Skipping tool with missing name");
            // Skip to next object
            while (parser.pos < parser.length && json_data[parser.pos] != '}') parser.pos++;
            parser.pos++;
            continue;
        }
        
        // Reset parser position to find optimized_prompt
        size_t obj_start = parser.pos;
        parser.pos -= strlen(tool_name) + 10;  // Rough backtrack
        while (parser.pos > 0 && json_data[parser.pos] != '{') parser.pos--;
        parser.pos++;
        
        // Parse optimized_prompt
        if (!find_key(&parser, "optimized_prompt") || 
            !parse_string(&parser, optimized_prompt, sizeof(optimized_prompt))) {
            ETHERVOX_LOGW("Tool '%s' missing optimized_prompt", tool_name);
            // Skip to next object
            parser.pos = obj_start;
            while (parser.pos < parser.length && json_data[parser.pos] != '}') parser.pos++;
            parser.pos++;
            continue;
        }
        
        // Store in cache
        cache->prompts[cache->prompt_count].tool_name = strdup(tool_name);
        cache->prompts[cache->prompt_count].optimized_prompt = strdup(optimized_prompt);
        cache->prompt_count++;
        
        // Skip to end of object
        while (parser.pos < parser.length && json_data[parser.pos] != '}') parser.pos++;
        parser.pos++;
    }
    
    free(json_data);
    
    if (cache->prompt_count == 0) {
        ETHERVOX_LOGW("No valid tools parsed from JSON");
        free(cache->prompts);
        free(cache->model_name);
        free(cache);
        registry->fallback_level = 1;
        return ETHERVOX_SUCCESS;
    }
    
    registry->optimized_cache = cache;
    registry->fallback_level = 0;  // Optimal level!
    
    ETHERVOX_LOGI("Loaded %u optimized prompts for model: %s", 
                  cache->prompt_count, cache->model_name);
    
    return ETHERVOX_SUCCESS;
}
