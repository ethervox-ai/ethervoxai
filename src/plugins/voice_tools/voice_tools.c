/**
#include "ethervox/error.h"
 * @file voice_tools.c
 * @brief Voice tools implementation (Mode 2: Transcription) using Granite
 *        Speech Plus's built-in Speaker-Attributed ASR (SAA)
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/voice_tools.h"
#include "ethervox/tool_catalogue.h"
#include "conversation_tools_catalogue.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>  // For Sleep
#else
#include <unistd.h>
#endif

#include "ethervox/file_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include "ethervox/config.h"
#include "ethervox/model_downloader.h"
#include "ethervox/platform_utils.h"
#include "ethervox/platform_utils.h"

// Forward declaration - implemented in JNI layer (Android) or returns NULL (other platforms)
extern const char* ethervox_get_android_files_dir(void);

#define LOG_ERROR(...) \
  ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...) \
  ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...) \
  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...) \
  ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

// TASK-N7.7: temporary transcription-latency instrumentation - measurement only, no
// behaviour change, removed by N7.8. Off unless ETHERVOX_LATENCY_TRACE is defined
// (see CMakeLists.txt option of the same name) - never on in playRelease.
#ifdef ETHERVOX_LATENCY_TRACE
static double latency_trace_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}
#define LOG_LATENCY(...) \
  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
static inline double latency_trace_now_ms(void) { return 0.0; }
#define LOG_LATENCY(...) ((void)0)
#endif

// Memory tag for path configurations
#define PATH_CONFIG_TAG "user_path"
#define PATH_CONFIG_PREFIX "USER_PATH:"

// Fixed filenames for the Granite Speech Plus (model, mmproj) GGUF pair -
// matches src/dialogue/voice_conversation.c's BASE-variant convention and
// src/common/model_downloader.c's GRANITE_SPEECH_PLUS_MODELS definitions.
#define GRANITE_SPEECH_PLUS_MODEL_FILENAME "granite-speech-4.1-2b-plus.Q4_K_M.gguf"
#define GRANITE_SPEECH_PLUS_MMPROJ_FILENAME "mmproj-granite-speech-4.1-2b-plus-Q4_K_M.gguf"

/**
 * Attempt to auto-download the Granite Speech Plus (model, mmproj) pair via
 * the shared model_downloader subsystem (see src/common/model_downloader.c).
 * Mirrors the Piper/Governor auto-download precedent already used elsewhere
 * in the codebase - both files are marked is_default in GRANITE_SPEECH_PLUS_MODELS.
 */
static ethervox_result_t download_granite_speech_plus_model(void) {
#if defined(TARGET_OS_IPHONE) || defined(__ANDROID__)
  // Model downloading not supported on mobile - models should be bundled with app
  LOG_ERROR("Model downloading is not supported on mobile platforms");
  LOG_ERROR("Models must be bundled with the app or preloaded");
  return ETHERVOX_ERROR_NOT_SUPPORTED;
#else
  LOG_INFO("Downloading Granite Speech Plus model (~1.6 GB) + mmproj (~300 MB)...");
  LOG_INFO("This may take several minutes depending on connection speed...");

  ethervox_result_t result = ethervox_model_download(
      ETHERVOX_MODEL_TYPE_GRANITE_SPEECH_PLUS, GRANITE_SPEECH_PLUS_MODEL_FILENAME, NULL, NULL);
  if (ethervox_is_error(result)) {
    LOG_ERROR("Failed to download Granite Speech Plus model");
    return result;
  }

  result = ethervox_model_download(ETHERVOX_MODEL_TYPE_GRANITE_SPEECH_PLUS,
                                    GRANITE_SPEECH_PLUS_MMPROJ_FILENAME, NULL, NULL);
  if (ethervox_is_error(result)) {
    LOG_ERROR("Failed to download Granite Speech Plus mmproj");
    return result;
  }

  LOG_INFO("Granite Speech Plus download complete");
  return ETHERVOX_SUCCESS;
#endif
}

// Global session pointer for tool wrappers
static ethervox_voice_session_t* g_voice_session = NULL;

// Defined below (needs the transcription-event queue machinery); called
// from the audio-finalize worker thread (TASK-N7.8), declared here so that
// worker doesn't need to be defined after it.
static bool finalize_chunk_and_restart(ethervox_voice_session_t* session);

// ============================================================================
// TASK-C3.5 backpressure: bounded async transcription-event delivery queue
//
// finalize_chunk_and_restart() runs on the audio-finalize worker thread (see
// TASK-N7.8 below - it used to run on the capture thread itself, which N7.7
// found could stall real-time audio capture for up to ~60s per chunk). If it
// called the host's event callback directly and the host were slow (e.g.
// blocked on UI), that delay would propagate straight into this queue's own
// consumer, backing up finalizes. Pushing onto this queue instead is O(1); a
// dedicated dispatch thread invokes the callback.
//
// Capacity is deliberately generous - 16 slots at one push per
// GRANITE_SPEECH_CHUNK_SECONDS is >10 minutes of undelivered backlog -
// specifically so the only segments this queue ever pushes (all final,
// see finalize_chunk_and_restart) are never dropped in any realistic
// scenario. If a host callback is ever so pathologically slow that the
// queue does fill, push blocks rather than drops: "never drop a final
// segment" outranks "never stall" in that unsatisfiable edge case.
// ============================================================================
#define ETHERVOX_TRANSCRIPTION_QUEUE_CAPACITY 16

typedef struct {
    uint32_t segment_id;
    int32_t speaker_id;
    bool is_final;
    char* text;  // heap copy, owned by the queue entry until dispatched
} transcription_queue_entry_t;

typedef struct {
    transcription_queue_entry_t entries[ETHERVOX_TRANSCRIPTION_QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    pthread_t dispatch_thread;
    bool dispatch_thread_running;
    bool shutdown_requested;
    ethervox_voice_session_t* session;  // for reading the current callback/user_data
} transcription_event_queue_t;

static void* transcription_dispatch_thread_main(void* arg) {
    transcription_event_queue_t* queue = (transcription_event_queue_t*)arg;

    for (;;) {
        pthread_mutex_lock(&queue->lock);
        while (queue->count == 0 && !queue->shutdown_requested) {
            pthread_cond_wait(&queue->not_empty, &queue->lock);
        }
        if (queue->count == 0 && queue->shutdown_requested) {
            pthread_mutex_unlock(&queue->lock);
            break;
        }

        transcription_queue_entry_t entry = queue->entries[queue->head];
        queue->head = (queue->head + 1) % ETHERVOX_TRANSCRIPTION_QUEUE_CAPACITY;
        queue->count--;
        pthread_cond_signal(&queue->not_full);
        pthread_mutex_unlock(&queue->lock);

        ethervox_event_cb cb = queue->session->transcription_event_cb;
        void* user_data = queue->session->transcription_event_user_data;
        if (cb) {
            ethervox_event_t event = {0};
            event.type = ETHERVOX_EVENT_TRANSCRIPTION_SEGMENT;
            event.transcription_segment.segment_id = entry.segment_id;
            event.transcription_segment.speaker_id = entry.speaker_id;
            event.transcription_segment.text = entry.text;
            event.transcription_segment.is_final = entry.is_final;
            cb(&event, user_data);
        }
        free(entry.text);
    }

    return NULL;
}

// Lazily starts the dispatch thread on first use; a session that never
// registers a callback never allocates or spins up any of this.
static transcription_event_queue_t* get_or_create_event_queue(ethervox_voice_session_t* session) {
    if (session->event_queue) {
        return (transcription_event_queue_t*)session->event_queue;
    }

    transcription_event_queue_t* queue =
        (transcription_event_queue_t*)calloc(1, sizeof(transcription_event_queue_t));
    if (!queue) return NULL;

    queue->session = session;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);

    if (pthread_create(&queue->dispatch_thread, NULL, transcription_dispatch_thread_main, queue) == 0) {
        queue->dispatch_thread_running = true;
    } else {
        LOG_ERROR("Failed to start transcription event dispatch thread");
        pthread_mutex_destroy(&queue->lock);
        pthread_cond_destroy(&queue->not_empty);
        pthread_cond_destroy(&queue->not_full);
        free(queue);
        return NULL;
    }

    session->event_queue = queue;
    return queue;
}

// Copies `text` and pushes it for async delivery; blocks only if the queue
// is completely full (see capacity note above). Safe to call whether or not
// a callback is currently registered - the dispatch thread simply drops the
// entry if `session->transcription_event_cb` is NULL when it's popped.
static void push_transcription_event(ethervox_voice_session_t* session, uint32_t segment_id,
                                      int32_t speaker_id, bool is_final, const char* text) {
    transcription_event_queue_t* queue = get_or_create_event_queue(session);
    if (!queue) return;

    char* text_copy = strdup(text);
    if (!text_copy) return;

    pthread_mutex_lock(&queue->lock);
    while (queue->count == ETHERVOX_TRANSCRIPTION_QUEUE_CAPACITY) {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }
    int slot = queue->tail;
    queue->entries[slot].segment_id = segment_id;
    queue->entries[slot].speaker_id = speaker_id;
    queue->entries[slot].is_final = is_final;
    queue->entries[slot].text = text_copy;
    queue->tail = (queue->tail + 1) % ETHERVOX_TRANSCRIPTION_QUEUE_CAPACITY;
    queue->count++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}

// Joins the dispatch thread and frees the queue. Safe to call on a session
// whose queue was never created (event callback never registered).
static void destroy_event_queue(ethervox_voice_session_t* session) {
    if (!session->event_queue) return;
    transcription_event_queue_t* queue = (transcription_event_queue_t*)session->event_queue;

    pthread_mutex_lock(&queue->lock);
    queue->shutdown_requested = true;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);

    if (queue->dispatch_thread_running) {
        pthread_join(queue->dispatch_thread, NULL);
    }

    // Drop any entries still queued at shutdown (host stopped listening).
    for (int i = 0; i < queue->count; i++) {
        int idx = (queue->head + i) % ETHERVOX_TRANSCRIPTION_QUEUE_CAPACITY;
        free(queue->entries[idx].text);
    }

    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    free(queue);
    session->event_queue = NULL;
}

// ============================================================================
// TASK-N7.8: audio finalize queue. N7.7 measured finalize_chunk_and_restart()
// taking up to ~60s for a single 45s chunk (fixed per-call cost dominates -
// see docs/tasks/N7.7-measure-transcription-latency.md), longer than its own
// window, while running synchronously on audio_capture_thread(). That thread
// cannot read audio while blocked inside finalize(), so real-time audio
// arriving during the stall was lost. This queue decouples the two: capture
// only reads hardware audio and pushes it here; a dedicated worker thread
// owns all STT interaction (process/finalize/start). The queue is an
// unbounded linked list, not a fixed-capacity ring buffer like the
// transcription-event queue above - capture must never block on it (see
// this task's design constraint: grow, never drop, never stall capture).
// ============================================================================

typedef struct audio_queue_node {
  float* samples;      // heap copy, owned by the node until the worker consumes it
  uint32_t n_samples;
  struct audio_queue_node* next;
} audio_queue_node_t;

typedef struct {
  audio_queue_node_t* head;
  audio_queue_node_t* tail;
  size_t depth;         // node count, for diagnostics only
  pthread_mutex_t lock;
  pthread_cond_t not_empty;
  pthread_t worker_thread;
  bool worker_running;
  bool shutdown_requested;
  bool drain_only;      // set during shutdown: consume audio, do not finalize (see stop path)
  ethervox_voice_session_t* session;
} audio_finalize_queue_t;

static void* audio_finalize_thread_main(void* arg) {
  audio_finalize_queue_t* queue = (audio_finalize_queue_t*)arg;
  ethervox_voice_session_t* session = queue->session;

  for (;;) {
    pthread_mutex_lock(&queue->lock);
    while (queue->head == NULL && !queue->shutdown_requested) {
      pthread_cond_wait(&queue->not_empty, &queue->lock);
    }
    if (queue->head == NULL && queue->shutdown_requested) {
      pthread_mutex_unlock(&queue->lock);
      break;
    }

    audio_queue_node_t* node = queue->head;
    queue->head = node->next;
    if (!queue->head) queue->tail = NULL;
    queue->depth--;
    bool drain_only = queue->drain_only;
    pthread_mutex_unlock(&queue->lock);

    ethervox_audio_buffer_t audio_buf;
    audio_buf.data = node->samples;
    audio_buf.size = node->n_samples;
    audio_buf.channels = 1;
    audio_buf.timestamp_us = 0;

    ethervox_stt_result_t result;
    ethervox_stt_process(&session->stt_runtime, &audio_buf, &result);

    // During shutdown drain (see stop path below) we only fold queued audio
    // into the STT accumulator - the caller does the one true tail finalize
    // itself, synchronously, after this thread has joined, so the existing
    // stop-path formatting/instrumentation stays exactly as it was.
    if (!drain_only &&
        difftime(time(NULL), session->chunk_start_time) >= GRANITE_SPEECH_CHUNK_SECONDS) {
      finalize_chunk_and_restart(session);
    }

    free(node->samples);
    free(node);
  }

  return NULL;
}

static audio_finalize_queue_t* get_or_create_audio_finalize_queue(ethervox_voice_session_t* session) {
  if (session->audio_finalize_queue) {
    return (audio_finalize_queue_t*)session->audio_finalize_queue;
  }

  audio_finalize_queue_t* queue =
      (audio_finalize_queue_t*)calloc(1, sizeof(audio_finalize_queue_t));
  if (!queue) return NULL;

  queue->session = session;
  pthread_mutex_init(&queue->lock, NULL);
  pthread_cond_init(&queue->not_empty, NULL);

  if (pthread_create(&queue->worker_thread, NULL, audio_finalize_thread_main, queue) == 0) {
    queue->worker_running = true;
  } else {
    LOG_ERROR("Failed to start audio finalize worker thread");
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_empty);
    free(queue);
    return NULL;
  }

  session->audio_finalize_queue = queue;
  return queue;
}

// Copies `audio_buf`'s samples and hands them to the finalize worker thread.
// Never blocks the caller (capture thread) beyond the brief lock hold to
// append a node - the queue grows rather than applying backpressure, per
// this task's "never stall capture" constraint.
static void push_audio_for_finalize(ethervox_voice_session_t* session,
                                     const ethervox_audio_buffer_t* audio_buf) {
  audio_finalize_queue_t* queue = get_or_create_audio_finalize_queue(session);
  if (!queue) return;

  audio_queue_node_t* node = (audio_queue_node_t*)malloc(sizeof(audio_queue_node_t));
  if (!node) return;
  node->samples = (float*)malloc(audio_buf->size * sizeof(float));
  if (!node->samples) {
    free(node);
    return;
  }
  memcpy(node->samples, audio_buf->data, audio_buf->size * sizeof(float));
  node->n_samples = audio_buf->size;
  node->next = NULL;

  pthread_mutex_lock(&queue->lock);
  if (queue->tail) {
    queue->tail->next = node;
  } else {
    queue->head = node;
  }
  queue->tail = node;
  queue->depth++;
  pthread_cond_signal(&queue->not_empty);
  pthread_mutex_unlock(&queue->lock);
}

// Signals the worker to drain any queued audio into the STT accumulator
// (without finalizing) and joins it. Called from the stop path, after
// capture has already stopped, so no more audio will ever be pushed.
static void drain_and_join_audio_finalize_queue(ethervox_voice_session_t* session) {
  if (!session->audio_finalize_queue) return;
  audio_finalize_queue_t* queue = (audio_finalize_queue_t*)session->audio_finalize_queue;

  pthread_mutex_lock(&queue->lock);
  queue->drain_only = true;
  queue->shutdown_requested = true;
  pthread_cond_signal(&queue->not_empty);
  pthread_mutex_unlock(&queue->lock);

  if (queue->worker_running) {
    pthread_join(queue->worker_thread, NULL);
  }

  pthread_mutex_destroy(&queue->lock);
  pthread_cond_destroy(&queue->not_empty);
  free(queue);
  session->audio_finalize_queue = NULL;
}

/**
 * Extract the highest [Speaker N]: id referenced anywhere in `text` and
 * fold it into session->max_speaker_id. Native Granite Speech Plus SAA tags
 * are 1-indexed with a trailing colon (e.g. "[Speaker 1]: hello"), unlike
 * the old 0-indexed/no-colon heuristic format - no backward compat.
 */
static void track_max_speaker_id(ethervox_voice_session_t* session, const char* text) {
  const char* marker = strstr(text, "[Speaker ");
  while (marker) {
    int speaker_id = -1;
    if (sscanf(marker, "[Speaker %d]:", &speaker_id) == 1) {
      if (speaker_id > session->max_speaker_id) {
        session->max_speaker_id = speaker_id;
      }
    }
    marker = strstr(marker + 1, "[Speaker ");
  }
}

/**
 * Highest [Speaker N]: id referenced in `text` alone (TASK-C3.5), or -1 if
 * none - unlike track_max_speaker_id, this doesn't touch session state; it's
 * the per-segment speaker_id carried on a single transcription-segment event.
 */
static int32_t max_speaker_id_in_text(const char* text) {
  int32_t max_id = -1;
  const char* marker = strstr(text, "[Speaker ");
  while (marker) {
    int speaker_id = -1;
    if (sscanf(marker, "[Speaker %d]:", &speaker_id) == 1 && speaker_id > max_id) {
      max_id = speaker_id;
    }
    marker = strstr(marker + 1, "[Speaker ");
  }
  return max_id;
}

/**
 * Append `text` to the session's in-memory transcript buffer (growing it as
 * needed) and to the live transcript file for real-time LLM/UI monitoring.
 */
static void append_transcript_segment(ethervox_voice_session_t* session, const char* text) {
  size_t needed = session->transcript_len + strlen(text) + 2;
  if (needed > session->transcript_capacity) {
    session->transcript_capacity = needed * 2;
    session->full_transcript = (char*)realloc(session->full_transcript, session->transcript_capacity);
  }

  if (session->transcript_len > 0) {
    strcat(session->full_transcript, "\n");
    session->transcript_len++;
  }
  strcat(session->full_transcript, text);
  session->transcript_len += strlen(text);
  session->segment_count++;

  if (session->last_transcript_file[0] != '\0') {
    FILE* f = fopen(session->last_transcript_file, "a");
    if (f) {
      fprintf(f, "%s\n", text);
      fflush(f);
      fclose(f);
      LOG_DEBUG("Updated live transcript file: %s", session->last_transcript_file);
    } else {
      LOG_WARN("Failed to open transcript file for writing: %s", session->last_transcript_file);
    }
  }
}

/**
 * Finalize the current Granite Speech Plus chunk, fold the result into the
 * session transcript, refresh the prefix_text carry-forward buffer, and
 * start the next chunk (resets the KV cache - see stt.h/granite_speech_backend.c).
 * Returns true if a non-empty segment was produced.
 */
static bool finalize_chunk_and_restart(ethervox_voice_session_t* session) {
  ethervox_stt_result_t result;
  bool produced_segment = false;

  const double chunk_audio_seconds = difftime(time(NULL), session->chunk_start_time);
  const double finalize_start_ms = latency_trace_now_ms();
  ethervox_result_t finalize_rc = ethervox_stt_finalize(&session->stt_runtime, &result);
  LOG_LATENCY("N7.7 chunk_finalize: audio=%.1fs wall=%.1fms",
              chunk_audio_seconds, latency_trace_now_ms() - finalize_start_ms);

  if (ethervox_is_success(finalize_rc)) {
    if (result.text && strlen(result.text) > 0) {
      LOG_INFO("📥 Granite Speech Plus chunk finalized: %zu chars", strlen(result.text));

      track_max_speaker_id(session, result.text);

      // Format segment with timestamp and language info
      char formatted_segment[8192];
      time_t now = time(NULL);
      struct tm* tm_info = localtime(&now);
      char datetime_str[64];
      strftime(datetime_str, sizeof(datetime_str), "%Y-%m-%d %H:%M:%S", tm_info);
      snprintf(formatted_segment, sizeof(formatted_segment), "[%s] (%s) %s", datetime_str,
               result.language ? result.language : "auto", result.text);

      append_transcript_segment(session, formatted_segment);
      LOG_INFO("Segment %u: %s", session->segment_count, formatted_segment);

      // TASK-C3.5: streaming transcription event, additive to full_transcript
      // above. Granite Speech Plus's finalize() is one-shot per chunk (see
      // this function's own doc comment) - there is no intra-chunk partial
      // to revise, so every segment this call site emits is final at birth;
      // the is_final=false/revision path exists in the event struct for a
      // future backend with real incremental decoding, not exercised here.
      // Pushed onto the async queue (not called directly) so a slow host
      // callback can never stall this capture thread - see
      // push_transcription_event()'s doc comment.
      if (session->transcription_event_cb) {
        push_transcription_event(session, session->segment_count,
                                  max_speaker_id_in_text(result.text), true, result.text);
      }

      // Carry a bounded tail of the raw (untagged-timestamp) transcript
      // forward as prefix_text so the next chunk keeps consistent
      // [Speaker N]: numbering (see GRANITE_SPEECH_PREFIX_CARRY_MAX_CHARS).
      //
      // BACKLOG-31 fix: config.prefix_text must always be either NULL or a
      // pointer ethervox_stt_cleanup() may safely free() - it does so
      // unconditionally (stt_core.c). Pointing it directly at
      // session->prefix_carry (a fixed array embedded in the session, not
      // heap memory) was undefined behaviour: on cleanup, free() would be
      // called on non-heap memory, corrupting the heap. Discovered via a
      // live-model test (tests/test_streaming_transcription_live.c) that
      // reliably crashed on ethervox_voice_tools_cleanup() after >=1 chunk.
      size_t text_len = strlen(result.text);
      size_t carry_len = text_len < (GRANITE_SPEECH_PREFIX_CARRY_MAX_CHARS - 1)
                             ? text_len
                             : (GRANITE_SPEECH_PREFIX_CARRY_MAX_CHARS - 1);
      const char* carry_start = result.text + (text_len - carry_len);
      strncpy(session->prefix_carry, carry_start, carry_len);
      session->prefix_carry[carry_len] = '\0';
      free((void*)session->stt_runtime.config.prefix_text);
      session->stt_runtime.config.prefix_text = strdup(session->prefix_carry);

      produced_segment = true;
    }
    ethervox_stt_result_free(&result);
  } else {
    LOG_WARN("Granite Speech Plus: chunk finalize() failed");
  }

  // Start the next chunk (clears KV cache + accumulator, see stt_core.c)
  ethervox_stt_start(&session->stt_runtime);
  session->chunk_start_time = time(NULL);

  return produced_segment;
}

/**
 * Background thread that captures audio and hands it off for Granite Speech
 * Plus processing.
 *
 * TASK-N7.8: this thread does hardware audio reads only - it no longer calls
 * ethervox_stt_process()/finalize()/start() itself. N7.7 measured
 * finalize_chunk_and_restart() taking up to ~60s for a single 45s chunk,
 * which - when run here, on the same thread that reads audio - stalled
 * capture and lost real-time audio for the duration. All STT interaction now
 * happens on a separate worker thread (see audio_finalize_thread_main()
 * above); this thread's only job is to keep reading hardware audio and
 * pushing it to that worker's queue, which can never block on it.
 */
static void* audio_capture_thread(void* arg) {
  ethervox_voice_session_t* session = (ethervox_voice_session_t*)arg;

  ethervox_audio_buffer_t audio_buf;
  audio_buf.data = (float*)malloc(16000 * sizeof(float));  // 1 second buffer
  audio_buf.size = 16000;
  audio_buf.channels = 1;
  audio_buf.timestamp_us = 0;
  const uint32_t buffer_capacity = audio_buf.size;

  if (!audio_buf.data) {
    LOG_ERROR("Failed to allocate audio buffer");
    return NULL;
  }

  LOG_INFO("Audio capture thread started - handing off to async finalize worker (%ds chunks)",
           GRANITE_SPEECH_CHUNK_SECONDS);

  // TASK-N7.7: cumulative samples captured vs. wall-clock elapsed, to detect whether
  // the finalize worker falls behind its 16kHz real-time input (see task doc,
  // hypothesis B). Logged once per second of session wall time.
#ifdef ETHERVOX_LATENCY_TRACE
  uint64_t latency_trace_samples_captured = 0;
  time_t latency_trace_session_start = time(NULL);
  time_t latency_trace_last_log = latency_trace_session_start;
#endif

  while (session->is_recording && !session->stop_requested) {
    // Read audio from platform driver
    // CRITICAL: Reset buffer size before each read to prevent buffer overflow
    audio_buf.size = buffer_capacity;
    ethervox_result_t read_result = ethervox_audio_read(&session->audio_runtime, &audio_buf);

    if (ethervox_is_success(read_result) && audio_buf.size > 0) {
      int samples_read = (int)audio_buf.size;

      // Clamp samples_read to buffer capacity to prevent overflow
      if (samples_read > (int)buffer_capacity) {
        LOG_WARN("Audio driver returned more samples (%d) than buffer capacity (%u) - clamping",
                 samples_read, buffer_capacity);
        samples_read = (int)buffer_capacity;
      }

      audio_buf.size = (uint32_t)samples_read;
#ifdef ETHERVOX_LATENCY_TRACE
      latency_trace_samples_captured += (uint64_t)samples_read;

      time_t latency_trace_now = time(NULL);
      if (latency_trace_now > latency_trace_last_log) {
        double elapsed_s = difftime(latency_trace_now, latency_trace_session_start);
        double captured_s = (double)latency_trace_samples_captured / 16000.0;
        LOG_LATENCY("N7.7 capture_backlog: elapsed=%.0fs captured=%.1fs backlog=%.1fs",
                    elapsed_s, captured_s, captured_s - elapsed_s);
        latency_trace_last_log = latency_trace_now;
      }
#endif

      // TASK-N7.8: hand off to the finalize worker thread; never touches
      // stt_runtime or the chunk-boundary timer directly from here.
      push_audio_for_finalize(session, &audio_buf);
    } else if (ethervox_is_success(read_result) && audio_buf.size == 0) {
      // No audio available yet, sleep briefly
#ifdef _WIN32
      Sleep(50);  // 50ms
#else
      usleep(50000);  // 50ms
#endif
    } else {
      LOG_ERROR("Audio read error: result=%d", read_result);
      break;
    }
  }

  free(audio_buf.data);
  LOG_INFO("Audio capture thread stopped");
  return NULL;
}

/**
 * Initialize voice tools with the Granite Speech Plus backend
 */
ethervox_result_t ethervox_voice_tools_init(ethervox_voice_session_t* session, void* memory, ethervox_model_pool_t* pool) {
  if (!session) {
    LOG_ERROR("Session is NULL");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  memset(session, 0, sizeof(ethervox_voice_session_t));

  session->memory_store = memory;
  session->transcript_capacity = 1024 * 100;  // 100KB initial
  session->full_transcript = (char*)calloc(session->transcript_capacity, 1);

  if (!session->full_transcript) {
    LOG_ERROR("Failed to allocate transcript buffer");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Initialize path config to use standard directories
  ethervox_path_config_t path_config;
  if (ethervox_is_error(ethervox_path_config_init(&path_config, memory))) {
    LOG_WARN("Failed to initialize path config, using fallback paths");
  }

  // Build search paths for the Granite Speech Plus (model, mmproj) pair.
  // Search order: custom path (path_set tool) > verified user paths >
  // standard relative/app-data locations > Android app files dir.
  const int MAX_MODEL_PATHS = 32;
  char possible_model_paths[MAX_MODEL_PATHS][ETHERVOX_FILE_MAX_PATH];
  char possible_mmproj_paths[MAX_MODEL_PATHS][ETHERVOX_FILE_MAX_PATH];
  int path_count = 0;

  char custom_path[ETHERVOX_FILE_MAX_PATH];
  bool has_custom_path = ethervox_is_success(
      ethervox_path_config_get(&path_config, "GraniteSpeechModels", custom_path, sizeof(custom_path)));

  ethervox_user_path_t* user_paths = NULL;
  uint32_t user_path_count = 0;
  bool has_user_paths = ethervox_is_success(ethervox_path_config_list(&path_config, &user_paths, &user_path_count));

  const char* android_files_dir = ethervox_get_android_files_dir();
  char default_model_dir[ETHERVOX_FILE_MAX_PATH];
  char local_app_data[512];

  if (android_files_dir) {
    snprintf(default_model_dir, sizeof(default_model_dir), "%s/models/%s", android_files_dir,
             ETHERVOX_GRANITE_SPEECH_SUBDIR);
    LOG_INFO("[Android] Using app files directory for models: %s", default_model_dir);
  } else if (platform_get_local_app_data_dir(local_app_data, sizeof(local_app_data)) == ETHERVOX_SUCCESS) {
    snprintf(default_model_dir, sizeof(default_model_dir), "%s%cmodels%c%s", local_app_data,
#ifdef _WIN32
             '\\', '\\',
#else
             '/', '/',
#endif
             ETHERVOX_GRANITE_SPEECH_SUBDIR);
    LOG_DEBUG("Using platform app data for models: %s", default_model_dir);
  } else {
    snprintf(default_model_dir, sizeof(default_model_dir), "models%c%s",
#ifdef _WIN32
             '\\',
#else
             '/',
#endif
             ETHERVOX_GRANITE_SPEECH_SUBDIR);
    LOG_WARN("No platform-specific path available, using relative path");
  }

  // Highest priority: Android app files dir
  if (android_files_dir && path_count < MAX_MODEL_PATHS) {
    snprintf(possible_model_paths[path_count], ETHERVOX_FILE_MAX_PATH, "%s/models/%s/%s",
             android_files_dir, ETHERVOX_GRANITE_SPEECH_SUBDIR, GRANITE_SPEECH_PLUS_MODEL_FILENAME);
    snprintf(possible_mmproj_paths[path_count], ETHERVOX_FILE_MAX_PATH, "%s/models/%s/%s",
             android_files_dir, ETHERVOX_GRANITE_SPEECH_SUBDIR, GRANITE_SPEECH_PLUS_MMPROJ_FILENAME);
    path_count++;
  }

  // Custom path (path_set "GraniteSpeechModels" ...)
  if (has_custom_path && path_count < MAX_MODEL_PATHS) {
    snprintf(possible_model_paths[path_count], ETHERVOX_FILE_MAX_PATH, "%s/%s", custom_path,
             GRANITE_SPEECH_PLUS_MODEL_FILENAME);
    snprintf(possible_mmproj_paths[path_count], ETHERVOX_FILE_MAX_PATH, "%s/%s", custom_path,
             GRANITE_SPEECH_PLUS_MMPROJ_FILENAME);
    path_count++;
  }

  // Verified user paths
  if (has_user_paths && user_paths) {
    for (uint32_t i = 0; i < user_path_count && path_count < MAX_MODEL_PATHS; i++) {
      if (!user_paths[i].verified) continue;
      snprintf(possible_model_paths[path_count], ETHERVOX_FILE_MAX_PATH, "%s/.ethervox/models/%s/%s",
               user_paths[i].path, ETHERVOX_GRANITE_SPEECH_SUBDIR, GRANITE_SPEECH_PLUS_MODEL_FILENAME);
      snprintf(possible_mmproj_paths[path_count], ETHERVOX_FILE_MAX_PATH, "%s/.ethervox/models/%s/%s",
               user_paths[i].path, ETHERVOX_GRANITE_SPEECH_SUBDIR, GRANITE_SPEECH_PLUS_MMPROJ_FILENAME);
      path_count++;
    }
    free(user_paths);
  }

  // Standard relative/home locations
  const char* standard_dirs[] = {".ethervox/models/%s/%s", "./.ethervox/models/%s/%s",
                                 "../.ethervox/models/%s/%s", "models/%s/%s", "./models/%s/%s",
                                 "../models/%s/%s", "../../models/%s/%s"};
  for (size_t i = 0; i < sizeof(standard_dirs) / sizeof(standard_dirs[0]) && path_count < MAX_MODEL_PATHS; i++) {
    snprintf(possible_model_paths[path_count], ETHERVOX_FILE_MAX_PATH, standard_dirs[i],
             ETHERVOX_GRANITE_SPEECH_SUBDIR, GRANITE_SPEECH_PLUS_MODEL_FILENAME);
    snprintf(possible_mmproj_paths[path_count], ETHERVOX_FILE_MAX_PATH, standard_dirs[i],
             ETHERVOX_GRANITE_SPEECH_SUBDIR, GRANITE_SPEECH_PLUS_MMPROJ_FILENAME);
    path_count++;
  }

  // Platform app-data directory (e.g. ~/.ethervox equivalent on this platform)
  if (platform_get_local_app_data_dir(local_app_data, sizeof(local_app_data)) == ETHERVOX_SUCCESS &&
      path_count < MAX_MODEL_PATHS) {
    snprintf(possible_model_paths[path_count], ETHERVOX_FILE_MAX_PATH, "%s%cmodels%c%s%c%s",
             local_app_data,
#ifdef _WIN32
             '\\', '\\', '\\',
#else
             '/', '/', '/',
#endif
             ETHERVOX_GRANITE_SPEECH_SUBDIR, GRANITE_SPEECH_PLUS_MODEL_FILENAME);
    snprintf(possible_mmproj_paths[path_count], ETHERVOX_FILE_MAX_PATH, "%s%cmodels%c%s%c%s",
             local_app_data,
#ifdef _WIN32
             '\\', '\\', '\\',
#else
             '/', '/', '/',
#endif
             ETHERVOX_GRANITE_SPEECH_SUBDIR, GRANITE_SPEECH_PLUS_MMPROJ_FILENAME);
    path_count++;
  }

  // Also try the plain home-directory convention used elsewhere in the
  // codebase (voice_conversation.c, model_downloader.c): ~/.ethervox/models/granite-speech/
  const char* home = getenv("HOME");
  if (home && path_count < MAX_MODEL_PATHS) {
    snprintf(possible_model_paths[path_count], ETHERVOX_FILE_MAX_PATH, "%s/.ethervox/models/%s/%s",
             home, ETHERVOX_GRANITE_SPEECH_SUBDIR, GRANITE_SPEECH_PLUS_MODEL_FILENAME);
    snprintf(possible_mmproj_paths[path_count], ETHERVOX_FILE_MAX_PATH, "%s/.ethervox/models/%s/%s",
             home, ETHERVOX_GRANITE_SPEECH_SUBDIR, GRANITE_SPEECH_PLUS_MMPROJ_FILENAME);
    path_count++;
  }

  // Find the first candidate where BOTH the model and mmproj files exist
  const char* model_path = NULL;
  const char* mmproj_path = NULL;
  for (int i = 0; i < path_count; i++) {
    FILE* model_test = fopen(possible_model_paths[i], "rb");
    if (!model_test) continue;
    fclose(model_test);

    FILE* mmproj_test = fopen(possible_mmproj_paths[i], "rb");
    if (!mmproj_test) {
      LOG_WARN("Found Granite Speech Plus model at %s but missing companion mmproj at %s",
               possible_model_paths[i], possible_mmproj_paths[i]);
      continue;
    }
    fclose(mmproj_test);

    model_path = strdup(possible_model_paths[i]);
    mmproj_path = strdup(possible_mmproj_paths[i]);
    session->model_path = (char*)model_path;
    session->mmproj_path = (char*)mmproj_path;
    LOG_INFO("Found Granite Speech Plus model at: %s", model_path);
    break;
  }

  if (!model_path) {
    LOG_WARN("Granite Speech Plus model not found. Searched %d locations:", path_count);
    for (int i = 0; i < path_count && i < 5; i++) {
      LOG_WARN("  - %s", possible_model_paths[i]);
    }
    if (path_count > 5) {
      LOG_WARN("  ... and %d more locations", path_count - 5);
    }

    LOG_INFO("========================================");
    LOG_INFO("Granite Speech Plus Auto-Download Available");
    LOG_INFO("========================================");
    LOG_INFO("Would you like to download the Granite Speech Plus model (~1.9 GB total)?");
    LOG_INFO("This is a one-time download and will be saved to: %s", default_model_dir);

    if (ethervox_is_success(download_granite_speech_plus_model())) {
      char downloaded_model[ETHERVOX_FILE_MAX_PATH];
      char downloaded_mmproj[ETHERVOX_FILE_MAX_PATH];
      snprintf(downloaded_model, sizeof(downloaded_model), "%s/%s", default_model_dir,
               GRANITE_SPEECH_PLUS_MODEL_FILENAME);
      snprintf(downloaded_mmproj, sizeof(downloaded_mmproj), "%s/%s", default_model_dir,
               GRANITE_SPEECH_PLUS_MMPROJ_FILENAME);

      FILE* model_test = fopen(downloaded_model, "rb");
      FILE* mmproj_test = fopen(downloaded_mmproj, "rb");
      if (model_test && mmproj_test) {
        fclose(model_test);
        fclose(mmproj_test);
        session->model_path = strdup(downloaded_model);
        session->mmproj_path = strdup(downloaded_mmproj);
        LOG_INFO("Successfully downloaded and loaded model from: %s", downloaded_model);
      } else {
        if (model_test) fclose(model_test);
        if (mmproj_test) fclose(mmproj_test);
        LOG_ERROR("Download completed but model files not found in %s", default_model_dir);
        ethervox_path_config_cleanup(&path_config);
        free(session->full_transcript);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
      }
    } else {
      LOG_ERROR("Auto-download failed. Manual download required:");
      LOG_ERROR("  1. Visit: https://huggingface.co/ibm-granite/granite-speech-4.1-2b-plus-GGUF");
      LOG_ERROR("  2. Download: %s and %s", GRANITE_SPEECH_PLUS_MODEL_FILENAME,
                GRANITE_SPEECH_PLUS_MMPROJ_FILENAME);
      LOG_ERROR("  3. Place both in: ~/.ethervox/models/%s/", ETHERVOX_GRANITE_SPEECH_SUBDIR);
      LOG_ERROR("  Or configure a custom path:");
      LOG_ERROR("  Use path_set(\"GraniteSpeechModels\", \"/your/path/to/granite-speech\")");

      ethervox_path_config_cleanup(&path_config);
      free(session->full_transcript);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
  }

  // Configure STT with the Granite Speech Plus backend (SAA prompt is
  // selected automatically from config.backend - see granite_speech_backend.c)
  ethervox_stt_config_t stt_config = {.backend = ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS,
                                      .model_path = session->model_path,
                                      .mmproj_path = session->mmproj_path,
                                      .language = "auto",
                                      .sample_rate = 16000,
                                      .enable_partial_results = false,
                                      .enable_punctuation = false,  // Plus variant: SAA tagging, no punctuation
                                      .vad_threshold = 0.5f,
                                      .pool = pool};  // N6.3: Pass pool for budget enforcement

  if (ethervox_stt_init(&session->stt_runtime, &stt_config) != 0) {
    LOG_ERROR("Failed to initialize Granite Speech Plus STT");
    free(session->model_path);
    free(session->mmproj_path);
    session->model_path = NULL;
    session->mmproj_path = NULL;
    ethervox_path_config_cleanup(&path_config);
    free(session->full_transcript);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Initialize audio (will use platform-specific implementation)
  ethervox_audio_config_t audio_config = ethervox_audio_get_default_config();
  if (ethervox_audio_init(&session->audio_runtime, &audio_config) != 0) {
    LOG_WARN("Audio init failed - will use test data");
    // Continue anyway - we can test with simulated audio
  }

  // Cleanup path config (model_path/mmproj_path are now owned by session, stt_config only borrowed them)
  ethervox_path_config_cleanup(&path_config);

  // Initialize speaker tracking
  session->max_speaker_id = 0;  // 0 = no speakers detected yet (1-indexed SAA tags start at 1)
  session->speaker_names = NULL;
  session->speaker_names_capacity = 0;
  session->needs_summarization = false;

  session->is_initialized = true;
  LOG_INFO("Voice tools initialized with Granite Speech Plus STT backend (SAA)");

  return ETHERVOX_SUCCESS;
}

/**
 * Start listening session
 */
ethervox_result_t ethervox_voice_tools_start_listen(ethervox_voice_session_t* session) {
  if (!session || !session->is_initialized) {
    LOG_ERROR("Session not initialized");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  if (session->is_recording) {
    LOG_WARN("Already recording");
    return ETHERVOX_SUCCESS;
  }

  // Reset transcript
  session->full_transcript[0] = '\0';
  session->transcript_len = 0;
  session->segment_count = 0;
  session->session_start_time = time(NULL);
  session->stop_requested = false;
  
  // Reset speaker tracking for new session (1-indexed: 0 = no speakers yet)
  session->max_speaker_id = 0;
  if (session->speaker_names) {
    for (int i = 0; i < session->speaker_names_capacity; i++) {
      free(session->speaker_names[i]);
    }
    free(session->speaker_names);
    session->speaker_names = NULL;
    session->speaker_names_capacity = 0;
  }

  // Reset chunked-decoding state (see GRANITE_SPEECH_CHUNK_SECONDS in voice_tools.h)
  session->chunk_start_time = time(NULL);
  session->prefix_carry[0] = '\0';
  // BACKLOG-31 fix: free() the previous (heap-owned, see
  // finalize_chunk_and_restart()) prefix_text before clearing it, rather
  // than leaking it - a fresh recording never carries a prior one forward.
  free((void*)session->stt_runtime.config.prefix_text);
  session->stt_runtime.config.prefix_text = NULL;

  // Start STT
  if (ethervox_stt_start(&session->stt_runtime) != 0) {
    LOG_ERROR("Failed to start STT");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  LOG_INFO("Voice recording started - listening for speech...");
  LOG_DEBUG("Language: %s, VAD threshold: %.2f", session->stt_runtime.config.language,
            session->stt_runtime.config.vad_threshold);

  // Start audio capture
  if (!session->audio_runtime.is_initialized) {
    LOG_ERROR("Audio runtime not initialized");
    ethervox_stt_stop(&session->stt_runtime);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  if (ethervox_audio_start_capture(&session->audio_runtime) != 0) {
    LOG_ERROR("Failed to start audio capture");
    ethervox_stt_stop(&session->stt_runtime);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  session->is_recording = true;

  // Create transcript file immediately for live LLM monitoring
  // Use platform-specific path (Android app files or platform app data)
  const char* android_files_dir = ethervox_get_android_files_dir();
  char transcript_dir[512];
  char local_app_data[512];
  
  if (android_files_dir) {
    // Android: Use app-specific files directory
    snprintf(transcript_dir, sizeof(transcript_dir), "%s/transcripts", android_files_dir);
    LOG_INFO("[Android] Saving transcripts to: %s", transcript_dir);
  } else if (platform_get_local_app_data_dir(local_app_data, sizeof(local_app_data)) == ETHERVOX_SUCCESS) {
    // Windows/macOS/Linux: Use platform-specific local app data
    snprintf(transcript_dir, sizeof(transcript_dir), "%s%ctranscripts", local_app_data,
#ifdef _WIN32
             '\\'
#else
             '/'
#endif
    );
    LOG_INFO("Saving transcripts to: %s", transcript_dir);
  } else {
    // Fallback: current directory
    snprintf(transcript_dir, sizeof(transcript_dir), "transcripts");
    LOG_WARN("No platform-specific path, using ./transcripts");
  }
  
  // Create directory (platform-safe)
#ifdef _WIN32
  int mkdir_result = _mkdir(transcript_dir);
#else
  int mkdir_result = mkdir(transcript_dir, 0755);
#endif
  if (mkdir_result != 0 && errno != EEXIST) {
    LOG_ERROR("Failed to create transcript directory %s: errno=%d", transcript_dir, errno);
  } else {
    LOG_INFO("Transcript directory ready: %s", transcript_dir);
  }

  time_t now = time(NULL);
  struct tm* tm_info = localtime(&now);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

  snprintf(session->last_transcript_file, sizeof(session->last_transcript_file),
           "%s/transcript_%s.txt", transcript_dir, timestamp);

  // Create file with header
  LOG_INFO("Attempting to create transcript file: %s", session->last_transcript_file);
  FILE* f = fopen(session->last_transcript_file, "w");
  if (f) {
    fprintf(f, "Voice Transcript - Recording Started: %s\n", timestamp);
    fprintf(f, "========================================\n\n");
    fflush(f);  // Force write to disk
    fclose(f);
    LOG_INFO("[OK] Created live transcript file: %s", session->last_transcript_file);
  } else {
    LOG_ERROR("[FAIL] Failed to create transcript file: %s (errno=%d)", session->last_transcript_file, errno);
    session->last_transcript_file[0] = '\0';  // Clear the path since file creation failed
  }

  // Start the async finalize worker (TASK-N7.8) before the capture thread,
  // so it is ready before any audio is pushed to it.
  if (!get_or_create_audio_finalize_queue(session)) {
    LOG_ERROR("Failed to start audio finalize worker thread");
    ethervox_audio_stop_capture(&session->audio_runtime);
    ethervox_stt_stop(&session->stt_runtime);
    session->is_recording = false;
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Start background capture thread
  pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t));
  if (thread && pthread_create(thread, NULL, audio_capture_thread, session) == 0) {
    session->capture_thread = thread;
    LOG_INFO("Voice recording session started with background thread");
  } else {
    LOG_ERROR("Failed to create capture thread");
    free(thread);
    drain_and_join_audio_finalize_queue(session);
    ethervox_audio_stop_capture(&session->audio_runtime);
    ethervox_stt_stop(&session->stt_runtime);
    session->is_recording = false;
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  return ETHERVOX_SUCCESS;
}

/**
 * Stop listening and get transcript
 */
ethervox_result_t ethervox_voice_tools_stop_listen(ethervox_voice_session_t* session,
                                     const char** transcript_out) {
  const double n7_7_stop_start_ms = latency_trace_now_ms();  // TASK-N7.7

  if (!session || !session->is_initialized) {
    LOG_ERROR("Session not initialized");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  bool was_recording = session->is_recording;
  bool has_thread = (session->capture_thread != NULL);

  if (!was_recording && !has_thread) {
    LOG_WARN("Not currently recording");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Signal thread to stop (even if flag already dropped)
  session->stop_requested = true;
  session->is_recording = false;

  // Wait for capture thread to finish if it is still running
  if (has_thread) {
    pthread_t* thread = (pthread_t*)session->capture_thread;
    pthread_join(*thread, NULL);
    free(thread);
    session->capture_thread = NULL;
    LOG_INFO("Capture thread joined - all audio handed to the finalize worker");
  }

  // Stop audio capture
  if (session->audio_runtime.is_initialized) {
    ethervox_audio_stop_capture(&session->audio_runtime);
  }

  // TASK-N7.8: capture has stopped, so no more audio will ever be pushed.
  // Drain whatever is still queued into the STT accumulator and join the
  // finalize worker before touching stt_runtime from this thread - this is
  // what makes the tail finalize below safe without a race.
  drain_and_join_audio_finalize_queue(session);

  // CRITICAL: Finalize the last (possibly partial) chunk to transcribe
  // whatever audio remains buffered - handles the /stoptranscribe case
  // where the current GRANITE_SPEECH_CHUNK_SECONDS window hasn't elapsed yet.
  LOG_INFO("Finalizing Granite Speech Plus transcription for remaining audio...");
  ethervox_stt_result_t final_result;
  const double n7_7_tail_start_ms = latency_trace_now_ms();  // TASK-N7.7
  ethervox_result_t n7_7_tail_rc = ethervox_stt_finalize(&session->stt_runtime, &final_result);
  LOG_LATENCY("N7.7 stop_finalize: wall=%.1fms", latency_trace_now_ms() - n7_7_tail_start_ms);
  if (n7_7_tail_rc == 0) {
    if (final_result.text && strlen(final_result.text) > 0) {
      LOG_INFO("Finalize produced %zu chars", strlen(final_result.text));

      track_max_speaker_id(session, final_result.text);

      char formatted_segment[8192];
      time_t now_ts = time(NULL);
      struct tm* tm_info_ts = localtime(&now_ts);
      char datetime_str[64];
      strftime(datetime_str, sizeof(datetime_str), "%Y-%m-%d %H:%M:%S", tm_info_ts);
      snprintf(formatted_segment, sizeof(formatted_segment), "[%s] (%s) %s", datetime_str,
               final_result.language ? final_result.language : "auto", final_result.text);

      append_transcript_segment(session, formatted_segment);

      // TASK-C3.5: same event push as finalize_chunk_and_restart() - this
      // is the session's last segment, produced synchronously at stop time
      // rather than mid-recording, but a host listening for events should
      // still see it rather than only the ones before it.
      if (session->transcription_event_cb) {
        push_transcription_event(session, session->segment_count,
                                  max_speaker_id_in_text(final_result.text), true, final_result.text);
      }
    }
    ethervox_stt_result_free(&final_result);
  }

  ethervox_stt_stop(&session->stt_runtime);
  session->is_recording = false;

  // Append completion status to transcript file (content already written live during recording)
  if (session->transcript_len > 0 && session->last_transcript_file[0] != '\0') {
    FILE* f = fopen(session->last_transcript_file, "a");
    if (f) {
      time_t now = time(NULL);
      struct tm* tm_info = localtime(&now);
      char timestamp[64];
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

      fprintf(f, "\n\n========================================\n");
      fprintf(f, "Recording completed: %s\n", timestamp);
      fprintf(f, "Duration: %llu seconds\n",
              (unsigned long long)(time(NULL) - session->session_start_time));
      fprintf(f, "Total segments: %u\n", session->segment_count);
      fprintf(f, "Total characters: %zu\n", session->transcript_len);
      fflush(f);
      fclose(f);

      LOG_INFO("Saved transcript to: %s (%zu chars, %u segments)", session->last_transcript_file,
               session->transcript_len, session->segment_count);
      
      // Prompt user to assign speaker names if speakers were detected
      // (max_speaker_id IS the speaker count - SAA tags are 1-indexed)
      if (session->max_speaker_id > 0) {
        LOG_INFO("Detected %d speaker(s) in conversation", session->max_speaker_id);
        int naming_result = ethervox_voice_tools_assign_speaker_names(session);
        if (naming_result == 0) {
          LOG_INFO("Speaker names assigned and transcript updated");
          
          // Trigger LLM summarization
          printf("Summarizing...\n");
          fflush(stdout);
          
          // Set flag to trigger summarization in main loop
          session->needs_summarization = true;
        } else if (naming_result == 1) {
          LOG_INFO("User chose to keep anonymous speaker labels");
        }
      }

      // Also store in memory system
      if (session->memory_store) {
        ethervox_memory_store_t* mem = (ethervox_memory_store_t*)session->memory_store;
        const char* tags[] = {"voice", "transcript", "granite-speech"};
        uint64_t memory_id = 0;

        // Store with file path reference
        char memory_content[2048];
        snprintf(memory_content, sizeof(memory_content),
                 "Voice transcript saved to: %s\n\nTranscript:\n%s", session->last_transcript_file,
                 session->full_transcript);

        ethervox_memory_store_add(mem, memory_content, tags, 3, 0.9f, false, &memory_id);
        LOG_INFO("Stored in memory (ID: %llu) with file reference", (unsigned long long)memory_id);
      }
    } else {
      LOG_ERROR("Failed to open transcript file for final update: %s",
                session->last_transcript_file);
    }
  }

  // Ensure we always return a valid string (never NULL)
  if (session->full_transcript && session->full_transcript[0] != '\0') {
    *transcript_out = session->full_transcript;
  } else {
    // Return empty string if no transcript (silence or error)
    static const char* empty_transcript = "";
    *transcript_out = empty_transcript;
    LOG_WARN("No transcript generated (silence or error) - returning empty string");
  }

  LOG_INFO("Voice recording session stopped - %zu chars transcribed", session->transcript_len);

  // TASK-C3.5: flush and join the dispatch thread now that no more events
  // will be pushed for this session - drains any already-queued segments
  // before returning (see transcription_dispatch_thread_main()'s loop).
  destroy_event_queue(session);

  LOG_LATENCY("N7.7 stop_total: wall=%.1fms", latency_trace_now_ms() - n7_7_stop_start_ms);

  return ETHERVOX_SUCCESS;
}

/**
 * Check if recording
 */
bool ethervox_voice_tools_is_recording(const ethervox_voice_session_t* session) {
  return session && session->is_recording;
}

/**
 * Register streaming transcription event callback (TASK-C3.5)
 */
void ethervox_voice_tools_set_event_callback(ethervox_voice_session_t* session,
                                              ethervox_event_cb callback,
                                              void* user_data) {
  if (!session) return;
  session->transcription_event_cb = callback;
  session->transcription_event_user_data = user_data;
}

/**
 * Cleanup
 */
void ethervox_voice_tools_cleanup(ethervox_voice_session_t* session) {
  if (!session)
    return;

  if (session->is_recording) {
    const char* transcript;
    ethervox_voice_tools_stop_listen(session, &transcript);
  }

  // Safety net: stop_listen() already tears this down on every normal path;
  // a no-op if event_queue is already NULL (session never registered a
  // callback, or was already stopped).
  destroy_event_queue(session);

  // Same safety net for the TASK-N7.8 audio finalize queue.
  drain_and_join_audio_finalize_queue(session);

  ethervox_stt_cleanup(&session->stt_runtime);

  if (session->audio_runtime.is_initialized) {
    ethervox_audio_cleanup(&session->audio_runtime);
  }

  if (session->full_transcript) {
    free(session->full_transcript);
    session->full_transcript = NULL;
  }
  
  // Cleanup speaker names
  if (session->speaker_names) {
    for (int i = 0; i < session->speaker_names_capacity; i++) {
      free(session->speaker_names[i]);
    }
    free(session->speaker_names);
    session->speaker_names = NULL;
    session->speaker_names_capacity = 0;
  }

  if (session->model_path) {
    free(session->model_path);
    session->model_path = NULL;
  }

  if (session->mmproj_path) {
    free(session->mmproj_path);
    session->mmproj_path = NULL;
  }

  session->is_initialized = false;

  LOG_INFO("Voice tools cleaned up");
}

/**
 * Prompt user to assign names to speakers and update transcript file
 */
ethervox_result_t ethervox_voice_tools_assign_speaker_names(ethervox_voice_session_t* session) {
  if (!session || session->max_speaker_id <= 0) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;  // No speakers detected
  }
  
  // Granite Speech Plus's SAA tags are 1-indexed ("[Speaker 1]: ..."), so
  // max_speaker_id IS the speaker count directly. Arrays below are sized
  // num_speakers+1 and index 0 is left unused, so speaker_id can be used
  // as the array index everywhere without an off-by-one translation.
  int num_speakers = session->max_speaker_id;
  
  // First, extract example quotes from each speaker
  typedef struct {
    char* quotes[ETHERVOX_SPEAKER_EXAMPLE_QUOTES];
    int quote_count;
  } speaker_examples_t;
  
  speaker_examples_t* examples = (speaker_examples_t*)calloc(num_speakers + 1, sizeof(speaker_examples_t));
  if (!examples) {
    LOG_ERROR("Failed to allocate speaker examples array");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  // Read transcript file to extract examples
  if (session->last_transcript_file[0] != '\0') {
    FILE* f = fopen(session->last_transcript_file, "r");
    if (f) {
      char line[1024];
      while (fgets(line, sizeof(line), f)) {
        // Look for speaker markers: [Speaker N]: anywhere in the line
        // Format: [2025-12-04 08:45:30] (en) [Speaker 1]: text here
        // (native Granite Speech Plus SAA tagging - 1-indexed, with colon)
        const char* speaker_marker = strstr(line, "[Speaker ");
        if (speaker_marker) {
          int speaker_id = -1;
          if (sscanf(speaker_marker, "[Speaker %d]:", &speaker_id) == 1) {
            if (speaker_id >= 1 && speaker_id <= num_speakers) {
              // Extract text after speaker marker
              const char* text_start = strstr(speaker_marker, "]:");
              if (text_start) {
                text_start += 2;  // Skip "]:"
                while (*text_start == ' ') text_start++;  // Skip spaces
                
                // Store this quote if we have room and it's not empty
                if (examples[speaker_id].quote_count < ETHERVOX_SPEAKER_EXAMPLE_QUOTES && 
                    strlen(text_start) > 1) {  // More than just newline
                  // Trim newline
                  size_t len = strlen(text_start);
                  if (len > 0 && text_start[len - 1] == '\n') {
                    char* quote = (char*)malloc(len);
                    if (quote) {
                      strncpy(quote, text_start, len - 1);
                      quote[len - 1] = '\0';
                      examples[speaker_id].quotes[examples[speaker_id].quote_count++] = quote;
                    }
                  } else {
                    examples[speaker_id].quotes[examples[speaker_id].quote_count++] = strdup(text_start);
                  }
                }
              }
            }
          }
        }
      }
      fclose(f);
    }
  }
  
  // Prompt user with examples
  printf("\n");
  printf("========================================\n");
  printf("Speaker Identification\n");
  printf("========================================\n");
  printf("Detected %d speaker(s) in the conversation.\n\n", num_speakers);
  
  // Show examples for each speaker (1-indexed, matches native SAA tagging)
  for (int i = 1; i <= num_speakers; i++) {
    printf("Speaker %d examples:\n", i);
    if (examples[i].quote_count == 0) {
      printf("  (no examples found)\n");
    } else {
      for (int j = 0; j < examples[i].quote_count; j++) {
        printf("  %d. \"%s\"\n", j + 1, examples[i].quotes[j]);
      }
    }
    printf("\n");
  }
  
  // Loop until valid input (y/Y/n/N or empty for default No)
  bool valid_input = false;
  bool assign_names = false;
  
  while (!valid_input) {
    printf("Would you like to assign names to the speakers? [y/N]: ");
    fflush(stdout);
    
    char response[10];
    if (fgets(response, sizeof(response), stdin) == NULL) {
      // Cleanup examples
      for (int i = 0; i < num_speakers; i++) {
        for (int j = 0; j < examples[i].quote_count; j++) {
          free(examples[i].quotes[j]);
        }
      }
      free(examples);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Trim newline and whitespace
    size_t len = strlen(response);
    while (len > 0 && (response[len - 1] == '\n' || response[len - 1] == ' ' || 
                       response[len - 1] == '\t')) {
      response[--len] = '\0';
    }
    
    // Skip leading whitespace
    char* input = response;
    while (*input == ' ' || *input == '\t') {
      input++;
    }
    
    // Validate input: empty (default No), y, Y, n, or N
    if (strlen(input) == 0 || input[0] == 'n' || input[0] == 'N') {
      valid_input = true;
      assign_names = false;
    } else if (input[0] == 'y' || input[0] == 'Y') {
      valid_input = true;
      assign_names = true;
    } else {
      printf("Invalid input. Please enter 'y' for yes or 'n' for no (or press Enter for no).\n\n");
    }
  }
  
  // Check if user declined
  if (!assign_names) {
    printf("Keeping anonymous speaker labels (Speaker 1, Speaker 2, etc.)\n");
    // Cleanup examples
    for (int i = 1; i <= num_speakers; i++) {
      for (int j = 0; j < examples[i].quote_count; j++) {
        free(examples[i].quotes[j]);
      }
    }
    free(examples);
    return ETHERVOX_SUCCESS;  // User declined
  }
  
  // Cleanup examples now that we're done showing them
  for (int i = 1; i <= num_speakers; i++) {
    for (int j = 0; j < examples[i].quote_count; j++) {
      free(examples[i].quotes[j]);
    }
  }
  free(examples);
  
  // Allocate speaker names array (index 0 unused - see voice_tools.h)
  session->speaker_names_capacity = num_speakers + 1;
  session->speaker_names = (char**)calloc(num_speakers + 1, sizeof(char*));
  if (!session->speaker_names) {
    LOG_ERROR("Failed to allocate speaker names array");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  // Prompt for each speaker's name (1-indexed)
  printf("\n");
  for (int i = 1; i <= num_speakers; i++) {
    printf("Enter name for Speaker %d: ", i);
    fflush(stdout);
    
    char name_buffer[128];
    if (fgets(name_buffer, sizeof(name_buffer), stdin) == NULL) {
      // Failed to read - use anonymous
      session->speaker_names[i] = strdup("Anonymous");
      continue;
    }
    
    // Trim newline and whitespace
    size_t name_len = strlen(name_buffer);
    while (name_len > 0 && (name_buffer[name_len - 1] == '\n' || name_buffer[name_len - 1] == ' ' || 
                       name_buffer[name_len - 1] == '\t')) {
      name_buffer[--name_len] = '\0';
    }
    
    // Skip leading whitespace
    char* name_start = name_buffer;
    while (*name_start == ' ' || *name_start == '\t') {
      name_start++;
    }
    
    // Use provided name or "Anonymous" if empty
    if (strlen(name_start) > 0) {
      session->speaker_names[i] = strdup(name_start);
    } else {
      session->speaker_names[i] = strdup("Anonymous");
    }
  }
  
  printf("\n");
  printf("Speaker assignments:\n");
  for (int i = 1; i <= num_speakers; i++) {
    printf("  Speaker %d → %s\n", i, session->speaker_names[i]);
  }
  printf("\n");
  
  // Update transcript file with named speakers
  if (session->last_transcript_file[0] == '\0') {
    LOG_WARN("No transcript file to update");
    return ETHERVOX_SUCCESS;  // Names saved but no file to update
  }
  
  // Read current transcript file
  FILE* f = fopen(session->last_transcript_file, "r");
  if (!f) {
    LOG_ERROR("Failed to open transcript file for reading: %s", session->last_transcript_file);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  // Read entire file into memory
  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  
  char* file_content = (char*)malloc(file_size + 1);
  if (!file_content) {
    fclose(f);
    LOG_ERROR("Failed to allocate memory for file content");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  size_t bytes_read = fread(file_content, 1, file_size, f);
  file_content[bytes_read] = '\0';
  fclose(f);
  
  // Create updated content by replacing [Speaker N] with names
  char* updated_content = (char*)malloc(file_size * 2 + 1024);  // Extra space for longer names
  if (!updated_content) {
    free(file_content);
    LOG_ERROR("Failed to allocate memory for updated content");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  // Process content and replace speaker markers
  const char* read_ptr = file_content;
  char* write_ptr = updated_content;
  
  while (*read_ptr) {
    // Look for native "[Speaker N]:" pattern (1-indexed, colon-terminated SAA tag)
    if (strncmp(read_ptr, "[Speaker ", 9) == 0) {
      int speaker_id = -1;
      int chars_read = 0;
      if (sscanf(read_ptr, "[Speaker %d]:%n", &speaker_id, &chars_read) == 1) {
        // Found a speaker marker - replace with name, keeping the colon
        if (speaker_id >= 1 && speaker_id <= num_speakers && session->speaker_names[speaker_id]) {
          int name_len = sprintf(write_ptr, "[%s]:", session->speaker_names[speaker_id]);
          write_ptr += name_len;
          read_ptr += chars_read;
          continue;
        }
      }
    }
    
    // Not a speaker marker or failed to parse - copy character as-is
    *write_ptr++ = *read_ptr++;
  }
  *write_ptr = '\0';
  
  // Write updated content back to file
  f = fopen(session->last_transcript_file, "w");
  if (!f) {
    free(file_content);
    free(updated_content);
    LOG_ERROR("Failed to open transcript file for writing: %s", session->last_transcript_file);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  fprintf(f, "%s", updated_content);
  fclose(f);
  
  // Rename file with speaker names if provided
  char new_filename[1024];
  bool should_rename = false;
  
  // Build new filename with first two speaker names (or first if only one)
  if (num_speakers >= 2 && session->speaker_names[1] && session->speaker_names[2]) {
    // Extract directory and timestamp from original filename
    const char* last_slash = strrchr(session->last_transcript_file, '/');
    size_t dir_len = last_slash ? (last_slash - session->last_transcript_file + 1) : 0;
    
    // Get timestamp from original filename (transcript_YYYYMMDD_HHMMSS.txt)
    const char* timestamp_start = strstr(session->last_transcript_file, "transcript_");
    char timestamp[32] = "";
    if (timestamp_start) {
      timestamp_start += 11; // Skip "transcript_"
      const char* timestamp_end = strchr(timestamp_start, '.');
      if (timestamp_end) {
        size_t ts_len = timestamp_end - timestamp_start;
        if (ts_len < sizeof(timestamp)) {
          strncpy(timestamp, timestamp_start, ts_len);
          timestamp[ts_len] = '\0';
        }
      }
    }
    
    if (dir_len > 0 && strlen(timestamp) > 0) {
      snprintf(new_filename, sizeof(new_filename), "%.*s%s_and_%s_%s.txt",
               (int)dir_len, session->last_transcript_file,
               session->speaker_names[1], session->speaker_names[2], timestamp);
      should_rename = true;
    }
  } else if (num_speakers == 1 && session->speaker_names[1]) {
    const char* last_slash = strrchr(session->last_transcript_file, '/');
    size_t dir_len = last_slash ? (last_slash - session->last_transcript_file + 1) : 0;
    
    const char* timestamp_start = strstr(session->last_transcript_file, "transcript_");
    char timestamp[32] = "";
    if (timestamp_start) {
      timestamp_start += 11;
      const char* timestamp_end = strchr(timestamp_start, '.');
      if (timestamp_end) {
        size_t ts_len = timestamp_end - timestamp_start;
        if (ts_len < sizeof(timestamp)) {
          strncpy(timestamp, timestamp_start, ts_len);
          timestamp[ts_len] = '\0';
        }
      }
    }
    
    if (dir_len > 0 && strlen(timestamp) > 0) {
      snprintf(new_filename, sizeof(new_filename), "%.*s%s_%s.txt",
               (int)dir_len, session->last_transcript_file,
               session->speaker_names[1], timestamp);
      should_rename = true;
    }
  }
  
  // Attempt to rename the file
  if (should_rename) {
    if (rename(session->last_transcript_file, new_filename) == 0) {
      LOG_INFO("Renamed transcript file to: %s", new_filename);
      printf("\n📝 Renamed transcript file:\n");
      printf("  Old: %s\n", session->last_transcript_file);
      printf("  New: %s\n\n", new_filename);
      
      // Update session with new filename
      strncpy(session->last_transcript_file, new_filename, sizeof(session->last_transcript_file) - 1);
      session->last_transcript_file[sizeof(session->last_transcript_file) - 1] = '\0';
    } else {
      LOG_WARN("Failed to rename transcript file");
    }
  }
  
  // Display the updated transcript content
  printf("\n========================================\n");
  printf("📄 Updated Transcript\n");
  printf("========================================\n\n");
  printf("%s\n", updated_content);
  printf("========================================\n\n");
  
  free(file_content);
  free(updated_content);
  
  LOG_INFO("[OK] Updated transcript file with speaker names: %s", session->last_transcript_file);
  printf("[OK] Transcript updated with speaker names\n");
  printf("  File: %s\n\n", session->last_transcript_file);
  
  return ETHERVOX_SUCCESS;
}

/**
 * Tool wrapper: listen_and_summarize
 *
 * This tool allows the LLM to start/stop voice recording with Granite Speech transcription.
 * The LLM must call with action="start" then action="stop" to get the transcript.
 */
static int tool_listen_and_summarize_wrapper(const char* args_json, char** result, char** error) {
  ethervox_voice_session_t* session = g_voice_session;

  if (!session || !session->is_initialized) {
    *error = strdup("Voice tools not initialized");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Parse action parameter
  char action[32] = {0};
  const char* action_start = strstr(args_json, "\"action\":");
  if (action_start) {
    action_start = strchr(action_start, ':') + 1;
    while (*action_start == ' ' || *action_start == '"')
      action_start++;
    const char* action_end = strchr(action_start, '"');
    if (action_end) {
      size_t len = action_end - action_start;
      if (len < sizeof(action)) {
        strncpy(action, action_start, len);
        action[len] = '\0';
      }
    }
  }

  if (strcmp(action, "start") == 0) {
    // Start recording
    if (ethervox_voice_tools_start_listen(session) != 0) {
      *error = strdup("Failed to start listening");
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    *result = strdup(
        "{\"status\":\"recording\",\"message\":\"Voice recording started. Use /end command or call "
        "with action='stop' to finish.\"}");
    return ETHERVOX_SUCCESS;

  } else if (strcmp(action, "stop") == 0) {
    // Stop recording and get transcript
    const char* transcript;
    if (ethervox_voice_tools_stop_listen(session, &transcript) != 0) {
      *error = strdup("Failed to stop listening or not recording");
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Build JSON response with transcript
    size_t res_len = strlen(transcript) + 512;
    char* res = malloc(res_len);
    snprintf(res, res_len,
             "{\"status\":\"completed\",\"transcript\":\"%s\",\"length\":%zu,\"segments\":%u}",
             transcript, session->transcript_len, session->segment_count);

    *result = res;

    // Store transcript in memory
    if (session->memory_store && strlen(transcript) > 0) {
      const char* tags[] = {"voice", "transcript", "granite-speech"};
      uint64_t memory_id = 0;

      // Cast to memory store and add
      ethervox_memory_store_t* mem = (ethervox_memory_store_t*)session->memory_store;
      ethervox_memory_store_add(mem, transcript, tags, 3, 0.8f, false, &memory_id);

      LOG_INFO("Stored voice transcript in memory (ID: %llu)", (unsigned long long)memory_id);
    }

    return ETHERVOX_SUCCESS;

  } else if (strcmp(action, "status") == 0) {
    // Check recording status
    char status_msg[256];
    snprintf(status_msg, sizeof(status_msg),
             "{\"is_recording\":%s,\"transcript_length\":%zu,\"segments\":%u}",
             session->is_recording ? "true" : "false", session->transcript_len,
             session->segment_count);
    *result = strdup(status_msg);
    return ETHERVOX_SUCCESS;

  } else {
    *error = strdup("Invalid action. Use 'start', 'stop', or 'status'");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
}

/**
 * Tool definition for listen_and_summarize
 */
static ethervox_tool_t listen_tool = {
    .test_scenario = "Listen to this recording and summarize it",
        .execute = tool_listen_and_summarize_wrapper,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = true,
    .estimated_latency_ms = 100.0f};
static bool listen_tool_loaded = false;

static void load_listen_tool_catalogue_once(void) {
    if (listen_tool_loaded) return;
    ethervox_tool_catalogue_load(ETHERVOX_CATALOGUE_CONVERSATION_TOOLS_JSON, "listen_and_summarize",
        ethervox_tool_catalogue_build_profile(), &listen_tool);
    listen_tool_loaded = true;
}

/**
 * Register base voice tools with Governor
 * 
 * Base tools: listen_and_summarize (always available)
 */
ethervox_result_t ethervox_voice_tools_register(void* registry, ethervox_voice_session_t* session) {
  if (!registry || !session) {
    LOG_ERROR("Invalid parameters for voice tools registration");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  g_voice_session = session;

  ethervox_tool_registry_t* reg = (ethervox_tool_registry_t*)registry;

  load_listen_tool_catalogue_once();
  if (ethervox_tool_registry_add(reg, &listen_tool) != 0) {
    LOG_ERROR("Failed to register listen_and_summarize tool");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  LOG_INFO("Registered listen_and_summarize tool with Governor (Granite Speech STT)");

  return ETHERVOX_SUCCESS;
}

/**
 * Register advanced voice training tools with Governor
 * 
 * Training tools: pronunciation training, voice training mode
 * These are optional/advanced features separate from base voice tools
 * 
 * @param registry Governor tool registry
 * @param session Voice session state  
 * @param phonemizer_ctx Phonemizer context for training
 * @param tts_ctx TTS context for synthesis
 * @param stt_ctx STT context for transcription
 * @return 0 on success, -1 on error
 */
ethervox_result_t ethervox_voice_tools_register_training(
    void* registry, 
    ethervox_voice_session_t* session,
    void* phonemizer_ctx,
    void* tts_ctx,
    void* stt_ctx
) {
  if (!registry || !session) {
    LOG_ERROR("Invalid parameters for training tools registration");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Training tools are registered in conversation_tools plugin
  // This function exists for future training-specific tools
  
  LOG_INFO("Training tools registration placeholder (tools in conversation_tools)");
  
  return ETHERVOX_SUCCESS;
}
