/**
 * @file main.c
 * @brief Main entry point for EthervoxAI application
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

#include <stdio.h>
#include <stdlib.h>
#include "ethervox/config.h"
#include "ethervox/platform.h"

int main(void) {
    printf("EthervoxAI v%s\n", ETHERVOX_VERSION_STRING);
    
    #ifdef ETHERVOX_PLATFORM_RPI
        printf("Platform: Raspberry Pi\n");
    #elif defined(ETHERVOX_PLATFORM_ESP32)
        printf("Platform: ESP32\n");
    #elif defined(ETHERVOX_PLATFORM_DESKTOP)
        printf("Platform: Desktop\n");
    #else
        printf("Platform: Unknown\n");
    #endif

    // Initialize platform
    ethervox_platform_t platform = {0};
    int result = ethervox_platform_register_hal(&platform);
    if (result == 0) {
        printf("Platform HAL registered: %s\n", platform.info.platform_name);
    }
    
    printf("Core modules initialization complete.\n");
    
    return 0;
}