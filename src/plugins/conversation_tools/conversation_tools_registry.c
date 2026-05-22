/**
#include "ethervox/error.h"
 * @file conversation_tools_registry.c
 * @brief Registration of conversation tools with Governor
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/conversation_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"

#include <stdlib.h>

#define LOG_ERROR(...) ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...) ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

// Forward declarations from speak.c, listen.c and train_pronunciation.c
extern ethervox_tool_t* ethervox_tool_speak_create(void);
extern ethervox_tool_t* ethervox_tool_listen_create(void);
extern ethervox_tool_t* ethervox_tool_train_pronunciation_create(void);

ethervox_result_t ethervox_conversation_tools_register(ethervox_tool_registry_t* registry) {
    if (!registry) {
        LOG_ERROR("NULL registry passed to conversation_tools_register");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int ret = 0;
    
    // Register speak tool
    ethervox_tool_t* speak_tool = ethervox_tool_speak_create();
    if (ethervox_tool_registry_add(registry, speak_tool) != 0) {
        LOG_ERROR("Failed to register speak tool");
        ret = -1;
    } else {
        LOG_INFO("Registered 'speak' tool for conversational TTS output");
    }
    
    // Register listen tool
    ethervox_tool_t* listen_tool = ethervox_tool_listen_create();
    if (ethervox_tool_registry_add(registry, listen_tool) != 0) {
        LOG_ERROR("Failed to register listen tool");
        ret = -1;
    } else {
        LOG_INFO("Registered 'listen' tool for conversational microphone input");
    }
    
    // Register train_pronunciation tool
    ethervox_tool_t* train_pronunciation_tool = ethervox_tool_train_pronunciation_create();
    if (ethervox_tool_registry_add(registry, train_pronunciation_tool) != 0) {
        LOG_ERROR("Failed to register train_pronunciation tool");
        ret = -1;
    } else {
        LOG_INFO("Registered 'train_pronunciation' tool for adaptive pronunciation learning");
    }
    
    return ret;
}
