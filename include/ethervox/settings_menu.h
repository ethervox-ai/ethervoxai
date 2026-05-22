/**
 * @file settings_menu.h
 * @brief Terminal-based settings menu using ncurses
 *
 * Provides an arrow-key navigable settings interface similar to BIOS/raspi-config.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_SETTINGS_MENU_H
#define ETHERVOX_SETTINGS_MENU_H

#include <stdbool.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Model reload callback - called when settings requiring reload are changed
 * @param model_path Path to model to reload
 * @param user_data User data pointer
 * @return ETHERVOX_SUCCESS or error code
 */
typedef ethervox_result_t (*ethervox_model_reload_callback_t)(const char* model_path, void* user_data);

/**
 * TTS reload callback - called when TTS voice settings change
 * @param settings New TTS settings
 * @param user_data User data pointer
 * @return ETHERVOX_SUCCESS or error code
 */
typedef ethervox_result_t (*ethervox_tts_reload_callback_t)(const void* settings, void* user_data);

/**
 * Settings menu context - holds all settings state
 */
typedef struct {
    bool debug_enabled;
    bool quiet_mode;
    bool streaming_enabled;
    bool engineering_mode;
    char model_path[512];
    char whisper_model_path[512];
    char memory_dir[512];
    char audio_device[256];
    char wake_word[64];
    bool wake_word_enabled;
    int log_level;
    char git_commit[64];
    char git_branch[64];
} ethervox_settings_t;

/**
 * Launch the interactive settings menu
 * 
 * @param settings Current settings to display/edit
 * @param model_path Currently loaded model path (NULL if no model loaded)
 * @param reload_callback Callback to reload model if settings change (can be NULL)
 * @param user_data User data passed to reload callback
 * @param tts_reload_callback Callback to reload TTS when voice settings change (can be NULL)
 * @param tts_user_data User data passed to TTS reload callback
 * @return 0 on success, -1 on error
 */
ethervox_result_t ethervox_settings_menu_show(ethervox_settings_t* settings, const char* model_path,
                                 ethervox_model_reload_callback_t reload_callback, void* user_data,
                                 ethervox_tts_reload_callback_t tts_reload_callback, void* tts_user_data);

/**
 * Check if ncurses is available on this platform
 * 
 * @return true if settings menu can be used
 */
bool ethervox_settings_menu_available(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_SETTINGS_MENU_H
