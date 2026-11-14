/**
 * @file test_c36b_threading_and_context.c
 * @brief Live-model verification for TASK-C3.6b's two remaining real acceptance
 *        criteria: the conversation context is byte-identical before and after
 *        an ASR pass, and concurrent chat generation + ASR decode on the
 *        Governor's unified context (Mode 1) does not crash or corrupt state
 *        now that ctx_mutex serializes them.
 *
 * Loads a real Governor with audio support attached
 * (ethervox_governor_load_model_with_audio, Mode 1's unified voice model
 * architecture), populates a real multi-turn conversation via
 * ethervox_governor_execute(), snapshots the KV cache position and decoded
 * contents, runs a real transcription via ethervox_governor_transcribe_audio()
 * (seq 2), then verifies the conversation snapshot (seq 0) is byte-for-byte
 * unchanged - the exact regression this task's fragility warning exists to
 * catch. Then repeats both operations concurrently from separate threads to
 * exercise ctx_mutex under real contention.
 *
 * Usage: test_c36b_threading_and_context <model.gguf> <mmproj.gguf> <speech.wav>
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/governor.h"
#include "ethervox/platform_thread.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Shared with test_c36b_shared_governor_stt.c's own copy - kept duplicated
// rather than factored out, matching this test directory's existing
// convention (test_streaming_transcription_live.c has its own copy too).
static float* load_wav_mono16_as_float(const char* path, uint32_t* out_sample_count) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return NULL;
    }
    char riff[4], wave[4];
    uint32_t riff_size;
    if (fread(riff, 1, 4, f) != 4 || fread(&riff_size, 4, 1, f) != 1 || fread(wave, 1, 4, f) != 4 ||
        memcmp(riff, "RIFF", 4) != 0 || memcmp(wave, "WAVE", 4) != 0) {
        fprintf(stderr, "%s is not a RIFF/WAVE file\n", path);
        fclose(f);
        return NULL;
    }
    uint16_t bits_per_sample = 0, channels = 0;
    uint32_t sample_rate = 0;
    int16_t* pcm = NULL;
    uint32_t pcm_samples = 0;
    while (!feof(f)) {
        char chunk_id[4];
        uint32_t chunk_size;
        if (fread(chunk_id, 1, 4, f) != 4 || fread(&chunk_size, 4, 1, f) != 1) break;
        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            uint16_t audio_format;
            fread(&audio_format, 2, 1, f);
            fread(&channels, 2, 1, f);
            fread(&sample_rate, 4, 1, f);
            fseek(f, 6, SEEK_CUR);
            fread(&bits_per_sample, 2, 1, f);
            long remaining = (long)chunk_size - 16;
            if (remaining > 0) fseek(f, remaining, SEEK_CUR);
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            pcm_samples = chunk_size / sizeof(int16_t);
            pcm = (int16_t*)malloc(chunk_size);
            if (!pcm || fread(pcm, 1, chunk_size, f) != chunk_size) {
                fprintf(stderr, "Failed to read data chunk of %s\n", path);
                free(pcm);
                fclose(f);
                return NULL;
            }
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
    }
    fclose(f);
    if (!pcm || bits_per_sample != 16 || channels != 1 || sample_rate != 16000) {
        fprintf(stderr, "%s: expected 16-bit mono 16kHz WAV (got %u-bit, %u ch, %u Hz)\n",
                path, bits_per_sample, channels, sample_rate);
        free(pcm);
        return NULL;
    }
    float* out = (float*)malloc(pcm_samples * sizeof(float));
    for (uint32_t i = 0; i < pcm_samples; i++) {
        out[i] = (float)pcm[i] / 32768.0f;
    }
    free(pcm);
    *out_sample_count = pcm_samples;
    return out;
}

typedef struct {
    ethervox_governor_t* governor;
    const float* samples;
    uint32_t n_samples;
    int transcribe_calls_ok;
} transcribe_thread_arg_t;

#ifdef _WIN32
static DWORD WINAPI transcribe_worker(LPVOID arg_ptr) {
#else
static void* transcribe_worker(void* arg_ptr) {
#endif
    transcribe_thread_arg_t* arg = (transcribe_thread_arg_t*)arg_ptr;
    for (int i = 0; i < 5; i++) {
        char* text = NULL;
        ethervox_result_t r = ethervox_governor_transcribe_audio(arg->governor, arg->samples,
                                                                  arg->n_samples, &text);
        if (ethervox_is_success(r) && text && strlen(text) > 0) {
            arg->transcribe_calls_ok++;
        }
        free(text);
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <model.gguf> <mmproj.gguf> <speech.wav>\n", argv[0]);
        fprintf(stderr, "Skipping (no real model/audio supplied) - not a ctest failure.\n");
        return 0;
    }
    const char* model_path = argv[1];
    const char* mmproj_path = argv[2];
    const char* wav_path = argv[3];

    uint32_t n_samples = 0;
    float* samples = load_wav_mono16_as_float(wav_path, &n_samples);
    if (!samples) {
        fprintf(stderr, "FAIL: could not load %s\n", wav_path);
        return 1;
    }

    ethervox_tool_registry_t registry;
    if (ethervox_tool_registry_init(&registry, 8) != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: tool registry init\n");
        free(samples);
        return 1;
    }

    ethervox_governor_config_t config = ethervox_governor_default_config();
    // MINIMAL mode's shorter system prompt caused an immediate EOG (empty
    // response) against this specific model/mmproj pair - FULL (the default)
    // is what was previously verified to produce coherent text output from
    // this exact GGUF (see docs/UNIFIED_VOICE_MODEL_ARCHITECTURE.md's own
    // note that the model, loaded without its mmproj and prompted with
    // text, produces coherent output). Chat-answer *quality* against this
    // model is C3.6c's job, not this test's - this only needs *some* real
    // tokens to land in the KV cache to make the byte-identical check below
    // meaningful.
    config.max_iterations = 2;
    config.max_tokens_per_response = 32;  // keep generation short - this test verifies context
                                          // integrity and concurrency, not answer quality

    ethervox_governor_t* governor = NULL;
    if (ethervox_governor_init(&governor, &config, &registry) != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: governor init\n");
        ethervox_tool_registry_cleanup(&registry);
        free(samples);
        return 1;
    }

    ethervox_result_t load_result = ethervox_governor_load_model_with_audio(
        governor, model_path, mmproj_path, NULL, NULL, NULL, NULL);
    if (ethervox_is_error(load_result)) {
        fprintf(stderr, "FAIL: load_model_with_audio: %d\n", load_result);
        ethervox_governor_cleanup(governor);
        ethervox_tool_registry_cleanup(&registry);
        free(samples);
        return 1;
    }
    if (!ethervox_governor_has_audio_support(governor)) {
        fprintf(stderr, "FAIL: governor reports no audio support after load_model_with_audio\n");
        ethervox_governor_cleanup(governor);
        ethervox_tool_registry_cleanup(&registry);
        free(samples);
        return 1;
    }

    // --- Populate a real conversation (seq 0) before touching ASR (seq 2) ---
    // Success here is measured by real prompt tokens landing in the KV cache
    // (kv_pos advancing past the system-prompt-only baseline), not by
    // response text quality: this exact GGUF was independently confirmed (via
    // a throwaway differential test, with and without mmproj attached) to
    // answer plain chat prompts with an immediate EOG/empty response even
    // with no audio support attached at all - a pre-existing model/prompt
    // quality question for C3.6c ("text generation quality... decides
    // whether the governor can be retired"), not a regression from anything
    // in this packet's scope.
    int32_t kv_pos_baseline = ethervox_governor_get_kv_pos(governor);
    char* response = NULL;
    char* error = NULL;
    ethervox_governor_status_t status = ethervox_governor_execute(
        governor, "Reply with just the word hello.", NULL, &response, &error, NULL, NULL, NULL,
        NULL, NULL);
    int32_t kv_pos_populated = ethervox_governor_get_kv_pos(governor);
    bool populate_ok = (status == ETHERVOX_GOVERNOR_SUCCESS && kv_pos_populated > kv_pos_baseline);
    printf("%s: populate conversation via ethervox_governor_execute (kv_pos %d -> %d, response=\"%s\")\n",
           populate_ok ? "PASS" : "FAIL", kv_pos_baseline, kv_pos_populated, response ? response : "(null)");
    free(response);
    free(error);
    if (!populate_ok) {
        ethervox_governor_cleanup(governor);
        ethervox_tool_registry_cleanup(&registry);
        free(samples);
        return 1;
    }

    // --- Snapshot the conversation before an ASR pass ---
    int32_t kv_pos_before = ethervox_governor_get_kv_pos(governor);
    char* contents_before = ethervox_governor_get_kv_cache_contents(governor);

    // --- Real ASR pass on seq 2 ---
    char* transcript = NULL;
    ethervox_result_t transcribe_result =
        ethervox_governor_transcribe_audio(governor, samples, n_samples, &transcript);
    bool transcribe_ok = ethervox_is_success(transcribe_result) && transcript && strlen(transcript) > 0;
    printf("%s: real transcription via governor's unified context (transcript=\"%s\")\n",
           transcribe_ok ? "PASS" : "FAIL", transcript ? transcript : "(null)");
    free(transcript);

    // --- The acceptance criterion this test exists for ---
    int32_t kv_pos_after = ethervox_governor_get_kv_pos(governor);
    char* contents_after = ethervox_governor_get_kv_cache_contents(governor);
    bool byte_identical = (kv_pos_before == kv_pos_after) &&
                           ((contents_before == NULL) == (contents_after == NULL)) &&
                           (!contents_before || strcmp(contents_before, contents_after) == 0);
    printf("%s: conversation (seq 0) byte-identical before/after ASR pass (kv_pos %d -> %d)\n",
           byte_identical ? "PASS" : "FAIL", kv_pos_before, kv_pos_after);
    free(contents_before);
    free(contents_after);

    // --- Concurrency: chat generation and ASR decode on the SAME governor
    //     context from two threads at once, now that ctx_mutex serializes
    //     them. Success here means no crash/hang/corruption; llama.cpp gives
    //     no way to assert absence of a data race directly, so - as with the
    //     ev-llm pool's own concurrency stress test - a clean, correct-looking
    //     completion under real contention is the available proof.
    transcribe_thread_arg_t thread_arg = {governor, samples, n_samples, 0};
    ethervox_thread_t worker;
    bool spawn_ok = ethervox_is_success(
        ethervox_thread_create(&worker, transcribe_worker, &thread_arg));

    int execute_calls_ok = 0;
    for (int i = 0; i < 3; i++) {
        char* r2 = NULL;
        char* e2 = NULL;
        // Reading kv_pos here would itself race against the concurrently-running
        // worker thread's transcribe_audio() calls, so this only checks that the
        // call completed successfully (structurally, no crash/hang) - not a
        // per-call KV delta. The sequential byte-identical check above already
        // covers state-correctness; this section exists purely to exercise
        // ctx_mutex under real contention between two threads.
        ethervox_governor_status_t s2 = ethervox_governor_execute(
            governor, "Reply with just the word hi.", NULL, &r2, &e2, NULL, NULL, NULL, NULL,
            NULL);
        if (s2 == ETHERVOX_GOVERNOR_SUCCESS) {
            execute_calls_ok++;
        }
        free(r2);
        free(e2);
    }

    if (spawn_ok) {
        ethervox_thread_join(worker);
    }

    bool concurrency_ok = spawn_ok && thread_arg.transcribe_calls_ok == 5 && execute_calls_ok == 3;
    printf("%s: concurrent execute() (%d/3) + transcribe_audio() (%d/5) on shared context, no crash\n",
           concurrency_ok ? "PASS" : "FAIL", execute_calls_ok, thread_arg.transcribe_calls_ok);

    ethervox_governor_cleanup(governor);
    ethervox_tool_registry_cleanup(&registry);
    free(samples);

    return (populate_ok && transcribe_ok && byte_identical && concurrency_ok) ? 0 : 1;
}
