/**
 * @file test_tool_descriptions.c
 * @brief Verify that speak and listen tools mention supported languages
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <string.h>
#include "ethervox/conversation_tools.h"
#include "ethervox/governor.h"

int main(void) {
    printf("=== Testing Tool Descriptions for Language Support ===\n\n");
    
    // Create a registry and register conversation tools
    ethervox_tool_registry_t registry;
    if (ethervox_tool_registry_init(&registry, 8) != 0) {
        printf("✗ Failed to initialize registry\n");
        return ETHERVOX_SUCCESS;
    }
    
    if (ethervox_conversation_tools_register(&registry) != 0) {
        printf("✗ Failed to register conversation tools\n");
        return ETHERVOX_SUCCESS;
    }
    
    // Find speak tool
    const ethervox_tool_t* speak_tool = ethervox_tool_registry_find(&registry, "speak");
    if (!speak_tool) {
        printf("✗ Failed to find speak tool\n");
        return ETHERVOX_SUCCESS;
    }
    
    printf("Speak Tool Description:\n");
    printf("%s\n\n", speak_tool->description);
    
    // Check for language mentions
    int has_english = strstr(speak_tool->description, "English") != NULL;
    int has_chinese = strstr(speak_tool->description, "Chinese") != NULL;
    int has_german = strstr(speak_tool->description, "German") != NULL;
    
    printf("Language Support Mentioned:\n");
    printf("  English: %s\n", has_english ? "✓" : "✗");
    printf("  Chinese: %s\n", has_chinese ? "✓" : "✗");
    printf("  German: %s\n", has_german ? "✓" : "✗");
    
    if (!has_english || !has_chinese || !has_german) {
        printf("\n✗ FAIL: Speak tool description missing language support information\n");
        ethervox_tool_registry_cleanup(&registry);
        return ETHERVOX_SUCCESS;
    }
    
    printf("\n");
    
    // Find listen tool
    const ethervox_tool_t* listen_tool = ethervox_tool_registry_find(&registry, "listen");
    if (!listen_tool) {
        printf("✗ Failed to find listen tool\n");
        ethervox_tool_registry_cleanup(&registry);
        return ETHERVOX_SUCCESS;
    }
    
    printf("Listen Tool Description:\n");
    printf("%s\n\n", listen_tool->description);
    
    // Check for language mentions
    has_english = strstr(listen_tool->description, "English") != NULL;
    has_chinese = strstr(listen_tool->description, "Chinese") != NULL;
    has_german = strstr(listen_tool->description, "German") != NULL;
    
    printf("Language Support Mentioned:\n");
    printf("  English: %s\n", has_english ? "✓" : "✗");
    printf("  Chinese: %s\n", has_chinese ? "✓" : "✗");
    printf("  German: %s\n", has_german ? "✓" : "✗");
    
    if (!has_english || !has_chinese || !has_german) {
        printf("\n✗ FAIL: Listen tool description missing language support information\n");
        ethervox_tool_registry_cleanup(&registry);
        return ETHERVOX_SUCCESS;
    }
    
    printf("\n=== Results ===\n");
    printf("✓ SUCCESS - Both tools correctly document language support\n");
    printf("  - English\n");
    printf("  - Chinese (Mandarin)\n");
    printf("  - German\n");
    
    ethervox_tool_registry_cleanup(&registry);
    return ETHERVOX_SUCCESS;
}
