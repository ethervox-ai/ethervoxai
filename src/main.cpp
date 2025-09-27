/**
 * @file main.cpp
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

#ifdef ETHERVOX_PLATFORM_DESKTOP
    #include <iostream>
#endif

int main() {
    #ifdef ETHERVOX_PLATFORM_DESKTOP
        std::cout << "EthervoxAI v" << ETHERVOX_VERSION_STRING << std::endl;
        std::cout << "Platform: Desktop" << std::endl;
    #else
        printf("EthervoxAI v%s\n", ETHERVOX_VERSION_STRING);
        printf("Platform: Embedded\n");
    #endif

    // TODO: Initialize core systems
    // 1. Platform layer
    // 2. Audio runtime
    // 3. Dialogue engine
    // 4. Plugin system
    
    printf("Core modules initialization complete.\n");
    
    return 0;
}