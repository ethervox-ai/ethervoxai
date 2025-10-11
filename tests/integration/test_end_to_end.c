/**
 * @file test_end_to_end.c
 * @brief Integration test for EthervoxAI end-to-end functionality
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * 
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - NonCommercial: Not for commercial use
 * - ShareAlike: Distribute under same license
 * 
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ethervox/config.h"
#include "ethervox/audio.h"
#include "ethervox/dialogue.h"
#include "ethervox/plugins.h"

void test_system_initialization() {
    printf("Testing complete system initialization...\n");
    
    // Test audio subsystem
    ethervox_audio_config_t audio_config = ethervox_audio_get_default_config();
    ethervox_audio_runtime_t audio_runtime;
    
    // Note: This may fail in CI without audio hardware, which is expected
    int audio_result = ethervox_audio_init(&audio_runtime, &audio_config);
    if (audio_result == 0) {
        printf("  ✓ Audio subsystem initialized successfully\n");
        ethervox_audio_cleanup(&audio_runtime);
    } else {
        printf("  ⚠ Audio subsystem failed to initialize (no hardware, expected)\n");
    }
    
    // Test dialogue subsystem  
    ethervox_dialogue_engine_t dialogue_engine;
    // int dialogue_result = ethervox_dialogue_init(&dialogue_engine, "en");
    ethervox_llm_config_t llm_config = ethervox_dialogue_get_default_llm_config();
    llm_config.language_code = "en";
    int dialogue_result = ethervox_dialogue_init(&dialogue_engine, &llm_config);
    if (dialogue_result == 0) {
        printf("  ✓ Dialogue engine initialized successfully\n");
        ethervox_dialogue_cleanup(&dialogue_engine);
    } else {
        printf("  ⚠ Dialogue engine failed to initialize (missing models, expected)\n");
    }
    
    // Test plugin manager
    ethervox_plugin_manager_t plugin_manager;
    int plugin_result = ethervox_plugin_manager_init(&plugin_manager,NULL);
    assert(plugin_result == 0);
    printf("  ✓ Plugin manager initialized successfully\n");
    ethervox_plugin_manager_cleanup(&plugin_manager);
    
    printf("✓ System initialization test completed\n");
}

void test_configuration_consistency() {
    printf("Testing configuration consistency across modules...\n");
    
    // Test that all modules use consistent configuration
    ethervox_audio_config_t audio_config = ethervox_audio_get_default_config();
    
    assert(audio_config.sample_rate == ETHERVOX_AUDIO_SAMPLE_RATE);
    assert(audio_config.buffer_size == ETHERVOX_AUDIO_BUFFER_SIZE);
    
    printf("  ✓ Audio configuration consistent with global config\n");
    
    // Test platform-specific configurations
    #ifdef ETHERVOX_PLATFORM_EMBEDDED
        assert(audio_config.buffer_size <= 1024);
        printf("  ✓ Embedded platform configuration validated\n");
    #else
        assert(audio_config.buffer_size <= 4096);
        printf("  ✓ Desktop platform configuration validated\n");
    #endif
    
    printf("✓ Configuration consistency test passed\n");
}

void test_error_handling_chain() {
    printf("Testing error handling across module boundaries...\n");
    
    // Test cascading error handling
    ethervox_audio_runtime_t runtime;
    
    // Test with invalid parameters
    int result = ethervox_audio_init(&runtime, NULL);
    assert(result == -1);
    printf("  ✓ Audio module properly rejects null config\n");
    
    // Test plugin manager error handling
    ethervox_plugin_manager_t manager;
    result = ethervox_plugin_manager_init(NULL,NULL);
    assert(result == -1);
    printf("  ✓ Plugin manager properly rejects null pointer\n");
    
    printf("✓ Error handling chain test passed\n");
}

void test_memory_management() {
    printf("Testing memory management across modules...\n");
    
    // Test multiple init/cleanup cycles
    for (int i = 0; i < 3; i++) {
        ethervox_plugin_manager_t manager;
        int result = ethervox_plugin_manager_init(&manager,NULL);
        assert(result == 0);
        
        // Simulate some operations
    assert(manager.plugin_count == ETHERVOX_BUILTIN_PLUGIN_COUNT);
        
        ethervox_plugin_manager_cleanup(&manager);
        printf("  ✓ Init/cleanup cycle %d completed\n", i + 1);
    }
    
    printf("✓ Memory management test passed\n");
}

void test_version_compatibility() {
    printf("Testing version and compatibility information...\n");
    
    // Test version information is accessible
    printf("  - EthervoxAI Version: %s\n", ETHERVOX_VERSION_STRING);
    printf("  - Major: %d, Minor: %d, Patch: %d\n", 
           ETHERVOX_VERSION_MAJOR, ETHERVOX_VERSION_MINOR, ETHERVOX_VERSION_PATCH);
    
    // Test platform information
    #ifdef ETHERVOX_PLATFORM_DESKTOP
        printf("  - Platform: Desktop\n");
    #else
        printf("  - Platform: Embedded\n");
    #endif
    
    #ifdef DEBUG_ENABLED
        printf("  - Build: Debug\n");
    #else
        printf("  - Build: Release\n");
    #endif
    
    printf("✓ Version compatibility test passed\n");
}

int main() {
    printf("Running EthervoxAI End-to-End Integration Tests\n");
    printf("===============================================\n");
    
    test_system_initialization();
    test_configuration_consistency();
    test_error_handling_chain();
    test_memory_management();
    test_version_compatibility();
    
    printf("===============================================\n");
    printf("All integration tests completed!\n");
    printf("Note: Some subsystem failures are expected in CI environments\n");
    printf("without audio hardware or language models.\n");
    
    return 0;
}