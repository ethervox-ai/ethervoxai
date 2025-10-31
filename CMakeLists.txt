cmake_minimum_required(VERSION 3.16)

project(EthervoxAI VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform detection and configuration
# Only auto-detect if not cross-compiling
if(NOT DEFINED TARGET_PLATFORM AND NOT CMAKE_TOOLCHAIN_FILE)
    if(WIN32)
        set(TARGET_PLATFORM "WINDOWS")
    elseif(UNIX AND NOT APPLE)
        set(TARGET_PLATFORM "LINUX")
    elseif(APPLE)
        set(TARGET_PLATFORM "MACOS")
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
elseif(NOT DEFINED TARGET_PLATFORM)
    # Cross-compiling - let toolchain set the platform
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        set(TARGET_PLATFORM "RPI")
    endif()
endif()

message(STATUS "Target platform: ${TARGET_PLATFORM}")
message(STATUS "===================================================")

# Platform-specific configurations
if(TARGET_PLATFORM STREQUAL "ESP32")
    # ESP32 specific settings
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/esp32-toolchain.cmake")
    add_definitions(-DTARGET_ESP32=1)
    add_definitions(-DPLATFORM_EMBEDDED=1)
    set(ETHERVOX_PLATFORM_ESP32 1)  # Set as CMake variable too
    
elseif(TARGET_PLATFORM STREQUAL "RPI")
    # Raspberry Pi specific settings
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpi-toolchain.cmake")
    add_definitions(-DTARGET_RPI=1)
    add_definitions(-DPLATFORM_EMBEDDED=1)
    set(ETHERVOX_PLATFORM_RPI 1)  # Set as CMake variable too
    
elseif(TARGET_PLATFORM STREQUAL "WINDOWS")
    # Windows specific settings
    add_definitions(-DTARGET_WINDOWS=1)
    add_definitions(-DETHERVOX_PLATFORM_WINDOWS=1)
    add_definitions(-DETHERVOX_PLATFORM_DESKTOP=1)
    add_definitions(-DPLATFORM_DESKTOP=1)
    set(ETHERVOX_PLATFORM_WINDOWS 1)
    set(ETHERVOX_PLATFORM_DESKTOP 1)
    
elseif(TARGET_PLATFORM STREQUAL "LINUX")
    # Linux specific settings
    add_definitions(-DTARGET_LINUX=1)
    add_definitions(-DETHERVOX_PLATFORM_LINUX=1)
    add_definitions(-DETHERVOX_PLATFORM_DESKTOP=1)
    add_definitions(-DPLATFORM_DESKTOP=1)
    set(ETHERVOX_PLATFORM_LINUX 1)
    set(ETHERVOX_PLATFORM_DESKTOP 1)

elseif(TARGET_PLATFORM STREQUAL "MACOS")
    # macOS specific settings
    add_definitions(-DTARGET_MACOS=1)
    add_definitions(-DETHERVOX_PLATFORM_MACOS=1)
    add_definitions(-DETHERVOX_PLATFORM_DESKTOP=1)
    add_definitions(-DPLATFORM_DESKTOP=1)
    set(ETHERVOX_PLATFORM_MACOS 1)
    set(ETHERVOX_PLATFORM_DESKTOP 1)
endif()

# Show which platform macros are defined
message(STATUS "Show which platform macros are defined")
if(DEFINED ETHERVOX_PLATFORM_RPI)
    message(STATUS "ETHERVOX_PLATFORM_RPI is DEFINED")
endif()
if(DEFINED ETHERVOX_PLATFORM_ESP32)
    message(STATUS "ETHERVOX_PLATFORM_ESP32 is DEFINED")
endif()
if(DEFINED ETHERVOX_PLATFORM_WINDOWS)
    message(STATUS "ETHERVOX_PLATFORM_WINDOWS is DEFINED")
endif()
if(DEFINED ETHERVOX_PLATFORM_LINUX)
    message(STATUS "ETHERVOX_PLATFORM_LINUX is DEFINED")
endif()
if(DEFINED ETHERVOX_PLATFORM_DESKTOP)
    message(STATUS "ETHERVOX_PLATFORM_DESKTOP is DEFINED")
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

# Common infrastructure sources
set(COMMON_SOURCES
    src/common/error.c
    src/common/logging.c
)

# Collect core source files (non-platform specific)
file(GLOB ETHERVOXAI_CORE_SOURCES 
    "src/dialogue/*.c"
    "src/plugins/*.c"
    "src/wake_word/*.c"
    "src/stt/*.c"
)

# Shared audio core implementation (excluding platform-specific files)
list(APPEND ETHERVOXAI_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_core.c")

# Platform-specific source files
# Check multiple conditions for RPI detection
if(ETHERVOX_PLATFORM_RPI OR 
   TARGET_PLATFORM STREQUAL "RPI" OR 
   CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    message(STATUS "Building for Raspberry Pi - using RPI platform sources")
    set(PLATFORM_SOURCES
        src/audio/platform_rpi.c
        src/platform/rpi_hal.c
    )
elseif(TARGET_PLATFORM STREQUAL "ESP32")
    message(STATUS "Building for ESP32 - using ESP32 platform sources")
    set(PLATFORM_SOURCES
        src/audio/platform_esp32.c
        src/platform/esp32_hal.c
    )
elseif(WIN32 OR TARGET_PLATFORM STREQUAL "WINDOWS")
    message(STATUS "Building for Windows - using Windows platform sources")
    set(PLATFORM_SOURCES
        src/audio/platform_windows.c
        src/platform/desktop_hal.c
    )
elseif(APPLE OR TARGET_PLATFORM STREQUAL "MACOS")
    message(STATUS "Building for macOS - using macOS platform sources")
    set(PLATFORM_SOURCES
        src/audio/platform_macos.c
        src/platform/desktop_hal.c
    )
else()
    message(STATUS "Building for Linux - using Linux platform sources")
    # Linux/Desktop default
    set(PLATFORM_SOURCES
        src/audio/platform_linux.c
        src/platform/desktop_hal.c
    )
endif()

# Combine core and platform sources for the reusable library
# set(ETHERVOXAI_SOURCES ${ETHERVOXAI_CORE_SOURCES} ${PLATFORM_SOURCES} src/platform/platform_core.c)

# Combine core and platform sources for the reusable library
set(ETHERVOXAI_SOURCES ${COMMON_SOURCES} ${ETHERVOXAI_CORE_SOURCES} ${PLATFORM_SOURCES} src/platform/platform_core.c)

# Debug: Print sources being compiled
# message(STATUS "Core sources: ${ETHERVOXAI_CORE_SOURCES}")
# message(STATUS "Platform sources: ${PLATFORM_SOURCES}")

# Debug: Print sources being compiled
message(STATUS "Common sources: ${COMMON_SOURCES}")
message(STATUS "Core sources: ${ETHERVOXAI_CORE_SOURCES}")
message(STATUS "Platform sources: ${PLATFORM_SOURCES}")

# Static library that can be reused by examples/tests
add_library(ethervoxai STATIC ${ETHERVOXAI_SOURCES})
target_include_directories(ethervoxai
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Platform-specific linking for library consumers
set(ETHERVOXAI_PLATFORM_LIBS "")

if(WIN32)
    set(ETHERVOXAI_PLATFORM_LIBS winmm ws2_32 crypt32)
elseif(TARGET_PLATFORM STREQUAL "RPI")
    set(ETHERVOXAI_PLATFORM_LIBS pthread m)

    set(_ethervox_rpi_lib_paths
        ${CMAKE_SYSROOT}/lib
        ${CMAKE_SYSROOT}/lib/arm-linux-gnueabihf
        ${CMAKE_SYSROOT}/../lib
        ${CMAKE_SYSROOT}/../lib/arm-linux-gnueabihf
    )

    find_library(BCM2835_LIB bcm2835 PATHS ${_ethervox_rpi_lib_paths} NO_DEFAULT_PATH)
    if(BCM2835_LIB)
        list(APPEND ETHERVOXAI_PLATFORM_LIBS ${BCM2835_LIB})
        target_compile_definitions(ethervoxai PUBLIC ETHERVOX_HAVE_BCM2835=1)
    else()
        target_compile_definitions(ethervoxai PUBLIC ETHERVOX_HAVE_BCM2835=0)
        message(WARNING "bcm2835 library not found in Raspberry Pi sysroot; GPIO features disabled")
    endif()

    foreach(_lib_name curl ssl crypto)
        find_library(_found_lib_${_lib_name} ${_lib_name} PATHS ${_ethervox_rpi_lib_paths} NO_DEFAULT_PATH)
        if(_found_lib_${_lib_name})
            list(APPEND ETHERVOXAI_PLATFORM_LIBS ${_found_lib_${_lib_name}})
        else()
            message(WARNING "${_lib_name} library not found in Raspberry Pi sysroot; skipping link")
        endif()
    endforeach()

elseif(UNIX AND NOT APPLE)
    set(ETHERVOXAI_PLATFORM_LIBS asound pthread curl ssl crypto m)
elseif(APPLE)
    set(ETHERVOXAI_PLATFORM_LIBS "-framework CoreAudio" "-framework AudioUnit" "-framework CoreFoundation" pthread curl)
endif()

if(ETHERVOXAI_PLATFORM_LIBS)
    target_link_libraries(ethervoxai PUBLIC ${ETHERVOXAI_PLATFORM_LIBS})
endif()

set_property(GLOBAL PROPERTY ETHERVOXAI_PLATFORM_LIBS "${ETHERVOXAI_PLATFORM_LIBS}")

# CLI executable built on top of the reusable library
add_executable(ethervoxai_app src/main.c)
target_link_libraries(ethervoxai_app PRIVATE ethervoxai)
set_target_properties(ethervoxai_app PROPERTIES OUTPUT_NAME ethervoxai)

# Optional: Examples and tests (if directories exist)
if(BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    add_subdirectory(examples)
endif()

if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS ethervoxai
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)
install(TARGETS ethervoxai_app RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(FILES README.md LICENSE DESTINATION share/doc/ethervoxai)

# Basic CPack configuration so `cmake --build --target package` works everywhere
set(CPACK_PACKAGE_NAME "EthervoxAI")
set(CPACK_PACKAGE_VENDOR "EtherVoxAI")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "EtherVoxAI Voice Runtime")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "TGZ")
include(CPack)