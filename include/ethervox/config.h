/**
 * @file config.h
 * @brief Configuration definitions and platform detection for EthervoxAI
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * 
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - NonCommercial: Not for commercial use
 * - ShareAlike: Distribute under same license
 * 
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_CONFIG_H
#define ETHERVOX_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// Platform detection macros
#if defined(TARGET_ESP32)
    #define ETHERVOX_PLATFORM_ESP32 1
    #define ETHERVOX_PLATFORM_EMBEDDED 1
#elif defined(TARGET_RPI)
    #define ETHERVOX_PLATFORM_RPI 1
    #define ETHERVOX_PLATFORM_EMBEDDED 1
#elif defined(TARGET_WINDOWS)
    #define ETHERVOX_PLATFORM_WINDOWS 1
    #define ETHERVOX_PLATFORM_DESKTOP 1
#elif defined(TARGET_LINUX)
    #define ETHERVOX_PLATFORM_LINUX 1
    #define ETHERVOX_PLATFORM_DESKTOP 1
#elif defined(TARGET_MACOS)
    #define ETHERVOX_PLATFORM_MACOS 1
    #define ETHERVOX_PLATFORM_DESKTOP 1
#endif

// Feature configuration
#ifndef ETHERVOX_MAX_LANGUAGES
    #ifdef ETHERVOX_PLATFORM_EMBEDDED
        #define ETHERVOX_MAX_LANGUAGES 3
    #else
        #define ETHERVOX_MAX_LANGUAGES 15
    #endif
#endif

#ifndef ETHERVOX_AUDIO_SAMPLE_RATE
    #define ETHERVOX_AUDIO_SAMPLE_RATE 16000
#endif

#ifndef ETHERVOX_AUDIO_BUFFER_SIZE
    #ifdef ETHERVOX_PLATFORM_EMBEDDED
        #define ETHERVOX_AUDIO_BUFFER_SIZE 1024
    #else
        #define ETHERVOX_AUDIO_BUFFER_SIZE 4096
    #endif
#endif

#ifndef ETHERVOX_MAX_PLUGINS
    #ifdef ETHERVOX_PLATFORM_EMBEDDED
        #define ETHERVOX_MAX_PLUGINS 8
    #else
        #define ETHERVOX_MAX_PLUGINS 32
    #endif
#endif

// Debug configuration
#ifdef DEBUG_ENABLED
    #define ETHERVOX_DEBUG 1
    #define ETHERVOX_LOG_LEVEL 0  // Verbose
#else
    #define ETHERVOX_DEBUG 0
    #define ETHERVOX_LOG_LEVEL 2  // Error only
#endif

// Version information
#define ETHERVOX_VERSION_MAJOR 0
#define ETHERVOX_VERSION_MINOR 1
#define ETHERVOX_VERSION_PATCH 0
#define ETHERVOX_VERSION_STRING "0.1.0"

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_CONFIG_H