/**
 * @file windows_stubs.c
 * @brief Windows stub implementations for excluded features
 * 
 * Provides empty stubs for memory tools and conversation tools that were
 * excluded from Windows build due to pthread dependencies.
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <stdbool.h>

/**
 * Stub for memory privacy mode (memory tools excluded on Windows)
 */
void ethervox_memory_set_privacy_mode(bool disable_logging) {
    // No-op on Windows - memory tools not available
    (void)disable_logging;
}

/**
 * Stub for conversation tools callbacks (conversation tools excluded on Windows)
 */
void ethervox_conversation_tools_set_callbacks(void* callbacks) {
    // No-op on Windows - conversation tools not available
    (void)callbacks;
}
