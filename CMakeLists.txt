cmake_minimum_required(VERSION 3.16)

project(EthervoxAI VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform detection and configuration
if(NOT DEFINED TARGET_PLATFORM)
    if(WIN32)
        set(TARGET_PLATFORM "WINDOWS")
    elseif(UNIX AND NOT APPLE)
        set(TARGET_PLATFORM "LINUX")
    elseif(APPLE)
        set(TARGET_PLATFORM "MACOS")
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()

message(STATUS "Target platform: ${TARGET_PLATFORM}")

# Platform-specific configurations
if(TARGET_PLATFORM STREQUAL "ESP32")
    # ESP32 specific settings
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/esp32-toolchain.cmake")
    add_definitions(-DTARGET_ESP32=1)
    add_definitions(-DPLATFORM_EMBEDDED=1)
    
elseif(TARGET_PLATFORM STREQUAL "RPI")
    # Raspberry Pi specific settings
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpi-toolchain.cmake")
    add_definitions(-DTARGET_RPI=1)
    add_definitions(-DPLATFORM_EMBEDDED=1)
    
elseif(TARGET_PLATFORM STREQUAL "WINDOWS")
    # Windows specific settings
    add_definitions(-DTARGET_WINDOWS=1)
    add_definitions(-DPLATFORM_DESKTOP=1)
    
elseif(TARGET_PLATFORM STREQUAL "LINUX")
    # Linux specific settings
    add_definitions(-DTARGET_LINUX=1)
    add_definitions(-DPLATFORM_DESKTOP=1)
    
elseif(TARGET_PLATFORM STREQUAL "MACOS")
    # macOS specific settings
    add_definitions(-DTARGET_MACOS=1)
    add_definitions(-DPLATFORM_DESKTOP=1)
endif()

# Build configuration
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example applications" ON)
option(ENABLE_DEBUG "Enable debug logging" OFF)

if(ENABLE_DEBUG)
    add_definitions(-DDEBUG_ENABLED=1)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Collect source files
file(GLOB_RECURSE ETHERVOXAI_SOURCES 
    "src/*.cpp"
    "src/*.c"
    "src/audio/*.cpp"
    "src/audio/*.c"
    "src/dialogue/*.cpp"
    "src/dialogue/*.c" 
    "src/platform/*.cpp"
    "src/platform/*.c"
    "src/plugins/*.cpp"
    "src/plugins/*.c"
)

# Collect header files
file(GLOB_RECURSE ETHERVOXAI_HEADERS
    "include/*.h"
    "include/*.hpp"
    "src/*.h"
    "src/*.hpp"
)

# Main executable target
add_executable(ethervoxai ${ETHERVOXAI_SOURCES} ${ETHERVOXAI_HEADERS})

# Platform-specific linking
if(WIN32)
    target_link_libraries(ethervoxai winmm ws2_32 crypt32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(ethervoxai asound pthread curl ssl crypto)
elseif(APPLE)
    target_link_libraries(ethervoxai "-framework CoreAudio" "-framework AudioUnit" "-framework CoreFoundation" pthread curl)
endif()

# Optional: Examples and tests (if directories exist)
if(BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    add_subdirectory(examples)
endif()

if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS ethervoxai DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(FILES README.md LICENSE DESTINATION share/doc/ethervoxai)