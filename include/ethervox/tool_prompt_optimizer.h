/**
 * @file tool_prompt_optimizer.h
 * @brief Self-optimizing tool prompt system
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_TOOL_PROMPT_OPTIMIZER_H
#define ETHERVOX_TOOL_PROMPT_OPTIMIZER_H

#include <stddef.h>
#include <stdbool.h>
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct ethervox_governor ethervox_governor_t;
struct tool_manifest_registry;

/**
 * Optimize tool prompts with JSON output and batch processing
 * 
 * Generates model-specific optimized prompts in JSON format.
 * Processes tools in batches (5 at a time) to avoid KV cache overflow.
 * Output: ~/.ethervox/tools/optimized/<model>.json
 * 
 * @param governor Governor instance with loaded model
 * @param model_path Path to the model file
 * @param manifest_registry Tool manifest registry
 * @param optimize_new_only If true, only optimize tools not already in the JSON file
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_optimize_tool_prompts(
    ethervox_governor_t* governor,
    const char* model_path,
    struct tool_manifest_registry* manifest_registry,
    bool optimize_new_only
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_TOOL_PROMPT_OPTIMIZER_H
