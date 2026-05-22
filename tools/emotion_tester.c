/**
 * @file emotion_tester.c
 * @brief Interactive emotion/speaker tester for multi-speaker Piper models
 * 
 * Allows testing different speaker IDs to find optimal emotional voices.
 * Useful for calibrating emotion → speaker_id mappings for the speak tool.
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
#define MAX_SPEAKERS 1024

typedef struct {
    char model_path[MAX_PATH];
    char config_path[MAX_PATH];
    char name[128];
    char language[16];
    int num_speakers;
    bool is_multispeaker;
    const char* test_sentence;
} voice_info_t;

typedef struct {
    const char* name;
    int speaker_id;
    const char* description;
} emotion_preset_t;

// Emotion presets for LibriTTS-R models (calibrated for expressive speech)
static const emotion_preset_t g_emotion_presets[] = {
    {"neutral",       0,   "Neutral, balanced tone"},
    {"happy",         45,  "Bright, cheerful, enthusiastic"},
    {"sad",           78,  "Lower pitch, slower, somber"},
    {"calm",          23,  "Gentle, measured, soothing"},
    {"excited",       101, "Fast-paced, dynamic, energetic"},
    {"professional",  12,  "Clear, authoritative, formal"},
    {"friendly",      67,  "Warm, approachable, conversational"},
    {"serious",       34,  "Deep, deliberate, grave"},
    {"enthusiastic",  89,  "Very energetic, expressive"},
    {"somber",        156, "Reflective, quiet, subdued"},
    {"soothing",      203, "Very calm, relaxing, peaceful"},
    {"energetic",     278, "High energy, fast, dynamic"},
    {"formal",        345, "Very professional, distant"},
    {"warm",          412, "Very friendly, inviting"},
    {"grave",         489, "Very serious, heavy, dark"}
};
static const int g_num_presets = sizeof(g_emotion_presets) / sizeof(emotion_preset_t);

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
 * Parse number of speakers from model config
 */
static int get_speaker_count(const char* config_path) {
    FILE* f = fopen(config_path, "r");
    if (!f) return 1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* json_str = malloc(size + 1);
    if (!json_str) { fclose(f); return 1; }
    
    fread(json_str, 1, size, f);
    json_str[size] = '\0';
    fclose(f);
    
    int num_speakers = 1;
    cJSON* root = cJSON_Parse(json_str);
    free(json_str);
    
    if (root) {
        cJSON* num_speakers_item = cJSON_GetObjectItem(root, "num_speakers");
        if (num_speakers_item && cJSON_IsNumber(num_speakers_item)) {
            num_speakers = num_speakers_item->valueint;
        } else {
            // Check if model has speaker embeddings
            cJSON* inference = cJSON_GetObjectItem(root, "inference");
            if (inference) {
                cJSON* speaker_id_map = cJSON_GetObjectItem(inference, "speaker_id_map");
                if (speaker_id_map && cJSON_IsObject(speaker_id_map)) {
                    num_speakers = cJSON_GetArraySize(speaker_id_map);
                }
            }
        }
        cJSON_Delete(root);
    }
    
    return num_speakers;
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
        
        // Parse language and speaker count
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
        
        voice->num_speakers = get_speaker_count(config_path);
        voice->is_multispeaker = (voice->num_speakers > 1);
        
        // Language-specific test sentences
        if (strncmp(voice->language, "de", 2) == 0) {
            voice->test_sentence = "Hallo, dies ist ein Test der emotionalen Sprachausgabe.";
        } else if (strncmp(voice->language, "zh", 2) == 0) {
            voice->test_sentence = "你好，这是一个情感语音测试。";
        } else if (strncmp(voice->language, "es", 2) == 0) {
            voice->test_sentence = "Hola, esta es una prueba de voz emocional.";
        } else if (strncmp(voice->language, "fr", 2) == 0) {
            voice->test_sentence = "Bonjour, ceci est un test de voix émotionnelle.";
        } else {
            voice->test_sentence = "Hello, this is a test of emotional voice synthesis.";
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
        const char* marker = g_voices[i].is_multispeaker ? "🎭" : "  ";
        printf("%s %2d. %-35s [%s] (%d speaker%s)\n", 
               marker, i + 1, g_voices[i].name, g_voices[i].language,
               g_voices[i].num_speakers,
               g_voices[i].num_speakers == 1 ? "" : "s");
    }
    
    printf("\n🎭 = Multi-speaker/emotional model\n");
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
 * Test speaker with specific configuration
 */
static int test_speaker(ethervox_audio_runtime_t* audio_runtime,
                       const voice_info_t* voice,
                       int speaker_id,
                       const char* emotion_name,
                       const char* custom_text) {
    const char* text = custom_text ? custom_text : voice->test_sentence;
    
    if (emotion_name) {
        printf("\nTesting: %s (speaker_id=%d)\n", emotion_name, speaker_id);
    } else {
        printf("\nTesting: speaker_id=%d\n", speaker_id);
    }
    printf("Text: \"%s\"\n", text);
    
    ethervox_tts_config_t config = {
        .backend = ETHERVOX_TTS_BACKEND_PIPER,
        .model_path = voice->model_path,
        .sample_rate = 16000,
        .channels = 1,
        .speaking_rate = 1.0f,
        .phoneme_variance = 0.667f,
        .prosody_variance = 0.8f,
        .speaker_id = speaker_id
    };
    
    ethervox_tts_context_t* tts = ethervox_tts_create(&config);
    if (!tts) {
        fprintf(stderr, "Failed to create TTS context\n");
        return -1;
    }
    
    ethervox_tts_audio_t audio = {0};
    if (ethervox_tts_synthesize_text(tts, text, &audio) != 0 ||
        !audio.samples || audio.sample_count == 0) {
        fprintf(stderr, "Failed to synthesize speech\n");
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
    
    return 0;
}

/**
 * Test emotion presets
 */
static void test_emotion_presets(ethervox_audio_runtime_t* audio_runtime,
                                const voice_info_t* voice) {
    printf("\n========================================\n");
    printf("  Emotion Preset Testing\n");
    printf("  Model: %s\n", voice->name);
    printf("========================================\n\n");
    
    if (!voice->is_multispeaker) {
        printf("⚠️  This is a single-speaker model.\n");
        printf("Emotion testing works best with multi-speaker models like:\n");
        printf("  - en_US-libritts_r-medium (904 speakers)\n");
        printf("  - en_US-libritts-high (904 speakers)\n");
        printf("  - de_DE-thorsten_emotional-medium\n\n");
        printf("Continue anyway? (y/n): ");
        fflush(stdout);
        
        char response = tolower(getchar());
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        
        if (response != 'y') return;
    }
    
    printf("\nAvailable emotion presets:\n\n");
    for (int i = 0; i < g_num_presets; i++) {
        printf("  %2d. %-15s (ID=%3d) - %s\n", 
               i + 1,
               g_emotion_presets[i].name,
               g_emotion_presets[i].speaker_id,
               g_emotion_presets[i].description);
    }
    
    printf("\nOptions:\n");
    printf("  1-%d: Test specific emotion\n", g_num_presets);
    printf("  a: Test all presets in sequence\n");
    printf("  c: Compare two emotions side-by-side\n");
    printf("  r: Enter custom speaker ID range\n");
    printf("  t: Enter custom text\n");
    printf("  q: Back to main menu\n\n");
    
    char custom_text[512] = {0};
    bool use_custom_text = false;
    
    enable_raw_mode();
    
    while (1) {
        printf("Choice: ");
        fflush(stdout);
        
        char choice = tolower(read_key());
        printf("%c\n", choice);
        
        if (choice == 'q') {
            break;
        } else if (choice == 'a') {
            // Test all presets
            printf("\nTesting all emotion presets...\n");
            printf("(Press Ctrl+C to skip)\n\n");
            
            for (int i = 0; i < g_num_presets; i++) {
                if (g_emotion_presets[i].speaker_id >= voice->num_speakers) {
                    printf("⊗ %s: speaker_id=%d exceeds model range (max=%d)\n",
                           g_emotion_presets[i].name,
                           g_emotion_presets[i].speaker_id,
                           voice->num_speakers - 1);
                    continue;
                }
                
                test_speaker(audio_runtime, voice,
                           g_emotion_presets[i].speaker_id,
                           g_emotion_presets[i].name,
                           use_custom_text ? custom_text : NULL);
                
                usleep(500000);  // 500ms pause between tests
            }
            
        } else if (choice == 'c') {
            // Compare two emotions
            disable_raw_mode();
            
            int first, second;
            printf("First emotion (1-%d): ", g_num_presets);
            if (scanf("%d", &first) != 1 || first < 1 || first > g_num_presets) {
                int c;
                while ((c = getchar()) != '\n' && c != EOF);
                enable_raw_mode();
                continue;
            }
            
            printf("Second emotion (1-%d): ", g_num_presets);
            if (scanf("%d", &second) != 1 || second < 1 || second > g_num_presets) {
                int c;
                while ((c = getchar()) != '\n' && c != EOF);
                enable_raw_mode();
                continue;
            }
            
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            
            printf("\n--- Comparison ---\n");
            test_speaker(audio_runtime, voice,
                       g_emotion_presets[first - 1].speaker_id,
                       g_emotion_presets[first - 1].name,
                       use_custom_text ? custom_text : NULL);
            
            printf("\nPress any key for second sample...");
            enable_raw_mode();
            read_key();
            printf("\n");
            
            test_speaker(audio_runtime, voice,
                       g_emotion_presets[second - 1].speaker_id,
                       g_emotion_presets[second - 1].name,
                       use_custom_text ? custom_text : NULL);
            
        } else if (choice == 'r') {
            // Test custom speaker ID range
            disable_raw_mode();
            
            int start_id, end_id;
            printf("Start speaker ID (0-%d): ", voice->num_speakers - 1);
            if (scanf("%d", &start_id) != 1) {
                int c;
                while ((c = getchar()) != '\n' && c != EOF);
                enable_raw_mode();
                continue;
            }
            
            printf("End speaker ID (%d-%d): ", start_id, voice->num_speakers - 1);
            if (scanf("%d", &end_id) != 1) {
                int c;
                while ((c = getchar()) != '\n' && c != EOF);
                enable_raw_mode();
                continue;
            }
            
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            
            if (start_id < 0) start_id = 0;
            if (end_id >= voice->num_speakers) end_id = voice->num_speakers - 1;
            
            printf("\nTesting speaker IDs %d to %d...\n", start_id, end_id);
            printf("(Press Ctrl+C to skip)\n\n");
            
            enable_raw_mode();
            
            for (int id = start_id; id <= end_id; id++) {
                test_speaker(audio_runtime, voice, id, NULL,
                           use_custom_text ? custom_text : NULL);
                usleep(300000);  // 300ms pause
            }
            
        } else if (choice == 't') {
            // Enter custom text
            disable_raw_mode();
            
            printf("Enter custom text (or press Enter to use default): ");
            if (fgets(custom_text, sizeof(custom_text), stdin)) {
                char* newline = strchr(custom_text, '\n');
                if (newline) *newline = '\0';
                
                if (strlen(custom_text) > 0) {
                    use_custom_text = true;
                    printf("✓ Using custom text: \"%s\"\n", custom_text);
                } else {
                    use_custom_text = false;
                    printf("✓ Using default test sentence\n");
                }
            }
            
            enable_raw_mode();
            
        } else if (choice >= '1' && choice <= '9') {
            int preset_idx = choice - '1';
            if (preset_idx < g_num_presets) {
                if (g_emotion_presets[preset_idx].speaker_id >= voice->num_speakers) {
                    printf("⚠️  speaker_id=%d exceeds model range (max=%d)\n",
                           g_emotion_presets[preset_idx].speaker_id,
                           voice->num_speakers - 1);
                    continue;
                }
                
                test_speaker(audio_runtime, voice,
                           g_emotion_presets[preset_idx].speaker_id,
                           g_emotion_presets[preset_idx].name,
                           use_custom_text ? custom_text : NULL);
            }
        }
    }
    
    disable_raw_mode();
}

int main(int argc, char** argv) {
    printf("\n========================================\n");
    printf("  EthervoxAI Emotion Tester\n");
    printf("  Multi-Speaker Model Explorer\n");
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
    
    int multispeaker_count = 0;
    for (int i = 0; i < g_voice_count; i++) {
        if (g_voices[i].is_multispeaker) multispeaker_count++;
    }
    
    if (multispeaker_count == 0) {
        printf("\n⚠️  No multi-speaker models found!\n");
        printf("Download emotional models with:\n");
        printf("  ./scripts/download-piper-model.sh en_US-libritts_r-medium\n");
        printf("  ./scripts/download-piper-model.sh en_US-libritts-high\n");
        printf("  ./scripts/download-piper-model.sh de_DE-thorsten_emotional-medium\n\n");
    } else {
        printf("Found %d multi-speaker model(s) 🎭\n", multispeaker_count);
    }
    
    while (1) {
        int idx = select_voice();
        if (idx < 0) break;
        
        test_emotion_presets(&audio_runtime, &g_voices[idx]);
        
        printf("\nTest another model? (y/n): ");
        fflush(stdout);
        
        char again;
        if (scanf(" %c", &again) != 1) break;
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        
        if (tolower(again) != 'y') break;
    }
    
    printf("\n✓ Done!\n");
    ethervox_audio_stop(&audio_runtime);
    ethervox_audio_cleanup(&audio_runtime);
    
    return 0;
}
