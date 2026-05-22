/**
 * @file startup_prompt_tools.h
 * @brief Tools for managing the startup prompt
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_STARTUP_PROMPT_TOOLS_H
#define ETHERVOX_STARTUP_PROMPT_TOOLS_H

#include <stddef.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register startup prompt management tool with the Governor
 * 
 * @param registry Tool registry to register with
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_startup_prompt_tools_register(void* registry);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_STARTUP_PROMPT_TOOLS_H
