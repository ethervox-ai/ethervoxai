/**
 * @file llm_tool_tests.h
 * @brief End-to-end LLM tool usage validation tests
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_LLM_TOOL_TESTS_H
#define ETHERVOX_LLM_TOOL_TESTS_H

#include "ethervox/governor.h"
#include "ethervox/memory_tools.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run comprehensive LLM tool usage tests
 * 
 * Tests that the LLM actually calls tools when prompted instead of hallucinating.
 * Validates each tool category individually to provide granular feedback.
 * 
 * Uses the existing Governor instance (which should have a model already loaded)
 * and the active memory store from the main program.
 * 
 * @param governor Active Governor instance (must have model loaded)
 * @param memory_store Active memory store instance
 * @param model_path Path to the loaded model (for lifecycle test)
 * @param verbose Enable verbose debug output in test report
 * @param test_filter Optional test name filter (case-insensitive, NULL = run all)
 */
void run_llm_tool_tests(ethervox_governor_t* governor, 
                       ethervox_memory_store_t* memory_store,
                       const char* model_path,
                       bool verbose,
                       const char* test_filter);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_LLM_TOOL_TESTS_H
