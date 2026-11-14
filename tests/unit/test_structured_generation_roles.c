// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
/**
 * @file test_structured_generation_roles.c
 * @brief ethervox_structured_render_prompt emits every role the model was trained on (C6.1a)
 *
 * The expected strings are Granite 4's own template output, captured from
 * `llama-server --jinja /apply-template` against granite-4.0-h-1b (BACKLOG-80): system role,
 * user role, assistant turn carrying a <tool_call> block, and the tool result as a *user* turn
 * wrapped in <tool_response> (not <tool_result>).
 */

#include "ethervox/structured_generation.h"
#include "test_utils.h"
#include <stdlib.h>
#include <string.h>

#define SYS "<|start_of_role|>system<|end_of_role|>SYS<|end_of_text|>\n"
#define USER "<|start_of_role|>user<|end_of_role|>Create a file notes.md<|end_of_text|>\n"
#define ASSISTANT_START "<|start_of_role|>assistant<|end_of_role|>"

static void test_single_user_turn_without_system_is_unchanged(void) {
    char* out = NULL;
    CHECK(ethervox_structured_render_prompt(NULL, "Create a file notes.md", NULL, 0, &out) == ETHERVOX_SUCCESS);
    CHECK(strcmp(out, USER ASSISTANT_START) == 0);
    free(out);
    printf("  ✓ no system role => exactly the legacy user + assistant_start prompt\n");
}

static void test_empty_system_prompt_emits_no_system_turn(void) {
    char* out = NULL;
    CHECK(ethervox_structured_render_prompt("", "Create a file notes.md", NULL, 0, &out) == ETHERVOX_SUCCESS);
    CHECK(strcmp(out, USER ASSISTANT_START) == 0);
    free(out);
    printf("  ✓ empty system prompt emits no system turn\n");
}

static void test_system_and_user(void) {
    char* out = NULL;
    CHECK(ethervox_structured_render_prompt("SYS", "Create a file notes.md", NULL, 0, &out) == ETHERVOX_SUCCESS);
    CHECK(strcmp(out, SYS USER ASSISTANT_START) == 0);
    free(out);
    printf("  ✓ system + user + assistant_start\n");
}

static void test_tool_round_trip_matches_granites_own_template(void) {
    const ethervox_chat_turn_t turns[] = {
        { ETHERVOX_CHAT_ROLE_ASSISTANT, "<tool_call>\n{\"name\": \"list_folder\", \"arguments\": {}}\n</tool_call>" },
        { ETHERVOX_CHAT_ROLE_TOOL, "{\"items\":[]}" },
    };
    char* out = NULL;
    CHECK(ethervox_structured_render_prompt("SYS", "Create a file notes.md", turns, 2, &out) == ETHERVOX_SUCCESS);
    const char* expected =
        SYS USER
        ASSISTANT_START "<tool_call>\n{\"name\": \"list_folder\", \"arguments\": {}}\n</tool_call><|end_of_text|>\n"
        "<|start_of_role|>user<|end_of_role|>\n<tool_response>\n{\"items\":[]}\n</tool_response><|end_of_text|>\n"
        ASSISTANT_START;
    CHECK(strcmp(out, expected) == 0);
    free(out);
    printf("  ✓ assistant tool_call + tool_response turns match Granite's template byte for byte\n");
}

static void test_invalid_arguments(void) {
    char* out = NULL;
    CHECK(ethervox_structured_render_prompt(NULL, NULL, NULL, 0, &out) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    CHECK(ethervox_structured_render_prompt(NULL, "x", NULL, 1, &out) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    const ethervox_chat_turn_t bad[] = { { (ethervox_chat_role_t)99, "x" } };
    CHECK(ethervox_structured_render_prompt(NULL, "x", bad, 1, &out) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    printf("  ✓ invalid arguments are rejected, not rendered\n");
}

int main(void) {
    printf("=== structured generation role rendering (C6.1a) ===\n");
    test_single_user_turn_without_system_is_unchanged();
    test_empty_system_prompt_emits_no_system_turn();
    test_system_and_user();
    test_tool_round_trip_matches_granites_own_template();
    test_invalid_arguments();
    printf("ALL PASSED\n");
    return 0;
}
