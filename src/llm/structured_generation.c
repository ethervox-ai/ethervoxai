// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary

/**
 * @file structured_generation.c
 * @brief Implementation of grammar-constrained generation with confidence scoring
 * 
 * Task: C3.3 - Log-probs and calibrated confidence for structured generation
 */

#include "ethervox/structured_generation.h"
#include "ethervox/chat_template.h"
#include "ethervox/error.h"
#include "ethervox/grammar.h"
#include "ethervox/logging.h"
#include "llama.h"
#include "cJSON.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>

// Maximum response length (256 KB)
#define MAX_RESPONSE_LENGTH (256 * 1024)

// Confidence clamp threshold
#define MIN_CONFIDENCE 0.01f

/**
 * Context for tracking generation state
 */
typedef struct {
    // Input
    struct llama_model* model;
    struct llama_context* ctx;
    ethervox_grammar_t* grammar;
    const ethervox_structured_gen_params_t* params;
    ethervox_event_cb event_callback;
    void* user_data;
    
    // Output accumulation
    char* output_buffer;
    size_t output_len;
    size_t output_capacity;
    
    // Confidence tracking
    double log_prob_sum;
    int content_token_count;    // Tokens that were NOT grammar-forced
    int total_token_count;

    // Usage tracking (ETHERVOX_EVENT_USAGE) -- `n_prompt_tokens` set once by `prefill_prompt`;
    // `total_token_count` above already tracks completion tokens, reused as-is.
    int n_prompt_tokens;
    struct timespec start_time;

    // Granite 4.2+ reasoning (see `ethervox_thinking_mode_t`)
    const char* model_path;          // The loaded model file (chat template detection)
    bool thinking_model;             // The model's template has thinking modes
    bool in_reasoning;               // Currently between <think> and </think>
    int reasoning_token_count;       // Reasoning tokens produced so far
    int reasoning_token_cap;         // After this many, </think> is forced
    llama_token think_open_token;    // Single-token <think>, or LLAMA_TOKEN_NULL
    llama_token think_close_token;   // Single-token </think>, or LLAMA_TOKEN_NULL

    // Sampling state
    struct llama_sampler* sampler;
    const struct llama_vocab* vocab;
} generation_context_t;

/**
 * Initialize generation context
 */
static ethervox_result_t init_generation_context(
    generation_context_t* gen_ctx,
    ethervox_model_handle_t* handle,
    ethervox_grammar_t* grammar,
    const ethervox_structured_gen_params_t* params,
    ethervox_event_cb event_callback,
    void* user_data
) {
    memset(gen_ctx, 0, sizeof(*gen_ctx));
    
    gen_ctx->model = ethervox_model_handle_get_model(handle);
    gen_ctx->ctx = ethervox_model_handle_get_context(handle);
    gen_ctx->grammar = grammar;
    gen_ctx->params = params;
    gen_ctx->event_callback = event_callback;
    gen_ctx->user_data = user_data;
    
    if (!gen_ctx->model || !gen_ctx->ctx) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Allocate output buffer
    gen_ctx->output_capacity = 16384;  // Start with 16 KB
    gen_ctx->output_buffer = malloc(gen_ctx->output_capacity);
    if (!gen_ctx->output_buffer) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    gen_ctx->output_buffer[0] = '\0';
    gen_ctx->output_len = 0;
    
    gen_ctx->vocab = llama_model_get_vocab(gen_ctx->model);
    clock_gettime(CLOCK_MONOTONIC, &gen_ctx->start_time);

    // Granite 4.2+: the prompt opens <think> (unless thinking is off), so the output starts as
    // reasoning and ends it with </think>; both are single tokens.
    gen_ctx->model_path = ethervox_model_handle_get_path(handle);
    const char* model_path = gen_ctx->model_path;
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_AUTO, model_path);
    gen_ctx->think_open_token = LLAMA_TOKEN_NULL;
    gen_ctx->think_close_token = LLAMA_TOKEN_NULL;
    if (chat_template_supports_thinking(tmpl)) {
        llama_token id = LLAMA_TOKEN_NULL;
        if (llama_tokenize(gen_ctx->vocab, "<think>", 7, &id, 1, false, true) == 1) {
            gen_ctx->think_open_token = id;
        }
        if (llama_tokenize(gen_ctx->vocab, "</think>", 8, &id, 1, false, true) == 1) {
            gen_ctx->think_close_token = id;
        }
        gen_ctx->thinking_model = gen_ctx->think_close_token != LLAMA_TOKEN_NULL;
        if (gen_ctx->thinking_model && params->thinking_mode != ETHERVOX_THINKING_OFF) {
            gen_ctx->in_reasoning = true;
            // Reasoning does not count against max_tokens (the answer's budget), but it is
            // bounded: past the cap </think> is forced so the model still answers.
            gen_ctx->reasoning_token_cap =
                params->thinking_mode == ETHERVOX_THINKING_LOW ? 1024 : 3072;
        }
    }

    return ETHERVOX_SUCCESS;
}

/**
 * Append token text to output buffer, growing if needed
 */
static ethervox_result_t append_token_text(
    generation_context_t* gen_ctx,
    const char* text,
    size_t len
) {
    // Check if we need to grow the buffer
    if (gen_ctx->output_len + len + 1 > gen_ctx->output_capacity) {
        size_t new_capacity = gen_ctx->output_capacity * 2;
        if (new_capacity > MAX_RESPONSE_LENGTH) {
            return ETHERVOX_ERROR_OUT_OF_MEMORY;
        }
        
        char* new_buffer = realloc(gen_ctx->output_buffer, new_capacity);
        if (!new_buffer) {
            return ETHERVOX_ERROR_OUT_OF_MEMORY;
        }
        
        gen_ctx->output_buffer = new_buffer;
        gen_ctx->output_capacity = new_capacity;
    }
    
    memcpy(gen_ctx->output_buffer + gen_ctx->output_len, text, len);
    gen_ctx->output_len += len;
    gen_ctx->output_buffer[gen_ctx->output_len] = '\0';
    
    return ETHERVOX_SUCCESS;
}

/**
 * Fire an event if callback is set. Returns the callback's bool result (true = keep going,
 * false = caller wants generation stopped now), or true when there is no callback.
 */
static bool fire_event(
    generation_context_t* gen_ctx,
    const ethervox_event_t* event
) {
    if (gen_ctx->event_callback) {
        return gen_ctx->event_callback(event, gen_ctx->user_data);
    }
    return true;
}

/**
 * Fires a real `ETHERVOX_EVENT_USAGE` -- always immediately before whichever `FINISHED` event
 * ends this call, so the stream still always ends with `FINISHED` (`ev-llm`'s own contract,
 * `generation.rs`'s doc comment: "the final element is always Finished"). Previously this event
 * type was declared in `event_stream.h` but constructed nowhere in the codebase.
 */
static void fire_usage_event(generation_context_t* gen_ctx) {
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_ms = (double)(end_time.tv_sec - gen_ctx->start_time.tv_sec) * 1000.0
        + (double)(end_time.tv_nsec - gen_ctx->start_time.tv_nsec) / 1e6;
    ethervox_event_t usage_event = {
        .type = ETHERVOX_EVENT_USAGE,
        .usage = {
            .prompt_tokens = (uint32_t)gen_ctx->n_prompt_tokens,
            .completion_tokens = (uint32_t)gen_ctx->total_token_count,
            .total_tokens = (uint32_t)(gen_ctx->n_prompt_tokens + gen_ctx->total_token_count),
            .elapsed_ms = elapsed_ms
        }
    };
    fire_event(gen_ctx, &usage_event);
}

/**
 * Detect if a token is purely structural JSON (not content)
 * 
 * Returns true if the token is a structural character like braces, commas, colons, quotes
 * that are mandated by JSON format. Content tokens (property values, numbers) return false.
 */
static bool is_token_structural(
    generation_context_t* gen_ctx,
    llama_token token,
    const char* token_text,
    int token_len
) {
    // If token text is empty or very short, check common structural tokens
    if (token_len == 0) {
        return true;  // Empty token is structural
    }
    
    // Single-character structural tokens
    if (token_len == 1) {
        char c = token_text[0];
        if (c == '{' || c == '}' || c == '[' || c == ']' || 
            c == ',' || c == ':' || c == '"' || c == ' ' || 
            c == '\n' || c == '\t') {
            return true;
        }
    }
    
    // Multi-character whitespace/formatting
    bool all_whitespace = true;
    for (int i = 0; i < token_len; i++) {
        char c = token_text[i];
        if (c != ' ' && c != '\n' && c != '\t' && c != '\r') {
            all_whitespace = false;
            break;
        }
    }
    
    return all_whitespace;
}

/**
 * Create sampler chain with grammar constraint
 */
static struct llama_sampler* create_sampler(
    generation_context_t* gen_ctx
) {
    struct llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    sparams.no_perf = false;
    
    struct llama_sampler* sampler = llama_sampler_chain_init(sparams);
    if (!sampler) {
        return NULL;
    }
    
    // Add penalties (frequency, presence, repeat)
    if (!gen_ctx->thinking_model) {
        llama_sampler_chain_add(sampler,
            llama_sampler_init_penalties(128, 1.1f, 0.0f, 0.0f));
    }
    
    // Add grammar constraint BEFORE temperature. At temperature <= 0 (greedy), the temperature
    // sampler collapses every candidate's logit to -inf except the single argmax token
    // (llama_sampler_temp_impl's own "find the token with the highest logit and set the rest to
    // -inf"). If grammar ran after that, and the argmax token happened not to be grammar-valid,
    // the grammar sampler would be left filtering a candidate set that is already all -inf --
    // producing a degenerate distribution the rest of the chain (top_k/top_p/dist) cannot sample
    // from, which crashes deep in llama.cpp's sampling code (confirmed: reproduces with the most
    // trivial possible grammar at temperature 0.0, independent of lazy mode or grammar
    // complexity -- BACKLOG-59). Applying grammar first means temperature's argmax search only
    // ever considers logits llama.cpp's grammar sampler has already left non -inf.
    if (gen_ctx->grammar) {
        // Get GBNF source from grammar
        const char* gbnf_source = ethervox_grammar_get_source(gen_ctx->grammar);
        const char* root_rule = ethervox_grammar_get_root(gen_ctx->grammar);
        
        if (gbnf_source && root_rule) {
            struct llama_sampler* grammar_sampler = NULL;
            size_t trigger_word_count = 0;
            const char* const* trigger_words =
                ethervox_grammar_get_trigger_words(gen_ctx->grammar, &trigger_word_count);
            
            if (ethervox_grammar_is_lazy(gen_ctx->grammar) && trigger_words && trigger_word_count > 0) {
                // Lazy mode: free text until a trigger word appears, then the grammar engages
                // (mirrors llama_backend.c's existing, working pattern -- this function
                // previously ignored lazy mode entirely and always applied the grammar
                // immediately, regardless of what the caller set via
                // ethervox_grammar_set_lazy_mode).
                grammar_sampler = llama_sampler_init_grammar_lazy_patterns(
                    gen_ctx->vocab,
                    gbnf_source,
                    root_rule,
                    trigger_words,
                    trigger_word_count,
                    NULL, 0
                );
            } else {
                grammar_sampler = llama_sampler_init_grammar(
                    gen_ctx->vocab,
                    gbnf_source,
                    root_rule
                );
            }
            
            if (grammar_sampler) {
                llama_sampler_chain_add(sampler, grammar_sampler);
            } else {
                ETHERVOX_LOG_ERROR("Failed to initialize grammar sampler");
            }
        }
    }
    
    // Granite 4.2: IBM recommends temperature 1.0 and top_p 0.95 for every task (chat, reasoning
    // and tool calling), and the 1.1 repetition penalty above would cut into a reasoning trace.
    float temperature = gen_ctx->params->temperature;
    float top_p = gen_ctx->params->top_p;
    if (gen_ctx->thinking_model) {
        temperature = 1.0f;
        top_p = 0.95f;
    }
    // Add temperature
    llama_sampler_chain_add(sampler, 
        llama_sampler_init_temp(temperature));
    
    // Add top-k and top-p
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(top_p, 1));
    
    // Add final distribution sampler with seed
    // If seed=0, use time-based random seed; otherwise use specified seed
    uint32_t actual_seed = gen_ctx->params->seed;
    if (actual_seed == 0) {
        actual_seed = (uint32_t)time(NULL);
    }
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(actual_seed));
    
    return sampler;
}

ethervox_result_t ethervox_structured_render_prompt(
    const char* system_prompt,
    const char* prompt,
    const ethervox_chat_turn_t* turns,
    uint32_t turn_count,
    char** out
) {
    return ethervox_structured_render_prompt_with_history(
        system_prompt, NULL, 0, prompt, turns, turn_count, out);
}

ethervox_result_t ethervox_structured_render_prompt_with_history(
    const char* system_prompt,
    const ethervox_chat_turn_t* history,
    uint32_t history_count,
    const char* prompt,
    const ethervox_chat_turn_t* turns,
    uint32_t turn_count,
    char** out
) {
    return ethervox_structured_render_prompt_for_model(
        system_prompt, history, history_count, prompt, turns, turn_count, NULL,
        ETHERVOX_THINKING_DEFAULT, out);
}

// ---------------------------------------------------------------------------------------------
// Granite 4.2+ rendering (chat_template.jinja of ibm-granite/granite-4.2-*)
// ---------------------------------------------------------------------------------------------

typedef struct {
    char* data;
    size_t len;
    size_t cap;
    bool failed;
} text_builder_t;

static void tb_append(text_builder_t* tb, const char* text, size_t n) {
    if (tb->failed) return;
    if (tb->len + n + 1 > tb->cap) {
        size_t cap = tb->cap ? tb->cap : 1024;
        while (cap < tb->len + n + 1) cap *= 2;
        char* grown = (char*)realloc(tb->data, cap);
        if (!grown) { tb->failed = true; return; }
        tb->data = grown;
        tb->cap = cap;
    }
    memcpy(tb->data + tb->len, text, n);
    tb->len += n;
    tb->data[tb->len] = '\0';
}

static void tb_str(text_builder_t* tb, const char* text) { tb_append(tb, text, strlen(text)); }

/** `text` without leading/trailing whitespace, as a [start, start+len) view. */
static const char* trim_view(const char* text, size_t* len) {
    const char* start = text;
    while (*start && isspace((unsigned char)*start)) start++;
    size_t n = strlen(start);
    while (n > 0 && isspace((unsigned char)start[n - 1])) n--;
    *len = n;
    return start;
}

/** What follows the last </think>, or the whole text when it has none. */
static const char* after_last_think(const char* text) {
    const char* last = NULL;
    for (const char* p = strstr(text, "</think>"); p; p = strstr(p + 1, "</think>")) last = p;
    return last ? last + strlen("</think>") : text;
}

/**
 * Appends one tool call the way the 4.2 template prints it:
 *   <tool_call>\n<function=NAME>\n<parameter=K>\nV\n</parameter>\n</function>\n</tool_call>\n
 * `json` is the `{"name": ..., "arguments": {...}}` envelope ev-agent produces.
 */
static bool append_tool_call_xml(text_builder_t* tb, const char* json) {
    cJSON* root = cJSON_Parse(json);
    if (!root) return false;
    const cJSON* name = cJSON_GetObjectItemCaseSensitive(root, "name");
    const cJSON* args = cJSON_GetObjectItemCaseSensitive(root, "arguments");
    if (!cJSON_IsString(name) || !name->valuestring) { cJSON_Delete(root); return false; }
    tb_str(tb, "<tool_call>\n<function=");
    tb_str(tb, name->valuestring);
    tb_str(tb, ">\n");
    if (cJSON_IsObject(args)) {
        const cJSON* arg = NULL;
        cJSON_ArrayForEach(arg, args) {
            tb_str(tb, "<parameter=");
            tb_str(tb, arg->string ? arg->string : "");
            tb_str(tb, ">\n");
            if (cJSON_IsString(arg)) {
                tb_str(tb, arg->valuestring ? arg->valuestring : "");
            } else if (cJSON_IsBool(arg)) {
                tb_str(tb, cJSON_IsTrue(arg) ? "True" : "False");  // as the jinja template prints it
            } else {
                char* printed = cJSON_PrintUnformatted(arg);
                if (printed) { tb_str(tb, printed); free(printed); }
            }
            tb_str(tb, "\n</parameter>\n");
        }
    }
    tb_str(tb, "</function>\n</tool_call>\n");
    cJSON_Delete(root);
    return true;
}

/**
 * An in-run assistant turn. Ordinary text becomes `<think></think>text`; a turn that holds a JSON
 * `<tool_call>` envelope has it converted to the XML form, as the template renders tool calls.
 */
static void append_assistant_turn_42(text_builder_t* tb, const chat_template_t* tmpl, const char* content) {
    tb_str(tb, tmpl->assistant_start);
    const char* call = strstr(content, "<tool_call>");
    const char* visible_end = call ? call : content + strlen(content);
    char* head = (char*)malloc((size_t)(visible_end - content) + 1);
    if (!head) { tb->failed = true; return; }
    memcpy(head, content, (size_t)(visible_end - content));
    head[visible_end - content] = '\0';

    // Reasoning is kept only when the turn carries it; otherwise an empty block, as the template does.
    const char* close_tag = strstr(head, "</think>");
    size_t text_len;
    const char* text = trim_view(after_last_think(head), &text_len);
    if (close_tag) {
        const char* open_tag = strstr(head, "<think>");
        tb_str(tb, "<think>");
        const char* reasoning_start = open_tag ? open_tag + strlen("<think>") : head;
        tb_append(tb, reasoning_start, (size_t)(close_tag - reasoning_start));
        tb_str(tb, "</think>");
    } else {
        tb_str(tb, "<think></think>");
    }
    tb_append(tb, text, text_len);

    if (call) {
        tb_str(tb, "\n");
        const char* body = call + strlen("<tool_call>");
        const char* end = strstr(body, "</tool_call>");
        size_t body_len = end ? (size_t)(end - body) : strlen(body);
        char* json = (char*)malloc(body_len + 1);
        if (json) {
            memcpy(json, body, body_len);
            json[body_len] = '\0';
            size_t jl;
            const char* jt = trim_view(json, &jl);
            char* trimmed = (char*)malloc(jl + 1);
            if (trimmed) {
                memcpy(trimmed, jt, jl);
                trimmed[jl] = '\0';
                if (!append_tool_call_xml(tb, trimmed)) {
                    // Not our envelope: keep whatever the caller wrote.
                    tb_str(tb, "<tool_call>\n");
                    tb_append(tb, jt, jl);
                    tb_str(tb, "\n</tool_call>\n");
                }
                free(trimmed);
            }
            free(json);
        }
    } else if (text_len == 0) {
        // nothing after the (empty) reasoning
    }
    free(head);
    tb_str(tb, tmpl->assistant_end);
}

static ethervox_result_t render_prompt_granite42(
    const chat_template_t* tmpl,
    const char* system_prompt,
    const ethervox_chat_turn_t* history,
    uint32_t history_count,
    const char* prompt,
    const ethervox_chat_turn_t* turns,
    uint32_t turn_count,
    int32_t thinking_mode,
    char** out
) {
    text_builder_t tb = {0};
    if (system_prompt && system_prompt[0] != '\0') {
        tb_str(&tb, tmpl->system_start);
        tb_str(&tb, system_prompt);
        tb_str(&tb, tmpl->system_end);
    }
    for (uint32_t i = 0; i < history_count; i++) {
        const char* content = history[i].content ? history[i].content : "";
        if (history[i].role == ETHERVOX_CHAT_ROLE_USER) {
            tb_str(&tb, tmpl->user_start);
            tb_str(&tb, content);
            tb_str(&tb, tmpl->user_end);
        } else if (history[i].role == ETHERVOX_CHAT_ROLE_ASSISTANT) {
            // Earlier answers keep no reasoning (the template's truncate_history_thinking).
            size_t n;
            const char* text = trim_view(after_last_think(content), &n);
            tb_str(&tb, tmpl->assistant_start);
            tb_str(&tb, "<think></think>");
            tb_append(&tb, text, n);
            tb_str(&tb, tmpl->assistant_end);
        } else {
            free(tb.data);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    tb_str(&tb, tmpl->user_start);
    tb_str(&tb, prompt);
    if (thinking_mode == ETHERVOX_THINKING_LOW) {
        tb_str(&tb, "\n\n{reasoning effort: low}");
    }
    tb_str(&tb, tmpl->user_end);
    for (uint32_t i = 0; i < turn_count; i++) {
        const char* content = turns[i].content ? turns[i].content : "";
        if (turns[i].role == ETHERVOX_CHAT_ROLE_ASSISTANT) {
            append_assistant_turn_42(&tb, tmpl, content);
        } else if (turns[i].role == ETHERVOX_CHAT_ROLE_TOOL) {
            tb_str(&tb, tmpl->tool_response_start);
            tb_str(&tb, content);
            tb_str(&tb, tmpl->tool_response_end);
        } else {
            free(tb.data);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    tb_str(&tb, tmpl->assistant_start);
    tb_str(&tb, thinking_mode == ETHERVOX_THINKING_OFF ? "<think></think>" : "<think>\n");
    if (tb.failed || !tb.data) {
        free(tb.data);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    *out = tb.data;
    return ETHERVOX_SUCCESS;
}

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
) {
    if (!prompt || !out || (turn_count > 0 && !turns) || (history_count > 0 && !history)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    // Granite 4.0/4.1 unless the model's file name says 4.2+, which has its own format and modes.
    const chat_template_t* tmpl = model_path
        ? chat_template_get(CHAT_TEMPLATE_AUTO, model_path)
        : chat_template_get(CHAT_TEMPLATE_GRANITE, NULL);
    if (chat_template_supports_thinking(tmpl)) {
        return render_prompt_granite42(
            tmpl, system_prompt, history, history_count, prompt, turns, turn_count, thinking_mode, out);
    }
    if (model_path && tmpl->type != CHAT_TEMPLATE_GRANITE_4 && tmpl->type != CHAT_TEMPLATE_GRANITE_3) {
        // A non-Granite file name: this product only ships Granite, so keep the 4.x format.
        tmpl = chat_template_get(CHAT_TEMPLATE_GRANITE, NULL);
    }
    // Heap-allocated: prompt length is caller-controlled. Each turn adds a bounded marker overhead.
    size_t capacity = 512 + strlen(prompt) + (system_prompt ? strlen(system_prompt) : 0);
    for (uint32_t i = 0; i < turn_count; i++) {
        capacity += 512 + (turns[i].content ? strlen(turns[i].content) : 0);
    }
    for (uint32_t i = 0; i < history_count; i++) {
        capacity += 512 + (history[i].content ? strlen(history[i].content) : 0);
    }
    char* buf = malloc(capacity);
    if (!buf) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    buf[0] = '\0';
    size_t offset = 0;
    ethervox_result_t result = ETHERVOX_SUCCESS;

    if (system_prompt && system_prompt[0] != '\0') {
        result = chat_template_format_system(tmpl, system_prompt, buf + offset, capacity - offset);
        if (result != ETHERVOX_SUCCESS) goto fail;
        offset = strlen(buf);
    }

    for (uint32_t i = 0; i < history_count; i++) {
        const char* content = history[i].content ? history[i].content : "";
        if (history[i].role == ETHERVOX_CHAT_ROLE_USER) {
            result = chat_template_format_user(tmpl, content, buf + offset, capacity - offset);
            if (result != ETHERVOX_SUCCESS) goto fail;
        } else if (history[i].role == ETHERVOX_CHAT_ROLE_ASSISTANT) {
            int n = snprintf(buf + offset, capacity - offset, "%s%s%s",
                             tmpl->assistant_start, content, tmpl->assistant_end);
            if (n < 0 || (size_t)n >= capacity - offset) { result = ETHERVOX_ERROR_BUFFER_TOO_SMALL; goto fail; }
        } else {
            result = ETHERVOX_ERROR_INVALID_ARGUMENT;
            goto fail;
        }
        offset = strlen(buf);
    }

    result = chat_template_format_user(tmpl, prompt, buf + offset, capacity - offset);
    if (result != ETHERVOX_SUCCESS) goto fail;
    offset = strlen(buf);

    for (uint32_t i = 0; i < turn_count; i++) {
        const char* content = turns[i].content ? turns[i].content : "";
        if (turns[i].role == ETHERVOX_CHAT_ROLE_ASSISTANT) {
            int n = snprintf(buf + offset, capacity - offset, "%s%s%s",
                             tmpl->assistant_start, content, tmpl->assistant_end);
            if (n < 0 || (size_t)n >= capacity - offset) { result = ETHERVOX_ERROR_BUFFER_TOO_SMALL; goto fail; }
        } else if (turns[i].role == ETHERVOX_CHAT_ROLE_TOOL) {
            result = chat_template_format_tool_response(tmpl, content, buf + offset, capacity - offset);
            if (result != ETHERVOX_SUCCESS) goto fail;
        } else {
            result = ETHERVOX_ERROR_INVALID_ARGUMENT;
            goto fail;
        }
        offset = strlen(buf);
    }

    result = chat_template_format_assistant_start(tmpl, buf + offset, capacity - offset);
    if (result != ETHERVOX_SUCCESS) goto fail;

    *out = buf;
    return ETHERVOX_SUCCESS;

fail:
    free(buf);
    return result;
}

/**
 * The prompt tokens last evaluated into one context, so the next call can reuse the part of its
 * prompt that matches. A single entry: one assistant conversation drives the governor model at a
 * time, and a second context simply replaces it (a miss, never a wrong hit -- the KV state itself
 * is checked before anything is reused).
 */
static struct {
    pthread_mutex_t lock;
    const struct llama_context* ctx;
    llama_token* tokens;
    int count;
} g_prefix = { .lock = PTHREAD_MUTEX_INITIALIZER };

static void prefix_cache_invalidate(void) {
    pthread_mutex_lock(&g_prefix.lock);
    free(g_prefix.tokens);
    g_prefix.tokens = NULL;
    g_prefix.count = 0;
    g_prefix.ctx = NULL;
    pthread_mutex_unlock(&g_prefix.lock);
}

static void prefix_cache_store(const struct llama_context* ctx, const llama_token* tokens, int count) {
    llama_token* copy = malloc((size_t)count * sizeof(llama_token));
    pthread_mutex_lock(&g_prefix.lock);
    free(g_prefix.tokens);
    if (copy) {
        memcpy(copy, tokens, (size_t)count * sizeof(llama_token));
    }
    g_prefix.tokens = copy;
    g_prefix.count = copy ? count : 0;
    g_prefix.ctx = copy ? ctx : NULL;
    pthread_mutex_unlock(&g_prefix.lock);
}

/**
 * How many leading tokens of `tokens` are already in `ctx`'s KV cache from the previous call.
 * At least one token is always left to evaluate, so the logits for sampling exist. 0 when reuse
 * is off (ETHERVOX_DISABLE_PREFIX_CACHE), the context differs, or its KV state no longer holds
 * what was cached (freed and reallocated context, another path decoded into it).
 */
static int prefix_cache_common_length(
    const struct llama_context* ctx, llama_memory_t mem, const llama_token* tokens, int n_tokens
) {
    if (getenv("ETHERVOX_DISABLE_PREFIX_CACHE")) {
        return 0;
    }
    int common = 0;
    pthread_mutex_lock(&g_prefix.lock);
    if (g_prefix.ctx == ctx && g_prefix.tokens && g_prefix.count > 0 &&
        llama_memory_seq_pos_max(mem, 0) >= (llama_pos)g_prefix.count - 1) {
        int limit = g_prefix.count < n_tokens - 1 ? g_prefix.count : n_tokens - 1;
        while (common < limit && g_prefix.tokens[common] == tokens[common]) {
            common++;
        }
    }
    pthread_mutex_unlock(&g_prefix.lock);
    return common;
}

/* Directory for the disk-backed system prefix; guarded by g_prefix.lock. */
static char g_prefix_cache_dir[512];

void ethervox_structured_set_prefix_cache_dir(const char* dir) {
    pthread_mutex_lock(&g_prefix.lock);
    g_prefix_cache_dir[0] = '\0';
    if (dir && dir[0]) {
        snprintf(g_prefix_cache_dir, sizeof(g_prefix_cache_dir), "%s", dir);
    }
    pthread_mutex_unlock(&g_prefix.lock);
    if (dir && dir[0]) {
        mkdir(dir, 0755);  // EEXIST is fine; a failed save is logged where it happens
    }
}

static uint64_t fnv1a(uint64_t hash, const void* data, size_t len) {
    const unsigned char* bytes = data;
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

/**
 * Path of the saved state for this model, context size and exact prefix tokens. Any change to
 * the prompt, model or context size changes the key, so stale state is never restored.
 */
static bool prefix_file_path(
    const generation_context_t* gen_ctx, const llama_token* tokens, int n, char* out, size_t cap
) {
    char dir[512] = "";
    pthread_mutex_lock(&g_prefix.lock);
    snprintf(dir, sizeof(dir), "%s", g_prefix_cache_dir);
    pthread_mutex_unlock(&g_prefix.lock);
    if (dir[0] == '\0') {
        return false;
    }
    const char* model = gen_ctx->model_path ? gen_ctx->model_path : "";
    uint32_t n_ctx = llama_n_ctx(gen_ctx->ctx);
    uint64_t hash = fnv1a(14695981039346656037ULL, model, strlen(model));
    hash = fnv1a(hash, &n_ctx, sizeof(n_ctx));
    hash = fnv1a(hash, tokens, (size_t)n * sizeof(llama_token));
    snprintf(out, cap, "%s/assistant-prefix-%016llx.state", dir, (unsigned long long)hash);
    return true;
}

/**
 * Length of the leading system-prompt tokens: the prompt rendered with no history and no user
 * text, compared token-by-token with the full prompt. 0 when the system prompt is empty.
 */
static int system_prefix_length(
    const generation_context_t* gen_ctx,
    const ethervox_structured_gen_params_t* params,
    const llama_token* tokens,
    int n_tokens
) {
    char* bare = NULL;
    if (ethervox_structured_render_prompt_for_model(
            params->system_prompt, NULL, 0, "", NULL, 0, gen_ctx->model_path,
            params->thinking_mode, &bare) != ETHERVOX_SUCCESS) {
        return 0;
    }
    int cap = (int)strlen(bare) + 256;
    llama_token* bare_tokens = malloc((size_t)cap * sizeof(llama_token));
    int n_bare = 0;
    if (bare_tokens) {
        n_bare = llama_tokenize(llama_model_get_vocab(gen_ctx->model), bare, (int)strlen(bare),
                                bare_tokens, cap, true, true);
    }
    int common = 0;
    if (n_bare > 0) {
        int limit = n_bare < n_tokens - 1 ? n_bare : n_tokens - 1;
        while (common < limit && bare_tokens[common] == tokens[common]) {
            common++;
        }
    }
    free(bare_tokens);
    free(bare);
    return common;
}

/** Evaluate tokens[from..to) in batches. Returns 0 on success. */
static int decode_range(struct llama_context* ctx, llama_token* tokens, int from, int to) {
    const int batch_size = 512;
    for (int i = from; i < to; i += batch_size) {
        int batch_count = (i + batch_size > to) ? (to - i) : batch_size;
        struct llama_batch batch = llama_batch_get_one(tokens + i, batch_count);
        if (llama_decode(ctx, batch) != 0) {
            return -1;
        }
    }
    return 0;
}

/**
 * Tokenize and evaluate prompt
 *
 * `prompt` arrives here as the caller's raw text (e.g. a user's chat message) -- with no chat
 * template applied, an instruct-tuned model like Granite sees ungrammatical input with no role
 * markers or turn-end signal and immediately predicts EOG (`0 content tokens`, confirmed against
 * a real model). `governor.c` (the older, Android-facing code path) already solved this via
 * `chat_template.c`; this path never carried that fix over when it was added. Every model this
 * product ships is Granite (`MODELS-LICENSING.md`'s allow-list), so the template is fixed rather
 * than filename-sniffed via `chat_template_detect()` (Android's own use of that detector is a
 * best-effort fallback for user-supplied models, not applicable here).
 *
 * `system_prompt`, if non-NULL/non-empty, is rendered in the template's own system role ahead
 * of `prompt`'s user turn (`BACKLOG-80`): standing instructions folded into the user turn
 * instead make the model answer them conversationally rather than follow them -- the exact
 * "Understood, I will handle questions..." failure `BACKLOG-73` chased through prompt wording.
 */
static ethervox_result_t prefill_prompt(
    generation_context_t* gen_ctx,
    const ethervox_structured_gen_params_t* params,
    const char* prompt
) {
    // Every call here is a fresh, complete prompt (the caller always resends the whole
    // cumulative text, never an incremental continuation of a previous call's KV state), so the
    // context must not just keep appending at its ever-advancing position -- confirmed live: a
    // handful of separate calls against one model exhausted a 4096-token context and
    // `llama_decode` aborted with "failed to find a memory slot". Instead of always clearing,
    // the tokens shared with the previous call's prompt are kept (see `prefix_cache` below) and
    // only the differing tail is evaluated; anything else is discarded, same as a clear.
    llama_memory_t mem = llama_get_memory(gen_ctx->ctx);

    char* formatted = NULL;
    ethervox_result_t render_result = ethervox_structured_render_prompt_for_model(
        params->system_prompt, params->history, params->history_count, prompt, params->turns,
        params->turn_count, gen_ctx->model_path, params->thinking_mode, &formatted);
    if (render_result != ETHERVOX_SUCCESS) {
        return render_result;
    }
    // Opt-in: the exact bytes handed to the tokenizer, for diagnosing prompt-shape problems
    // (`BACKLOG-80` -- the diagnosis was made from reading code, this is how to check it live).
    if (getenv("ETHERVOX_LOG_PROMPT")) {
        fprintf(stderr, "[structured_generation] prompt bytes (%zu):\n%s\n<<<end prompt>>>\n",
                strlen(formatted), formatted);
    }

    // Tokenize prompt
    int n_tokens_max = (int)strlen(formatted) + 256;  // Over-estimate
    llama_token* tokens = malloc(n_tokens_max * sizeof(llama_token));
    if (!tokens) {
        free(formatted);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    const struct llama_vocab* vocab = llama_model_get_vocab(gen_ctx->model);
    int n_tokens = llama_tokenize(
        vocab,
        formatted,
        (int)strlen(formatted),
        tokens,
        n_tokens_max,
        true,   // add_special (add BOS)
        true    // parse_special -- the role markers above are literal text and must be
                // recognised as the model's actual special tokens, not tokenized as sub-word text
    );
    
    if (n_tokens < 0) {
        free(tokens);
        free(formatted);
        return ETHERVOX_ERROR_FAILED;
    }
    
    // Keep the longest prefix the context already holds for exactly these tokens.
    int reuse = prefix_cache_common_length(gen_ctx->ctx, mem, tokens, n_tokens);
    // Recurrent/hybrid models cannot drop the tail of a sequence: the trim reports failure and
    // the whole context is cleared instead (`data=false`: only the bookkeeping needs resetting).
    if (reuse > 0 && !llama_memory_seq_rm(mem, 0, reuse, -1)) {
        reuse = 0;
    }
    if (reuse == 0) {
        llama_memory_clear(mem, false);
    }

    // The system prefix can also come from disk: restore it when this context holds nothing,
    // otherwise decode it on its own, save it, then continue with the rest of the prompt.
    char prefix_file[640] = "";
    int prefix_len = 0;
    bool save_prefix = false;
    if (reuse == 0) {
        prefix_len = system_prefix_length(gen_ctx, params, tokens, n_tokens);
        if (prefix_len > 0 && prefix_file_path(gen_ctx, tokens, prefix_len, prefix_file, sizeof(prefix_file))) {
            llama_token* restored = malloc((size_t)prefix_len * sizeof(llama_token));
            size_t n_loaded = 0;
            bool hit = restored &&
                llama_state_seq_load_file(gen_ctx->ctx, prefix_file, 0, restored, (size_t)prefix_len, &n_loaded) > 0 &&
                n_loaded == (size_t)prefix_len &&
                memcmp(restored, tokens, (size_t)prefix_len * sizeof(llama_token)) == 0;
            free(restored);
            if (hit) {
                reuse = prefix_len;
                ETHERVOX_LOG_INFO("Restored %d-token system prefix from disk", prefix_len);
            } else {
                llama_memory_clear(mem, false);
                save_prefix = true;
            }
        }
    }
    ETHERVOX_LOG_INFO("Prompt %d tokens, %d reused from the previous call", n_tokens, reuse);

    if (save_prefix) {
        if (decode_range(gen_ctx->ctx, tokens, 0, prefix_len) != 0) {
            prefix_cache_invalidate();
            free(tokens);
            free(formatted);
            return ETHERVOX_ERROR_FAILED;
        }
        if (llama_state_seq_save_file(gen_ctx->ctx, prefix_file, 0, tokens, (size_t)prefix_len) == 0) {
            ETHERVOX_LOG_WARN("Could not save system prefix state to %s", prefix_file);
        } else {
            ETHERVOX_LOG_INFO("Saved %d-token system prefix to disk", prefix_len);
        }
        reuse = prefix_len;
    }

    // Evaluate the rest of the prompt in batches
    if (decode_range(gen_ctx->ctx, tokens, reuse, n_tokens) != 0) {
        prefix_cache_invalidate();
        free(tokens);
        free(formatted);
        return ETHERVOX_ERROR_FAILED;
    }
    prefix_cache_store(gen_ctx->ctx, tokens, n_tokens);
    
    free(tokens);
    free(formatted);
    gen_ctx->n_prompt_tokens = n_tokens;
    return ETHERVOX_SUCCESS;
}

/**
 * Main generation loop
 */
static ethervox_result_t generate_tokens(generation_context_t* gen_ctx) {
    gen_ctx->sampler = create_sampler(gen_ctx);
    if (!gen_ctx->sampler) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    uint32_t max_tokens = gen_ctx->params->max_tokens;
    bool finished_on_eog = false;
    
    // `i` counts answer tokens only; reasoning tokens are bounded separately (see above).
    uint32_t i = 0;
    while (i < max_tokens) {
        // Sample next token. Reasoning is bounded: past the cap the model is made to close its
        // <think> block so it still answers.
        llama_token new_token = llama_sampler_sample(gen_ctx->sampler, gen_ctx->ctx, -1);
        if (gen_ctx->in_reasoning && gen_ctx->reasoning_token_count >= gen_ctx->reasoning_token_cap) {
            new_token = gen_ctx->think_close_token;
        }
        
        // Check for EOS
        if (llama_vocab_is_eog(gen_ctx->vocab, new_token)) {
            // Fire finished event
            fire_usage_event(gen_ctx);
            ethervox_event_t event = {
                .type = ETHERVOX_EVENT_FINISHED,
                .finished = { .finish_reason = "stop" }
            };
            fire_event(gen_ctx, &event);
            finished_on_eog = true;
            break;
        }
        
        // Get token text
        char piece[256];
        int n_piece = llama_token_to_piece(gen_ctx->vocab, new_token, piece, sizeof(piece) - 1, 0, false);
        
        // Granite 4.2 reasoning: <think> / </think> are markers, and the text between them is
        // delivered as REASONING events, never as part of the answer.
        if (gen_ctx->thinking_model
            && (new_token == gen_ctx->think_open_token || new_token == gen_ctx->think_close_token)) {
            gen_ctx->in_reasoning = (new_token == gen_ctx->think_open_token);
            n_piece = 0;
        } else if (gen_ctx->in_reasoning) {
            if (n_piece > 0) {
                piece[n_piece] = '\0';
                ethervox_event_t reasoning_event = {
                    .type = ETHERVOX_EVENT_REASONING,
                    .token = { .text = piece, .token_id = new_token }
                };
                gen_ctx->reasoning_token_count++;
                gen_ctx->total_token_count++;
                if (!fire_event(gen_ctx, &reasoning_event)) {
                    fire_usage_event(gen_ctx);
                    ethervox_event_t cancelled_event = {
                        .type = ETHERVOX_EVENT_FINISHED,
                        .finished = { .finish_reason = "cancelled" }
                    };
                    fire_event(gen_ctx, &cancelled_event);
                    llama_sampler_free(gen_ctx->sampler);
                    return ETHERVOX_SUCCESS;
                }
            } else {
                gen_ctx->reasoning_token_count++;
            }
            n_piece = 0;
            struct llama_batch reasoning_batch = llama_batch_get_one(&new_token, 1);
            if (llama_decode(gen_ctx->ctx, reasoning_batch) != 0) {
                llama_sampler_free(gen_ctx->sampler);
                return ETHERVOX_ERROR_FAILED;
            }
            continue;
        }

        if (n_piece > 0) {
            // llama_token_to_piece() does not NUL-terminate; callers across the FFI boundary
            // (ev-llm's token_text()) read this as a C string via CStr::from_ptr, so leaving it
            // unterminated reads past the real text into stale stack bytes from prior iterations.
            piece[n_piece] = '\0';

            // Append to output
            ethervox_result_t append_result = append_token_text(gen_ctx, piece, n_piece);
            if (append_result != ETHERVOX_SUCCESS) {
                llama_sampler_free(gen_ctx->sampler);
                return append_result;
            }
            
            // Fire token event. The callback returning false (e.g. the caller cancelled) must
            // stop generation immediately -- previously this return value was discarded, so a
            // cancelled generation ran all the way to max_tokens regardless, pegging the CPU for
            // as long as that took and freezing the whole app/UI.
            ethervox_event_t token_event = {
                .type = ETHERVOX_EVENT_TOKEN,
                .token = {
                    .text = piece,
                    .token_id = new_token
                }
            };
            if (!fire_event(gen_ctx, &token_event)) {
                fire_usage_event(gen_ctx);
                ethervox_event_t cancelled_event = {
                    .type = ETHERVOX_EVENT_FINISHED,
                    .finished = { .finish_reason = "cancelled" }
                };
                fire_event(gen_ctx, &cancelled_event);
                llama_sampler_free(gen_ctx->sampler);
                return ETHERVOX_SUCCESS;
            }
        }
        
        // Extract log-probability (AFTER sampling, BEFORE decode)
        // Get logits for current position
        float* logits = llama_get_logits_ith(gen_ctx->ctx, -1);
        if (logits) {
            // Get sampled token's logit
            float sampled_logit = logits[new_token];
            
            // Calculate log probability (softmax)
            int n_vocab = llama_vocab_n_tokens(gen_ctx->vocab);
            float max_logit = sampled_logit;
            for (int j = 0; j < n_vocab; j++) {
                if (logits[j] > max_logit) {
                    max_logit = logits[j];
                }
            }
            
            // Log-sum-exp for normalization
            double sum_exp = 0.0;
            for (int j = 0; j < n_vocab; j++) {
                sum_exp += exp(logits[j] - max_logit);
            }
            float log_prob = (sampled_logit - max_logit) - logf(sum_exp);
            float prob = expf(log_prob);
            
            // Detect if token is structural (not content)
            bool is_structural = is_token_structural(gen_ctx, new_token, piece, n_piece);
            
            // Track for confidence calculation (exclude structural tokens)
            gen_ctx->total_token_count++;
            if (!is_structural) {
                gen_ctx->log_prob_sum += log_prob;
                gen_ctx->content_token_count++;
            }
            
            // Fire logprob event if requested
            if (gen_ctx->params->include_logprobs) {
                ethervox_event_t logprob_event = {
                    .type = ETHERVOX_EVENT_LOGPROB,
                    .logprob = {
                        .token_id = new_token,
                        .token_text = n_piece > 0 ? piece : "",
                        .logprob = log_prob,
                        .prob = prob
                    }
                };
                fire_event(gen_ctx, &logprob_event);
            }
        }
        
        // Decode token for next iteration
        struct llama_batch batch = llama_batch_get_one(&new_token, 1);
        int decode_result = llama_decode(gen_ctx->ctx, batch);
        if (decode_result != 0) {
            llama_sampler_free(gen_ctx->sampler);
            return ETHERVOX_ERROR_FAILED;
        }
        i++;
    }
    
    // Loop exhausted max_tokens without hitting EOS or cancellation -- no FINISHED event fires
    // here; the caller (`ev-llm`'s `run_generation`/`run_tool_aware_generation`) synthesizes one
    // when it sees the call succeeded without ever observing a Finished event. Usage still needs
    // firing here, before that synthesized Finished, for the same "always ends with Finished"
    // ordering. Not on the EOG path: it already fired Usage then Finished, and a second Usage
    // after Finished breaks the "stream ends with Finished" contract.
    if (!finished_on_eog) {
        fire_usage_event(gen_ctx);
    }
    llama_sampler_free(gen_ctx->sampler);
    return ETHERVOX_SUCCESS;
}

/**
 * Calculate final confidence score
 */
static float calculate_confidence(const generation_context_t* gen_ctx) {
    if (gen_ctx->content_token_count == 0) {
        // All tokens were grammar-forced, or no tokens generated
        return 0.0f;
    }
    
    // Geometric mean: exp(mean of log-probs)
    double mean_log_prob = gen_ctx->log_prob_sum / gen_ctx->content_token_count;
    float confidence = expf(mean_log_prob);
    
    // Clamp very low confidence to 0.0
    if (confidence < MIN_CONFIDENCE) {
        confidence = 0.0f;
    }
    
    // Clamp to [0.0, 1.0]
    if (confidence > 1.0f) confidence = 1.0f;
    if (confidence < 0.0f) confidence = 0.0f;
    
    return confidence;
}

// ============================================================================
// Public API
// ============================================================================

ethervox_structured_gen_params_t ethervox_structured_gen_params_default(void) {
    ethervox_structured_gen_params_t params = {
        .max_tokens = 512,
        .temperature = 0.7f,
        .top_p = 0.9f,
        .seed = 0,
        .include_logprobs = false,
        .system_prompt = NULL,
        .turns = NULL,
        .turn_count = 0,
        .history = NULL,
        .history_count = 0,
        .thinking_mode = ETHERVOX_THINKING_DEFAULT
    };
    return params;
}

ethervox_result_t ethervox_generate_structured(
    ethervox_model_handle_t* handle,
    const char* prompt,
    ethervox_grammar_t* grammar,
    const ethervox_structured_gen_params_t* params,
    ethervox_event_cb event_callback,
    void* user_data,
    char** out_json,
    float* out_confidence
) {
    if (!handle || !prompt || !out_json) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Use default params if not provided
    ethervox_structured_gen_params_t default_params = ethervox_structured_gen_params_default();
    if (!params) {
        params = &default_params;
    }
    
    // Initialize generation context
    generation_context_t gen_ctx;
    ethervox_result_t result = init_generation_context(
        &gen_ctx, handle, grammar, params, event_callback, user_data
    );
    
    if (result != ETHERVOX_SUCCESS) {
        return result;
    }
    
    // Prefill prompt
    result = prefill_prompt(&gen_ctx, params, prompt);
    if (result != ETHERVOX_SUCCESS) {
        free(gen_ctx.output_buffer);
        return result;
    }
    
    // Generate tokens
    result = generate_tokens(&gen_ctx);
    if (result != ETHERVOX_SUCCESS) {
        free(gen_ctx.output_buffer);
        return result;
    }
    
    // Calculate confidence
    float confidence = calculate_confidence(&gen_ctx);

    // Return results
    *out_json = gen_ctx.output_buffer;  // Caller takes ownership
    
    if (out_confidence) {
        *out_confidence = confidence;
    }
    
    ETHERVOX_LOG_INFO("Structured generation complete: %zu chars, %d content tokens, confidence %.3f",
                      gen_ctx.output_len, gen_ctx.content_token_count, confidence);
    
    return ETHERVOX_SUCCESS;
}
