/**
 * @file test_tool_catalogue.c
 * @brief Golden + behaviour tests for the tool catalogue loader (TASK-C2.6a)
 *
 * Golden test: the compute_tools group's name/description/schema, now loaded
 * from tools/catalogue/compute_tools.json, must be byte-identical to the
 * hardcoded C literals they replaced (16-TOOLS.md's whole point - description
 * text is the prompt, and it must not drift).
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/compute_tools.h"
#include "ethervox/system_info_tools.h"
#include "ethervox/context_tools.h"
#include "ethervox/get_tool_info.h"
#include "ethervox/startup_prompt_tools.h"
#include "ethervox/timer_tools.h"
#include "ethervox/unit_conversion.h"
#include "ethervox/memory_tools.h"
#include "ethervox/file_tools.h"
#include "ethervox/conversation_tools.h"
#include "ethervox/tool_catalogue.h"
#include "workspace_tools_catalogue.h"
#include "ethervox/error.h"
#include "unit/test_utils.h"
#include <stdio.h>
#include <string.h>

static void test_golden_calculator(void) {
    const ethervox_tool_t* t = ethervox_tool_calculator();
    CHECK(strcmp(t->name, "calculator_compute") == 0);
    CHECK(strcmp(t->description,
        "Compute ANY math calculation - use for all arithmetic, don't calculate mentally. "
        "Supports +, -, *, /, ^, sqrt, abs, parentheses.") == 0);
    CHECK(strcmp(t->parameters_json_schema,
        "{\"type\":\"object\",\"properties\":{"
        "\"expression\":{\"type\":\"string\",\"description\":\"Mathematical expression to evaluate\"},"
        "\"decimal_places\":{\"type\":\"integer\",\"description\":\"Number of decimal places (0-15, default: 2)\",\"default\":2}"
        "},\"required\":[\"expression\"]}") == 0);
}

static void test_golden_percentage(void) {
    const ethervox_tool_t* t = ethervox_tool_percentage();
    CHECK(strcmp(t->name, "percentage_calculate") == 0);
    CHECK(strcmp(t->description,
        "Calculate percentages for tips, tax, discounts (operations: of, increase, decrease, is_what_percent)") == 0);
    CHECK(strcmp(t->parameters_json_schema,
        "{\"type\":\"object\",\"properties\":{"
        "\"value\":{\"type\":\"number\",\"description\":\"The value to calculate percentage on\"},"
        "\"percentage\":{\"type\":\"number\",\"description\":\"The percentage amount\"},"
        "\"operation\":{\"type\":\"string\",\"enum\":[\"of\",\"increase\",\"decrease\",\"is_what_percent\"],\"description\":\"Operation to perform\"},"
        "\"decimal_places\":{\"type\":\"integer\",\"description\":\"Number of decimal places (0-15, default: 2)\",\"default\":2}"
        "},\"required\":[\"value\",\"percentage\",\"operation\"]}") == 0);
}

static void test_golden_time_tools(void) {
    const ethervox_tool_t* t;

    t = ethervox_tool_time_get_current();
    CHECK(strcmp(t->name, "get_time") == 0);
    CHECK(strcmp(t->description,
        "Get current time. Use when user asks 'what time is it' or needs to know the current time.") == 0);
    CHECK(strcmp(t->parameters_json_schema, "{}") == 0);

    t = ethervox_tool_time_get_date();
    CHECK(strcmp(t->name, "get_date") == 0);
    CHECK(strcmp(t->description,
        "Get current date (day/month/year). Use to get today's date. Use when user asks 'what's the date', "
        "needs today's date for calculations, or asks about days until/since an event.") == 0);
    CHECK(strcmp(t->parameters_json_schema, "{}") == 0);

    t = ethervox_tool_time_get_day_of_week();
    CHECK(strcmp(t->name, "get_day") == 0);
    CHECK(strcmp(t->description,
        "Get the current day of the week. Use when user asks 'what day is it' or 'what day of the week'.") == 0);
    CHECK(strcmp(t->parameters_json_schema, "{}") == 0);

    t = ethervox_tool_time_get_week_number();
    CHECK(strcmp(t->name, "time_get_week_number") == 0);
    CHECK(strcmp(t->description,
        "Get the current week number of the year. Use when user asks 'what week is it' or 'what week number'.") == 0);
    CHECK(strcmp(t->parameters_json_schema, "{}") == 0);
}

// Loader behaviour, independent of the embedded compute_tools.json above.
static void test_golden_context_manage(void) {
    ethervox_tool_registry_t registry;
    CHECK(ethervox_is_success(ethervox_tool_registry_init(&registry, 4)));
    CHECK(register_context_manage_tool(&registry, NULL) == 0);

    const ethervox_tool_t* t = ethervox_tool_registry_find(&registry, "context_manage");
    CHECK(t != NULL);
    CHECK(strcmp(t->description,
        "Manage context window when running low on space. "
        "CRITICAL: You MUST call this when context usage exceeds 80%. "
        "Choose action based on situation: summarize_old (best - preserves info), "
        "shift_window (fast - drops old), prune_unimportant (selective).") == 0);

    ethervox_tool_registry_cleanup(&registry);
}

static void test_golden_get_tool_info(void) {
    ethervox_tool_registry_t registry;
    CHECK(ethervox_is_success(ethervox_tool_registry_init(&registry, 4)));
    CHECK(ethervox_is_success(ethervox_get_tool_info_register(&registry)));

    const ethervox_tool_t* t = ethervox_tool_registry_find(&registry, "get_tool_info");
    CHECK(t != NULL);
    CHECK(strcmp(t->parameters_json_schema,
        "{\"type\":\"object\",\"properties\":{"
        "\"tool_name\":{\"type\":\"string\",\"description\":\"Name of tool to get info for, or '*' for all tools\"}"
        "},\"required\":[\"tool_name\"]}") == 0);

    ethervox_tool_registry_cleanup(&registry);
}

// The startup-prompt and file tools write or read the file system, so they exist only in profiles
// with ETHERVOX_FEATURE_FILE_TOOLS (not EDGE/WORKSPACE).
#if ETHERVOX_FEATURE_FILE_TOOLS
static void test_golden_startup_prompt_tools(void) {
    ethervox_tool_registry_t registry;
    CHECK(ethervox_is_success(ethervox_tool_registry_init(&registry, 4)));
    CHECK(ethervox_is_success(ethervox_startup_prompt_tools_register(&registry)));

    const ethervox_tool_t* t = ethervox_tool_registry_find(&registry, "startup_prompt_update");
    CHECK(t != NULL);
    CHECK(strcmp(t->description,
        "Update the startup prompt that runs when the assistant starts. Use this to customize the "
        "initial greeting or behavior. The prompt will be saved and used on next restart.") == 0);

    t = ethervox_tool_registry_find(&registry, "startup_prompt_read");
    CHECK(t != NULL);
    CHECK(strcmp(t->parameters_json_schema, "{\"type\":\"object\",\"properties\":{}}") == 0);

    ethervox_tool_registry_cleanup(&registry);
}
#endif

static void test_golden_timer_tools(void) {
    const ethervox_tool_t* t = ethervox_tool_timer_create();
    CHECK(strcmp(t->name, "timer_create") == 0);
    CHECK(strcmp(t->description,
        "Create a timer with duration in seconds. Use for countdown timers (e.g., '5 minute timer', "
        "'timer for 30 seconds')") == 0);

    t = ethervox_tool_alarm_create();
    CHECK(strcmp(t->name, "alarm_create") == 0);
    CHECK(strcmp(t->parameters_json_schema,
        "{\"type\":\"object\",\"properties\":{"
        "\"hour\":{\"type\":\"integer\",\"description\":\"Hour in 24-hour format (0-23)\"},"
        "\"minute\":{\"type\":\"integer\",\"description\":\"Minute (0-59)\"},"
        "\"label\":{\"type\":\"string\",\"description\":\"Optional label/name for the alarm\"}"
        "},\"required\":[\"hour\",\"minute\"]}") == 0);
}

static void test_golden_unit_conversion(void) {
    ethervox_tool_registry_t registry;
    CHECK(ethervox_is_success(ethervox_tool_registry_init(&registry, 4)));
    CHECK(ethervox_is_success(ethervox_unit_conversion_register(&registry)));

    const ethervox_tool_t* t = ethervox_tool_registry_find(&registry, "unit_convert");
    CHECK(t != NULL);
    CHECK(strcmp(t->test_scenario, "Convert 10 miles to kilometers") == 0);

    ethervox_tool_registry_cleanup(&registry);
}

// Loader behaviour, independent of the embedded compute_tools.json above.
static void test_golden_system_info(void) {
    ethervox_tool_registry_t registry;
    CHECK(ethervox_is_success(ethervox_tool_registry_init(&registry, 8)));
    CHECK(ethervox_is_success(ethervox_system_info_tools_register(&registry)));

    const ethervox_tool_t* t = ethervox_tool_registry_find(&registry, "system_version");
    CHECK(t != NULL);
    CHECK(strcmp(t->description,
        "Get EthervoxAI version and build information including version number, git commit hash, "
        "build type, and platform.") == 0);
    CHECK(strcmp(t->parameters_json_schema, "{\"type\":\"object\",\"properties\":{},\"required\":[]}") == 0);

    t = ethervox_tool_registry_find(&registry, "system_capabilities");
    CHECK(t != NULL);
    CHECK(strcmp(t->description,
        "Get system capabilities and configuration limits including max languages, plugins, audio "
        "settings, and platform type.") == 0);
    CHECK(strcmp(t->parameters_json_schema, "{\"type\":\"object\",\"properties\":{},\"required\":[]}") == 0);

    ethervox_tool_registry_cleanup(&registry);
}
static void test_loader_not_found(void) {
    ethervox_tool_t tool = {0};
    ethervox_result_t r = ethervox_tool_catalogue_load(
        "[{\"name\":\"a\",\"profiles\":[\"DESKTOP\"],\"description\":\"d\",\"schema\":{}}]",
        "does_not_exist", "DESKTOP", &tool);
    CHECK(r == ETHERVOX_ERROR_NOT_FOUND);
}

static void test_loader_profile_excluded(void) {
    ethervox_tool_t tool = {0};
    ethervox_result_t r = ethervox_tool_catalogue_load(
        "[{\"name\":\"a\",\"profiles\":[\"EDGE\"],\"description\":\"d\",\"schema\":{}}]",
        "a", "DESKTOP", &tool);
    CHECK(r == ETHERVOX_ERROR_NOT_SUPPORTED);
}

static void test_loader_unknown_profile_is_error(void) {
    ethervox_tool_t tool = {0};
    ethervox_result_t r = ethervox_tool_catalogue_load(
        "[{\"name\":\"a\",\"profiles\":[\"NOT_A_PROFILE\"],\"description\":\"d\",\"schema\":{}}]",
        "a", "DESKTOP", &tool);
    CHECK(r == ETHERVOX_ERROR_INVALID_ARGUMENT);
}

static void test_loader_success(void) {
    ethervox_tool_t tool = {0};
    ethervox_result_t r = ethervox_tool_catalogue_load(
        "[{\"name\":\"a\",\"profiles\":[\"DESKTOP\",\"WORKSPACE\"],\"description\":\"hello\","
        "\"schema\":{\"type\":\"object\"}}]",
        "a", "DESKTOP", &tool);
    CHECK(ethervox_is_success(r));
    CHECK(strcmp(tool.name, "a") == 0);
    CHECK(strcmp(tool.description, "hello") == 0);
    CHECK(strcmp(tool.parameters_json_schema, "{\"type\":\"object\"}") == 0);
}

static void test_golden_memory_tools(void) {
    ethervox_tool_registry_t registry;
    CHECK(ethervox_is_success(ethervox_tool_registry_init(&registry, 16)));
    ethervox_memory_store_t store;
    CHECK(ethervox_is_success(ethervox_memory_init(&store, "test_tool_catalogue", NULL)));
    CHECK(ethervox_is_success(ethervox_memory_tools_register(&registry, &store)));

    const ethervox_tool_t* t = ethervox_tool_registry_find(&registry, "memory_store");
    CHECK(t != NULL);
    CHECK(strcmp(t->test_scenario, "Remember my favorite color is blue") == 0);

    t = ethervox_tool_registry_find(&registry, "memory_store_pattern");
    CHECK(t != NULL);
    CHECK(strcmp(t->parameters_json_schema,
        "{\"type\":\"object\",\"properties\":{"
        "\"pattern\":{\"type\":\"string\",\"description\":\"Description of what worked well (required)\"}"
        "},\"required\":[\"pattern\"]}") == 0);

    ethervox_memory_cleanup(&store);
    ethervox_tool_registry_cleanup(&registry);
}

#if ETHERVOX_FEATURE_FILE_TOOLS
static void test_golden_file_tools(void) {
    ethervox_tool_registry_t registry;
    CHECK(ethervox_is_success(ethervox_tool_registry_init(&registry, 16)));

    ethervox_file_tools_config_t config;
    const char* base_paths[] = { "/tmp", NULL };
    CHECK(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    CHECK(ethervox_is_success(ethervox_file_tools_register(&registry, &config)));

    const ethervox_tool_t* t = ethervox_tool_registry_find(&registry, "file_read");
    CHECK(t != NULL);
    CHECK(strcmp(t->description,
        "Read contents of a text based file (.txt, .md, .org, .c, etc.). Maximum 10MB.") == 0);

    ethervox_path_config_t path_config = {0};
    CHECK(ethervox_is_success(ethervox_path_config_register(&registry, &path_config)));

    t = ethervox_tool_registry_find(&registry, "path_set");
    CHECK(t != NULL);
    CHECK(strcmp(t->parameters_json_schema,
        "{\"type\":\"object\",\"properties\":{"
        "\"label\":{\"type\":\"string\",\"description\":\"Human-friendly label (e.g., 'Notes', 'Projects', 'Documents')\"},"
        "\"path\":{\"type\":\"string\",\"description\":\"Absolute directory path\"},"
        "\"description\":{\"type\":\"string\",\"description\":\"Optional description of what this path contains\"}"
        "},\"required\":[\"label\",\"path\"]}") == 0);

    ethervox_file_tools_cleanup(&config);
    ethervox_tool_registry_cleanup(&registry);
}
#endif

static void test_golden_conversation_tools(void) {
    ethervox_tool_registry_t registry;
    CHECK(ethervox_is_success(ethervox_tool_registry_init(&registry, 8)));
    CHECK(ethervox_is_success(ethervox_conversation_tools_register(&registry)));

    const ethervox_tool_t* t = ethervox_tool_registry_find(&registry, "speak");
    CHECK(t != NULL);
    CHECK(strcmp(t->test_scenario, "Say hello out loud") == 0);

    t = ethervox_tool_registry_find(&registry, "listen");
    CHECK(t != NULL);
    CHECK(strcmp(t->parameters_json_schema,
        "{\"type\":\"object\",\"properties\":{"
        "\"timeout_ms\":{\"type\":\"integer\",\"description\":\"Maximum time to wait for user input in milliseconds (default: 5000, range: 1000-30000)\"},"
        "\"prompt_hint\":{\"type\":\"string\",\"description\":\"Optional hint about what you're waiting for (e.g., 'waiting for your answer...')\"}"
        "},\"required\":[]}") == 0);

    ethervox_tool_registry_cleanup(&registry);
}

// workspace_tools' ethervox_workspace_tools_register() takes a private
// workspace_operations_t (src/plugins/workspace_tools/, not include/) - go
// straight at the embedded catalogue instead of standing up a fake registry.
static void test_golden_workspace_tools_catalogue(void) {
    // workspace_tools is WORKSPACE-only; pass that profile explicitly rather
    // than ethervox_tool_catalogue_build_profile(), which reflects this test
    // binary's own (DESKTOP) build, not the profile this contract targets.
    //
    // workspace_create_note's description is 514 chars - already over
    // ethervox_tool_t's 256-byte description[] before this migration existed
    // (a pre-existing bug: the original static initializer's string literal
    // didn't fit either, so it truncated with no guaranteed NUL terminator -
    // undefined behaviour in C, not "byte-identical" to preserve). The
    // loader's memset+strncpy makes truncation deterministic and
    // NUL-terminated instead, which is a genuine (if incidental) fix, so this
    // asserts the safe truncated form rather than the un-truncatable original.
    ethervox_tool_t tool = {0};
    CHECK(ethervox_is_success(ethervox_tool_catalogue_load(
        ETHERVOX_CATALOGUE_WORKSPACE_TOOLS_JSON, "workspace_create_note",
        "WORKSPACE", &tool)));
    const char* full_description =
        "CREATE a new graph node/object in the workspace (stored as a markdown note). Use this "
        "when the user asks to create/add a new 'node', 'object', 'note', or 'item' in the graph. "
        "Parameters: 'title' (string, required) - the node name/title, and 'content' (string, "
        "required) - node content in markdown format. Returns the newly created node's UUID. "
        "IMPORTANT: This creates a NEW standalone graph node. To add info to an EXISTING node, "
        "first search for it with workspace_search_objects, then use workspace_update_object.";
    CHECK(strlen(tool.description) == sizeof(tool.description) - 1);
    CHECK(strncmp(tool.description, full_description, sizeof(tool.description) - 1) == 0);

    CHECK(ethervox_is_success(ethervox_tool_catalogue_load(
        ETHERVOX_CATALOGUE_WORKSPACE_TOOLS_JSON, "workspace_highlight_nodes",
        "WORKSPACE", &tool)));
    CHECK(strcmp(tool.parameters_json_schema,
        "{\"type\":\"object\",\"properties\":{\"node_ids\":{\"type\":\"array\",\"description\":"
        "\"Array of node UUIDs to highlight in the graph\",\"items\":{\"type\":\"string\"}}},"
        "\"required\":[\"node_ids\"]}") == 0);
}

int main(void) {
    RUN_TEST(test_golden_calculator);
    RUN_TEST(test_golden_percentage);
    RUN_TEST(test_golden_time_tools);
    RUN_TEST(test_golden_system_info);
    RUN_TEST(test_golden_context_manage);
    RUN_TEST(test_golden_get_tool_info);
#if ETHERVOX_FEATURE_FILE_TOOLS
    RUN_TEST(test_golden_startup_prompt_tools);
#endif
    RUN_TEST(test_golden_timer_tools);
    RUN_TEST(test_golden_unit_conversion);
    RUN_TEST(test_golden_memory_tools);
#if ETHERVOX_FEATURE_FILE_TOOLS
    RUN_TEST(test_golden_file_tools);
#endif
    RUN_TEST(test_golden_conversation_tools);
    RUN_TEST(test_golden_workspace_tools_catalogue);
    RUN_TEST(test_loader_not_found);
    RUN_TEST(test_loader_profile_excluded);
    RUN_TEST(test_loader_unknown_profile_is_error);
    RUN_TEST(test_loader_success);
    printf("\nAll tool_catalogue tests passed.\n");
    return 0;
}
