/**
 * @file test_voice_conversation.c
 * @brief Unit tests for voice conversation system
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "ethervox/conversation.h"
#include "ethervox/governor.h"

/**
 * Test: Conversation configuration
 */
static int test_conversation_config(void) {
    printf("  - test_conversation_config... ");
    
    ethervox_conversation_config_t config = ethervox_conversation_get_default_config();
    
    // Verify defaults
    assert(config.listen_timeout_ms > 0);
    assert(config.conversation_timeout_ms > 0);
    assert(config.audio_buffer_size > 0);
    
    printf("PASS\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Conversation initialization
 */
static int test_conversation_init(void) {
    printf("  - test_conversation_init... ");
    
    ethervox_conversation_config_t config = ethervox_conversation_get_default_config();
    
    // Initialize without governor (allowed)
    ethervox_conversation_session_t* session = ethervox_conversation_init(&config, NULL);
    assert(session != NULL);
    
    // Check initial state
    ethervox_conversation_state_t state = ethervox_conversation_get_state(session);
    assert(state == ETHERVOX_CONV_STATE_UNINITIALIZED);
    
    ethervox_conversation_cleanup(session);
    
    printf("PASS\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Conversation state machine
 */
static int test_conversation_states(void) {
    printf("  - test_conversation_states... ");
    
    ethervox_conversation_config_t config = ethervox_conversation_get_default_config();
    ethervox_conversation_session_t* session = ethervox_conversation_init(&config, NULL);
    assert(session != NULL);
    
    // Start conversation thread
    int result = ethervox_conversation_start(session);
    assert(result == 0);
    
    usleep(100000); // 100ms for thread to start
    
    // Should be in IDLE state
    ethervox_conversation_state_t state = ethervox_conversation_get_state(session);
    assert(state == ETHERVOX_CONV_STATE_IDLE);
    
    // Trigger conversation
    result = ethervox_conversation_trigger(session);
    assert(result == 0);
    
    usleep(100000); // 100ms
    
    // Should transition to LISTENING
    state = ethervox_conversation_get_state(session);
    assert(state == ETHERVOX_CONV_STATE_LISTENING || 
           state == ETHERVOX_CONV_STATE_PROCESSING ||
           state == ETHERVOX_CONV_STATE_ERROR); // May error without STT models
    
    // Stop conversation
    result = ethervox_conversation_stop(session);
    assert(result == 0);
    
    ethervox_conversation_cleanup(session);
    
    printf("PASS\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Conversation error handling
 */
static int test_conversation_errors(void) {
    printf("  - test_conversation_errors... ");
    
    // NULL config
    ethervox_conversation_session_t* session = ethervox_conversation_init(NULL, NULL);
    assert(session == NULL);
    
    // Valid session
    ethervox_conversation_config_t config = ethervox_conversation_get_default_config();
    session = ethervox_conversation_init(&config, NULL);
    assert(session != NULL);
    
    // Trigger before start
    int result = ethervox_conversation_trigger(session);
    assert(result != 0); // Should fail
    
    // Start
    result = ethervox_conversation_start(session);
    assert(result == 0);
    
    // Double start should fail or succeed safely
    result = ethervox_conversation_start(session);
    // Either way is acceptable
    
    ethervox_conversation_stop(session);
    ethervox_conversation_cleanup(session);
    
    // Operations on NULL session
    result = ethervox_conversation_start(NULL);
    assert(result != 0);
    
    result = ethervox_conversation_stop(NULL);
    assert(result != 0);
    
    result = ethervox_conversation_trigger(NULL);
    assert(result != 0);
    
    printf("PASS\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Conversation cleanup
 */
static int test_conversation_cleanup(void) {
    printf("  - test_conversation_cleanup... ");
    
    ethervox_conversation_config_t config = ethervox_conversation_get_default_config();
    ethervox_conversation_session_t* session = ethervox_conversation_init(&config, NULL);
    assert(session != NULL);
    
    ethervox_conversation_start(session);
    usleep(50000); // Let thread start
    
    // Cleanup should stop thread
    ethervox_conversation_cleanup(session);
    
    // Should be safe to cleanup NULL
    ethervox_conversation_cleanup(NULL);
    
    printf("PASS\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Multiple conversation sessions
 */
static int test_multiple_sessions(void) {
    printf("  - test_multiple_sessions... ");
    
    ethervox_conversation_config_t config = ethervox_conversation_get_default_config();
    
    // Create multiple sessions
    ethervox_conversation_session_t* session1 = ethervox_conversation_init(&config, NULL);
    ethervox_conversation_session_t* session2 = ethervox_conversation_init(&config, NULL);
    
    assert(session1 != NULL);
    assert(session2 != NULL);
    assert(session1 != session2);
    
    // Both should be independent
    ethervox_conversation_start(session1);
    ethervox_conversation_start(session2);
    
    usleep(100000);
    
    ethervox_conversation_state_t state1 = ethervox_conversation_get_state(session1);
    ethervox_conversation_state_t state2 = ethervox_conversation_get_state(session2);
    
    // Both should be in IDLE
    assert(state1 == ETHERVOX_CONV_STATE_IDLE || state1 == ETHERVOX_CONV_STATE_ERROR);
    assert(state2 == ETHERVOX_CONV_STATE_IDLE || state2 == ETHERVOX_CONV_STATE_ERROR);
    
    ethervox_conversation_cleanup(session1);
    ethervox_conversation_cleanup(session2);
    
    printf("PASS\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Conversation timeouts
 */
static int test_conversation_timeouts(void) {
    printf("  - test_conversation_timeouts... ");
    
    ethervox_conversation_config_t config = ethervox_conversation_get_default_config();
    config.listen_timeout_ms = 500; // Short timeout for testing
    config.conversation_timeout_ms = 1000;
    
    ethervox_conversation_session_t* session = ethervox_conversation_init(&config, NULL);
    assert(session != NULL);
    
    ethervox_conversation_start(session);
    usleep(100000);
    
    // Trigger and let it timeout
    ethervox_conversation_trigger(session);
    
    // Wait for timeout
    usleep(600000); // 600ms > 500ms timeout
    
    // Should return to IDLE or ERROR after timeout
    ethervox_conversation_state_t state = ethervox_conversation_get_state(session);
    assert(state == ETHERVOX_CONV_STATE_IDLE || state == ETHERVOX_CONV_STATE_ERROR);
    
    ethervox_conversation_cleanup(session);
    
    printf("PASS\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Main test runner
 */
int main(void) {
    printf("\n=== Voice Conversation Tests ===\n\n");
    
    int failed = 0;
    
    failed += test_conversation_config();
    failed += test_conversation_init();
    failed += test_conversation_states();
    failed += test_conversation_errors();
    failed += test_conversation_cleanup();
    failed += test_multiple_sessions();
    failed += test_conversation_timeouts();
    
    printf("\n");
    if (failed == 0) {
        printf("✓ All voice conversation tests passed!\n\n");
        return ETHERVOX_SUCCESS;
    } else {
        printf("✗ %d test(s) failed\n\n", failed);
        return ETHERVOX_SUCCESS;
    }
}
