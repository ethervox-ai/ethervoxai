/**
 * @file system_info_tools.h
 * @brief System information tools API
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_SYSTEM_INFO_TOOLS_H
#define ETHERVOX_SYSTEM_INFO_TOOLS_H

#include "ethervox/governor.h"
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register system info tools with the tool registry
 * 
 * Registers the following tools:
 * - system_version: Get version, build info, and git commit hash
 * - system_capabilities: Get system capabilities and limits
 * 
 * @param registry Tool registry to register with
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_system_info_tools_register(ethervox_tool_registry_t* registry);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_SYSTEM_INFO_TOOLS_H
