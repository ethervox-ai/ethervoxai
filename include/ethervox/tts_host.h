/**
 * @file tts_host.h
 * @brief TTS as a host capability (TASK-C1.0, docs/07-BACKEND-CHANGES.md §0.4)
 *
 * The core no longer synthesises speech itself; it calls back into a platform-provided
 * host (AVSpeechSynthesizer, android.speech.tts.TextToSpeech, Windows.Media.SpeechSynthesis,
 * speech-dispatcher, ...). Exactly one host may be registered at a time, process-wide.
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_TTS_HOST_H
#define ETHERVOX_TTS_HOST_H

#include "ethervox/error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* voice_id;     /* NULL = the user's configured system voice */
    float       rate;         /* 1.0 = platform default */
    float       pitch;
    const char* emotion;      /* optional hint; platform engines ignore it */
    float       intensity;
} ethervox_tts_style_t;

typedef void (*ethervox_tts_word_cb)(uint32_t char_start, uint32_t char_len, void* ud);

typedef struct {
    ethervox_result_t (*speak)(const char* utf8, const ethervox_tts_style_t*,
                                ethervox_tts_word_cb, void* ud);
    void (*stop)(void* ud);
    void (*pause)(void* ud);
    void (*resume)(void* ud);
    bool (*is_speaking)(void* ud);
    void* user_data;
} ethervox_tts_host_t;

/**
 * Register the process-wide TTS host. Pass NULL to clear it.
 *
 * The struct is copied; the caller does not need to keep it alive afterwards, but
 * host->user_data must remain valid for as long as the host is registered.
 */
ethervox_result_t ethervox_tts_set_host(const ethervox_tts_host_t* host);

/**
 * Speak via the registered host.
 *
 * @return ETHERVOX_ERROR_NO_TTS_HOST if no host is registered (never crashes).
 */
ethervox_result_t ethervox_tts_host_speak(const char* utf8, const ethervox_tts_style_t* style,
                                           ethervox_tts_word_cb word_cb);

/** No-op (not an error) if no host is registered. */
void ethervox_tts_host_stop(void);
void ethervox_tts_host_pause(void);
void ethervox_tts_host_resume(void);

/** Returns false if no host is registered. */
bool ethervox_tts_host_is_speaking(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_TTS_HOST_H
