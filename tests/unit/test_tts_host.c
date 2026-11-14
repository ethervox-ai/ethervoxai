// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
// Copyright (c) 2024-2026 EthervoxAI Team
//
// Verifies ethervox_tts_set_host / ethervox_tts_host_* (TASK-C1.0, docs/07-BACKEND-CHANGES.md
// §0.4) against a fake host: speak, stop, pause, resume, is_speaking and word callbacks.
#include "ethervox/tts_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test_func) do { \
    printf("Running %s...\n", #test_func); \
    tests_run++; \
    test_func(); \
    tests_passed++; \
    printf("  \xe2\x9c\x93 %s passed\n", #test_func); \
} while (0)

// Deliberately not CHECK(): this test suite is built with -DNDEBUG (see
// CMakeLists.txt Release flags), which turns CHECK() into a no-op and would
// make every test below vacuously "pass". CHECK always evaluates.
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "  \xe2\x9c\x97 CHECK failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

// ---------------------------------------------------------------------------
// Fake host
// ---------------------------------------------------------------------------

typedef struct {
    bool speaking;
    bool paused;
    int  stop_calls;
    int  pause_calls;
    int  resume_calls;
    char last_utf8[256];
    ethervox_tts_style_t last_style;
    ethervox_tts_word_cb last_word_cb;
} fake_host_state_t;

static fake_host_state_t g_fake;

static ethervox_result_t fake_speak(const char* utf8, const ethervox_tts_style_t* style,
                                     ethervox_tts_word_cb word_cb, void* ud) {
    fake_host_state_t* state = (fake_host_state_t*)ud;
    strncpy(state->last_utf8, utf8, sizeof(state->last_utf8) - 1);
    state->last_utf8[sizeof(state->last_utf8) - 1] = '\0';
    state->last_style = style ? *style : (ethervox_tts_style_t){0};
    state->last_word_cb = word_cb;
    state->speaking = true;
    state->paused = false;
    if (word_cb) {
        word_cb(0, (uint32_t)strlen(utf8), ud);
    }
    return ETHERVOX_SUCCESS;
}

static void fake_stop(void* ud) {
    fake_host_state_t* state = (fake_host_state_t*)ud;
    state->stop_calls++;
    state->speaking = false;
    state->paused = false;
}

static void fake_pause(void* ud) {
    fake_host_state_t* state = (fake_host_state_t*)ud;
    state->pause_calls++;
    state->paused = true;
}

static void fake_resume(void* ud) {
    fake_host_state_t* state = (fake_host_state_t*)ud;
    state->resume_calls++;
    state->paused = false;
}

static bool fake_is_speaking(void* ud) {
    fake_host_state_t* state = (fake_host_state_t*)ud;
    return state->speaking;
}

static void reset_fake(void) {
    memset(&g_fake, 0, sizeof(g_fake));
}

static ethervox_tts_host_t make_fake_host(void) {
    ethervox_tts_host_t host;
    host.speak = fake_speak;
    host.stop = fake_stop;
    host.pause = fake_pause;
    host.resume = fake_resume;
    host.is_speaking = fake_is_speaking;
    host.user_data = &g_fake;
    return host;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static int g_word_cb_calls = 0;
static uint32_t g_word_cb_last_start = 0;
static uint32_t g_word_cb_last_len = 0;

static void word_cb(uint32_t char_start, uint32_t char_len, void* ud) {
    (void)ud;
    g_word_cb_calls++;
    g_word_cb_last_start = char_start;
    g_word_cb_last_len = char_len;
}

static void test_no_host_returns_no_tts_host_error(void) {
    ethervox_tts_set_host(NULL);
    CHECK(!ethervox_tts_host_is_speaking());

    ethervox_result_t result = ethervox_tts_host_speak("hello", NULL, NULL);
    CHECK(result == ETHERVOX_ERROR_NO_TTS_HOST);

    // stop/pause/resume must not crash with no host registered.
    ethervox_tts_host_stop();
    ethervox_tts_host_pause();
    ethervox_tts_host_resume();
}

static void test_set_host_rejects_incomplete_host(void) {
    ethervox_tts_host_t incomplete = {0};
    incomplete.speak = fake_speak;
    // stop/pause/resume/is_speaking left NULL.
    ethervox_result_t result = ethervox_tts_set_host(&incomplete);
    CHECK(result == ETHERVOX_ERROR_INVALID_ARGUMENT);

    ethervox_tts_set_host(NULL);
}

static void test_speak_reaches_fake_host(void) {
    reset_fake();
    ethervox_tts_host_t host = make_fake_host();
    CHECK(ethervox_tts_set_host(&host) == ETHERVOX_SUCCESS);

    ethervox_tts_style_t style = {.voice_id = "test-voice", .rate = 1.5f, .pitch = 0.9f,
                                   .emotion = "calm", .intensity = 0.5f};
    g_word_cb_calls = 0;
    ethervox_result_t result = ethervox_tts_host_speak("hello world", &style, word_cb);
    CHECK(result == ETHERVOX_SUCCESS);
    CHECK(strcmp(g_fake.last_utf8, "hello world") == 0);
    CHECK(g_fake.last_style.rate == 1.5f);
    CHECK(strcmp(g_fake.last_style.emotion, "calm") == 0);

    ethervox_tts_set_host(NULL);
}

static void test_word_callback_fires(void) {
    reset_fake();
    ethervox_tts_host_t host = make_fake_host();
    CHECK(ethervox_tts_set_host(&host) == ETHERVOX_SUCCESS);

    g_word_cb_calls = 0;
    ethervox_result_t result = ethervox_tts_host_speak("hi", NULL, word_cb);
    CHECK(result == ETHERVOX_SUCCESS);
    CHECK(g_word_cb_calls == 1);
    CHECK(g_word_cb_last_start == 0);
    CHECK(g_word_cb_last_len == 2);

    ethervox_tts_set_host(NULL);
}

static void test_is_speaking_reflects_host_state(void) {
    reset_fake();
    ethervox_tts_host_t host = make_fake_host();
    CHECK(ethervox_tts_set_host(&host) == ETHERVOX_SUCCESS);

    CHECK(!ethervox_tts_host_is_speaking());
    ethervox_tts_host_speak("hello", NULL, NULL);
    CHECK(ethervox_tts_host_is_speaking());

    ethervox_tts_set_host(NULL);
}

static void test_stop_pause_resume_reach_fake_host(void) {
    reset_fake();
    ethervox_tts_host_t host = make_fake_host();
    CHECK(ethervox_tts_set_host(&host) == ETHERVOX_SUCCESS);

    ethervox_tts_host_speak("a long utterance", NULL, NULL);
    CHECK(ethervox_tts_host_is_speaking());

    ethervox_tts_host_pause();
    CHECK(g_fake.pause_calls == 1);
    CHECK(g_fake.paused);

    ethervox_tts_host_resume();
    CHECK(g_fake.resume_calls == 1);
    CHECK(!g_fake.paused);

    ethervox_tts_host_stop();
    CHECK(g_fake.stop_calls == 1);
    CHECK(!ethervox_tts_host_is_speaking());

    ethervox_tts_set_host(NULL);
}

static void test_clearing_host_returns_to_no_host_behaviour(void) {
    reset_fake();
    ethervox_tts_host_t host = make_fake_host();
    CHECK(ethervox_tts_set_host(&host) == ETHERVOX_SUCCESS);
    ethervox_tts_host_speak("hello", NULL, NULL);
    CHECK(ethervox_tts_host_is_speaking());

    CHECK(ethervox_tts_set_host(NULL) == ETHERVOX_SUCCESS);
    CHECK(!ethervox_tts_host_is_speaking());
    CHECK(ethervox_tts_host_speak("hello", NULL, NULL) == ETHERVOX_ERROR_NO_TTS_HOST);
}

int main(void) {
    RUN_TEST(test_no_host_returns_no_tts_host_error);
    RUN_TEST(test_set_host_rejects_incomplete_host);
    RUN_TEST(test_speak_reaches_fake_host);
    RUN_TEST(test_word_callback_fires);
    RUN_TEST(test_is_speaking_reflects_host_state);
    RUN_TEST(test_stop_pause_resume_reach_fake_host);
    RUN_TEST(test_clearing_host_returns_to_no_host_behaviour);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
