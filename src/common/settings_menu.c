/**
 * @file settings_menu.c
 * @brief Terminal-based settings menu implementation
 *
 * Cross-platform TUI settings interface using ncurses (macOS/Linux) or PDCurses (Windows).
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/settings_menu.h"
#include "ethervox/error.h"
#include "ethervox/config.h"
#include "ethervox/settings.h"
#include "ethervox/logging.h"
#include "ethervox/governor.h"
#include "ethervox/tts.h"
#include "ethervox/audio.h"
#include "../tts/phonemizer/pronunciation_overrides.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

// Check if ncurses is available
#ifdef __ANDROID__
    // Android: ncurses not available
    #define HAVE_NCURSES 0
#elif defined(__APPLE__)
    #define HAVE_NCURSES 1
    #include <ncurses.h>
#elif defined(__linux__)
    #define HAVE_NCURSES 1
    #include <ncurses.h>
#elif defined(_WIN32)
    // Windows: would use PDCurses if available
    #define HAVE_NCURSES 0
#else
    #define HAVE_NCURSES 0
#endif

#if HAVE_NCURSES

// Menu item types
typedef enum {
    MENU_ITEM_TOGGLE,      // Boolean on/off
    MENU_ITEM_NUMERIC,     // Integer numeric value
    MENU_ITEM_FLOAT,       // Float numeric value
    MENU_ITEM_ACTION,      // Execute action
    MENU_ITEM_SUBMENU,     // Open submenu
    MENU_ITEM_BACK         // Go back
} menu_item_type_t;

// Menu item definition
typedef struct {
    const char* label;
    const char* description;
    menu_item_type_t type;
    void* value_ptr;       // Pointer to value for toggles/numerics
    int (*action)(void*);  // Action callback
    void* action_data;     // Data for action
} menu_item_t;

// Colors
#define COLOR_HEADER    1
#define COLOR_SELECTED  2
#define COLOR_NORMAL    3
#define COLOR_DISABLED  4

static WINDOW* main_win = NULL;
static WINDOW* status_win = NULL;

// Initialize ncurses display
static int init_display(void) {
    main_win = initscr();
    if (!main_win) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    cbreak();              // Disable line buffering
    noecho();              // Don't echo keypresses
    keypad(main_win, TRUE); // Enable arrow keys
    curs_set(0);           // Hide cursor
    
    // Initialize colors if available
    if (has_colors()) {
        start_color();
        init_pair(COLOR_HEADER, COLOR_CYAN, COLOR_BLACK);
        init_pair(COLOR_SELECTED, COLOR_BLACK, COLOR_WHITE);
        init_pair(COLOR_NORMAL, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_DISABLED, COLOR_BLACK, COLOR_BLACK);
    }
    
    return ETHERVOX_SUCCESS;
}

// Cleanup ncurses
static void cleanup_display(void) {
    if (status_win) {
        delwin(status_win);
        status_win = NULL;
    }
    if (main_win) {
        endwin();
        main_win = NULL;
    }
}

// Draw header
static void draw_header(const char* title) {
    int width = getmaxx(main_win);
    
    attron(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
    mvprintw(0, 0, "+");
    for (int i = 1; i < width - 1; i++) printw("=");
    printw("+");
    
    int title_pos = (width - strlen(title)) / 2;
    mvprintw(1, title_pos, "%s", title);
    
    mvprintw(2, 0, "+");
    for (int i = 1; i < width - 1; i++) printw("=");
    printw("+");
    attroff(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
}

// Draw footer with help
static void draw_footer(void) {
    int height = getmaxy(main_win);
    int width = getmaxx(main_win);
    
    attron(COLOR_PAIR(COLOR_HEADER));
    mvprintw(height - 2, 2, "↑↓: Navigate  ←→: Change  Enter: Select  Ctrl+S: Save  Q: Quit");
    attroff(COLOR_PAIR(COLOR_HEADER));
}

// Draw menu items
static void draw_menu(menu_item_t* items, int item_count, int selected, int scroll_offset) {
    int start_y = 4;
    int height = getmaxy(main_win);
    int visible_items = (height - 8) / 2;  // Account for header/footer/status
    
    for (int i = scroll_offset; i < item_count && i < scroll_offset + visible_items; i++) {
        int y = start_y + ((i - scroll_offset) * 2);
        
        // Check if this is a section header (starts with dashes)
        bool is_header = (strncmp(items[i].label, "---", 3) == 0);
        
        if (is_header) {
            // Draw section header with special styling
            attron(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
            move(y, 2);
            clrtoeol();
            mvprintw(y, 4, "%s", items[i].label);
            attroff(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
            continue;
        }
        
        if (i == selected) {
            attron(COLOR_PAIR(COLOR_SELECTED) | A_BOLD);
        } else {
            attron(COLOR_PAIR(COLOR_NORMAL));
        }
        
        // Clear line
        move(y, 2);
        clrtoeol();
        
        // Draw item
        mvprintw(y, 4, "%s", items[i].label);
        
        // Draw value based on type
        if (items[i].type == MENU_ITEM_TOGGLE && items[i].value_ptr) {
            bool* val = (bool*)items[i].value_ptr;
            mvprintw(y, 40, "[%s]", *val ? "ON " : "OFF");
        } else if (items[i].type == MENU_ITEM_NUMERIC && items[i].value_ptr) {
            uint32_t* val = (uint32_t*)items[i].value_ptr;
            // Show signed values properly (for n_threads = -1)
            if (*val == (uint32_t)-1) {
                mvprintw(y, 40, "[auto]");
            } else if (*val > 1000000) {
                mvprintw(y, 40, "[%uk]", *val / 1024);
            } else {
                mvprintw(y, 40, "[%u]", *val);
            }
        } else if (items[i].type == MENU_ITEM_FLOAT && items[i].value_ptr) {
            float* val = (float*)items[i].value_ptr;
            mvprintw(y, 40, "[%.2f]", *val);
        } else if (items[i].type == MENU_ITEM_ACTION) {
            if (items[i].value_ptr && items[i].action) {
                // Action with value - show value then arrow (e.g., voice selection)
                char* str_val = (char*)items[i].value_ptr;
                if (str_val && str_val[0]) {
                    mvprintw(y, 40, "[%s] →", str_val);
                } else {
                    mvprintw(y, 40, "→");
                }
            } else if (items[i].action) {
                // Action only - just show arrow
                mvprintw(y, 40, "→");
            }
        }
        
        if (i == selected) {
            attroff(COLOR_PAIR(COLOR_SELECTED) | A_BOLD);
        } else {
            attroff(COLOR_PAIR(COLOR_NORMAL));
        }
        
        // Draw description
        if (items[i].description && items[i].description[0]) {
            attron(COLOR_PAIR(COLOR_NORMAL) | A_DIM);
            mvprintw(y + 1, 6, "%s", items[i].description);
            attroff(COLOR_PAIR(COLOR_NORMAL) | A_DIM);
        }
    }
}

// Status message
static void show_status(const char* message) {
    int height = getmaxy(main_win);
    attron(COLOR_PAIR(COLOR_HEADER));
    mvprintw(height - 4, 2, "Status: %-60s", message);
    attroff(COLOR_PAIR(COLOR_HEADER));
    refresh();
}

// Action: Export configuration
static int action_export_config(void* data) {
    cleanup_display();
    
    printf("\n+===================================================================+\n");
    printf("|                    EXPORT CONFIGURATION                       |\n");
    printf("+===================================================================+\n\n");
    
    // Load current settings
    ethervox_persistent_settings_t settings = ethervox_settings_get_defaults();
    if (ethervox_is_error(ethervox_settings_load(&settings, NULL))) {
        printf("Warning: Could not load current settings, using defaults\n\n");
    }
    
    // Prompt for export path
    char export_path[512];
    printf("Enter export path (or press Enter for default 'ethervox_config.json'): ");
    if (fgets(export_path, sizeof(export_path), stdin)) {
        // Remove newline
        export_path[strcspn(export_path, "\n")] = 0;
        
        if (export_path[0] == '\0') {
            strncpy(export_path, "ethervox_config.json", sizeof(export_path) - 1);
        }
        
        // Export to JSON string
        char* json_string = ethervox_settings_export(&settings);
        if (json_string) {
            // Write to file
            FILE* f = fopen(export_path, "w");
            if (f) {
                fprintf(f, "%s", json_string);
                fclose(f);
                printf("\n[OK] Configuration exported to: %s\n", export_path);
            } else {
                printf("\n[FAIL] Failed to write to file: %s\n", export_path);
            }
            free(json_string);
        } else {
            printf("\n[FAIL] Failed to export configuration\n");
        }
    }
    
    printf("\nPress Enter to return...\n");
    getchar();
    
    init_display();
    return ETHERVOX_SUCCESS;
}

// Action: Import configuration
static int action_import_config(void* data) {
    cleanup_display();
    
    printf("\n+===================================================================+\n");
    printf("|                    IMPORT CONFIGURATION                       |\n");
    printf("+===================================================================+\n\n");
    printf("⚠️  WARNING: This will overwrite your current settings!\n\n");
    
    // Prompt for import path
    char import_path[512];
    printf("Enter import path (or press Enter to cancel): ");
    if (fgets(import_path, sizeof(import_path), stdin)) {
        // Remove newline
        import_path[strcspn(import_path, "\n")] = 0;
        
        if (import_path[0] != '\0') {
            // Load from specified path
            ethervox_persistent_settings_t settings;
            ethervox_result_t load_result = ethervox_settings_load(&settings, import_path);
            if (ethervox_is_success(load_result)) {
                // Save to default location
                ethervox_result_t save_result = ethervox_settings_save(&settings, NULL);
                if (ethervox_is_success(save_result)) {
                    printf("\n[OK] Configuration imported from: %s\n", import_path);
                    printf("[OK] Saved to: %s\n", ethervox_settings_get_default_path());
                    printf("\nRestart the application for changes to take effect.\n");
                } else {
                    printf("\n[FAIL] Failed to save imported configuration: %s\n", ethervox_error_string(save_result));
                }
            } else {
                printf("\n[FAIL] Failed to load configuration from: %s (%s)\n", import_path, ethervox_error_string(load_result));
            }
        } else {
            printf("\nImport cancelled.\n");
        }
    }
    
    printf("\nPress Enter to return...\n");
    getchar();
    
    init_display();
    return ETHERVOX_SUCCESS;
}

// Action: Run tool optimization
static int action_optimize_tools(void* data) {
    ethervox_settings_t* settings = (ethervox_settings_t*)data;
    
    cleanup_display();
    
    printf("\n+===================================================================+\n");
    printf("|          TOOL PROMPT OPTIMIZATION ROUTINE                     |\n");
    printf("+===================================================================+\n\n");
    printf("This will optimize tool prompts for your model.\n");
    printf("This may take several minutes...\n\n");
    printf("Press Ctrl+C to cancel, or Enter to continue...\n");
    getchar();
    
    // TODO: Call ethervox_optimize_tool_prompts() here when Governor is available
    printf("\nOptimization would run here (needs Governor integration)\n");
    printf("Press Enter to return to settings...\n");
    getchar();
    
    init_display();
    return ETHERVOX_SUCCESS;
}

// Action: Select TTS voice
static int action_select_tts_voice(void* data) {
    // Data is a struct containing settings and TTS reload callback
    typedef struct {
        ethervox_persistent_settings_t* settings;
        ethervox_tts_reload_callback_t tts_reload_callback;
        void* tts_user_data;
    } voice_action_data_t;
    
    voice_action_data_t* action_data = (voice_action_data_t*)data;
    ethervox_persistent_settings_t* settings = action_data->settings;
    
    cleanup_display();
    
    // Available voices per language
    typedef struct {
        const char* display_name;
        const char* voice_id;
        const char* model_filename;
        const char* description;
        const char* test_sentence;
        const char* lang_code;
    } voice_option_t;
    
    voice_option_t english_voices[] = {
        // Expressive/Emotional models (recommended)
        {"LibriTTS-R Medium ⭐", "en_US-libritts_r-medium", "en_US-libritts_r-medium.onnx", 
         "High quality, natural prosody, multi-speaker (904 speakers)", 
         "Welcome to EthervoxAI, your intelligent voice assistant.", "en"},
        {"LibriTTS High ⭐", "en_US-libritts-high", "en_US-libritts-high.onnx", 
         "Very high quality, expressive, multi-speaker", 
         "Experience the power of natural speech synthesis.", "en"},
        
        // US English - Lessac (male, clear)
        {"Lessac High", "en_US-lessac-high", "en_US-lessac-high.onnx", 
         "US male, clear articulation, high quality", 
         "The weather today will be sunny and warm.", "en"},
        {"Lessac Medium", "en_US-lessac-medium", "en_US-lessac-medium.onnx", 
         "US male, clear articulation, medium quality", 
         "This is a test of the text-to-speech system.", "en"},
        {"Lessac Low", "en_US-lessac-low", "en_US-lessac-low.onnx", 
         "US male, clear articulation, fast/lightweight", 
         "Quick and efficient voice synthesis.", "en"},
        
        // US English - Other voices
        {"Amy Medium", "en_US-amy-medium", "en_US-amy-medium.onnx", 
         "US female, friendly and warm", 
         "Hello, I'm Amy. How can I help you today?", "en"},
        {"Danny Low", "en_US-danny-low", "en_US-danny-low.onnx", 
         "US male, casual and conversational", 
         "Hey there! Let's get started.", "en"},
        {"Joe Medium", "en_US-joe-medium", "en_US-joe-medium.onnx", 
         "US male, professional tone", 
         "Welcome to the voice assistant system.", "en"},
        
        // UK English
        {"Alan Medium", "en_GB-alan-medium", "en_GB-alan-medium.onnx", 
         "British male, clear and professional", 
         "Good day. How may I assist you?", "en"},
        {"Alba Medium", "en_GB-alba-medium", "en_GB-alba-medium.onnx", 
         "British female, warm and natural", 
         "Welcome to EthervoxAI voice assistant.", "en"},
        {"Jenny Dioco Medium", "en_GB-jenny_dioco-medium", "en_GB-jenny_dioco-medium.onnx", 
         "British female, expressive", 
         "Hello! I'm here to help you today.", "en"}
    };
    
    voice_option_t chinese_voices[] = {
        {"Huayan High ⭐", "zh_CN-huayan-high", "zh_CN-huayan-high.onnx", 
         "Mandarin Chinese, high quality, natural tone", 
         "欢迎使用EthervoxAI语音助手", "zh"},
        {"Huayan Medium", "zh_CN-huayan-medium", "zh_CN-huayan-medium.onnx", 
         "Mandarin Chinese, medium quality, natural tone", 
         "欢迎使用EthervoxAI语音助手", "zh"}
    };
    
    voice_option_t german_voices[] = {
        {"Thorsten Emotional Medium ⭐", "de_DE-thorsten_emotional-medium", "de_DE-thorsten_emotional-medium.onnx", 
         "German male, emotional and expressive", 
         "Ich freue mich, Ihnen heute zu helfen!", "de"},
        {"Thorsten Medium", "de_DE-thorsten-medium", "de_DE-thorsten-medium.onnx", 
         "German male, clear and natural", 
         "Willkommen beim EthervoxAI Sprachassistenten", "de"},
        {"Eva K Medium", "de_DE-eva_k-medium", "de_DE-eva_k-medium.onnx", 
         "German female, warm and friendly", 
         "Guten Tag! Wie kann ich Ihnen helfen?", "de"}
    };
    
    voice_option_t spanish_voices[] = {
        {"Ald Medium", "es_MX-ald-medium", "es_MX-ald-medium.onnx", 
         "Mexican Spanish male, natural and clear", 
         "Bienvenido al asistente de voz EthervoxAI", "es"}
    };
    
    // Language selection menu
    while (1) {
        printf("\n+==================================================================+\n");
        printf("|              SELECT LANGUAGE FOR TTS VOICE                   |\n");
        printf("+==================================================================+\n\n");
        
        printf("Current voice settings:\n");
        printf("  English: %s\n", settings->tts.voice_en[0] ? settings->tts.voice_en : "(none)");
        printf("  Chinese: %s\n", settings->tts.voice_zh[0] ? settings->tts.voice_zh : "(none)");
        printf("  German:  %s\n", settings->tts.voice_de[0] ? settings->tts.voice_de : "(none)");
        printf("  Spanish: %s\n\n", settings->tts.voice_es[0] ? settings->tts.voice_es : "(none)");
        
        printf("Select language to configure:\n\n");
        printf("  [1] English\n");
        printf("  [2] Chinese (Mandarin)\n");
        printf("  [3] German\n");
        printf("  [4] Spanish\n");
        printf("  [0] Back to main menu\n\n");
        printf("Choice: ");
        fflush(stdout);
        
        char input[16];
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        input[strcspn(input, "\n")] = '\0';
        
        int lang_choice = atoi(input);
        if (lang_choice == 0) {
            break;
        }
        
        voice_option_t* voices = NULL;
        int num_voices = 0;
        char* voice_setting = NULL;
        const char* lang_name = NULL;
        
        switch (lang_choice) {
            case 1:
                voices = english_voices;
                num_voices = sizeof(english_voices) / sizeof(english_voices[0]);
                voice_setting = settings->tts.voice_en;
                lang_name = "English";
                break;
            case 2:
                voices = chinese_voices;
                num_voices = sizeof(chinese_voices) / sizeof(chinese_voices[0]);
                voice_setting = settings->tts.voice_zh;
                lang_name = "Chinese";
                break;
            case 3:
                voices = german_voices;
                num_voices = sizeof(german_voices) / sizeof(german_voices[0]);
                voice_setting = settings->tts.voice_de;
                lang_name = "German";
                break;
            case 4:
                voices = spanish_voices;
                num_voices = sizeof(spanish_voices) / sizeof(spanish_voices[0]);
                voice_setting = settings->tts.voice_es;
                lang_name = "Spanish";
                break;
            default:
                printf("\n⚠ Invalid choice\n");
                printf("Press Enter to continue...");
                fgets(input, sizeof(input), stdin);
                continue;
        }
        
        // Voice selection for chosen language with arrow key navigation
        int selected_idx = 0;  // Currently highlighted voice
        int quit_selection = 0;
        
        // Find current voice index
        for (int i = 0; i < num_voices; i++) {
            if (strcmp(voices[i].voice_id, voice_setting) == 0) {
                selected_idx = i;
                break;
            }
        }
        
        while (!quit_selection) {
            printf("\n+==================================================================+\n");
            printf("|              SELECT %s VOICE                           |\n", lang_name);
            printf("+==================================================================+\n\n");
            
            printf("Current: %s\n\n", voice_setting[0] ? voice_setting : "(none)");
            
            printf("Available voices (use ↑↓ arrows to navigate):\n\n");
            for (int i = 0; i < num_voices; i++) {
                // Highlight selected voice
                if (i == selected_idx) {
                    printf("  → ");
                } else {
                    printf("    ");
                }
                printf("[%d] %s\n", i + 1, voices[i].display_name);
                printf("      %s\n", voices[i].description);
                if (i == selected_idx) {
                    printf("      Test sentence: \"%s\"\n", voices[i].test_sentence);
                }
                printf("\n");
            }
            
            printf("\nCommands:\n");
            printf("  ↑/↓ or k/j:  Navigate voices\n");
            printf("  ENTER/SPACE: Test current voice\n");
            printf("  s:           Select current voice\n");
            printf("  1-%d:        Quick select by number\n", num_voices);
            printf("  q or 0:      Back to language selection\n\n");
            printf("Choice: ");
            fflush(stdout);
            
            if (!fgets(input, sizeof(input), stdin)) {
                break;
            }
            input[strcspn(input, "\n")] = '\0';
            
            // Handle arrow keys and commands
            if (strlen(input) == 0 || strcmp(input, "\n") == 0 || strcmp(input, " ") == 0) {
                // ENTER or SPACE: test current voice
                voice_option_t* test_voice = &voices[selected_idx];
                
                printf("\n🔊 Testing: %s\n", test_voice->display_name);
                printf("   \"%s\"\n\n", test_voice->test_sentence);
                
                // Build model path for test
                char test_model_path[512];
                const char* home = getenv("HOME");
                if (home) {
                    snprintf(test_model_path, sizeof(test_model_path),
                            "%s/.ethervox/models/piper/%s", home, test_voice->model_filename);
                    
                    // Create TTS context with test model
                    ethervox_tts_config_t test_config = {
                        .backend = ETHERVOX_TTS_BACKEND_PIPER,
                        .model_path = test_model_path,
                        .sample_rate = 16000,
                        .channels = 1,
                        .speaking_rate = settings->tts.speed,
                        .phoneme_variance = settings->tts.phoneme_variance,
                        .prosody_variance = settings->tts.prosody_variance
                    };
                    
                    ethervox_tts_context_t* test_tts = ethervox_tts_create(&test_config);
                    if (test_tts) {
                        printf("Model: %s\n", test_voice->model_filename);
                        printf("Expected language: %s\n", test_voice->lang_code);
                        printf("Synthesizing audio...\n");
                        
                        ethervox_tts_audio_t audio = {0};
                        ethervox_result_t result = ethervox_tts_synthesize_text(test_tts, test_voice->test_sentence, &audio);
                        
                        if (ethervox_is_success(result) && audio.samples && audio.sample_count > 0) {
                            printf("Playing %zu samples (%d Hz)...\n", audio.sample_count, audio.sample_rate);
                            
                            // Initialize audio runtime for playback
                            ethervox_audio_runtime_t audio_runtime = {0};
                            ethervox_audio_config_t audio_config = {
                                .sample_rate = audio.sample_rate,
                                .channels = audio.channels,
                                .bits_per_sample = 16,
                                .buffer_size = 1024,
                                .enable_noise_suppression = false,
                                .enable_echo_cancellation = false
                            };
                            
                            // Register platform driver before initializing
                            if (ethervox_audio_register_platform_driver(&audio_runtime) == 0 &&
                                ethervox_audio_init(&audio_runtime, &audio_config) == 0) {
                                // Convert float samples to int16_t for playback
                                int16_t* int16_samples = (int16_t*)malloc(audio.sample_count * sizeof(int16_t));
                                if (int16_samples) {
                                    // Find peak amplitude for normalization (Piper's approach)
                                    float max_abs_value = 0.01f;  // Minimum to avoid division by zero
                                    for (size_t i = 0; i < audio.sample_count; i++) {
                                        float abs_value = fabsf(audio.samples[i]);
                                        if (abs_value > max_abs_value) {
                                            max_abs_value = abs_value;
                                        }
                                    }
                                    
                                    // Calculate scaling factor to use full 16-bit range
                                    float audio_scale = 32767.0f / max_abs_value;
                                    printf("[Audio] Normalizing with scale factor: %.2f (peak: %.4f)\n", 
                                           audio_scale, max_abs_value);
                                    
                                    // Convert with normalization
                                    for (size_t i = 0; i < audio.sample_count; i++) {
                                        float scaled = audio.samples[i] * audio_scale;
                                        // Clamp to int16 range
                                        if (scaled > 32767.0f) scaled = 32767.0f;
                                        if (scaled < -32768.0f) scaled = -32768.0f;
                                        int16_samples[i] = (int16_t)scaled;
                                    }
                                    
                                    // Create audio buffer for playback
                                    ethervox_audio_buffer_t play_buffer = {
                                        .data = (float*)int16_samples,
                                        .size = audio.sample_count * sizeof(int16_t),
                                        .channels = audio.channels,
                                        .timestamp_us = 0
                                    };
                                    
                                    int play_result = audio_runtime.driver.write_audio(&audio_runtime, &play_buffer);
                                    if (play_result == 0) {
                                        printf("[OK] Audio played successfully\n");
                                        // Wait for audio to finish playing
                                        usleep((audio.sample_count * 1000000) / audio.sample_rate + 500000);
                                    } else {
                                        printf("⚠ Failed to play audio (error %d)\n", play_result);
                                    }
                                    
                                    free(int16_samples);
                                } else {
                                    printf("⚠ Failed to allocate audio conversion buffer\n");
                                }
                                
                                ethervox_audio_cleanup(&audio_runtime);
                            } else {
                                printf("⚠ Failed to initialize audio for playback\n");
                            }
                            
                            ethervox_tts_audio_free(&audio);
                        } else {
                            printf("⚠ Failed to synthesize test sentence (error %d)\n", result);
                            printf("   Check that model exists: %s\n", test_model_path);
                        }
                        ethervox_tts_destroy(test_tts);
                    } else {
                        printf("⚠ Failed to initialize TTS for testing\n");
                        printf("   Model: %s\n", test_model_path);
                    }
                } else {
                    printf("⚠ Cannot determine model path (HOME not set)\n");
                }
                
                printf("\nPress Enter to continue...");
                fgets(input, sizeof(input), stdin);
            } else if (strcmp(input, "k") == 0 || strcmp(input, "K") == 0) {
                // k = up (vim style)
                selected_idx = (selected_idx - 1 + num_voices) % num_voices;
            } else if (strcmp(input, "j") == 0 || strcmp(input, "J") == 0) {
                // j = down (vim style)
                selected_idx = (selected_idx + 1) % num_voices;
            } else if (strcmp(input, "s") == 0 || strcmp(input, "S") == 0) {
                // Select current voice
                voice_option_t* selected = &voices[selected_idx];
                
                // Update voice ID for the selected language
                strncpy(voice_setting, selected->voice_id, 64 - 1);
                voice_setting[64 - 1] = '\0';
                
                // Update piper_model_path to match the selected voice
                // This ensures the voice takes effect immediately and persists correctly
                const char* home = getenv("HOME");
                if (home) {
                    snprintf(settings->tts.piper_model_path, sizeof(settings->tts.piper_model_path),
                            "%s/.ethervox/models/piper/%s.onnx", home, selected->voice_id);
                }
                
                // Also update the deprecated global voice field (use English as default)
                if (lang_choice == 1) {
                    strncpy(settings->tts.voice, selected->voice_id, sizeof(settings->tts.voice) - 1);
                    settings->tts.voice[sizeof(settings->tts.voice) - 1] = '\0';
                }
                
                // Ensure piper engine is selected
                strncpy(settings->tts.engine, "piper", sizeof(settings->tts.engine) - 1);
                
                printf("\n[OK] %s voice changed to: %s\n", lang_name, selected->display_name);
                printf("    Model path: %s\n", settings->tts.piper_model_path);
                
                // Save settings immediately to persist voice selection
                ethervox_result_t save_result = ethervox_settings_save(settings, NULL);
                if (ethervox_is_success(save_result)) {
                    printf("💾 Settings saved successfully\n");
                } else {
                    printf("⚠️  Warning: Failed to save settings to disk: %s\n", ethervox_error_string(save_result));
                }
                
                // Try to reload TTS immediately if callback is available
                if (action_data->tts_reload_callback) {
                    printf("\n🔄 Reloading TTS with new voice...\n");
                    if (action_data->tts_reload_callback(&settings->tts, action_data->tts_user_data) == 0) {
                        printf("[OK] TTS reloaded successfully - new voice is active!\n");
                    } else {
                        printf("⚠️  TTS reload failed - restart may be required\n");
                    }
                } else {
                    printf("\n⚠️  Note: Voice change will take effect after restarting EthervoxAI\n");
                    printf("    or use Ctrl+S to save and restart the application\n");
                }
                printf("\nPress Enter to continue...");
                fgets(input, sizeof(input), stdin);
                quit_selection = 1;
            } else if (strcmp(input, "q") == 0 || strcmp(input, "Q") == 0 || strcmp(input, "0") == 0) {
                // Auto-select the currently highlighted voice before quitting
                // This makes the UX more intuitive - navigating to a voice selects it
                voice_option_t* selected = &voices[selected_idx];
                
                // Only update if the voice has changed
                if (strcmp(voice_setting, selected->voice_id) != 0) {
                    strncpy(voice_setting, selected->voice_id, 64 - 1);
                    voice_setting[64 - 1] = '\0';
                    
                    // Update piper_model_path to match the selected voice
                    const char* home = getenv("HOME");
                    if (home) {
                        snprintf(settings->tts.piper_model_path, sizeof(settings->tts.piper_model_path),
                                "%s/.ethervox/models/piper/%s.onnx", home, selected->voice_id);
                    }
                    
                    // Also update the deprecated global voice field (use English as default)
                    if (lang_choice == 1) {
                        strncpy(settings->tts.voice, selected->voice_id, sizeof(settings->tts.voice) - 1);
                        settings->tts.voice[sizeof(settings->tts.voice) - 1] = '\0';
                    }
                    
                    // Ensure piper engine is selected
                    strncpy(settings->tts.engine, "piper", sizeof(settings->tts.engine) - 1);
                    
                    printf("\n[OK] %s voice changed to: %s\n", lang_name, selected->display_name);
                    printf("    Model path: %s\n", settings->tts.piper_model_path);
                    
                    // Save settings immediately
                    ethervox_result_t save_result = ethervox_settings_save(settings, NULL);
                    if (ethervox_is_success(save_result)) {
                        printf("💾 Settings saved successfully\n");
                    } else {
                        printf("⚠️  Warning: Failed to save settings to disk: %s\n", ethervox_error_string(save_result));
                    }
                    
                    // Try to reload TTS if callback is available
                    if (action_data->tts_reload_callback) {
                        printf("\n🔄 Reloading TTS with new voice...\n");
                        if (action_data->tts_reload_callback(&settings->tts, action_data->tts_user_data) == 0) {
                            printf("[OK] TTS reloaded successfully - new voice is active!\n");
                        } else {
                            printf("⚠️  TTS reload failed - restart may be required\n");
                        }
                    }
                }
                
                // Quit back to language menu
                quit_selection = 1;
            } else if (input[0] == 't' || input[0] == 'T') {
                // Test command: t<num>
                int test_choice = atoi(&input[1]);
                if (test_choice >= 1 && test_choice <= num_voices) {
                    selected_idx = test_choice - 1;  // Update selection to tested voice
                    voice_option_t* test_voice = &voices[selected_idx];
                
                    printf("\n🔊 Testing: %s\n", test_voice->display_name);
                    printf("   \"%s\"\n\n", test_voice->test_sentence);
                    
                    // Build model path for test (reuse the code above)
                    char test_model_path[512];
                    const char* home = getenv("HOME");
                    if (home) {
                        snprintf(test_model_path, sizeof(test_model_path),
                                "%s/.ethervox/models/piper/%s", home, test_voice->model_filename);
                        
                        ethervox_tts_config_t test_config = {
                            .backend = ETHERVOX_TTS_BACKEND_PIPER,
                            .model_path = test_model_path,
                            .sample_rate = 16000,
                            .channels = 1,
                            .speaking_rate = settings->tts.speed,
                            .phoneme_variance = settings->tts.phoneme_variance,
                            .prosody_variance = settings->tts.prosody_variance
                        };
                        
                        ethervox_tts_context_t* test_tts = ethervox_tts_create(&test_config);
                        if (test_tts) {
                            ethervox_tts_audio_t audio = {0};
                            ethervox_result_t result = ethervox_tts_synthesize_text(test_tts, test_voice->test_sentence, &audio);
                            
                            if (result == 0 && audio.samples && audio.sample_count > 0) {
                                ethervox_audio_runtime_t audio_runtime = {0};
                                ethervox_audio_config_t audio_config = {
                                    .sample_rate = audio.sample_rate,
                                    .channels = audio.channels,
                                    .bits_per_sample = 16,
                                    .buffer_size = 1024
                                };
                                
                                if (ethervox_audio_register_platform_driver(&audio_runtime) == 0 &&
                                    ethervox_audio_init(&audio_runtime, &audio_config) == 0) {
                                    int16_t* int16_samples = (int16_t*)malloc(audio.sample_count * sizeof(int16_t));
                                    if (int16_samples) {
                                        float max_abs_value = 0.01f;
                                        for (size_t i = 0; i < audio.sample_count; i++) {
                                            float abs_value = fabsf(audio.samples[i]);
                                            if (abs_value > max_abs_value) max_abs_value = abs_value;
                                        }
                                        float audio_scale = 32767.0f / max_abs_value;
                                        for (size_t i = 0; i < audio.sample_count; i++) {
                                            float scaled = audio.samples[i] * audio_scale;
                                            if (scaled > 32767.0f) scaled = 32767.0f;
                                            if (scaled < -32768.0f) scaled = -32768.0f;
                                            int16_samples[i] = (int16_t)scaled;
                                        }
                                        ethervox_audio_buffer_t play_buffer = {
                                            .data = (float*)int16_samples,
                                            .size = audio.sample_count * sizeof(int16_t),
                                            .channels = audio.channels
                                        };
                                        audio_runtime.driver.write_audio(&audio_runtime, &play_buffer);
                                        usleep((audio.sample_count * 1000000) / audio.sample_rate + 500000);
                                        free(int16_samples);
                                    }
                                    ethervox_audio_cleanup(&audio_runtime);
                                }
                                ethervox_tts_audio_free(&audio);
                            }
                            ethervox_tts_destroy(test_tts);
                        }
                    }
                    printf("\nPress Enter to continue...");
                    fgets(input, sizeof(input), stdin);
                } else {
                    printf("\n⚠ Invalid voice number for test\n");
                    printf("Press Enter to continue...");
                    fgets(input, sizeof(input), stdin);
                }
            } else {
                // Try numeric selection
                int choice = atoi(input);
                if (choice >= 1 && choice <= num_voices) {
                    selected_idx = choice - 1;  // Just update the selection
                } else if (choice != 0) {
                    printf("\n⚠ Invalid command. Use k/j (↑↓), ENTER (test), s (select), or q (quit)\n");
                    printf("Press Enter to continue...");
                    fgets(input, sizeof(input), stdin);
                }
            }
        }
    }
    
    init_display();
    return ETHERVOX_SUCCESS;
}

// Action: View system info
static int action_view_info(void* data) {
    ethervox_settings_t* settings = (ethervox_settings_t*)data;
    
    cleanup_display();
    
    printf("\n+===================================================================+\n");
    printf("|                     SYSTEM INFORMATION                        |\n");
    printf("+===================================================================+\n\n");
    printf("Version:        0.0.6\n");
    printf("Branch:         %s\n", settings->git_branch[0] ? settings->git_branch : "unknown");
    printf("Commit:         %s\n\n", settings->git_commit[0] ? settings->git_commit : "unknown");
    
    printf("Governor Model: %s\n", settings->model_path);
    printf("Whisper Model:  %s\n", settings->whisper_model_path[0] ? settings->whisper_model_path : "(not loaded)");
    printf("Memory Dir:     %s\n\n", settings->memory_dir);
    
    printf("Audio Device:   %s\n", settings->audio_device[0] ? settings->audio_device : "(default)");
    printf("Wake Word:      %s [%s]\n\n", 
           settings->wake_word[0] ? settings->wake_word : "(not set)",
           settings->wake_word_enabled ? "ENABLED" : "DISABLED");
    
    printf("Log Level:      %d\n", settings->log_level);
    printf("Debug:          %s\n", settings->debug_enabled ? "ON" : "OFF");
    printf("Engineering:    %s\n", settings->engineering_mode ? "ON" : "OFF");
    printf("Quiet Mode:     %s\n", settings->quiet_mode ? "ON" : "OFF");
    
    printf("\nPress Enter to return...\n");
    getchar();
    
    init_display();
    return ETHERVOX_SUCCESS;
}

// Action: Reset pronunciation overrides
static int action_reset_pronunciation(void* data) {
    cleanup_display();
    
    printf("\n+===================================================================+\n");
    printf("|               RESET PRONUNCIATION OVERRIDES                   |\n");
    printf("+===================================================================+\n\n");
    
    printf("⚠️  This will delete all trained pronunciation corrections.\n");
    printf("   This action cannot be undone!\n\n");
    
    printf("Are you sure you want to reset? (type 'yes' to confirm): ");
    char confirm[32];
    if (fgets(confirm, sizeof(confirm), stdin)) {
        confirm[strcspn(confirm, "\n")] = 0;  // Remove newline
        
        if (strcmp(confirm, "yes") == 0) {
            printf("\nResetting pronunciation overrides...\n");
            if (pronunciation_overrides_reset() == 0) {
                printf("\n✅ Reset complete!\n");
            } else {
                printf("\n❌ Reset failed (some files may not exist)\n");
            }
        } else {
            printf("\n❌ Reset cancelled\n");
        }
    }
    
    printf("\nPress Enter to continue...");
    getchar();
    
    init_display();
    return ETHERVOX_SUCCESS;
}

// Main settings menu
ethervox_result_t ethervox_settings_menu_show(ethervox_settings_t* settings, const char* model_path,
                                 ethervox_model_reload_callback_t reload_callback, void* user_data,
                                 ethervox_tts_reload_callback_t tts_reload_callback, void* tts_user_data) {
    if (!settings) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    if (init_display() != 0) {
        fprintf(stderr, "Failed to initialize ncurses\n");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Load persistent settings
    ethervox_persistent_settings_t persistent = ethervox_settings_get_defaults();
    ethervox_settings_load(&persistent, NULL);
    
    // Store TTS reload callback for voice selection
    typedef struct {
        ethervox_persistent_settings_t* settings;
        ethervox_tts_reload_callback_t tts_reload_callback;
        void* tts_user_data;
    } voice_action_data_t;
    
    static voice_action_data_t voice_action_data;
    voice_action_data.settings = &persistent;
    voice_action_data.tts_reload_callback = tts_reload_callback;
    voice_action_data.tts_user_data = tts_user_data;
    
    // Store initial values of parameters that require model reload
    uint32_t initial_gov_gpu_layers = persistent.governor.gpu_layers;
    uint32_t initial_gov_context_size = persistent.governor.context_size;
    int initial_gov_n_threads = persistent.governor.n_threads;
    uint32_t initial_llm_gpu_layers = persistent.llm.gpu_layers;
    uint32_t initial_llm_context = persistent.llm.context_length;
    int initial_llm_threads = persistent.llm.n_threads;
    
    // Define menu items
    menu_item_t items[] = {
        {
            .label = "--- General ---",
            .description = "",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Debug Mode",
            .description = "Enable detailed logging and debug output",
            .type = MENU_ITEM_TOGGLE,
            .value_ptr = &settings->debug_enabled,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Quiet Mode",
            .description = "Suppress non-essential output",
            .type = MENU_ITEM_TOGGLE,
            .value_ptr = &settings->quiet_mode,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Token Streaming",
            .description = "Display LLM response as tokens are generated (real-time)",
            .type = MENU_ITEM_TOGGLE,
            .value_ptr = &settings->streaming_enabled,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Engineering Mode",
            .description = "Enable advanced features and detailed stats",
            .type = MENU_ITEM_TOGGLE,
            .value_ptr = &settings->engineering_mode,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Log Level",
            .description = "Logging verbosity (0=TRACE to 6=OFF)",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &settings->log_level,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "--- Whisper STT ---",
            .description = "",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Whisper Temperature",
            .description = "Sampling temperature 0.0-1.0 (lower = more deterministic)",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.whisper.temperature,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Whisper Beam Size",
            .description = "Beam search width 1-10 (higher = more accurate, slower)",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.whisper.beam_size,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Translate to English",
            .description = "Automatically translate input to English",
            .type = MENU_ITEM_TOGGLE,
            .value_ptr = &persistent.whisper.translate_to_english,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Whisper GPU Acceleration",
            .description = "Use GPU if available (experimental)",
            .type = MENU_ITEM_TOGGLE,
            .value_ptr = &persistent.whisper.use_gpu,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "--- Conversation ---",
            .description = "",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Listen Timeout",
            .description = "Max wait time for user speech in ms (1000-30000)",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.conversation.listen_timeout_ms,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Conversation Timeout",
            .description = "Total conversation duration limit in ms (5000-60000)",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.conversation.conversation_timeout_ms,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Silence Timeout",
            .description = "Silence detection threshold in ms (500-5000)",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.conversation.silence_timeout_ms,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Audio Energy Threshold",
            .description = "Voice activity detection threshold 0.0-1.0",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.conversation.audio_energy_threshold,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Always Listening Mode",
            .description = "Continuous STT without wake word (desktop only)",
            .type = MENU_ITEM_TOGGLE,
            .value_ptr = &persistent.conversation.always_listening,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Filter Hallucinations",
            .description = "Remove known Whisper hallucination patterns",
            .type = MENU_ITEM_TOGGLE,
            .value_ptr = &persistent.conversation.filter_hallucinations,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "--- LLM (Language Model) ---",
            .description = "",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "LLM Max Tokens",
            .description = "Maximum response length 64-2048 tokens",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.llm.max_tokens,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "LLM Context Length",
            .description = "Context window size 512-8192 tokens (affects memory)",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.llm.context_length,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "LLM Temperature",
            .description = "Creativity 0.0-2.0 (0=deterministic, higher=creative)",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.llm.temperature,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "LLM Top-P",
            .description = "Nucleus sampling 0.0-1.0 (lower=focused, higher=diverse)",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.llm.top_p,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "LLM GPU Layers",
            .description = "Layers offloaded to GPU 0-999 (0=CPU only, 999=all)",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.llm.gpu_layers,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "LLM Threads",
            .description = "CPU threads 1-32 (-1=auto-detect)",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.llm.n_threads,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "--- Governor (Tool Orchestration) ---",
            .description = "",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Governor Max Iterations",
            .description = "Maximum reasoning loop iterations 1-50",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.governor.max_iterations,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Governor Timeout",
            .description = "Maximum execution time in seconds 10-600",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.governor.timeout_seconds,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Governor Temperature",
            .description = "Tool calling precision 0.0-1.0 (lower=deterministic)",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.governor.temperature,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Governor Max Tokens/Iter",
            .description = "Tokens per reasoning step 16-256",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.governor.max_tokens_per_iteration,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Governor Confidence",
            .description = "Minimum confidence to execute tools 0.0-1.0",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.governor.confidence_threshold,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "--- Wake Word ---",
            .description = "",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Wake Word Detection",
            .description = "Enable voice activation (wake word)",
            .type = MENU_ITEM_TOGGLE,
            .value_ptr = &settings->wake_word_enabled,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Wake Word Threshold",
            .description = "Detection sensitivity 0.0-1.0 (higher = stricter)",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.wake_word.detection_threshold,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "--- Echo Cancellation ---",
            .description = "",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "AEC Enabled",
            .description = "Enable acoustic echo cancellation",
            .type = MENU_ITEM_TOGGLE,
            .value_ptr = &persistent.aec.enabled,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "AEC Suppression",
            .description = "Echo reduction strength 0.0-1.0 (higher = more aggressive)",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.aec.suppression_level,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "AEC Filter Length",
            .description = "Filter duration in ms (32-128, higher = better long echoes)",
            .type = MENU_ITEM_NUMERIC,
            .value_ptr = &persistent.aec.filter_length_ms,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "--- Text-to-Speech ---",
            .description = "",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "TTS Engine",
            .description = "TTS backend: system (macOS say), piper (neural), none",
            .type = MENU_ITEM_ACTION,
            .value_ptr = persistent.tts.engine,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "TTS Voice",
            .description = "Select language and voice quality (EN/ZH/DE high-quality)",
            .type = MENU_ITEM_ACTION,
            .value_ptr = persistent.tts.voice_en,  // Show current English voice
            .action = action_select_tts_voice,
            .action_data = &voice_action_data
        },
        {
            .label = "TTS Speed",
            .description = "Speech rate 0.5-2.0 (1.0 = normal)",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.tts.speed,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "TTS Volume",
            .description = "Output volume 0.0-1.0 (1.0 = max)",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.tts.volume,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Phoneme Variance",
            .description = "Duration timing 0.0-1.0 (0.667 = default, higher = more natural)",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.tts.phoneme_variance,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Prosody Variance",
            .description = "Pitch expression 0.0-1.5 (0.8 = default, 1.0-1.2 = more human)",
            .type = MENU_ITEM_FLOAT,
            .value_ptr = &persistent.tts.prosody_variance,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "--- Actions ---",
            .description = "",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = NULL,
            .action_data = NULL
        },
        {
            .label = "Export Configuration",
            .description = "Save current settings to a backup file",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = action_export_config,
            .action_data = settings
        },
        {
            .label = "Import Configuration",
            .description = "Load settings from a backup file (overwrites current)",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = action_import_config,
            .action_data = settings
        },
        {
            .label = "Optimize Tool Prompts",
            .description = "Run LLM-based tool optimization (improves performance)",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = action_optimize_tools,
            .action_data = settings
        },
        {
            .label = "Reset Pronunciation Training",
            .description = "Clear all learned pronunciation overrides",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = action_reset_pronunciation,
            .action_data = NULL
        },
        {
            .label = "View System Info",
            .description = "Display current configuration and version",
            .type = MENU_ITEM_ACTION,
            .value_ptr = NULL,
            .action = action_view_info,
            .action_data = settings
        },
        {
            .label = "Exit Settings",
            .description = "Return to main application",
            .type = MENU_ITEM_BACK,
            .value_ptr = NULL,
            .action = NULL,
            .action_data = NULL
        }
    };
    
    int item_count = sizeof(items) / sizeof(items[0]);
    int selected = 0;
    int scroll_offset = 0;
    bool done = false;
    
    int height = getmaxy(main_win);
    int visible_items = (height - 8) / 2;  // Account for header/footer/status
    
    while (!done) {
        // Auto-scroll to keep selected item visible
        if (selected < scroll_offset) {
            scroll_offset = selected;
        } else if (selected >= scroll_offset + visible_items) {
            scroll_offset = selected - visible_items + 1;
        }
        
        clear();
        draw_header("ETHERVOX SETTINGS");
        draw_menu(items, item_count, selected, scroll_offset);
        draw_footer();
        refresh();
        
        int ch = getch();
        
        switch (ch) {
            case KEY_UP:
                do {
                    selected = (selected - 1 + item_count) % item_count;
                } while (strncmp(items[selected].label, "---", 3) == 0);
                break;
                
            case KEY_DOWN:
                do {
                    selected = (selected + 1) % item_count;
                } while (strncmp(items[selected].label, "---", 3) == 0);
                break;
                
            case KEY_LEFT:
                if (items[selected].type == MENU_ITEM_TOGGLE && items[selected].value_ptr) {
                    bool* val = (bool*)items[selected].value_ptr;
                    *val = false;
                    show_status("Setting disabled");
                } else if (items[selected].type == MENU_ITEM_NUMERIC && items[selected].value_ptr) {
                    uint32_t* val = (uint32_t*)items[selected].value_ptr;
                    // Smart decrement based on value magnitude
                    uint32_t decrement = (*val > 1000) ? 128 : (*val > 100) ? 16 : (*val > 10) ? 1 : 1;
                    if (*val > decrement) {
                        *val -= decrement;
                    } else {
                        *val = 0;
                    }
                    show_status("Value decreased");
                } else if (items[selected].type == MENU_ITEM_FLOAT && items[selected].value_ptr) {
                    float* val = (float*)items[selected].value_ptr;
                    if (*val > 0.0f) {
                        *val -= 0.05f;
                        if (*val < 0.0f) *val = 0.0f;
                        show_status("Value decreased");
                    }
                }
                break;
                
            case KEY_RIGHT:
                if (items[selected].type == MENU_ITEM_TOGGLE && items[selected].value_ptr) {
                    bool* val = (bool*)items[selected].value_ptr;
                    *val = true;
                    show_status("Setting enabled");
                } else if (items[selected].type == MENU_ITEM_NUMERIC && items[selected].value_ptr) {
                    uint32_t* val = (uint32_t*)items[selected].value_ptr;
                    // Smart increment based on value magnitude
                    uint32_t increment = (*val >= 1000) ? 128 : (*val >= 100) ? 16 : (*val >= 10) ? 1 : 1;
                    *val += increment;
                    
                    // Apply field-specific caps
                    if (val == &persistent.llm.gpu_layers || val == &persistent.governor.gpu_layers) {
                        if (*val > 999) *val = 999;  // GPU layers cap
                    } else if (val == &persistent.governor.context_size) {
                        if (*val > 16384) *val = 16384;  // Context size cap
                    } else {
                        if (*val > 16384) *val = 16384;  // Default cap
                    }
                    show_status("Value increased");
                } else if (items[selected].type == MENU_ITEM_FLOAT && items[selected].value_ptr) {
                    float* val = (float*)items[selected].value_ptr;
                    *val += 0.05f;
                    // Cap at reasonable maximum
                    if (*val > 2.0f) *val = 2.0f;
                    show_status("Value increased");
                }
                break;
                
            case 10: // Enter
            case KEY_ENTER:
                if (items[selected].type == MENU_ITEM_TOGGLE && items[selected].value_ptr) {
                    bool* val = (bool*)items[selected].value_ptr;
                    *val = !(*val);
                    show_status(*val ? "Enabled" : "Disabled");
                } else if (items[selected].type == MENU_ITEM_ACTION && items[selected].action) {
                    items[selected].action(items[selected].action_data);
                    show_status("Action completed");
                } else if (items[selected].type == MENU_ITEM_BACK) {
                    done = true;
                }
                break;
                
            case 19: // Ctrl+S (save settings)
                // Save settings immediately without exiting
                {
                    ethervox_result_t save_result = ethervox_settings_save(&persistent, NULL);
                    if (ethervox_is_success(save_result)) {
                        show_status("[OK] Settings saved successfully!");
                    } else {
                        show_status("⚠ Failed to save settings");
                    }
                }
                break;
                
            case 'q':
            case 'Q':
            case 27: // ESC
                done = true;
                break;
        }
    }
    
    cleanup_display();
    
    // Check if any model reload parameters changed (LLM settings affect Governor model)
    bool reload_required = false;
    reload_required |= (persistent.llm.gpu_layers != initial_llm_gpu_layers);
    reload_required |= (persistent.llm.context_length != initial_llm_context);
    reload_required |= (persistent.llm.n_threads != initial_llm_threads);
    
    // Save persistent settings to disk
    ethervox_result_t save_result = ethervox_settings_save(&persistent, NULL);
    if (ethervox_is_success(save_result)) {
        printf("\n[OK] Settings saved to %s\n\n", ethervox_settings_get_default_path());
    } else {
        printf("\n⚠ Warning: Failed to save settings: %s\n\n", ethervox_error_string(save_result));
    }
    
    // Prompt for model reload if needed (only if model is loaded)
    if (reload_required && model_path && model_path[0] != '\0') {
        printf("+===================================================================+\n");
        printf("|                   MODEL RELOAD REQUIRED                           |\n");
        printf("+===================================================================+\n\n");
        printf("The following settings require reloading the model:\n");
        if (persistent.llm.gpu_layers != initial_llm_gpu_layers) {
            printf("  • LLM GPU Layers: %u → %u\n", initial_llm_gpu_layers, persistent.llm.gpu_layers);
        }
        if (persistent.llm.context_length != initial_llm_context) {
            printf("  • LLM Context Length: %u → %u\n", initial_llm_context, persistent.llm.context_length);
        }
        if (persistent.llm.n_threads != initial_llm_threads) {
            printf("  • LLM Threads: %d → %d\n", initial_llm_threads, persistent.llm.n_threads);
        }
        printf("\nCurrent model: %s\n\n", model_path);
        printf("Would you like to reload the model now? (y/n): ");
        fflush(stdout);
        
        char response[10];
        if (fgets(response, sizeof(response), stdin)) {
            if (response[0] == 'y' || response[0] == 'Y') {
                printf("\n🔄 Reloading model...\n");
                
                // Trigger model reload via callback
                if (reload_callback) {
                    ethervox_result_t ret = reload_callback(model_path, user_data);
                    if (ethervox_is_success(ret)) {
                        printf("[OK] Model reloaded successfully with new settings\n\n");
                    } else {
                        printf("[FAIL] Failed to reload model (code: %d)\n", ret);
                        printf("  You can manually reload with: /load %s\n\n", model_path);
                    }
                } else {
                    printf("⚠ No reload callback available\n");
                    printf("  Reload manually with: /load %s\n\n", model_path);
                }
            } else {
                printf("\n⚠ Model not reloaded. New settings will apply on next model load.\n");
                printf("  Reload manually with: /load %s\n\n", model_path);
            }
        }
    }
    
    return ETHERVOX_SUCCESS;
}

bool ethervox_settings_menu_available(void) {
    return true;
}

#else // No ncurses available

ethervox_result_t ethervox_settings_menu_show(ethervox_settings_t* settings, const char* model_path,
                                 ethervox_model_reload_callback_t reload_callback, void* user_data,
                                 ethervox_tts_reload_callback_t tts_reload_callback, void* tts_user_data) {
    (void)model_path;
    (void)reload_callback;
    (void)user_data;
    fprintf(stderr, "Settings menu not available on this platform (ncurses not found)\n");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
}

bool ethervox_settings_menu_available(void) {
    return false;
}

#endif
