/**
 * @file tts_host.c
 * @brief TTS as a host capability (TASK-C1.0, docs/07-BACKEND-CHANGES.md §0.4)
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/tts_host.h"
#include <string.h>

static ethervox_tts_host_t g_host;
static bool g_host_registered = false;

ethervox_result_t ethervox_tts_set_host(const ethervox_tts_host_t* host) {
    if (!host) {
        memset(&g_host, 0, sizeof(g_host));
        g_host_registered = false;
        return ETHERVOX_SUCCESS;
    }

    if (!host->speak || !host->stop || !host->pause || !host->resume || !host->is_speaking) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_INVALID_ARGUMENT,
                               "ethervox_tts_host_t must implement every callback");
    }

    g_host = *host;
    g_host_registered = true;
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_tts_host_speak(const char* utf8, const ethervox_tts_style_t* style,
                                           ethervox_tts_word_cb word_cb) {
    if (!g_host_registered) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NO_TTS_HOST, "No TTS host registered");
    }
    if (!utf8) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NULL_POINTER, "utf8 text is NULL");
    }
    return g_host.speak(utf8, style, word_cb, g_host.user_data);
}

void ethervox_tts_host_stop(void) {
    if (g_host_registered) {
        g_host.stop(g_host.user_data);
    }
}

void ethervox_tts_host_pause(void) {
    if (g_host_registered) {
        g_host.pause(g_host.user_data);
    }
}

void ethervox_tts_host_resume(void) {
    if (g_host_registered) {
        g_host.resume(g_host.user_data);
    }
}

bool ethervox_tts_host_is_speaking(void) {
    if (!g_host_registered) {
        return false;
    }
    return g_host.is_speaking(g_host.user_data);
}
