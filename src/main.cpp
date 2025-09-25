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