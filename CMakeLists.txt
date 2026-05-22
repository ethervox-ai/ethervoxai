cmake_minimum_required(VERSION 3.16)

project(EthervoxAI VERSION 0.0.5 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =============================================================================
# Git Information Extraction (REQUIRED)
# =============================================================================

# Get version tag (cpp repo uses v* pattern, not backend-v*)
execute_process(
    COMMAND git describe --tags --abbrev=0 --match=v*
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_TAG_RESULT
)

# Get branch name (REQUIRED)
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_BRANCH_RESULT
)

# Get commit hash (REQUIRED)
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_COMMIT_RESULT
)

# Verify git information is available
if(GIT_BRANCH_RESULT OR NOT GIT_BRANCH)
    message(FATAL_ERROR "Failed to get git branch. Ensure this is a git repository with commits.")
endif()

if(GIT_COMMIT_RESULT OR NOT GIT_COMMIT_HASH)
    message(FATAL_ERROR "Failed to get git commit hash. Ensure this is a git repository with commits.")
endif()

message(STATUS "Git Branch: ${GIT_BRANCH}")
message(STATUS "Git Commit: ${GIT_COMMIT_HASH}")

# Set backend version string
if(GIT_TAG_RESULT EQUAL 0 AND GIT_VERSION_TAG)
    # Tag exists - strip 'v' prefix (e.g., v0.0.5 -> 0.0.5)
    string(REPLACE "v" "" ETHERVOX_BACKEND_VERSION "${GIT_VERSION_TAG}")
    message(STATUS "Backend Version Tag: ${ETHERVOX_BACKEND_VERSION}")
else()
    # No tag - use branch-commit format
    set(ETHERVOX_BACKEND_VERSION "${GIT_BRANCH}-${GIT_COMMIT_HASH}")
    message(STATUS "Backend Version (no tag): ${ETHERVOX_BACKEND_VERSION}")
endif()

# Always define these at compile time
add_definitions(-DETHERVOX_BACKEND_VERSION="${ETHERVOX_BACKEND_VERSION}")
add_definitions(-DETHERVOX_GIT_BRANCH="${GIT_BRANCH}")
add_definitions(-DETHERVOX_GIT_COMMIT="${GIT_COMMIT_HASH}")

# =============================================================================
# GitHub Token Configuration (REQUIRED for bug reporter)
# =============================================================================
# Read GitHub token from environment variable at build time and validate it
# Priority: ETHERVOX_GITHUB_TOKEN_DESKTOP → ETHERVOX_GITHUB_TOKEN
# 
# The token MUST be set for builds to succeed. The validation script will:
# 1. Check if token exists
# 2. Verify token is valid via GitHub API
# 3. Check token has not expired
# 4. Verify access to timkos98/ethervoxai repository
#
# To create a token: https://github.com/settings/tokens
# Required permission: issues:write for timkos98/ethervoxai
#
# GitHub token support removed - bug reporter feature is deprecated
# The ETHERVOX_GITHUB_TOKEN definition is kept for backwards compatibility
set(ETHERVOX_GITHUB_TOKEN "")
add_definitions(-DETHERVOX_GITHUB_TOKEN="")

# Parse version components (MAJOR.MINOR.PATCH) only if we have a proper tag
if(GIT_TAG_RESULT EQUAL 0 AND GIT_VERSION_TAG AND ETHERVOX_BACKEND_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(VERSION_MAJOR ${CMAKE_MATCH_1})
    set(VERSION_MINOR ${CMAKE_MATCH_2})
    set(VERSION_PATCH ${CMAKE_MATCH_3})
    add_definitions(-DETHERVOX_VERSION_MAJOR=${VERSION_MAJOR})
    add_definitions(-DETHERVOX_VERSION_MINOR=${VERSION_MINOR})
    add_definitions(-DETHERVOX_VERSION_PATCH=${VERSION_PATCH})
    message(STATUS "Version components: ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
else()
    # No version tag - components remain 0.0.0 from config.h defaults
    message(STATUS "No version tag found - using 0.0.0 from config.h defaults")
endif()

# Platform detection and configuration
# Only auto-detect if not cross-compiling
if(NOT DEFINED TARGET_PLATFORM AND NOT CMAKE_TOOLCHAIN_FILE)
    if(ANDROID)
        set(TARGET_PLATFORM "ANDROID")
    elseif(WIN32)
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
    if(ANDROID)
        set(TARGET_PLATFORM "ANDROID")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
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
    add_definitions(-DETHERVOX_PLATFORM_RPI=1)  # Define as preprocessor macro
    add_definitions(-DPLATFORM_EMBEDDED=1)
    set(ETHERVOX_PLATFORM_RPI 1)  # Set as CMake variable too
    
elseif(TARGET_PLATFORM STREQUAL "WINDOWS")
    # Windows specific settings
    add_definitions(-DTARGET_WINDOWS=1)
    add_definitions(-DETHERVOX_PLATFORM_WINDOWS=1)
    add_definitions(-DETHERVOX_PLATFORM_DESKTOP=1)
    add_definitions(-DPLATFORM_DESKTOP=1)
    
    # Windows compatibility: Disable advanced KV cache features that cause crashes
    # LM Studio and llama.cpp CLI use standard F16 cache - we should too
    add_definitions(-DETHERVOX_GOVERNOR_KV_CACHE_TYPE=GGML_TYPE_F16)  # Use F16 instead of Q8_0
    add_definitions(-DETHERVOX_GOVERNOR_FLASH_ATTN_TYPE=0)            # Disable flash attention
    
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

elseif(TARGET_PLATFORM STREQUAL "ANDROID")
    # Android specific settings
    add_definitions(-DTARGET_ANDROID=1)
    add_definitions(-DETHERVOX_PLATFORM_ANDROID=1)
    add_definitions(-DPLATFORM_MOBILE=1)
    set(ETHERVOX_PLATFORM_ANDROID 1)
    set(ETHERVOX_PLATFORM_MOBILE 1)
    
    # Architecture-specific optimizations for Android - SAFE BASELINE
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        # ARM64 baseline optimizations - disabled SIMD/crypto to fix SIGILL crash
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fno-finite-math-only -march=armv8-a")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fno-finite-math-only -march=armv8-a")
        message(STATUS "Enabled SAFE ARM64 baseline optimizations for ${ANDROID_ABI}")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        # x86_64 AVX optimizations - maximum performance
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -fno-finite-math-only -march=x86-64-v3 -mtune=generic")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -fno-finite-math-only -march=x86-64-v3 -mtune=generic")
        message(STATUS "Enabled MAXIMUM x86_64 optimizations for ${ANDROID_ABI}")
    endif()
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
if(DEFINED ETHERVOX_PLATFORM_ANDROID)
    message(STATUS "ETHERVOX_PLATFORM_ANDROID is DEFINED")
endif()
if(DEFINED ETHERVOX_PLATFORM_MOBILE)
    message(STATUS "ETHERVOX_PLATFORM_MOBILE is DEFINED")
endif()

# Define runtime directory paths based on platform
if(TARGET_PLATFORM STREQUAL "WINDOWS")
    # Windows: %USERPROFILE%\.ethervox
    add_definitions(-DETHERVOX_RUNTIME_DIR_BASE="%USERPROFILE%\\\\.ethervox")
elseif(TARGET_PLATFORM STREQUAL "ANDROID")
    # Android: app-specific internal storage (handled at runtime via Java)
    add_definitions(-DETHERVOX_RUNTIME_DIR_BASE=NULL)
elseif(TARGET_PLATFORM STREQUAL "ESP32")
    # ESP32: SPIFFS or SD card
    add_definitions(-DETHERVOX_RUNTIME_DIR_BASE="/spiffs/ethervox")
else()
    # Unix-like (macOS, Linux, RPI): ~/.ethervox
    add_definitions(-DETHERVOX_RUNTIME_DIR_BASE="~/.ethervox")
endif()

message(STATUS "Runtime directory base configured for ${TARGET_PLATFORM}")

# Git info already extracted and defined above
# GIT_COMMIT_HASH and GIT_BRANCH are already available

# Build configuration
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example applications" ON)
option(ENABLE_DEBUG "Enable debug logging" OFF)
option(WITH_LLAMA "Enable Llama.cpp backend support" ON)
option(WITH_TINYLLAMA "Enable TinyLlama backend support" ON)

# Fetch cJSON for settings persistence
include(FetchContent)
FetchContent_Declare(
    cJSON
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG v1.7.18
    PATCH_COMMAND ${CMAKE_COMMAND} -E echo_append ""
)
# Set minimum CMake version for cJSON subdirectory before including it
set(CMAKE_POLICY_DEFAULT_CMP0000 NEW CACHE STRING "" FORCE)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_AND_STATIC_LIBS OFF CACHE BOOL "" FORCE)
set(CJSON_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "" FORCE)
FetchContent_GetProperties(cJSON)
if(NOT cjson_POPULATED)
    FetchContent_Populate(cJSON)
    # Patch the CMakeLists.txt to require CMake 3.16
    file(READ "${cjson_SOURCE_DIR}/CMakeLists.txt" CJSON_CMAKE_CONTENT)
    string(REPLACE "cmake_minimum_required(VERSION 3.0)" 
                   "cmake_minimum_required(VERSION 3.16)" 
                   CJSON_CMAKE_CONTENT "${CJSON_CMAKE_CONTENT}")
    file(WRITE "${cjson_SOURCE_DIR}/CMakeLists.txt" "${CJSON_CMAKE_CONTENT}")
    add_subdirectory(${cjson_SOURCE_DIR} ${cjson_BINARY_DIR})
    # Remove -Werror from cjson compilation flags
    if(TARGET cjson)
        get_target_property(CJSON_FLAGS cjson COMPILE_OPTIONS)
        if(CJSON_FLAGS)
            list(REMOVE_ITEM CJSON_FLAGS "-Werror")
            set_target_properties(cjson PROPERTIES COMPILE_OPTIONS "${CJSON_FLAGS}")
        endif()
        # Android 15+ 16KB page size compatibility
        if(ANDROID)
            target_link_options(cjson PRIVATE "-Wl,-z,max-page-size=16384")
            message(STATUS "Applied 16KB page alignment to cjson library for Android 15+ compatibility")
        endif()
    endif()
endif()

# Find libcurl for bug reporter (optional on Windows, not needed on Android)
if(NOT TARGET_PLATFORM STREQUAL "ANDROID" AND NOT WIN32)
    find_package(CURL REQUIRED)
    if(CURL_FOUND)
        message(STATUS "Found CURL: ${CURL_LIBRARIES}")
    else()
        message(FATAL_ERROR "CURL not found - required for bug reporter")
    endif()
elseif(WIN32)
    # CURL is optional on Windows - bug reporter will be disabled if not found
    find_package(CURL QUIET)
    if(CURL_FOUND)
        message(STATUS "Found CURL: ${CURL_LIBRARIES} - bug reporter enabled")
    else()
        message(STATUS "CURL not found on Windows - bug reporter will be disabled")
    endif()
else()
    message(STATUS "Skipping CURL (Android uses Kotlin BugReporter)")
endif()

if(ENABLE_DEBUG)
    add_definitions(-DDEBUG_ENABLED=1)
endif()

# LLM backend configuration
if(WITH_LLAMA)
    add_definitions(-DETHERVOX_WITH_LLAMA=1)
    message(STATUS "Llama.cpp backend enabled")
endif()

if(WITH_TINYLLAMA)
    add_definitions(-DETHERVOX_WITH_TINYLLAMA=1)
    message(STATUS "TinyLlama backend enabled")
endif()

# ============================================================================
# Dependency Management - llama.cpp and whisper.cpp
# ============================================================================
# Automatically fetches dependencies if not present as git submodules
# Users can:
#   1. Use git submodules: git submodule update --init --recursive
#   2. Let CMake auto-download (default, one-time)
#   3. Provide custom paths: -DLLAMA_CPP_CUSTOM_DIR=/path/to/llama.cpp
#   4. Disable auto-fetch: -DETHERVOX_AUTO_FETCH_DEPS=OFF

include(cmake/FetchDependencies.cmake)

# Fetch llama.cpp
if(WITH_LLAMA)
    fetch_llama_cpp()
endif()

# Fetch whisper.cpp  
option(WITH_WHISPER "Enable whisper.cpp for speech-to-text" ON)
if(WITH_WHISPER)
    fetch_whisper_cpp()
endif()

# Print summary
print_dependency_status()

# ============================================================================
# llama.cpp Integration
# ============================================================================

if(WITH_LLAMA AND LLAMA_CPP_SOURCE_DIR)
    message(STATUS "Configuring llama.cpp from: ${LLAMA_CPP_SOURCE_DIR}")
    
    # llama.cpp build options
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    
    # Enable CPU backend optimizations
    set(GGML_CPU ON CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)  # Disable native optimizations (can cause slowdowns)
    
    # Android-specific settings
    if(ANDROID)
        set(LLAMA_METAL OFF CACHE BOOL "" FORCE)
        set(LLAMA_CUBLAS OFF CACHE BOOL "" FORCE)
        set(LLAMA_CLBLAST OFF CACHE BOOL "" FORCE)
        set(LLAMA_OPENBLAS OFF CACHE BOOL "" FORCE)
        set(GGML_BACKEND_DL OFF CACHE BOOL "" FORCE)
        
        # Vulkan GPU acceleration is complex on Android and requires device-specific setup
        # Currently disabled - relying on ARM NEON optimizations instead
        # Future: Enable with proper Android Vulkan SDK integration
        set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
        
        # Disable NEON for ARM processors to fix SIGILL crash
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR ANDROID_ABI STREQUAL "arm64-v8a")
            set(GGML_CPU_ARM_NEON OFF CACHE BOOL "" FORCE)
            message(STATUS "Disabled ARM NEON optimizations for llama.cpp (fixing SIGILL crash)")
        endif()
        
        # Android 15+ 16KB page size compatibility - apply to llama.cpp libraries
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")
        message(STATUS "Applied 16KB page alignment to llama.cpp libraries for Android 15+ compatibility")
    endif()
    
    # Add llama.cpp as subdirectory
    add_subdirectory(${LLAMA_CPP_SOURCE_DIR})
    
    add_definitions(-DLLAMA_CPP_AVAILABLE=1)
    message(STATUS "✓ llama.cpp successfully configured")
elseif(WITH_LLAMA)
    message(WARNING "llama.cpp not available - LLM features will be limited")
    message(WARNING "To enable: git submodule update --init or set ETHERVOX_AUTO_FETCH_DEPS=ON")
    add_definitions(-DLLAMA_CPP_AVAILABLE=0)
else()
    add_definitions(-DLLAMA_CPP_AVAILABLE=0)
endif()

# ============================================================================
# whisper.cpp Integration
# ============================================================================

if(WITH_WHISPER AND WHISPER_CPP_SOURCE_DIR)
    message(STATUS "Configuring whisper.cpp from: ${WHISPER_CPP_SOURCE_DIR}")
    
    # whisper.cpp build options
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    
    # Match llama.cpp settings for consistency
    set(GGML_CPU ON CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    
    # Android-specific settings
    if(ANDROID)
        set(WHISPER_METAL OFF CACHE BOOL "" FORCE)
        set(WHISPER_CUBLAS OFF CACHE BOOL "" FORCE)
        set(WHISPER_CLBLAST OFF CACHE BOOL "" FORCE)
        set(WHISPER_OPENBLAS OFF CACHE BOOL "" FORCE)
        
        # Disable NEON for ARM processors to fix SIGILL crash
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR ANDROID_ABI STREQUAL "arm64-v8a")
            set(GGML_CPU_ARM_NEON OFF CACHE BOOL "" FORCE)
            message(STATUS "Disabled ARM NEON optimizations for whisper.cpp (fixing SIGILL crash)")
        endif()
        
        # Android 15+ 16KB page size compatibility
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")
        message(STATUS "Applied 16KB page alignment to whisper.cpp libraries for Android 15+ compatibility")
    endif()
    
    # Add whisper.cpp as subdirectory
    add_subdirectory(${WHISPER_CPP_SOURCE_DIR})
    
    add_definitions(-DWHISPER_CPP_AVAILABLE=1)
    message(STATUS "✓ whisper.cpp successfully configured")
elseif(WITH_WHISPER)
    message(WARNING "whisper.cpp not available - Voice features will be disabled")
    message(WARNING "To enable: git submodule update --init or set ETHERVOX_AUTO_FETCH_DEPS=ON")
    add_definitions(-DWHISPER_CPP_AVAILABLE=0)
else()
    add_definitions(-DWHISPER_CPP_AVAILABLE=0)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Common infrastructure sources
set(COMMON_SOURCES
    src/common/error.c
    src/common/logging.c
    src/common/language_detector.c
    src/common/model_downloader.c
    src/config/settings.c
)

# Terminal settings menu (requires unistd.h - skip on Windows)
if(NOT WIN32)
    list(APPEND COMMON_SOURCES src/common/settings_menu.c)
endif()

# Platform-specific utility implementations
if(ETHERVOX_PLATFORM_WINDOWS)
    list(APPEND COMMON_SOURCES src/common/platform_utils_windows.c)
elseif(ETHERVOX_PLATFORM_LINUX OR ETHERVOX_PLATFORM_MACOS OR ETHERVOX_PLATFORM_RPI OR ETHERVOX_PLATFORM_ANDROID)
    list(APPEND COMMON_SOURCES src/common/platform_utils_unix.c)
endif()

# Continue adding common sources
if(WIN32)
    # Windows: Exclude pthread and M_PI dependent sources
    list(APPEND COMMON_SOURCES
        src/audio/aec_speex.c
        src/tts/tts.c
        src/tts/text_normalizer.c
        src/tts/piper_backend.c
        src/tts/phonemizer/phonemizer.c
        src/tts/phonemizer/dictionary.c
        src/tts/phonemizer/espeak_dict.c
        src/tts/phonemizer/rules_en.c
        src/tts/phonemizer/rules_de.c
        src/tts/phonemizer/dict_chinese.c
        src/tts/phonemizer/chinese_segmenter.c
        src/tts/phonemizer/pronunciation_overrides.c
        src/tts/phonemizer/stress_reduction.c
    )
    message(STATUS "Windows: Excluded reference_buffer, audio_stream_player (pthread), pronunciation_trainer (M_PI)")
else()
    list(APPEND COMMON_SOURCES
        src/audio/reference_buffer.c
        src/audio/aec_speex.c
        src/audio/audio_stream_player.c
        src/tts/tts.c
        src/tts/text_normalizer.c
        src/tts/piper_backend.c
        src/tts/phonemizer/phonemizer.c
        src/tts/phonemizer/dictionary.c
        src/tts/phonemizer/espeak_dict.c
        src/tts/phonemizer/rules_en.c
        src/tts/phonemizer/rules_de.c
        src/tts/phonemizer/dict_chinese.c
        src/tts/phonemizer/chinese_segmenter.c
        src/tts/phonemizer/pronunciation_overrides.c
        src/tts/phonemizer/pronunciation_trainer.c
        src/tts/phonemizer/stress_reduction.c
    )
endif()

# Espeak-trained dictionary embeddings (VARIANT-SPECIFIC DICTIONARIES REQUIRED)
# Each Piper model is trained with a SPECIFIC espeak language variant (e.g., en-us vs en-gb-x-rp).
# Using the wrong variant causes poor quality because espeak phonemes differ by variant:
#   en-us:     həlˈoʊ  (US English: /oʊ/ diphthong)
#   en-gb-x-rp: həlˈəʊ (British RP: /əʊ/ diphthong)
# 
# The dictionaries must match the model's phonemes.language field from its .onnx.json config.
# Generated dictionaries are variant-specific:
#   - espeak_dict_en_us.h      → for models with language: "en-us"
#   - espeak_dict_en_gb_rp.h   → for models with language: "en-gb-x-rp"
#   - espeak_dict_de.h         → for models with language: "de"
#   - espeak_dict_cmn.h        → for models with language: "cmn"
#   - espeak_dict_es_419.h     → for models with language: "es-419"
# 
# Generate all variants with: ./tools/train_phonemizer.sh (takes ~30 min per variant)
option(ENABLE_ESPEAK_DICT "Enable embedded espeak-trained dictionaries (variant-specific)" OFF)

set(ESPEAK_DICT_ENABLED FALSE)
if(ENABLE_ESPEAK_DICT)
    # Check for generated dictionary headers
    set(ESPEAK_DICT_DIR "${CMAKE_SOURCE_DIR}/src/tts/phonemizer/data")
    
    if(EXISTS "${ESPEAK_DICT_DIR}/espeak_dict_en_us.h")
        message(STATUS "✅ Espeak dictionary (en-us) found - enabling")
        add_definitions(-DESPEAK_DICT_EN_US_ENABLED)
        set(ESPEAK_DICT_ENABLED TRUE)
    else()
        message(STATUS "⚠️  Espeak dictionary (en-us) not found. Run: tools/train_phonemizer.sh")
    endif()
    
    if(EXISTS "${ESPEAK_DICT_DIR}/espeak_dict_en_gb_rp.h")
        message(STATUS "✅ Espeak dictionary (en-gb-rp) found - enabling")
        add_definitions(-DESPEAK_DICT_EN_GB_RP_ENABLED)
        set(ESPEAK_DICT_ENABLED TRUE)
    else()
        message(STATUS "⚠️  Espeak dictionary (en-gb-rp) not found. Run: tools/train_phonemizer.sh")
    endif()
    
    if(EXISTS "${ESPEAK_DICT_DIR}/espeak_dict_de.h")
        message(STATUS "✅ Espeak dictionary (de) found - enabling")
        add_definitions(-DESPEAK_DICT_DE_ENABLED)
        set(ESPEAK_DICT_ENABLED TRUE)
    else()
        message(STATUS "⚠️  Espeak dictionary (de) not found. Run: tools/train_phonemizer.sh")
    endif()
    
    if(EXISTS "${ESPEAK_DICT_DIR}/espeak_dict_cmn.h")
        message(STATUS "✅ Espeak dictionary (cmn) found - enabling")
        add_definitions(-DESPEAK_DICT_CMN_ENABLED)
        set(ESPEAK_DICT_ENABLED TRUE)
    else()
        message(STATUS "⚠️  Espeak dictionary (cmn) not found. Run: tools/train_phonemizer.sh")
    endif()
    
    if(EXISTS "${ESPEAK_DICT_DIR}/espeak_dict_es_419.h")
        message(STATUS "✅ Espeak dictionary (es-419) found - enabling")
        add_definitions(-DESPEAK_DICT_ES_419_ENABLED)
        set(ESPEAK_DICT_ENABLED TRUE)
    else()
        message(STATUS "⚠️  Espeak dictionary (es-419) not found. Run: tools/train_phonemizer.sh")
    endif()
    
    # Add the data file if any dictionaries are enabled
    if(ESPEAK_DICT_ENABLED)
        list(APPEND COMMON_SOURCES src/tts/phonemizer/espeak_dict_data.c)
        message(STATUS "✅ Espeak dictionary data file included in build")
    endif()
endif()

# Add bug_reporter.c only for non-Android platforms (Android uses Kotlin BugReporter)
if(NOT TARGET_PLATFORM STREQUAL "ANDROID" AND NOT WIN32)
    list(APPEND COMMON_SOURCES src/common/bug_reporter.c)
    message(STATUS "Bug reporter enabled")
else()
    message(STATUS "Bug reporter disabled (Android or Windows)")
endif()

# Collect core source files (non-platform specific)
file(GLOB ETHERVOXAI_CORE_SOURCES 
    "src/dialogue/*.c"
    "src/plugins/*.c"
    "src/wake_word/*.c"
    "src/stt/*.c"
    "src/governor/*.c"
    "src/plugins/compute_tools/*.c"
    "src/plugins/timer_tools/*.c"
)

# Note: src/dialogue/voice_conversation.c is included via GLOB above

# Exclude test files from the main library
list(FILTER ETHERVOXAI_CORE_SOURCES EXCLUDE REGEX ".*test_.*\\.c$")

# Explicitly add kv_cache_persistence.c in case glob misses it
list(APPEND ETHERVOXAI_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/governor/kv_cache_persistence.c")

# Exclude Windows-incompatible sources (wake_word, stt require Unix headers)
if(WIN32)
    list(FILTER ETHERVOXAI_CORE_SOURCES EXCLUDE REGEX ".*/wake_word/.*\\.c$")
    list(FILTER ETHERVOXAI_CORE_SOURCES EXCLUDE REGEX ".*/stt/.*\\.c$")
    # Exclude dialogue sources that require pthread
    list(FILTER ETHERVOXAI_CORE_SOURCES EXCLUDE REGEX ".*/dialogue/global_tts\\.c$")
    list(FILTER ETHERVOXAI_CORE_SOURCES EXCLUDE REGEX ".*/dialogue/voice_conversation\\.c$")
    list(FILTER ETHERVOXAI_CORE_SOURCES EXCLUDE REGEX ".*/dialogue/voice_training\\.c$")
    list(FILTER ETHERVOXAI_CORE_SOURCES EXCLUDE REGEX ".*/dialogue/integration_tests\\.c$")
    list(FILTER ETHERVOXAI_CORE_SOURCES EXCLUDE REGEX ".*/dialogue/llm_tool_tests\\.c$")
    message(STATUS "Excluded wake_word, STT, and pthread-dependent dialogue sources on Windows")
endif()

# Add LLM sources only if LLM backends are enabled
if(WITH_LLAMA OR WITH_TINYLLAMA)
    file(GLOB LLM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/llm/*.c")
    message(STATUS "Found LLM sources: ${LLM_SOURCES}")
    # Exclude tinyllama_backend.c as it uses outdated APIs
    list(FILTER LLM_SOURCES EXCLUDE REGEX ".*tinyllama_backend\\.c$")
    message(STATUS "After filtering: ${LLM_SOURCES}")
    list(APPEND ETHERVOXAI_CORE_SOURCES ${LLM_SOURCES})
    message(STATUS "LLM backend sources included")
else()
    message(STATUS "LLM backend sources excluded (WITH_LLAMA=OFF, WITH_TINYLLAMA=OFF)")
endif()

# Add memory tools plugin sources
if(NOT WIN32)
    file(GLOB MEMORY_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/memory_tools/*.c")
    list(APPEND ETHERVOXAI_CORE_SOURCES ${MEMORY_TOOLS_SOURCES})
    message(STATUS "Memory tools plugin sources included")
else()
    message(STATUS "Memory tools plugin SKIPPED on Windows (requires dirent.h)")
endif()

# Add file tools plugin sources
if(NOT WIN32)
    file(GLOB FILE_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/file_tools/*.c")
    list(APPEND ETHERVOXAI_CORE_SOURCES ${FILE_TOOLS_SOURCES})
    message(STATUS "File tools plugin sources included")
else()
    message(STATUS "File tools plugin SKIPPED on Windows (requires PATH_MAX)")
endif()

# Add unit conversion plugin sources
file(GLOB UNIT_CONVERSION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/unit_conversion/*.c")
list(APPEND ETHERVOXAI_CORE_SOURCES ${UNIT_CONVERSION_SOURCES})
message(STATUS "Unit conversion plugin sources included")

# Add startup prompt tools plugin sources
file(GLOB STARTUP_PROMPT_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/startup_prompt_tools/*.c")
list(APPEND ETHERVOXAI_CORE_SOURCES ${STARTUP_PROMPT_TOOLS_SOURCES})

# System info tools
file(GLOB SYSTEM_INFO_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/system_info_tools/*.c")
list(APPEND ETHERVOXAI_CORE_SOURCES ${SYSTEM_INFO_TOOLS_SOURCES})
message(STATUS "Startup prompt tools plugin sources included")

# Add context tools plugin sources
file(GLOB CONTEXT_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/context_tools/*.c")
list(APPEND ETHERVOXAI_CORE_SOURCES ${CONTEXT_TOOLS_SOURCES})
message(STATUS "Context tools plugin sources included")

# Add conversation tools plugin sources (speak, listen for LLM-controlled interaction)
if(NOT WIN32)
    file(GLOB CONVERSATION_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/conversation_tools/*.c")
    list(APPEND ETHERVOXAI_CORE_SOURCES ${CONVERSATION_TOOLS_SOURCES})
    message(STATUS "Conversation tools plugin sources included")
else()
    message(STATUS "Conversation tools plugin SKIPPED on Windows (requires voice tools)")
endif()

# Add voice tools plugin sources
if(NOT WIN32)
    file(GLOB VOICE_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/voice_tools/*.c")
    list(APPEND ETHERVOXAI_CORE_SOURCES ${VOICE_TOOLS_SOURCES})
    message(STATUS "Voice tools plugin sources included")
else()
    message(STATUS "Voice tools plugin SKIPPED on Windows (requires pthread.h)")
endif()

# Add meta tools plugin sources (get_tool_info for dynamic schema loading)
file(GLOB META_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/meta_tools/*.c")
list(APPEND ETHERVOXAI_CORE_SOURCES ${META_TOOLS_SOURCES})
message(STATUS "Meta tools plugin sources included")

# Add workspace tools plugin sources (Tauri workspace integration)
file(GLOB WORKSPACE_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/workspace_tools/*.c")
list(APPEND ETHERVOXAI_CORE_SOURCES ${WORKSPACE_TOOLS_SOURCES})
message(STATUS "Workspace tools plugin sources included")

# Check for libcurl availability (needed for weather tools)
if(NOT ANDROID)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        set(HAVE_LIBCURL TRUE)
    endif()
endif()

# Add weather tools plugin sources (Open-Meteo API integration)
# Desktop: requires libcurl, Android: uses JNI HTTP client
if(ANDROID OR HAVE_LIBCURL OR CURL_FOUND)
    file(GLOB WEATHER_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/weather_tools/*.c")
    list(APPEND ETHERVOXAI_CORE_SOURCES ${WEATHER_TOOLS_SOURCES})
    if(ANDROID)
        message(STATUS "Weather tools plugin sources included (Android JNI HTTP)")
    else()
        message(STATUS "Weather tools plugin sources included (libcurl available)")
    endif()
else()
    message(STATUS "Weather tools plugin SKIPPED (libcurl not available)")
endif()

# Note: Dialogue sources already added via initial GLOB (lines 596-603)
# and filtered for Windows compatibility above

# Shared audio core implementation (excluding platform-specific files)
list(APPEND ETHERVOXAI_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_core.c")
list(APPEND ETHERVOXAI_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_recording.c")

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
        src/platform/windows_stubs.c  # Stubs for excluded features
        src/platform/device_profile.c  # Hardware detection and adaptive configuration (needed for governor)
        src/platform/gguf_config_helper.c  # GGUF metadata extraction and config calculation
    )
elseif(APPLE OR TARGET_PLATFORM STREQUAL "MACOS")
    message(STATUS "Building for macOS - using macOS platform sources")
    set(PLATFORM_SOURCES
        src/audio/platform_macos.c
        src/platform/desktop_hal.c
        src/platform/device_profile.c  # Hardware detection and adaptive configuration (needed for governor)
        src/platform/gguf_config_helper.c  # GGUF metadata extraction and config calculation
    )
elseif(ANDROID OR TARGET_PLATFORM STREQUAL "ANDROID")
    message(STATUS "Building for Android - using Android platform sources")
    set(PLATFORM_SOURCES
        src/audio/platform_android.c
        src/platform/android_hal.c
        src/platform/device_profile.c  # Hardware detection and adaptive configuration
        src/platform/gguf_config_helper.c  # GGUF metadata extraction and config calculation
    )
else()
    message(STATUS "Building for Linux - using Linux platform sources")
    # Linux/Desktop default
    set(PLATFORM_SOURCES
        src/audio/platform_linux.c
        src/platform/desktop_hal.c
        src/platform/device_profile.c  # Hardware detection and adaptive configuration (needed for governor)
        src/platform/gguf_config_helper.c  # GGUF metadata extraction and config calculation
    )
endif()

# Combine core and platform sources for the reusable library
# set(ETHERVOXAI_SOURCES ${ETHERVOXAI_CORE_SOURCES} ${PLATFORM_SOURCES} src/platform/platform_core.c)

# Combine core and platform sources for the reusable library
set(ETHERVOXAI_SOURCES ${COMMON_SOURCES} ${ETHERVOXAI_CORE_SOURCES} ${PLATFORM_SOURCES} src/platform/platform_core.c)

# Add JNI wrapper for Android
if(ANDROID)
    list(APPEND ETHERVOXAI_SOURCES 
        src/platform/ethervox_android_core.c
        src/platform/model_management_jni.c
    )
endif()

# Debug: Print sources being compiled
# message(STATUS "Core sources: ${ETHERVOXAI_CORE_SOURCES}")
# message(STATUS "Platform sources: ${PLATFORM_SOURCES}")

# Debug: Print sources being compiled
message(STATUS "Common sources: ${COMMON_SOURCES}")
message(STATUS "Core sources: ${ETHERVOXAI_CORE_SOURCES}")
message(STATUS "Platform sources: ${PLATFORM_SOURCES}")
message(STATUS "FINAL ETHERVOXAI_SOURCES: ${ETHERVOXAI_SOURCES}")

# Create library - shared for Android, static for others
if(ANDROID)
    add_library(ethervoxai SHARED ${ETHERVOXAI_SOURCES})
    
    # Add 16KB page alignment for Android 15+ compatibility
    target_link_options(ethervoxai PRIVATE
        -Wl,-z,max-page-size=16384
    )
    message(STATUS "Enabled 16KB page alignment for Android")
else()
    add_library(ethervoxai STATIC ${ETHERVOXAI_SOURCES})
endif()

target_include_directories(ethervoxai
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link llama.cpp if available
if(WITH_LLAMA)
    if(TARGET llama)
        target_include_directories(ethervoxai PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/include
            ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/src
            ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/vendor
        )
        target_link_libraries(ethervoxai PUBLIC llama)
        message(STATUS "Linked llama.cpp library to ethervoxai")
    else()
        message(WARNING "llama target not found - checking for ggml and llama-base")
        if(TARGET ggml AND TARGET llama-base)
            target_include_directories(ethervoxai PRIVATE 
                ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/include
                ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/src
            )
            target_link_libraries(ethervoxai PUBLIC ggml llama-base)
            message(STATUS "Linked ggml and llama-base libraries to ethervoxai")
        else()
            message(WARNING "No llama.cpp targets found - LLM will not be available")
        endif()
    endif()
endif()

# Link whisper.cpp if available
if(WITH_WHISPER)
    if(TARGET whisper)
        target_include_directories(ethervoxai PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/external/whisper.cpp/include
            ${CMAKE_CURRENT_SOURCE_DIR}/external/whisper.cpp/src
        )
        target_link_libraries(ethervoxai PUBLIC whisper)
        message(STATUS "Linked whisper.cpp library to ethervoxai")
    else()
        message(WARNING "whisper target not found - Voice features will be limited")
    endif()
endif()

# Link cJSON for settings persistence
if(TARGET cjson)
    target_link_libraries(ethervoxai PUBLIC cjson)
    target_include_directories(ethervoxai PRIVATE ${cjson_SOURCE_DIR})
    message(STATUS "Linked cJSON library to ethervoxai")
else()
    message(WARNING "cJSON target not found - settings persistence will fail")
endif()

# Link Speex DSP for AEC (echo cancellation) and resampling - Skip on Android
# Android uses built-in audio processing or will handle this differently
find_package(PkgConfig)
if(PkgConfig_FOUND)
    if(NOT ANDROID)
        pkg_check_modules(SPEEX speexdsp)
        if(SPEEX_FOUND)
            target_link_libraries(ethervoxai PUBLIC ${SPEEX_LINK_LIBRARIES})
            target_include_directories(ethervoxai PRIVATE ${SPEEX_INCLUDE_DIRS})
            target_link_directories(ethervoxai PRIVATE ${SPEEX_LIBRARY_DIRS})
            message(STATUS "Linked Speex DSP library: ${SPEEX_VERSION}")
            message(STATUS "Speex lib dirs: ${SPEEX_LIBRARY_DIRS}")
            message(STATUS "Speex link libs: ${SPEEX_LINK_LIBRARIES}")
        else()
            message(WARNING "Speex DSP not found - AEC will use passthrough mode. Install with: brew install speexdsp")
        endif()
    else()
        message(STATUS "⊘ Skipped Speex DSP (Android uses platform audio processing)")
    endif()
    
    # Link ONNX Runtime for Piper TTS
    pkg_check_modules(ONNXRUNTIME onnxruntime)
    if(ONNXRUNTIME_FOUND)
        target_link_libraries(ethervoxai PUBLIC ${ONNXRUNTIME_LINK_LIBRARIES})
        target_include_directories(ethervoxai PRIVATE ${ONNXRUNTIME_INCLUDE_DIRS})
        target_link_directories(ethervoxai PRIVATE ${ONNXRUNTIME_LIBRARY_DIRS})
        target_compile_definitions(ethervoxai PUBLIC HAVE_PIPER_TTS=1)
        message(STATUS "Linked ONNX Runtime library: ${ONNXRUNTIME_VERSION}")
        message(STATUS "✅ Piper TTS backend ENABLED")
    else()
        # Try manual detection (Homebrew on macOS)
        find_path(ONNXRUNTIME_INCLUDE_DIR 
            NAMES onnxruntime_c_api.h
            PATHS /opt/homebrew/Cellar/onnxruntime/*/include/onnxruntime
                  /usr/local/Cellar/onnxruntime/*/include/onnxruntime
                  /opt/homebrew/include/onnxruntime
                  /usr/local/include/onnxruntime
        )
        find_library(ONNXRUNTIME_LIBRARY
            NAMES onnxruntime
            PATHS /opt/homebrew/Cellar/onnxruntime/*/lib
                  /usr/local/Cellar/onnxruntime/*/lib
                  /opt/homebrew/lib
                  /usr/local/lib
        )
        
        if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
            # Include parent directory so we can use #include <onnxruntime/...>
            get_filename_component(ONNXRUNTIME_INCLUDE_PARENT "${ONNXRUNTIME_INCLUDE_DIR}" DIRECTORY)
            target_include_directories(ethervoxai PRIVATE ${ONNXRUNTIME_INCLUDE_PARENT})
            target_link_libraries(ethervoxai PUBLIC ${ONNXRUNTIME_LIBRARY})
            target_compile_definitions(ethervoxai PUBLIC HAVE_PIPER_TTS=1)
            message(STATUS "Found ONNX Runtime manually:")
            message(STATUS "  Include: ${ONNXRUNTIME_INCLUDE_PARENT}")
            message(STATUS "  Library: ${ONNXRUNTIME_LIBRARY}")
            message(STATUS "✅ Piper TTS backend ENABLED")
        else()
            message(WARNING "ONNX Runtime not found - Piper TTS will be unavailable. Install with: brew install onnxruntime")
        endif()
    endif()
    
    # espeak-ng REMOVED - replaced with custom phonemizer (MIT-licensed)
    # See src/tts/phonemizer.c for dictionary-based G2P implementation
    message(STATUS "Using custom phonemizer (espeak-ng dependency removed for licensing)")
    
    # Link libcurl for HTTP downloads (model downloader) - Skip on Android (uses Kotlin HTTP)
    if(NOT ANDROID)
        pkg_check_modules(CURL libcurl)
        if(CURL_FOUND)
            target_link_libraries(ethervoxai PUBLIC ${CURL_LINK_LIBRARIES})
            target_include_directories(ethervoxai PRIVATE ${CURL_INCLUDE_DIRS})
            target_link_directories(ethervoxai PRIVATE ${CURL_LIBRARY_DIRS})
            target_compile_definitions(ethervoxai PUBLIC HAVE_LIBCURL=1)
            message(STATUS "✓ Linked libcurl: ${CURL_VERSION}")
        else()
            # Try find_package fallback for Windows
            find_package(CURL)
            if(CURL_FOUND)
                target_link_libraries(ethervoxai PUBLIC CURL::libcurl)
                target_compile_definitions(ethervoxai PUBLIC HAVE_LIBCURL=1)
                message(STATUS "✓ Linked libcurl via find_package")
            else()
                message(WARNING "libcurl not found - model downloads will require manual installation")
                target_compile_definitions(ethervoxai PUBLIC HAVE_LIBCURL=0)
            endif()
        endif()
    else()
        message(STATUS "⊘ Skipped libcurl (Android uses Kotlin HTTP)")
        target_compile_definitions(ethervoxai PUBLIC HAVE_LIBCURL=0)
    endif()
else()
    if(NOT ANDROID)
        message(WARNING "PkgConfig not found - trying direct libcurl detection")
        
        # Try to find libcurl without pkg-config (Windows MinGW)
        find_package(CURL)
        if(CURL_FOUND)
            target_link_libraries(ethervoxai PUBLIC CURL::libcurl)
            target_compile_definitions(ethervoxai PUBLIC HAVE_LIBCURL=1)
            message(STATUS "✓ Linked libcurl via find_package")
        else()
            message(WARNING "libcurl not found - model downloads will require manual installation")
            target_compile_definitions(ethervoxai PUBLIC HAVE_LIBCURL=0)
        endif()
    else()
        message(STATUS "⊘ Skipped libcurl (Android uses Kotlin HTTP)")
        target_compile_definitions(ethervoxai PUBLIC HAVE_LIBCURL=0)
    endif()
endif()

# Add platform HTTP utilities (uses libcurl) - only on non-Android platforms with curl
# Android uses Kotlin ModelDownloader.kt for HTTP downloads instead
if(NOT ANDROID)
    get_target_property(HAVE_CURL ethervoxai COMPILE_DEFINITIONS)
    if(HAVE_CURL MATCHES "HAVE_LIBCURL=1")
        target_sources(ethervoxai PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/common/platform_http.c)
        message(STATUS "✓ Enabled platform_http.c (libcurl available)")
    else()
        message(STATUS "⊘ Skipped platform_http.c (libcurl not available)")
    endif()
else()
    message(STATUS "⊘ Skipped platform_http.c (Android uses Kotlin HTTP)")
endif()

# Platform-specific linking for library consumers
set(ETHERVOXAI_PLATFORM_LIBS "")

if(WIN32)
    set(ETHERVOXAI_PLATFORM_LIBS winmm ws2_32 crypt32 wininet)
elseif(TARGET_PLATFORM STREQUAL "RPI")
    # For cross-compilation, link libraries directly (they're installed via apt for the target arch)
    set(ETHERVOXAI_PLATFORM_LIBS asound pthread curl ssl crypto m)
    
    # Only search for bcm2835 in sysroot if CMAKE_SYSROOT is defined
    if(CMAKE_SYSROOT)
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
    else()
        # No sysroot - skip bcm2835 (typical for GitHub Actions cross-compilation)
        target_compile_definitions(ethervoxai PUBLIC ETHERVOX_HAVE_BCM2835=0)
        message(STATUS "No CMAKE_SYSROOT defined; skipping bcm2835 library search")
    endif()

elseif(ANDROID)
    # Android libraries
    find_library(LOG_LIB log)
    find_library(ANDROID_LIB android)
    set(ETHERVOXAI_PLATFORM_LIBS ${LOG_LIB} ${ANDROID_LIB} m)
    
    # Add AAudio for API 26+, OpenSL ES for API 24+
    if(ANDROID_PLATFORM_LEVEL GREATER_EQUAL 26)
        find_library(AAUDIO_LIB aaudio)
        if(AAUDIO_LIB)
            list(APPEND ETHERVOXAI_PLATFORM_LIBS ${AAUDIO_LIB})
            message(STATUS "AAudio library found: ${AAUDIO_LIB}")
        endif()
    endif()
    
    find_library(OPENSLES_LIB OpenSLES)
    if(OPENSLES_LIB)
        list(APPEND ETHERVOXAI_PLATFORM_LIBS ${OPENSLES_LIB})
        message(STATUS "OpenSL ES library found: ${OPENSLES_LIB}")
    endif()
    
elseif(UNIX AND NOT APPLE)
    set(ETHERVOXAI_PLATFORM_LIBS asound pthread ${CURL_LIBRARIES} ssl crypto m)
elseif(APPLE)
    set(ETHERVOXAI_PLATFORM_LIBS "-framework CoreAudio" "-framework AudioToolbox" "-framework AudioUnit" "-framework CoreFoundation" pthread ${CURL_LIBRARIES})
endif()

if(ETHERVOXAI_PLATFORM_LIBS)
    target_link_libraries(ethervoxai PUBLIC ${ETHERVOXAI_PLATFORM_LIBS})
    target_include_directories(ethervoxai PRIVATE ${CURL_INCLUDE_DIRS})
endif()

set_property(GLOBAL PROPERTY ETHERVOXAI_PLATFORM_LIBS "${ETHERVOXAI_PLATFORM_LIBS}")

# CLI executable built on top of the reusable library (not for Android or Windows)
if(NOT ANDROID AND NOT WIN32)
    add_executable(ethervoxai_app src/main.c)
    target_link_libraries(ethervoxai_app PRIVATE ethervoxai)
    
    # Add readline for command history on macOS/Linux
    if(APPLE OR UNIX)
        find_library(READLINE_LIBRARY readline)
        if(READLINE_LIBRARY)
            target_link_libraries(ethervoxai_app PRIVATE ${READLINE_LIBRARY})
            message(STATUS "Readline library found: ${READLINE_LIBRARY}")
        else()
            message(WARNING "Readline library not found - command history will be disabled")
        endif()
        
        # Add ncurses for settings menu
        find_library(NCURSES_LIBRARY ncurses)
        if(NCURSES_LIBRARY)
            target_link_libraries(ethervoxai_app PRIVATE ${NCURSES_LIBRARY})
            message(STATUS "Ncurses library found: ${NCURSES_LIBRARY}")
        else()
            message(WARNING "Ncurses library not found - settings menu will be disabled")
        endif()
    endif()
    
    set_target_properties(ethervoxai_app PROPERTIES OUTPUT_NAME ethervoxai)
endif()

# Voice quality tuner tool (Unix only - uses termios.h)
if(NOT ANDROID AND NOT WIN32)
    add_executable(voice_tuner tools/voice_tuner.c)
    target_link_libraries(voice_tuner PRIVATE ethervoxai cjson)
    target_include_directories(voice_tuner PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${cjson_SOURCE_DIR})
    set_target_properties(voice_tuner PROPERTIES OUTPUT_NAME ethervox-tune-voice)
    install(TARGETS voice_tuner RUNTIME DESTINATION bin)
    
    # Emotion tester tool for multi-speaker models
    add_executable(emotion_tester tools/emotion_tester.c)
    target_link_libraries(emotion_tester PRIVATE ethervoxai cjson)
    target_include_directories(emotion_tester PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${cjson_SOURCE_DIR})
    set_target_properties(emotion_tester PROPERTIES OUTPUT_NAME ethervox-test-emotions)
    install(TARGETS emotion_tester RUNTIME DESTINATION bin)
endif()

# Optional: Examples and tests (if directories exist and not Android)
if(BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples" AND NOT ANDROID)
    add_subdirectory(examples)
endif()

if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests" AND NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS ethervoxai
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)
if(NOT ANDROID AND NOT WIN32)
    install(TARGETS ethervoxai_app RUNTIME DESTINATION bin)
endif()
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