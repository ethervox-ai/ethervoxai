/**
 * @file voice_training.c
 * @brief Interactive voice training mode for pronunciation improvement
 *
 * LLM generates text, user speaks it, system learns correct pronunciation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/voice_training.h"
#include "ethervox/governor.h"
#include "ethervox/tts.h"
#include "ethervox/stt.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"
#include "ethervox/pronunciation_trainer.h"
#include "ethervox/audio_recording.h"
#include "../tts/phonemizer/phonemizer.h"
#include "../tts/phonemizer/pronunciation_overrides.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <signal.h>
#endif
#include <math.h>

#ifdef _WIN32
#include <direct.h>
#include <signal.h>  // For signal() on Windows
#endif

// Signal handler for Ctrl+C
#ifndef _WIN32
static volatile sig_atomic_t g_training_interrupted = 0;
#else
static volatile int g_training_interrupted = 0;
#endif

static void training_signal_handler(int sig) {
    (void)sig;  // Unused
    g_training_interrupted = 1;
    printf("\n\n⚠️  Training interrupted (Ctrl+C pressed)\n");
}

#define LOG_INFO(...) ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(...) ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

typedef struct {
    ethervox_governor_t* governor;
    phonemizer_t* phonemizer;
    void* tts;  // tts_context_t* (internal type)
    void* stt;  // stt_context_t* (internal type)
    pronunciation_override_store_t* overrides;
    char training_dir[512];
    int words_trained;
    int session_count;
} voice_training_session_t;

/**
 * Generate training phrase using LLM
 */
static char* generate_training_phrase(ethervox_governor_t* governor, const char* focus_area) {
    if (!governor) return NULL;
    
    char prompt[1024];
    snprintf(prompt, sizeof(prompt),
             "Generate a short, clear sentence (5-10 words) for pronunciation training. "
             "Focus on: %s. "
             "The sentence should be natural and contain common words that people often mispronounce. "
             "Respond with ONLY the sentence, no extra text.",
             focus_area ? focus_area : "general pronunciation");
    
    // Execute the governor query
    char* response = NULL;
    char* error = NULL;
    
    ethervox_governor_status_t status = ethervox_governor_execute(
        governor,
        prompt,
        &response,
        &error,
        NULL,  // metrics
        NULL,  // progress_callback
        NULL,  // token_callback
        NULL   // user_data
    );
    
    if (status != ETHERVOX_GOVERNOR_SUCCESS || !response) {
        LOG_ERROR("Failed to generate training phrase: %s", error ? error : "unknown error");
        free(error);
        free(response);
        return NULL;
    }
    
    free(error);
    
    // Strip common instruction prefixes that LLMs sometimes add
    char* cleaned = response;
    const char* prefixes[] = {
        "Practice saying: ",
        "Practice saying ",
        "Say: ",
        "Say ",
        "Repeat: ",
        "Repeat ",
        "Read: ",
        "Read ",
        NULL
    };
    
    for (int i = 0; prefixes[i] != NULL; i++) {
        if (strncmp(cleaned, prefixes[i], strlen(prefixes[i])) == 0) {
            cleaned += strlen(prefixes[i]);
            break;
        }
    }
    
    // Also strip quotes if the phrase is wrapped in them
    while (*cleaned == '"' || *cleaned == '\'' || *cleaned == ' ') {
        cleaned++;
    }
    size_t len = strlen(cleaned);
    while (len > 0 && (cleaned[len-1] == '"' || cleaned[len-1] == '\'' || cleaned[len-1] == ' ' || cleaned[len-1] == '\n')) {
        cleaned[len-1] = '\0';
        len--;
    }
    
    // Return a new copy of the cleaned string
    char* result = strdup(cleaned);
    free(response);
    return result;
}

/**
 * Trim silence from audio using simple energy-based VAD
 */
static int trim_silence(float* audio, int n_samples, int* start_idx, int* end_idx) {
    const float silence_threshold = 0.01f; // RMS threshold for silence
    const int frame_size = 160; // 10ms frames at 16kHz
    
    // Find start of speech
    *start_idx = 0;
    for (int i = 0; i < n_samples - frame_size; i += frame_size) {
        float rms = 0.0f;
        for (int j = 0; j < frame_size; j++) {
            rms += audio[i + j] * audio[i + j];
        }
        rms = sqrtf(rms / frame_size);
        
        if (rms > silence_threshold) {
            *start_idx = (i > frame_size) ? (i - frame_size) : 0; // Include one frame before
            break;
        }
    }
    
    // Find end of speech
    *end_idx = n_samples - 1;
    for (int i = n_samples - frame_size; i >= 0; i -= frame_size) {
        float rms = 0.0f;
        for (int j = 0; j < frame_size; j++) {
            rms += audio[i + j] * audio[i + j];
        }
        rms = sqrtf(rms / frame_size);
        
        if (rms > silence_threshold) {
            *end_idx = (i + frame_size * 2 < n_samples) ? (i + frame_size * 2) : n_samples - 1;
            break;
        }
    }
    
    return ETHERVOX_SUCCESS;
}

/**
 * Record user audio with silence trimming
 */
static int record_user_audio(const char* output_path, int duration_seconds) {
    // Record to temporary file first
    char temp_path[512];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", output_path);
    
    ethervox_result_t result = ethervox_audio_record_to_file(temp_path, duration_seconds, 16000, 1);
    if (result != 0) {
        return result;
    }
    
    // Read the recorded audio
    FILE* f = fopen(temp_path, "rb");
    if (!f) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Skip WAV header (44 bytes)
    fseek(f, 44, SEEK_SET);
    
    // Read audio data
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 44, SEEK_SET);
    
    int data_size = file_size - 44;
    int16_t* pcm_data = (int16_t*)malloc(data_size);
    fread(pcm_data, 1, data_size, f);
    fclose(f);
    
    int n_samples = data_size / sizeof(int16_t);
    float* float_data = (float*)malloc(n_samples * sizeof(float));
    
    // Convert to float
    for (int i = 0; i < n_samples; i++) {
        float_data[i] = pcm_data[i] / 32768.0f;
    }
    
    // Trim silence
    int start_idx, end_idx;
    trim_silence(float_data, n_samples, &start_idx, &end_idx);
    
    int trimmed_samples = end_idx - start_idx + 1;
    printf("  ✂️  Trimmed silence: %d samples → %d samples (%.1f%% kept)\n",
           n_samples, trimmed_samples, 100.0f * trimmed_samples / n_samples);
    
    // Write trimmed audio
    result = ethervox_audio_write_wav(output_path, 
                                      float_data + start_idx, 
                                      trimmed_samples, 
                                      16000, 1);
    
    free(float_data);
    free(pcm_data);
    remove(temp_path);
    
    return result;
}

/**
 * Train word pronunciation
 */
static int train_word_pronunciation(
    voice_training_session_t* session,
    const char* word,
    const char* audio_path
) {
    // Check phonemizer (required for pronunciation analysis)
    if (!session->phonemizer) {
        printf("  ⚠️  Phonemizer not available (recording saved but cannot train)\n");
        printf("  💡 This is needed for pronunciation analysis\n");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    pronunciation_training_config_t config = pronunciation_trainer_default_config();
    config.max_variants = 15;
    config.min_similarity = 0.15f;  // Lowered to account for silence padding in recordings
    config.verbose = true;  // Enable verbose to see what's failing
    
    pronunciation_training_result_t result = {0};  // Initialize to zero!
    ethervox_result_t ret = pronunciation_trainer_train(
        word,
        audio_path,
        session->phonemizer,
        session->tts,
        session->stt,
        &config,
        &result
    );
    
    if (ethervox_is_success(ret) && result.success) {
        printf("  [OK] Learned pronunciation: %s → %s (similarity: %.2f)\n",
               word, result.best_phonemes, result.similarity_score);
        
        // Save to override store
        pronunciation_override_store_t* store = 
            (pronunciation_override_store_t*)phonemizer_get_override_store(session->phonemizer);
        
        if (store) {
            pronunciation_override_t override = {0};
            strncpy(override.word, word, MAX_WORD_LENGTH - 1);
            
            // Use ARPABET for phonemes field if available, otherwise IPA
            if (result.best_arpabet) {
                strncpy(override.phonemes, result.best_arpabet, MAX_PHONEME_LENGTH - 1);
            } else if (result.best_phonemes) {
                strncpy(override.phonemes, result.best_phonemes, MAX_PHONEME_LENGTH - 1);
            }
            
            // IPA field
            if (result.best_phonemes) {
                strncpy(override.ipa, result.best_phonemes, MAX_IPA_LENGTH - 1);
            }
            
            override.confidence = result.similarity_score;
            override.usage_count = 1;
            override.trained_speaker_id = 0;  // Default speaker
            override.created = time(NULL);
            override.last_used = override.created;
            override.is_community = false;
            
            if (pronunciation_overrides_add(store, &override) == 0) {
                pronunciation_overrides_save(store);
                printf("    💾 Saved to pronunciation database\n");
            } else {
                printf("    ⚠️  Failed to save override\n");
            }
        }
        
        session->words_trained++;
        
        pronunciation_training_result_free(&result);
        return ETHERVOX_SUCCESS;
    } else {
        printf("  [FAIL] Training failed: %s\n",
               result.error_message ? result.error_message : "unknown error");
        pronunciation_training_result_free(&result);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
}

/**
 * Run interactive training session
 */
ethervox_result_t ethervox_voice_training_run(
    ethervox_governor_t* governor,
    phonemizer_t* phonemizer,
    void* tts,  // tts_context_t* (internal type)
    void* stt   // stt_context_t* (internal type)
) {
    if (!governor) {
        printf("❌ Governor not initialized\n");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Initialize session
    voice_training_session_t session = {0};
    session.governor = governor;
    session.phonemizer = phonemizer;
    session.tts = tts;
    session.stt = stt;
    session.words_trained = 0;
    session.session_count = 0;
    
    // Install signal handler for Ctrl+C
    g_training_interrupted = 0;
#ifndef _WIN32
    struct sigaction sa;
    sa.sa_handler = training_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    struct sigaction old_sa;
    sigaction(SIGINT, &sa, &old_sa);
#else
    signal(SIGINT, training_signal_handler);
#endif
    
    // Create training directory
    const char* home = getenv("HOME");
    if (home) {
        snprintf(session.training_dir, sizeof(session.training_dir),
                 "%s/.ethervox/voice_training", home);
#ifdef _WIN32
        _mkdir(session.training_dir);
#else
        mkdir(session.training_dir, 0755);
#endif
    }
    
    // Print welcome message
    printf("\n");
    printf("╭---------------------------------------------------------------╮\n");
    printf("|           🎙️  Voice Pronunciation Training Mode              |\n");
    printf("╰---------------------------------------------------------------╯\n");
    printf("\n");
    printf("This mode helps improve TTS pronunciation by learning from your voice.\n");
    printf("\n");
    printf("How it works:\n");
    printf("  1. LLM generates a training sentence\n");
    printf("  2. Read it aloud (system records)\n");
    printf("  3. System analyzes pronunciation\n");
    printf("  4. Learns corrections from your speech\n");
    printf("\n");
    printf("Commands:\n");
    printf("  [Enter]    Generate and practice next phrase\n");
    printf("  'word'     Train specific word\n");
    printf("  'stats'    Show training statistics\n");
    printf("  'quit'     Exit training mode\n");
    printf("  Ctrl+C     Interrupt and exit\n");
    printf("\n");
    printf("Press Enter to start...\n");
    
    char input[256];
    if (!fgets(input, sizeof(input), stdin)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check for initial command
    input[strcspn(input, "\n")] = 0;
    
    // Training loop
    bool running = true;
    bool single_word_mode = false;
    int phrases_practiced = 0;
    
    // Check if starting with 'word' command
    if (strcmp(input, "word") == 0) {
        single_word_mode = true;
    }
    
    while (running && !g_training_interrupted) {
        printf("\n========================================================\n");
        printf("Training Session #%d\n", ++session.session_count);
        printf("========================================================\n\n");
        
        // Generate training phrase or prompt for word
        char* phrase = NULL;
        
        if (single_word_mode) {
            // Single word mode: prompt for word
            printf("📝 Enter a word to train (or 'back' to return to phrase mode): ");
            char word_input[256];
            if (fgets(word_input, sizeof(word_input), stdin)) {
                word_input[strcspn(word_input, "\n")] = 0;
                
                if (strcmp(word_input, "back") == 0) {
                    single_word_mode = false;
                    continue;
                }
                
                if (strlen(word_input) > 0) {
                    phrase = strdup(word_input);
                }
            }
        } else {
            // Regular mode: generate phrase from LLM
            printf("🤖 Generating training phrase...\n");
            phrase = generate_training_phrase(governor, "common mispronunciations");
        }
        
        if (!phrase) {
            printf("❌ Failed to generate phrase\n");
            break;
        }
        
        printf("\n📝 Practice this phrase:\n");
        printf("   \"%s\"\n\n", phrase);
        
        // TODO: Synthesize and play reference audio (requires TTS file synthesis API)
        // For now, user will read and speak the phrase without reference
        (void)tts; // Suppress unused warning
        
        printf("\n🎤 Now you speak it!\n");
        printf("   Press Enter when ready to record...\n");
        
        if (!fgets(input, sizeof(input), stdin) || g_training_interrupted) {
            free(phrase);
            break;
        }
        
        // Check for commands
        input[strcspn(input, "\n")] = 0; // Remove newline
        
        if (strcmp(input, "quit") == 0 || strcmp(input, "q") == 0) {
            free(phrase);
            running = false;
            continue;
        }
        
        if (strcmp(input, "stats") == 0) {
            printf("\n📊 Training Statistics:\n");
            printf("   Sessions completed: %d\n", session.session_count - 1);
            printf("   Words trained: %d\n", session.words_trained);
            printf("   Phrases practiced: %d\n", phrases_practiced);
            free(phrase);
            continue;
        }
        
        if (strcmp(input, "word") == 0) {
            single_word_mode = true;
            printf("\n[OK] Switched to single-word training mode\n");
            free(phrase);
            continue;
        }
        
        printf("\n🔍 Analyzing pronunciation...\n");
        
        // Debug: show what we're parsing
        printf("  [DEBUG] Phrase to tokenize: '%s' (length: %zu)\n", phrase, strlen(phrase));
        
        // Extract words to train
        char* phrase_copy = strdup(phrase);
        char* words_list[32];  // Max 32 words per phrase
        int words_in_phrase = 0;
        
        char* word = strtok(phrase_copy, " ,.!?;:");
        while (word != NULL && words_in_phrase < 32) {
            if (strlen(word) > 2) { // Skip very short words
                words_list[words_in_phrase++] = strdup(word);
            }
            word = strtok(NULL, " ,.!?;:");
        }
        free(phrase_copy);
        
        if (words_in_phrase == 0) {
            printf("  ⚠️  No words to train in this phrase\n");
            free(phrase);
            continue;
        }
        
        printf("  Found %d words to train\n\n", words_in_phrase);
        
        // Train each word individually with separate recording
        int words_trained_this_session = 0;
        for (int i = 0; i < words_in_phrase; i++) {
            bool word_recorded_successfully = false;
            
            while (!word_recorded_successfully && !g_training_interrupted) {
                printf("--------------------------------------------------\n");
                printf("📝 Word %d/%d: '%s'\n", i+1, words_in_phrase, words_list[i]);
                if (i > 0) {
                    printf("🎤 Press Enter to record, or 'r' to repeat previous word: ");
                } else {
                    printf("🎤 Press Enter to record: ");
                }
                fflush(stdout);
                
                char input_buf[256];
                if (!fgets(input_buf, sizeof(input_buf), stdin) || g_training_interrupted) {
                    goto training_interrupted;
                }
                
                // Check if user wants to repeat previous word
                input_buf[strcspn(input_buf, "\n")] = 0;
                if (i > 0 && strcmp(input_buf, "r") == 0) {
                    i--; // Go back to previous word
                    printf("↩️  Repeating word %d: '%s'\n", i+1, words_list[i]);
                    continue;
                }
                
                // Record this word (duration based on word length)
                char user_audio[512];
                snprintf(user_audio, sizeof(user_audio),
                         "%s/user_%d_word%d.wav", session.training_dir, session.session_count, i);
                
                // Dynamic duration: short words = 1s, medium = 1.5s, long = 2s
                int word_len = strlen(words_list[i]);
                int duration = (word_len <= 4) ? 1 : (word_len <= 8) ? 2 : 2;
                
                printf("🔴 Recording... (%d second%s)\n", duration, duration > 1 ? "s" : "");
                if (record_user_audio(user_audio, duration) != 0) {
                    printf("❌ Recording failed - press Enter to try again, or 's' to skip: ");
                    fgets(input_buf, sizeof(input_buf), stdin);
                    input_buf[strcspn(input_buf, "\n")] = 0;
                    if (strcmp(input_buf, "s") == 0) {
                        word_recorded_successfully = true; // Skip this word
                        continue;
                    }
                    continue; // Retry recording
                }
                
                // Ask if recording was good
                printf("  ✅ Recording saved. Press Enter to train, or 'r' to re-record: ");
                fgets(input_buf, sizeof(input_buf), stdin);
                input_buf[strcspn(input_buf, "\n")] = 0;
                
                if (strcmp(input_buf, "r") == 0) {
                    printf("🔄 Re-recording...\n");
                    continue; // Retry this word
                }
                
                // Train the word
                printf("  Training: '%s'\n", words_list[i]);
                if (train_word_pronunciation(&session, words_list[i], user_audio) == 0) {
                    words_trained_this_session++;
                }
                word_recorded_successfully = true;
            }
        }
        
        training_interrupted:
        if (g_training_interrupted) {
            printf("\n⚠️  Training interrupted\n");
        }
        
        // Free word list
        for (int i = 0; i < words_in_phrase; i++) {
            free(words_list[i]);
        }
        
        free(phrase);
        phrases_practiced++;
        
        if (words_trained_this_session > 0) {
            printf("\n✨ Session complete! Successfully trained %d/%d words\n", 
                   words_trained_this_session, words_in_phrase);
        } else {
            printf("\n⚠️  Session complete. No words successfully trained (%d attempted)\n",
                   words_in_phrase);
            printf("   💡 Tip: Make sure voice conversation is active (/convon)\n");
        }
        printf("\nCommands: [Enter]=next, 'word'=single word mode, 'stats'=statistics, 'quit'=exit\n");
        printf("> ");
        
        if (!fgets(input, sizeof(input), stdin) || g_training_interrupted) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        if (strcmp(input, "quit") == 0 || strcmp(input, "q") == 0) {
            running = false;
        } else if (strcmp(input, "word") == 0) {
            single_word_mode = true;
            printf("\n[OK] Switched to single-word training mode\n");
        } else if (strcmp(input, "stats") == 0) {
            printf("\n📊 Training Statistics:\n");
            printf("   Sessions completed: %d\n", session.session_count);
            printf("   Words trained: %d\n", session.words_trained);
            printf("   Phrases practiced: %d\n", phrases_practiced);
        }
    }
    
    // Final statistics
    printf("\n");
    printf("╭---------------------------------------------------------------╮\n");
    printf("|                  Training Session Complete                    |\n");
    printf("╰---------------------------------------------------------------╯\n");
    printf("\n");
    printf("📊 Final Statistics:\n");
    printf("   Sessions completed: %d\n", session.session_count);
    printf("   Words trained: %d\n", session.words_trained);
    printf("   Phrases practiced: %d\n", phrases_practiced);
    printf("\n");
    printf("Training data saved to: %s\n", session.training_dir);
    printf("\n");
    
    // Restore original signal handler
#ifndef _WIN32
    sigaction(SIGINT, &old_sa, NULL);
#endif
    
    if (g_training_interrupted) {
        printf("Training interrupted by user.\n");
    }
    
    return ETHERVOX_SUCCESS;
}
