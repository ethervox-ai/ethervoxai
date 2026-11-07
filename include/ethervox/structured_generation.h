/**
 * @file structured_generation.h
 * @brief Grammar-constrained generation with calibrated confidence scoring
 *
 * Provides structured generation (JSON output) with confidence metrics based on
 * log-probabilities. Confidence excludes grammar-forced structural tokens to
 * measure only the model's content choices.
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_STRUCTURED_GENERATION_H
#define ETHERVOX_STRUCTURED_GENERATION_H

#include "ethervox/error.h"
#include "ethervox/event_stream.h"
#include "ethervox/grammar.h"
#include "ethervox/model_pool.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generation parameters for structured output
 * 
 * Determinism boundary (C4.2):
 * Given the same seed, same prompt, same model, and same thread count, generation
 * produces IDENTICAL byte-for-byte output. This holds for:
 * - Fixed build configuration (same llama.cpp version, same compile flags)
 * - Same backend (CPU-only, Metal, CUDA, etc.) - switching backends breaks determinism
 * - Same thread count (n_threads) - different thread counts may reorder floating-point ops
 * 
 * Determinism does NOT hold across:
 * - Different llama.cpp versions (internal sampling may change)
 * - Different backends (Metal vs CPU may use different math)
 * - Different thread counts (parallel reductions may sum in different orders)
 * - Different hardware with different SIMD instructions
 * 
 * For reproducible evals and golden tests: pin build, backend, threads, and seed.
 * Use seed=0 for true randomness (seeded from system entropy).
 */
typedef enum {
    ETHERVOX_CHAT_ROLE_ASSISTANT = 1,  /**< Prior model output (e.g. a <tool_call> block) */
    ETHERVOX_CHAT_ROLE_TOOL = 2,       /**< A tool's result, rendered the way the model was trained */
    ETHERVOX_CHAT_ROLE_USER = 3        /**< A user message; only meaningful in `history` */
} ethervox_chat_role_t;

/** One turn after the initial user prompt. */
typedef struct {
    ethervox_chat_role_t role;
    const char* content;
} ethervox_chat_turn_t;

typedef struct {
    uint32_t max_tokens;        /**< Maximum tokens to generate (default: 512) */
    float temperature;          /**< Sampling temperature (default: 0.7) */
    float top_p;                /**< Nucleus sampling parameter (default: 0.9) */
    uint32_t seed;              /**< Random seed (0 = random, non-zero = deterministic) */
    bool include_logprobs;      /**< Fire ETHERVOX_EVENT_LOGPROB events (default: false) */
    /**
     * Optional standing instructions, rendered in the chat template's own system role
     * (`chat_template_format_system`) ahead of `prompt`'s user turn, instead of folded into
     * `prompt` itself (`BACKLOG-80`). NULL (default) emits no system turn at all.
     */
    const char* system_prompt;
    /**
     * Turns following `prompt`'s user turn, in order (assistant tool calls, tool results). NULL/0
     * (default) = a single user turn. Rendered by the chat template, never concatenated into
     * `prompt` -- the model must see real assistant and tool-result turns (`BACKLOG-80`).
     */
    const ethervox_chat_turn_t* turns;
    uint32_t turn_count;
    /**
     * Earlier conversation (USER and ASSISTANT turns), rendered as real turns between the system
     * prompt and `prompt`'s user turn. NULL/0 (default) = none. Because it precedes `prompt`, a
     * follow-up's prompt starts with the previous call's prompt, which lets the KV cache reuse
     * that shared prefix instead of re-evaluating it.
     */
    const ethervox_chat_turn_t* history;
    uint32_t history_count;
    /**
     * How much the model reasons before answering -- an `ethervox_thinking_mode_t`. Only models
     * with thinking modes (Granite 4.2+, `chat_template_supports_thinking`) act on it; others
     * ignore it. 0 (default) = the model's own default (full thinking).
     */
    int32_t thinking_mode;
} ethervox_structured_gen_params_t;

/**
 * Thinking modes of Granite 4.2+ (https://www.ibm.com/granite/docs/models/granite4-2).
 */
typedef enum {
    ETHERVOX_THINKING_DEFAULT = 0,  /**< The model's default: full thinking */
    ETHERVOX_THINKING_OFF = 1,      /**< No reasoning: a direct answer (`enable_thinking=False`) */
    ETHERVOX_THINKING_LOW = 2,      /**< Brief reasoning for simpler queries (`low_effort=True`) */
    ETHERVOX_THINKING_FULL = 3      /**< Full chain-of-thought reasoning in <think>...</think> */
} ethervox_thinking_mode_t;

/**
 * As ethervox_structured_render_prompt_with_history, for the model at `model_path` (its chat
 * template is detected from the file name; NULL = Granite 4.0/4.1) and a thinking mode. Granite 4.2+
 * is rendered exactly as its chat_template.jinja does: ChatML roles, `<think></think>` on earlier
 * assistant turns, tool calls as `<function=NAME><parameter=K>` blocks (the JSON `<tool_call>`
 * envelope the caller supplies is converted), and the generation prompt opening `<think>`.
 */
ethervox_result_t ethervox_structured_render_prompt_for_model(
    const char* system_prompt,
    const ethervox_chat_turn_t* history,
    uint32_t history_count,
    const char* prompt,
    const ethervox_chat_turn_t* turns,
    uint32_t turn_count,
    const char* model_path,
    int32_t thinking_mode,
    char** out
);

/**
 * Renders system + user + `turns` + the assistant-start marker exactly as
 * `ethervox_generate_structured` feeds them to the tokenizer. Exposed so the byte string is
 * testable without a model. Caller frees `*out` with free().
 */
ethervox_result_t ethervox_structured_render_prompt(
    const char* system_prompt,
    const char* prompt,
    const ethervox_chat_turn_t* turns,
    uint32_t turn_count,
    char** out
);

/** As above, with earlier conversation turns rendered between the system prompt and `prompt`. */
ethervox_result_t ethervox_structured_render_prompt_with_history(
    const char* system_prompt,
    const ethervox_chat_turn_t* history,
    uint32_t history_count,
    const char* prompt,
    const ethervox_chat_turn_t* turns,
    uint32_t turn_count,
    char** out
);

/**
 * Generate structured output with calibrated confidence
 *
 * Performs grammar-constrained generation and calculates a confidence score
 * based on the geometric mean of content token probabilities, excluding
 * grammar-forced structural tokens (e.g., JSON braces, commas where required).
 *
 * Confidence calculation:
 * - For each generated token, get its log-probability
 * - Determine if token was grammar-forced (only 1 valid candidate)
 * - Exclude forced tokens from confidence calculation
 * - confidence = exp(mean(log_probs of non-forced tokens))
 * - Range: [0.0, 1.0], where 0.0 = no confidence, 1.0 = certain
 *
 * Edge cases:
 * - All tokens grammar-forced → confidence = 0.0
 * - Empty output → confidence = 0.0
 * - Very low confidence (<0.01) → clamped to 0.0
 *
 * @param handle Model handle from model pool
 * @param prompt Input prompt text
 * @param grammar Grammar constraint (from ethervox_grammar_compile or _from_json_schema)
 * @param params Generation parameters (NULL = defaults)
 * @param event_callback Optional event callback for streaming (NULL = no events)
 * @param user_data User data passed to event callback
 * @param out_json Output JSON string (caller must free with free())
 * @param out_confidence Output confidence score [0.0, 1.0] (nullable if not needed)
 * @return ETHERVOX_SUCCESS or error code
 *
 * @note Thread-safety: Serialized via model handle's inference mutex
 * @note Grammar must remain valid for the duration of the call
 * @note Fires ETHERVOX_EVENT_LOGPROB if params->include_logprobs == true
 * @note Fires ETHERVOX_EVENT_TOKEN, ETHERVOX_EVENT_FINISHED, ETHERVOX_EVENT_ERROR as appropriate
 *
 * @example
 * ethervox_grammar_t* grammar = NULL;
 * ethervox_grammar_from_json_schema("{\"type\": \"object\", ...}", &grammar);
 * 
 * char* json_output = NULL;
 * float confidence = 0.0f;
 * 
 * ethervox_structured_gen_params_t params = {
 *     .max_tokens = 256,
 *     .temperature = 0.7f,
 *     .include_logprobs = true
 * };
 * 
 * ethervox_result_t result = ethervox_generate_structured(
 *     model_handle,
 *     "Extract person name from: John Smith, age 30",
 *     grammar,
 *     &params,
 *     my_event_callback,
 *     user_data,
 *     &json_output,
 *     &confidence
 * );
 * 
 * if (result == ETHERVOX_SUCCESS) {
 *     printf("JSON: %s (confidence: %.2f)\n", json_output, confidence);
 *     free(json_output);
 * }
 * 
 * ethervox_grammar_free(grammar);
 */
ethervox_result_t ethervox_generate_structured(
    ethervox_model_handle_t* handle,
    const char* prompt,
    ethervox_grammar_t* grammar,
    const ethervox_structured_gen_params_t* params,
    ethervox_event_cb event_callback,
    void* user_data,
    char** out_json,
    float* out_confidence
);

/**
 * Set the directory where the system-prompt prefix state is saved between loads.
 *
 * Once set, the KV state of the system prompt is written there after its first prefill and
 * restored on later calls whose prompt begins with the same tokens, skipping the re-prefill.
 * The file name is keyed by model path, context size and the prefix tokens, so a changed prompt
 * or model never restores stale state. NULL or empty disables the disk cache.
 *
 * @param dir Existing or creatable directory, copied internally
 */
void ethervox_structured_set_prefix_cache_dir(const char* dir);

/**
 * Get default generation parameters
 *
 * @return Default parameters struct
 */
ethervox_structured_gen_params_t ethervox_structured_gen_params_default(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_STRUCTURED_GENERATION_H
