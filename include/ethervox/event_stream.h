/**
 * @file event_stream.h
 * @brief Structured event stream for LLM operations
 *
 * Provides a unified event callback interface that can express token generation,
 * tool calls, usage metrics, load stages, log-probs, and errors. Token fragments
 * are guaranteed to never split UTF-8 multi-byte sequences.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_EVENT_STREAM_H
#define ETHERVOX_EVENT_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Event types in the stream
 *
 * Events are delivered in order on the inference thread. String pointers
 * are valid only for the callback's duration.
 */
typedef enum {
    /** Model loading stage update (e.g., "loading_model", "processing_prompt") */
    ETHERVOX_EVENT_LOAD_STAGE,
    
    /** Token generated - UTF-8 safe, never splits multi-byte sequences */
    ETHERVOX_EVENT_TOKEN,
    
    /** Tool call requested by the model */
    ETHERVOX_EVENT_TOOL_CALL_REQUESTED,
    
    /** Tool call result (success or error) */
    ETHERVOX_EVENT_TOOL_CALL_RESULT,
    
    /** Usage statistics (tokens, timing) */
    ETHERVOX_EVENT_USAGE,
    
    /** Log probability for generated token */
    ETHERVOX_EVENT_LOGPROB,
    
    /** Generation finished (includes finish reason) */
    ETHERVOX_EVENT_FINISHED,
    
    /** Error during operation */
    ETHERVOX_EVENT_ERROR,

    /** Streaming transcription segment (TASK-C3.5) - partial or final ASR result during capture */
    ETHERVOX_EVENT_TRANSCRIPTION_SEGMENT,

    /**
     * A piece of the model's reasoning ("thinking", Granite 4.2+): text between <think> and
     * </think>. Delivered through the `token` payload (text, token_id); reasoning text is never
     * delivered as ETHERVOX_EVENT_TOKEN and never part of the generated output.
     */
    ETHERVOX_EVENT_REASONING
} ethervox_event_type_t;

/**
 * Load stage event data
 */
typedef struct {
    const char* stage;      /**< Stage name (e.g., "loading_model", "processing_prompt") */
    float progress;         /**< Progress value 0.0-1.0 */
    const char* message;    /**< Human-readable message */
} ethervox_event_load_stage_t;

/**
 * Token event data
 *
 * The text is guaranteed to be valid UTF-8 and never splits a multi-byte
 * sequence, even if the underlying model produces incomplete bytes.
 */
typedef struct {
    const char* text;       /**< UTF-8 token text (valid for callback duration only) */
    int32_t token_id;       /**< Model token ID */
} ethervox_event_token_t;

/**
 * Tool call requested event data
 */
typedef struct {
    const char* tool_name;  /**< Name of the tool being called */
    const char* arguments;  /**< JSON-encoded tool arguments */
    uint32_t call_id;       /**< Unique ID for this tool call */
} ethervox_event_tool_call_requested_t;

/**
 * Tool call result event data
 */
typedef struct {
    uint32_t call_id;       /**< ID of the tool call this result is for */
    bool success;           /**< true if tool executed successfully */
    const char* result;     /**< Tool result (JSON or text) */
    const char* error;      /**< Error message if success=false, NULL otherwise */
} ethervox_event_tool_call_result_t;

/**
 * Usage statistics event data
 */
typedef struct {
    uint32_t prompt_tokens;     /**< Tokens in the prompt */
    uint32_t completion_tokens; /**< Tokens generated */
    uint32_t total_tokens;      /**< prompt_tokens + completion_tokens */
    double elapsed_ms;          /**< Time elapsed in milliseconds */
} ethervox_event_usage_t;

/**
 * Log probability event data
 */
typedef struct {
    int32_t token_id;       /**< Token ID */
    const char* token_text; /**< Token text */
    float logprob;          /**< Log probability */
    float prob;             /**< Probability (exp(logprob)) */
} ethervox_event_logprob_t;

/**
 * Finished event data
 */
typedef struct {
    const char* finish_reason;  /**< Reason generation stopped (e.g., "stop", "length", "tool_calls") */
} ethervox_event_finished_t;

/**
 * Error event data
 */
typedef struct {
    ethervox_result_t error_code;   /**< Error code */
    const char* message;             /**< Error message */
} ethervox_event_error_t;

/**
 * Streaming transcription segment event data (TASK-C3.5)
 *
 * Emitted as a host-facing transcript segment becomes available during
 * capture, in addition to (not instead of) the existing batch STT result
 * returned when the recording stops. `segment_id` is stable across
 * revisions of the same segment: a host that keys its displayed lines by
 * `segment_id` can update a line in place instead of duplicating it.
 *
 * Once a segment has been emitted with `is_final = true`, it is never
 * re-emitted - the id is retired. A non-final segment may be re-emitted
 * later under the same `segment_id` with corrected `text`/`speaker_id`.
 */
typedef struct {
    uint32_t segment_id;     /**< Stable id; unchanged across a segment's revisions */
    int32_t speaker_id;      /**< Speaker id for this segment, or -1 if unknown/undiarized */
    const char* text;        /**< UTF-8 segment text (valid for callback duration only) */
    bool is_final;           /**< true if this segment will never be revised again */
} ethervox_event_transcription_segment_t;

/**
 * Unified event structure
 *
 * Check the type field, then access the corresponding union member.
 */
typedef struct {
    ethervox_event_type_t type;
    
    union {
        ethervox_event_load_stage_t load_stage;
        ethervox_event_token_t token;
        ethervox_event_tool_call_requested_t tool_call_requested;
        ethervox_event_tool_call_result_t tool_call_result;
        ethervox_event_usage_t usage;
        ethervox_event_logprob_t logprob;
        ethervox_event_finished_t finished;
        ethervox_event_error_t error;
        ethervox_event_transcription_segment_t transcription_segment;
    };
} ethervox_event_t;

/**
 * Event callback function
 *
 * Called in order on the inference thread. String pointers in the event
 * are valid only for the callback's duration - copy if you need to retain.
 *
 * @param event The event
 * @param user_data User data pointer passed to the operation
 * @return true to continue, false to cancel
 */
typedef bool (*ethervox_event_cb)(const ethervox_event_t* event, void* user_data);

// ============================================================================
// UTF-8 Validation
// ============================================================================

/**
 * Validate and fix UTF-8 string at incomplete multi-byte sequences
 *
 * Checks that the string is valid UTF-8. If it ends with an incomplete
 * multi-byte sequence (common when streaming tokens), truncates to the
 * last complete character.
 *
 * @param str String to validate (may be NULL)
 * @param out_valid_len Output: length of valid UTF-8 prefix
 * @return true if completely valid, false if truncated or invalid
 */
bool ethervox_validate_utf8(const char* str, size_t* out_valid_len);

/**
 * Create a UTF-8 safe copy of a string
 *
 * Allocates a new string containing only the valid UTF-8 prefix.
 * Caller must free the result.
 *
 * @param str Source string (may be NULL)
 * @param out_len Output: length of the safe string (excluding null terminator)
 * @return Allocated UTF-8 safe string, or NULL on allocation failure
 */
char* ethervox_create_safe_utf8(const char* str, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_EVENT_STREAM_H
