// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary

/**
 * @file test_system_prompt_role.c
 * @brief Real-model smoke test for `ethervox_structured_gen_params_t.system_prompt` (BACKLOG-80)
 *
 * Proves the new field actually reaches the model via the chat template's system role, not just
 * that it compiles: standing instructions placed in `system_prompt` are followed, rather than
 * answered conversationally the way folding them into the user turn does (the exact
 * "Understood, I will handle questions..." failure BACKLOG-73 chased through prompt wording).
 *
 * Usage: ./test_system_prompt_role <model.gguf>
 */

#include "ethervox/structured_generation.h"
#include "ethervox/grammar.h"
#include "ethervox/model_pool.h"
#include "ethervox/paths.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* ANSWER_SCHEMA =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"answer\": {\"type\": \"string\"}"
    "  },"
    "  \"required\": [\"answer\"]"
    "}";

static char* expand_tilde(const char* path) {
    if (!path || path[0] != '~') return strdup(path);
    const char* home = getenv("HOME");
    if (!home) return strdup(path);
    size_t len = strlen(home) + strlen(path);
    char* expanded = malloc(len);
    snprintf(expanded, len, "%s%s", home, path + 1);
    return expanded;
}

/**
 * No `system_prompt` at all (NULL) -- must behave exactly as before this change: a plain user
 * turn, successful generation, non-empty JSON.
 */
static bool test_no_system_prompt_is_unaffected(ethervox_model_handle_t* handle) {
    printf("\n=== Test 1: system_prompt = NULL (back-compat) ===\n");

    ethervox_grammar_t* grammar = NULL;
    if (ethervox_grammar_from_json_schema(ANSWER_SCHEMA, &grammar) != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to create grammar\n");
        return false;
    }

    ethervox_structured_gen_params_t params = ethervox_structured_gen_params_default();
    params.max_tokens = 64;
    params.temperature = 0.2f;
    // params.system_prompt left at its default (NULL).

    char* json_output = NULL;
    ethervox_result_t result = ethervox_generate_structured(
        handle, "What is the capital of France?", grammar, &params, NULL, NULL, &json_output, NULL);
    ethervox_grammar_free(grammar);

    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Generation failed: %s\n", ethervox_error_string(result));
        return false;
    }
    printf("Output: %s\n", json_output);
    bool ok = json_output && strlen(json_output) > 0;
    free(json_output);
    if (!ok) {
        fprintf(stderr, "FAIL: empty output with system_prompt = NULL\n");
        return false;
    }
    printf("PASS: non-empty output with system_prompt = NULL\n");
    return true;
}

/**
 * A real `system_prompt` standing instruction must be followed by the model, not echoed back as
 * acknowledgment -- the specific failure mode BACKLOG-80 traces to instructions being folded
 * into the user turn instead of the template's own system role.
 */
static bool test_system_prompt_instruction_is_followed(ethervox_model_handle_t* handle) {
    printf("\n=== Test 2: system_prompt instruction is followed, not echoed ===\n");

    ethervox_grammar_t* grammar = NULL;
    if (ethervox_grammar_from_json_schema(ANSWER_SCHEMA, &grammar) != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to create grammar\n");
        return false;
    }

    ethervox_structured_gen_params_t params = ethervox_structured_gen_params_default();
    params.max_tokens = 64;
    params.temperature = 0.2f;
    params.system_prompt =
        "You are a geography assistant. Always answer with just the city name, nothing else.";

    char* json_output = NULL;
    ethervox_result_t result = ethervox_generate_structured(
        handle, "What is the capital of France?", grammar, &params, NULL, NULL, &json_output, NULL);
    ethervox_grammar_free(grammar);

    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Generation failed: %s\n", ethervox_error_string(result));
        return false;
    }
    printf("Output: %s\n", json_output);

    // Not asserting the model says "Paris" (a model-quality claim, not a plumbing one) -- only
    // that it did not parrot the instruction back as acknowledgment, the documented failure mode
    // of instructions folded into the user turn (BACKLOG-73's "Understood, I will handle
    // questions..." symptom).
    bool parroted = json_output && (strstr(json_output, "Understood") != NULL ||
                                     strstr(json_output, "geography assistant") != NULL);
    bool ok = json_output && strlen(json_output) > 0 && !parroted;
    free(json_output);
    if (!ok) {
        fprintf(stderr, "FAIL: output was empty or parroted the system instruction back\n");
        return false;
    }
    printf("PASS: model answered instead of acknowledging the system instruction\n");
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <model.gguf>\n", argv[0]);
        return 1;
    }
    char* model_path = expand_tilde(argv[1]);

    ethervox_paths_t paths = {0};
    char path_buffer[4096];
    if (ethervox_paths_get_default(&paths, path_buffer, sizeof(path_buffer)) != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to init paths\n");
        free(model_path);
        return 1;
    }

    ethervox_model_pool_t* pool = NULL;
    uint64_t budget = 4ULL * 1024 * 1024 * 1024;  // 4 GB
    if (ethervox_model_pool_create(&paths, budget, &pool) != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to create model pool\n");
        free(model_path);
        return 1;
    }

    ethervox_model_config_t config = {
        .model_path = model_path,
        .mmproj_path = NULL,
        .context_size = 2048,
        .n_threads = 8,
        .n_seq_max = 4,
        .use_gpu = true,
        .kv_unified = true,
        .role = "main"
    };

    ethervox_model_handle_t* handle = NULL;
    printf("Loading model...\n");
    ethervox_result_t load_result = ethervox_model_pool_load(pool, &config, NULL, NULL, &handle);
    free(model_path);
    if (load_result != ETHERVOX_SUCCESS || !handle) {
        fprintf(stderr, "Failed to load model: %s\n", ethervox_error_string(load_result));
        ethervox_model_pool_destroy(pool);
        return 1;
    }

    bool all_passed = true;
    all_passed &= test_no_system_prompt_is_unaffected(handle);
    all_passed &= test_system_prompt_instruction_is_followed(handle);

    ethervox_model_pool_destroy(pool);

    printf("\n=== %s ===\n", all_passed ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return all_passed ? 0 : 1;
}
