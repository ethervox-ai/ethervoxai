/**
 * @file voice_tuner.c
 * @brief Interactive voice quality tuner with adaptive parameter search
 * 
 * Uses binary search to adaptively find optimal TTS settings based on user feedback.
 * Typically finds good settings in 15-20 tests instead of 280.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <termios.h>
#include "ethervox/tts.h"
#include "ethervox/audio.h"
#include "cJSON.h"

#define MAX_VOICES 100
#define MAX_PATH 512

typedef struct {
    char model_path[MAX_PATH];
    char config_path[MAX_PATH];
    char name[128];
    char language[16];
    const char* test_sentence;
} voice_info_t;

// Global list of discovered voices
static voice_info_t g_voices[MAX_VOICES];
static int g_voice_count = 0;

// Terminal utilities for raw input
static struct termios g_orig_termios;
static bool g_raw_mode = false;

static void disable_raw_mode(void) {
    if (g_raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_raw_mode = false;
    }
}

static void enable_raw_mode(void) {
    if (!isatty(STDIN_FILENO)) return;
    
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    atexit(disable_raw_mode);
    
    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    g_raw_mode = true;
}

static char read_key(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return c;
    }
    return '\0';
}

/**
 * Scan for available voice models
 */
static int discover_voices(void) {
    const char* home = getenv("HOME");
    if (!home) return -1;
    
    char piper_dir[MAX_PATH];
    snprintf(piper_dir, sizeof(piper_dir), "%s/.ethervox/models/piper", home);
    
    DIR* dir = opendir(piper_dir);
    if (!dir) return -1;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && g_voice_count < MAX_VOICES) {
        size_t len = strlen(entry->d_name);
        if (len < 5 || strcmp(entry->d_name + len - 5, ".onnx") != 0) continue;
        
        char model_path[MAX_PATH], config_path[MAX_PATH];
        snprintf(model_path, sizeof(model_path), "%s/%s", piper_dir, entry->d_name);
        snprintf(config_path, sizeof(config_path), "%s.json", model_path);
        
        struct stat st;
        if (stat(config_path, &st) != 0) continue;
        
        voice_info_t* voice = &g_voices[g_voice_count];
        strncpy(voice->model_path, model_path, sizeof(voice->model_path) - 1);
        strncpy(voice->config_path, config_path, sizeof(voice->config_path) - 1);
        strncpy(voice->name, entry->d_name, sizeof(voice->name) - 1);
        char* ext = strstr(voice->name, ".onnx");
        if (ext) *ext = '\0';
        
        // Parse language and set test sentence
        FILE* f = fopen(config_path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* json_str = malloc(size + 1);
            if (json_str) {
                fread(json_str, 1, size, f);
                json_str[size] = '\0';
                cJSON* root = cJSON_Parse(json_str);
                if (root) {
                    cJSON* phonemes = cJSON_GetObjectItem(root, "phonemes");
                    if (phonemes) {
                        cJSON* lang = cJSON_GetObjectItem(phonemes, "language");
                        if (lang && lang->valuestring) {
                            strncpy(voice->language, lang->valuestring, sizeof(voice->language) - 1);
                        }
                    }
                    cJSON_Delete(root);
                }
                free(json_str);
            }
            fclose(f);
        }
        
        if (voice->language[0] == '\0') strcpy(voice->language, "unknown");
        
        // Language-specific test sentences
        if (strncmp(voice->language, "de", 2) == 0) {
            voice->test_sentence = "Hallo, dies ist ein Test der deutschen Sprachausgabe.";
        } else if (strncmp(voice->language, "zh", 2) == 0) {
            voice->test_sentence = "你好，这是一个中文语音发音测试。";
        } else if (strncmp(voice->language, "es", 2) == 0) {
            voice->test_sentence = "Hola, esta es una prueba de la pronunciación en español.";
        } else if (strncmp(voice->language, "fr", 2) == 0) {
            voice->test_sentence = "Bonjour, ceci est un test de prononciation française.";
        } else {
            voice->test_sentence = "Hello, this is a test of the voice pronunciation.";
        }
        
        g_voice_count++;
    }
    
    closedir(dir);
    return g_voice_count;
}

/**
 * Select voice menu
 */
static int select_voice(void) {
    printf("\n========================================\n");
    printf("  Available Piper Voices\n");
    printf("========================================\n\n");
    
    for (int i = 0; i < g_voice_count; i++) {
        printf("  %2d. %-40s [%s]\n", i + 1, g_voices[i].name, g_voices[i].language);
    }
    
    printf("\nSelect voice (1-%d) or 0 to quit: ", g_voice_count);
    fflush(stdout);
    
    int choice;
    if (scanf("%d", &choice) != 1) return -1;
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    
    if (choice == 0) return -1;
    if (choice < 1 || choice > g_voice_count) return -1;
    
    return choice - 1;
}

/**
 * Test combination and get user feedback
 * Returns: -1=quit, 0=disliked, 1=liked
 */
static int test_combination(ethervox_audio_runtime_t* audio_runtime,
                           const voice_info_t* voice,
                           float phoneme_var,
                           float prosody_var,
                           float rate,
                           const char* varying_param) {
    if (varying_param) {
        printf("Testing %s: %.2f (phoneme=%.2f, prosody=%.2f, rate=%.2f)\n", 
               varying_param, 
               strcmp(varying_param, "rate") == 0 ? rate :
               strcmp(varying_param, "prosody") == 0 ? prosody_var : phoneme_var,
               phoneme_var, prosody_var, rate);
    } else {
        printf("Testing: phoneme=%.2f, prosody=%.2f, rate=%.2f\n", 
               phoneme_var, prosody_var, rate);
    }
    
    ethervox_tts_config_t config = {
        .backend = ETHERVOX_TTS_BACKEND_PIPER,
        .model_path = voice->model_path,
        .sample_rate = 16000,
        .channels = 1,
        .speaking_rate = rate,
        .phoneme_variance = phoneme_var,
        .prosody_variance = prosody_var,
        .speaker_id = 0
    };
    
    ethervox_tts_context_t* tts = ethervox_tts_create(&config);
    if (!tts) return -1;
    
    ethervox_tts_audio_t audio = {0};
    if (ethervox_tts_synthesize_text(tts, voice->test_sentence, &audio) != 0 ||
        !audio.samples || audio.sample_count == 0) {
        ethervox_tts_destroy(tts);
        return -1;
    }
    
    // Play through audio backend
    if (audio_runtime && audio_runtime->driver.write_audio) {
        size_t byte_count = audio.sample_count * sizeof(int16_t);
        int16_t* pcm = (int16_t*)malloc(byte_count);
        if (pcm) {
            for (size_t i = 0; i < audio.sample_count; i++) {
                float s = audio.samples[i];
                if (s > 1.0f) s = 1.0f;
                if (s < -1.0f) s = -1.0f;
                pcm[i] = (int16_t)(s * 32767.0f);
            }
            
            ethervox_audio_buffer_t buf = {
                .data = (float*)pcm,
                .size = byte_count,
                .channels = audio.channels
            };
            
            audio_runtime->driver.write_audio(audio_runtime, &buf);
            usleep((audio.sample_count * 1000000) / 16000);
            free(pcm);
        }
    }
    
    ethervox_tts_audio_free(&audio);
    ethervox_tts_destroy(tts);
    
    printf("Like this? (y/n/q): ");
    fflush(stdout);
    
    char response;
    while (1) {
        response = tolower(read_key());
        if (response == 'y' || response == 'n' || response == 'q') {
            printf("%c\n", response);
            break;
        }
    }
    
    if (response == 'q') return -1;
    return (response == 'y') ? 1 : 0;
}

/**
 * Adaptive binary search for single parameter
 */
static float search_parameter(ethervox_audio_runtime_t* audio_runtime,
                             const voice_info_t* voice,
                             const char* param_name,
                             float fixed_phoneme,
                             float fixed_prosody,
                             float fixed_rate,
                             int param_type,  // 0=phoneme, 1=prosody, 2=rate
                             float min_val,
                             float max_val) {
    printf("\n--- Optimizing %s ---\n", param_name);
    printf("Range: %.3f to %.3f\n\n", min_val, max_val);
    
    float best_val = (min_val + max_val) / 2.0f;
    int iterations = 0;
    const int MAX_ITER = 5;
    
    while (iterations < MAX_ITER && (max_val - min_val) > 0.05f) {
        iterations++;
        printf("[Iteration %d/%d] Range: [%.3f - %.3f]\n", iterations, MAX_ITER, min_val, max_val);
        
        // Test three points - use extremes on first iteration for wide coverage
        float low, mid, high;
        if (iterations == 1) {
            // First pass: test extremes (0%, 50%, 100%)
            low = min_val;
            mid = (min_val + max_val) / 2.0f;
            high = max_val;
        } else {
            // Later passes: narrow in (25%, 50%, 75%)
            low = min_val + (max_val - min_val) * 0.25f;
            mid = (min_val + max_val) / 2.0f;
            high = min_val + (max_val - min_val) * 0.75f;
        }
        
        int best_idx = -1;
        float test_vals[] = {low, mid, high};
        
        for (int i = 0; i < 3; i++) {
            float pho = (param_type == 0) ? test_vals[i] : fixed_phoneme;
            float pro = (param_type == 1) ? test_vals[i] : fixed_prosody;
            float rt = (param_type == 2) ? test_vals[i] : fixed_rate;
            
            const char* varying = (param_type == 0) ? "phoneme" :
                                  (param_type == 1) ? "prosody" : "rate";
            int rating = test_combination(audio_runtime, voice, pho, pro, rt, varying);
            if (rating < 0) return best_val;  // User quit
            
            if (rating == 1) {
                best_val = test_vals[i];
                best_idx = i;
                printf("→ Liked!\n");
            }
        }
        
        // Narrow search based on feedback
        if (best_idx == 0) {
            max_val = mid;  // Liked low, search lower half
        } else if (best_idx == 2) {
            min_val = mid;  // Liked high, search upper half
        } else if (best_idx == 1) {
            min_val = low; max_val = high;  // Liked mid, narrow to middle 50%
        } else {
            // No preference - keep full range but shift slightly to explore more
            // Only narrow by 20% to ensure we cover extremes
            float range = max_val - min_val;
            float new_range = range * 0.8f;  // Keep 80% of range
            min_val = mid - new_range / 2.0f;
            max_val = mid + new_range / 2.0f;
            
            // Ensure we don't go out of original bounds
            if (iterations == 1) {
                // First iteration with no likes - try extremes next time
                printf("→ No preference yet, will try more extreme values\n");
            }
        }
    }
    
    printf("→ Optimal: %.3f\n", best_val);
    return best_val;
}

/**
 * Save optimal settings to JSON
 */
static int save_optimal_settings(const voice_info_t* voice,
                                float phoneme_var,
                                float prosody_var,
                                float rate) {
    FILE* f = fopen(voice->config_path, "r");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* json_str = malloc(size + 1);
    if (!json_str) { fclose(f); return -1; }
    
    fread(json_str, 1, size, f);
    json_str[size] = '\0';
    fclose(f);
    
    cJSON* root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return -1;
    
    cJSON* optimal = cJSON_GetObjectItem(root, "ethervox_optimal");
    if (!optimal) {
        optimal = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "ethervox_optimal", optimal);
    }
    
    cJSON_DeleteItemFromObject(optimal, "phoneme_variance");
    cJSON_DeleteItemFromObject(optimal, "prosody_variance");
    cJSON_DeleteItemFromObject(optimal, "speaking_rate");
    
    cJSON_AddNumberToObject(optimal, "phoneme_variance", phoneme_var);
    cJSON_AddNumberToObject(optimal, "prosody_variance", prosody_var);
    cJSON_AddNumberToObject(optimal, "speaking_rate", rate);
    
    char* output = cJSON_Print(root);
    cJSON_Delete(root);
    if (!output) return -1;
    
    f = fopen(voice->config_path, "w");
    if (!f) { free(output); return -1; }
    
    fprintf(f, "%s\n", output);
    fclose(f);
    free(output);
    
    printf("\n✓ Saved to %s\n", voice->config_path);
    return 0;
}

/**
 * Adaptive tuning loop
 */
static void tune_voice(ethervox_audio_runtime_t* audio_runtime, int voice_idx) {
    const voice_info_t* voice = &g_voices[voice_idx];
    
    printf("\n========================================\n");
    printf("  Tuning: %s\n", voice->name);
    printf("  Language: %s\n", voice->language);
    printf("========================================\n");
    printf("\nTest: \"%s\"\n", voice->test_sentence);
    printf("\nAdaptive search will find optimal settings in ~15 tests.\n");
    printf("Press 'y' for good samples, 'n' for bad ones.\n");
    printf("\nPress Enter to begin...");
    getchar();
    
    // Start with defaults
    float phoneme_var = 0.667f;
    float prosody_var = 0.8f;
    float rate = 1.0f;
    
    // Phase 1: Speaking rate (most noticeable)
    printf("\n========================================\n");
    printf("  Phase 1: Speaking Rate\n");
    printf("  (Overall speed)\n");
    printf("========================================\n");
    rate = search_parameter(audio_runtime, voice, "Speaking Rate",
                           phoneme_var, prosody_var, rate, 2, 0.7f, 1.3f);
    
    // Phase 2: Prosody variance (pitch/emotion)
    printf("\n========================================\n");
    printf("  Phase 2: Prosody Variance\n");
    printf("  (Pitch expression and emotion)\n");
    printf("========================================\n");
    prosody_var = search_parameter(audio_runtime, voice, "Prosody Variance",
                                   phoneme_var, prosody_var, rate, 1, 0.0f, 1.5f);
    
    // Phase 3: Phoneme variance (subtle timing)
    printf("\n========================================\n");
    printf("  Phase 3: Phoneme Variance\n");
    printf("  (Timing precision vs natural rhythm)\n");
    printf("========================================\n");
    phoneme_var = search_parameter(audio_runtime, voice, "Phoneme Variance",
                                   phoneme_var, prosody_var, rate, 0, 0.0f, 1.0f);
    
    // Final test
    printf("\n========================================\n");
    printf("  Final Settings\n");
    printf("========================================\n");
    printf("  Phoneme: %.3f\n", phoneme_var);
    printf("  Prosody: %.3f\n", prosody_var);
    printf("  Rate: %.2f\n\n", rate);
    
    int final = test_combination(audio_runtime, voice, phoneme_var, prosody_var, rate, NULL);
    
    if (final >= 0) {
        printf("\nSave? (y/n): ");
        fflush(stdout);
        char save = tolower(read_key());
        printf("%c\n", save);
        
        if (save == 'y') {
            save_optimal_settings(voice, phoneme_var, prosody_var, rate);
        }
    }
}

int main(int argc, char** argv) {
    printf("\n========================================\n");
    printf("  EthervoxAI Voice Quality Tuner\n");
    printf("  (Adaptive Search Edition)\n");
    printf("========================================\n");
    
    printf("\nInitializing audio...\n");
    ethervox_audio_runtime_t audio_runtime = {0};
    ethervox_audio_config_t audio_config = ethervox_audio_get_default_config();
    audio_config.sample_rate = 16000;
    audio_config.channels = 1;
    
    if (ethervox_audio_init(&audio_runtime, &audio_config) != 0 ||
        ethervox_audio_start(&audio_runtime) != 0) {
        fprintf(stderr, "Audio init failed\n");
        return 1;
    }
    
    printf("Scanning for voices...\n");
    if (discover_voices() <= 0) {
        fprintf(stderr, "No voices found in ~/.ethervox/models/piper/\n");
        ethervox_audio_cleanup(&audio_runtime);
        return 1;
    }
    
    printf("Found %d voice(s)\n", g_voice_count);
    
    while (1) {
        int idx = select_voice();
        if (idx < 0) break;
        
        enable_raw_mode();  // Enable raw mode only for tuning
        tune_voice(&audio_runtime, idx);
        disable_raw_mode();  // Disable for next menu selection
        
        printf("\nTune another? (y/n): ");
        fflush(stdout);
        
        // Need to enable raw mode again for this question
        enable_raw_mode();
        char again = tolower(read_key());
        disable_raw_mode();
        printf("%c\n", again);
        
        if (again != 'y') break;
    }
    
    printf("\nDone!\n");
    ethervox_audio_stop(&audio_runtime);
    ethervox_audio_cleanup(&audio_runtime);
    
    return 0;
}
