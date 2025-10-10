idf_component_register(
    SRCS 
        "main.c"
        # "../src/audio/audio_core.c"
        "../src/audio/platform_esp32.c"
        "../src/platform/esp32_hal.c"
        "../src/platform/platform_core.c"
        "../src/dialogue/dialogue_core.c"
        "../src/plugins/plugin_manager.c"
    INCLUDE_DIRS 
        "../include"
        "."
REQUIRES 
    driver              # GPIO, I2C, SPI, I2S drivers
    esp_wifi            # WiFi functionality
    nvs_flash           # Non-volatile storage
    esp_http_client     # HTTP client
    esp_timer           # High-resolution timers
    spi_flash           # Flash operations
    # Optional - add as needed:
    # freertos          # Already included by default
    # esp_event         # Event loop
    # esp_netif         # Network interface
    # lwip              # Lightweight IP stack
    # mbedtls           # TLS/SSL support
    # esp_adc           # ADC operations
    # esp_https_ota     # Over-the-air updates
)

# Add ESP32 platform definitions
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ETHERVOX_PLATFORM_ESP32=1
    PLATFORM_EMBEDDED=1
    ETHERVOX_VERSION_MAJOR=0
    ETHERVOX_VERSION_MINOR=1
    ETHERVOX_VERSION_PATCH=0
)

# Remove any desktop/linux definitions if present
target_compile_options(${COMPONENT_LIB} PRIVATE
    -UETHERVOX_PLATFORM_LINUX
    -UETHERVOX_PLATFORM_DESKTOP
    -UETHERVOX_PLATFORM_WINDOWS
    -UETHERVOX_PLATFORM_RPI
)

# Set C standard
set_target_properties(${COMPONENT_LIB} PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# Optimization flags for ESP32
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare
)

# Linker flags for garbage collection
target_link_options(${COMPONENT_LIB} PRIVATE
    -Wl,--gc-sections
)