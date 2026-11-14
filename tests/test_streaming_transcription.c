/**
 * @file test_streaming_transcription.c
 * @brief Tests for streaming transcription events (TASK-C3.5)
 *
 * Full end-to-end coverage (segments actually arriving mid-recording from a
 * live Granite Speech Plus model, revision semantics, UTF-8 boundary safety)
 * needs a real model and audio pipeline and is not exercised here. This
 * covers what's testable without one: the event struct's shape, and the
 * callback registration API's contract.
 *
 * The async delivery queue that backs backpressure (push_transcription_event
 * et al. in voice_tools.c) is static to that translation unit and isn't
 * exercised by a dedicated concurrency test here either - it's build- and
 * code-review-verified (bounded capacity, dedicated dispatch thread, no
 * direct callback invocation from the capture thread), not test-verified.
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/event_stream.h"
#include "ethervox/voice_tools.h"
#include "unit/test_utils.h"
#include <string.h>

static int captured_calls = 0;
static uint32_t captured_segment_id = 0;
static int32_t captured_speaker_id = -99;
static bool captured_is_final = false;
static char captured_text[256];

static bool capture_cb(const ethervox_event_t* event, void* user_data) {
    (void)user_data;
    if (event->type == ETHERVOX_EVENT_TRANSCRIPTION_SEGMENT) {
        captured_calls++;
        captured_segment_id = event->transcription_segment.segment_id;
        captured_speaker_id = event->transcription_segment.speaker_id;
        captured_is_final = event->transcription_segment.is_final;
        strncpy(captured_text, event->transcription_segment.text, sizeof(captured_text) - 1);
        captured_text[sizeof(captured_text) - 1] = '\0';
    }
    return true;
}

// The union member itself round-trips correctly - a regression here would
// mean a struct-layout mistake silently reading the wrong union member.
static void test_event_struct_shape(void) {
    ethervox_event_t event = {0};
    event.type = ETHERVOX_EVENT_TRANSCRIPTION_SEGMENT;
    event.transcription_segment.segment_id = 3;
    event.transcription_segment.speaker_id = 2;
    event.transcription_segment.text = "[Speaker 2]: hello";
    event.transcription_segment.is_final = true;

    CHECK(event.type == ETHERVOX_EVENT_TRANSCRIPTION_SEGMENT);
    CHECK(event.transcription_segment.segment_id == 3);
    CHECK(event.transcription_segment.speaker_id == 2);
    CHECK(strcmp(event.transcription_segment.text, "[Speaker 2]: hello") == 0);
    CHECK(event.transcription_segment.is_final == true);
}

// Registering NULL clears the callback (additive contract: a host that never
// calls this setter never sees a behaviour change - see voice_tools.h).
static void test_set_event_callback_register_and_clear(void) {
    ethervox_voice_session_t session = {0};

    ethervox_voice_tools_set_event_callback(&session, capture_cb, (void*)0x1234);
    CHECK(session.transcription_event_cb == capture_cb);
    CHECK(session.transcription_event_user_data == (void*)0x1234);

    ethervox_voice_tools_set_event_callback(&session, NULL, NULL);
    CHECK(session.transcription_event_cb == NULL);
    CHECK(session.transcription_event_user_data == NULL);
}

// NULL session must not crash (defensive contract every setter here follows).
static void test_set_event_callback_null_session_safe(void) {
    ethervox_voice_tools_set_event_callback(NULL, capture_cb, NULL);
}

// Simulates what finalize_chunk_and_restart() does when a callback is
// registered, since that function is static to voice_tools.c and needs a
// live Granite Speech Plus model to reach for real.
static void test_callback_invoked_with_expected_fields(void) {
    ethervox_voice_session_t session = {0};
    ethervox_voice_tools_set_event_callback(&session, capture_cb, NULL);
    captured_calls = 0;

    session.segment_count = 5;
    if (session.transcription_event_cb) {
        ethervox_event_t event = {0};
        event.type = ETHERVOX_EVENT_TRANSCRIPTION_SEGMENT;
        event.transcription_segment.segment_id = session.segment_count;
        event.transcription_segment.speaker_id = 1;
        event.transcription_segment.text = "[Speaker 1]: testing streaming segments";
        event.transcription_segment.is_final = true;
        session.transcription_event_cb(&event, session.transcription_event_user_data);
    }

    CHECK(captured_calls == 1);
    CHECK(captured_segment_id == 5);
    CHECK(captured_speaker_id == 1);
    CHECK(captured_is_final == true);
    CHECK(strcmp(captured_text, "[Speaker 1]: testing streaming segments") == 0);
}

int main(void) {
    RUN_TEST(test_event_struct_shape);
    RUN_TEST(test_set_event_callback_register_and_clear);
    RUN_TEST(test_set_event_callback_null_session_safe);
    RUN_TEST(test_callback_invoked_with_expected_fields);
    printf("\nAll streaming_transcription tests passed.\n");
    return 0;
}
