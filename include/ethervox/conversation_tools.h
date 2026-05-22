/**
 * @file conversation_tools.h
 * @brief Conversational AI tools for LLM-controlled speech interaction
 *
 * Provides speak (TTS) and listen (microphone) tools that the LLM can invoke
 * to control conversational flow, enabling natural turn-taking and bidirectional
 * interruption.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_CONVERSATION_TOOLS_H
#define ETHERVOX_CONVERSATION_TOOLS_H

#include "ethervox/governor.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register conversation tools (speak, listen) with the tool registry
 * 
 * These tools enable LLM-controlled conversational interaction:
 * - speak: Text-to-speech output with turn-taking control
 * - listen: Microphone input capture with timeout
 * 
 * @param registry Tool registry to register into
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_conversation_tools_register(ethervox_tool_registry_t* registry);

/**
 * Set the conversation callbacks for real-time tool execution
 * 
 * Must be called before the Governor can execute speak/listen tools.
 * Typically called by voice_conversation.c when initializing a session.
 * 
 * @param callbacks Callback functions for speak/listen/interrupt
 */
void ethervox_conversation_tools_set_callbacks(void* callbacks);

/**
 * Get current conversation callbacks
 * 
 * @return Pointer to callbacks or NULL if not set
 */
void* ethervox_conversation_tools_get_callbacks(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_CONVERSATION_TOOLS_H
