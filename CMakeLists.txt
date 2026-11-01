cmake_minimum_required(VERSION 3.16)

project(EthervoxAI VERSION 0.0.5 LANGUAGES C CXX)

# Enable Objective-C for Apple-platform-specific shims (e.g. weather_http_ios.m,
# and the future AVAudioSession audio HAL shim for iOS voice capture) - the
# C/CXX languages declared above do not include OBJC by default, and CMake
# will fail to compile any .m/.mm source without this.
if(APPLE)
    enable_language(OBJC)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =============================================================================
# Git Information Extraction (REQUIRED)
# =============================================================================

# Get version tag. Releases are tagged `core-vX.Y.Z`; older tags are plain `vX.Y.Z`, used only
# when no `core-v*` tag is reachable.
execute_process(
    COMMAND git describe --tags --abbrev=0 --match=core-v*
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_TAG_RESULT
)
if(NOT GIT_TAG_RESULT EQUAL 0 OR NOT GIT_VERSION_TAG)
    execute_process(
        COMMAND git describe --tags --abbrev=0 --match=v*
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_TAG_RESULT
    )
endif()

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
    # Tag exists - strip the prefix (core-v0.5.0 -> 0.5.0, v0.0.5 -> 0.0.5)
    string(REGEX REPLACE "^(core-)?v" "" ETHERVOX_BACKEND_VERSION "${GIT_VERSION_TAG}")
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

elseif(TARGET_PLATFORM STREQUAL "IOS" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # iOS platform configuration
    message(STATUS "===================================================")
    message(STATUS "iOS platform configuration enabled")
    add_definitions(-DTARGET_IOS=1)
    add_definitions(-DETHERVOX_PLATFORM_IOS=1)
    add_definitions(-DETHERVOX_PLATFORM_MACOS=1)  # iOS shares many macOS APIs
    add_definitions(-DPLATFORM_MOBILE=1)
    set(ETHERVOX_PLATFORM_IOS 1)
    set(ETHERVOX_PLATFORM_MACOS 1)  # For platform_utils_unix.c
    set(ETHERVOX_PLATFORM_MOBILE 1)

    # iOS requires all libraries to be static (no dylibs in app bundles)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries for iOS" FORCE)
    message(STATUS "Forcing static libraries for iOS platform")
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

# Optional network / mutating-file subsystems (TASK-C1.0) - defaults ON, all
# guards live in cmake/EthervoxFeatures.cmake so profiles can compose them.
include(cmake/EthervoxFeatures.cmake)

# Build configuration
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example applications" ON)
option(BUILD_EXECUTABLES "Build CLI executables (main app and tools)" ON)
option(ENABLE_DEBUG "Enable debug logging" OFF)
option(WITH_LLAMA "Enable Llama.cpp backend support" ON)
option(WITH_TINYLLAMA "Enable TinyLlama backend support" ON)
# TASK-N7.7: temporary transcription-latency instrumentation, removed by N7.8 - never on in playRelease.
option(ETHERVOX_LATENCY_TRACE "Log transcription pipeline stage timings (temporary, N7.7)" OFF)

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

# Find libcurl for bug reporter (optional on Windows, not needed on Android/iOS)
if(NOT TARGET_PLATFORM STREQUAL "ANDROID" AND NOT TARGET_PLATFORM STREQUAL "IOS" AND NOT WIN32)
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
    message(STATUS "Skipping CURL (Android/iOS uses native BugReporter)")
endif()

if(ENABLE_DEBUG)
    add_definitions(-DDEBUG_ENABLED=1)
endif()

if(ETHERVOX_LATENCY_TRACE)
    add_definitions(-DETHERVOX_LATENCY_TRACE=1)
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
# Dependency Management - llama.cpp (+ mtmd/Granite Speech)
# ============================================================================
# Automatically fetches dependencies if not present as git submodules
# Users can:
#   1. Use git submodules: git submodule update --init --recursive
#   2. Let CMake auto-download (default, one-time)
#   3. Provide custom paths: -DLLAMA_CPP_CUSTOM_DIR=/path/to/llama.cpp
#   4. Disable auto-fetch: -DETHERVOX_AUTO_FETCH_DEPS=OFF

include(cmake/FetchDependencies.cmake)

# Fetch llama.cpp (also provides mtmd - the multimodal library used to load
# Granite Speech's audio-in GGUF pair. whisper.cpp has been removed entirely;
# there is no WITH_WHISPER option anymore.)
if(WITH_LLAMA)
    fetch_llama_cpp()
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
    # iOS/Android require static linking - dynamic libs not supported in app bundles
    if(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR TARGET_PLATFORM STREQUAL "IOS" OR ANDROID)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        message(STATUS "Using static libraries for mobile platform")
    elseif(ETHERVOX_FORCE_STATIC)
        # Opt-in (default OFF, i.e. unchanged behavior for every other consumer): a macOS/desktop
        # static build, for consumers that can't ship/rpath dylibs (e.g. an Xcode/SwiftPM
        # XCFramework, `ethervoxai-apple` TASK-E6.3). Set via `-DETHERVOX_FORCE_STATIC=ON` or
        # `ev-llm`'s `EV_LLM_STATIC_APPLE=1` (which passes this through).
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        message(STATUS "Using static libraries (ETHERVOX_FORCE_STATIC)")
    else()
        set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    endif()
    
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
        
        # Vulkan GPU acceleration disabled on Android for CI/CD compatibility
        # Most Android devices don't support the required Vulkan features anyway
        # (storageBuffer16BitAccess, Vulkan 1.2, etc.)
        set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
        message(STATUS "Vulkan GPU disabled on Android (limited device support, CI build compatibility)")
        
        # Disable NEON for ARM processors to fix SIGILL crash
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR ANDROID_ABI STREQUAL "arm64-v8a")
            set(GGML_CPU_ARM_NEON OFF CACHE BOOL "" FORCE)
            message(STATUS "Disabled ARM NEON optimizations for llama.cpp (fixing SIGILL crash)")
        endif()
        
        # Android 15+ 16KB page size compatibility - apply to llama.cpp libraries
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")
        message(STATUS "Applied 16KB page alignment to llama.cpp libraries for Android 15+ compatibility")
    
    # iOS-specific optimizations
    elseif(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR TARGET_PLATFORM STREQUAL "IOS")
        message(STATUS "=== Configuring llama.cpp with iOS optimizations ===")
        
        # ✅ CRITICAL: Enable Metal GPU acceleration (10-50x speedup)
        set(GGML_METAL ON CACHE BOOL "" FORCE)
        set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
        message(STATUS "✓ Metal GPU acceleration enabled")
        
        # ✅ CRITICAL: Enable Accelerate framework (Apple BLAS, 2-5x speedup)
        set(GGML_ACCELERATE ON CACHE BOOL "" FORCE)
        set(GGML_BLAS ON CACHE BOOL "" FORCE)
        set(GGML_BLAS_VENDOR "Apple" CACHE STRING "" FORCE)
        message(STATUS "✓ Accelerate framework (Apple BLAS) enabled")
        
        # ✅ IMPORTANT: Enable ARM NEON SIMD (safe on iOS, 2-3x speedup)
        # All iOS devices are ARMv8-A minimum and support NEON
        message(STATUS "✓ ARM NEON SIMD enabled (safe on iOS)")
        
        # ❌ Disable other GPU backends (not available on iOS)
        set(GGML_CUDA OFF CACHE BOOL "" FORCE)
        set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
        set(GGML_CLBLAST OFF CACHE BOOL "" FORCE)
        set(GGML_OPENBLAS OFF CACHE BOOL "" FORCE)
        set(GGML_BACKEND_DL OFF CACHE BOOL "" FORCE)
        
        message(STATUS "=== iOS optimizations complete - expect 20-30x speedup ===")
    endif()
    
    # Add llama.cpp as subdirectory
    add_subdirectory(${LLAMA_CPP_SOURCE_DIR})

    # ------------------------------------------------------------------------
    # mtmd (multimodal library) - required for Granite Speech's audio-in GGUF.
    # llama.cpp's own tools/CMakeLists.txt (add_subdirectory(tools) at the top
    # level, gated behind LLAMA_BUILD_TOOLS) would also build llama-server,
    # llama-cli, rpc, etc. - unnecessary weight and platform risk (server
    # networking code, RPC sockets) for a mobile/embedded STT dependency. mtmd
    # itself deliberately does not depend on llama-common (enforced by its own
    # CMakeLists.txt), so it is added directly here instead, pulling in only
    # `libmtmd` (ggml + llama only) and nothing else from tools/.
    # ------------------------------------------------------------------------
    if(COMMAND llama_add_compile_flags)
        llama_add_compile_flags()
    endif()
    # LLAMA_INSTALL_VERSION is normally set by llama.cpp's own top-level
    # CMakeLists.txt (cmake/build-info.cmake), but that scope isn't visible
    # here since mtmd is added directly rather than through llama.cpp's own
    # tools/CMakeLists.txt. mtmd/CMakeLists.txt's set_target_properties()
    # call needs it defined, so provide the same default it would otherwise
    # compute (exact version number doesn't matter for our use - this is not
    # a public install target).
    if(NOT DEFINED LLAMA_INSTALL_VERSION)
        set(LLAMA_INSTALL_VERSION "0.0.0")
    endif()
    # EXCLUDE_FROM_ALL: mtmd/CMakeLists.txt unconditionally also defines
    # llama-mtmd-cli/llama-mtmd-debug executables that link against
    # llama-common (arg.h, etc.), which we deliberately don't build here (see
    # comment above - avoiding the full tools/ tree). EXCLUDE_FROM_ALL keeps
    # those extra targets out of the default build; the `mtmd` library target
    # itself still builds because ethervoxai links against it directly below.
    add_subdirectory(${LLAMA_CPP_SOURCE_DIR}/tools/mtmd mtmd_build EXCLUDE_FROM_ALL)
    if(TARGET mtmd)
        add_definitions(-DMTMD_AVAILABLE=1)
        message(STATUS "✓ mtmd (Granite Speech audio support) successfully configured")
    else()
        add_definitions(-DMTMD_AVAILABLE=0)
        message(WARNING "mtmd target not found after add_subdirectory - Granite Speech STT will not be available")
    endif()

    add_definitions(-DLLAMA_CPP_AVAILABLE=1)
    message(STATUS "✓ llama.cpp successfully configured")
elseif(WITH_LLAMA)
    message(WARNING "llama.cpp not available - LLM features will be limited")
    message(WARNING "To enable: git submodule update --init or set ETHERVOX_AUTO_FETCH_DEPS=ON")
    add_definitions(-DLLAMA_CPP_AVAILABLE=0)
    add_definitions(-DMTMD_AVAILABLE=0)
else()
    add_definitions(-DLLAMA_CPP_AVAILABLE=0)
    add_definitions(-DMTMD_AVAILABLE=0)
endif()

# ============================================================================
# mtmd / Granite Speech Integration (see llama.cpp block above)
# ============================================================================

# ============================================================================
# whisper.cpp has been removed entirely - see mtmd setup above (Granite
# Speech replaces it for all STT, both BASE and PLUS/SAA variants).
# ============================================================================

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Common infrastructure sources
set(COMMON_SOURCES
    src/common/error.c
    src/common/logging.c
    src/common/paths.c
    src/common/event_stream.c
    src/common/language_detector.c
    src/common/model_downloader.c
    src/common/version.c
    src/config/settings.c
)

# Terminal settings_menu (requires ncurses - skip on Windows, Android, iOS)
if(NOT WIN32 AND NOT ANDROID AND NOT TARGET_PLATFORM STREQUAL "IOS")
    list(APPEND COMMON_SOURCES src/common/settings_menu.c)
endif()

# Platform-specific utility implementations
if(ETHERVOX_PLATFORM_WINDOWS)
    list(APPEND COMMON_SOURCES src/common/platform_utils_windows.c)
elseif(ETHERVOX_PLATFORM_LINUX OR ETHERVOX_PLATFORM_MACOS OR ETHERVOX_PLATFORM_RPI OR ETHERVOX_PLATFORM_ANDROID OR ETHERVOX_PLATFORM_IOS)
    list(APPEND COMMON_SOURCES src/common/platform_utils_unix.c)
endif()

# Continue adding common sources
if(WIN32)
    # Windows: Exclude pthread and M_PI dependent sources
    list(APPEND COMMON_SOURCES
        src/audio/aec_speex.c
        src/tts/tts_host.c
        src/tts/text_normalizer.c
    )
    message(STATUS "Windows: Excluded reference_buffer, audio_stream_player (pthread)")
else()
    list(APPEND COMMON_SOURCES
        src/audio/reference_buffer.c
        src/audio/aec_speex.c
        src/audio/audio_stream_player.c
        src/tts/tts_host.c
        src/tts/text_normalizer.c
    )
endif()

# Phonemizer and Piper TTS removed (TASK-C1.0) - TTS now handled via ethervox_tts_host_t

# Add bug_reporter.c only for non-Android platforms (Android uses Kotlin BugReporter)
if(ETHERVOX_FEATURE_BUG_REPORT AND NOT TARGET_PLATFORM STREQUAL "ANDROID" AND NOT WIN32)
    list(APPEND COMMON_SOURCES src/common/bug_reporter.c)
    message(STATUS "Bug reporter enabled")
else()
    message(STATUS "Bug reporter disabled (ETHERVOX_FEATURE_BUG_REPORT=OFF, Android, or Windows)")
endif()

# TASK-C1.0: Exclude voice_training.c and global_tts.c - depend on removed phonemizer/TTS
file(GLOB ETHERVOXAI_CORE_SOURCES 
    "src/dialogue/*.c"
    "src/plugins/*.c"
    "src/wake_word/*.c"
    "src/stt/*.c"
    "src/governor/*.c"
    "src/plugins/compute_tools/*.c"
    "src/plugins/timer_tools/*.c"
)
list(REMOVE_ITEM ETHERVOXAI_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/dialogue/voice_training.c")
list(REMOVE_ITEM ETHERVOXAI_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/dialogue/global_tts.c")

# TASK-C2.6a: tool contracts (name/description/schema/profiles) come from
# tools/catalogue/*.json, not C literals - see ethervox_tool_catalogue_load().
# Embedded at configure time so there is no runtime file I/O (works sandboxed
# and on ESP32).
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedCatalogue.cmake)
set(ETHERVOX_CATALOGUE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${ETHERVOX_CATALOGUE_GENERATED_DIR}")
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/compute_tools.json
    ETHERVOX_CATALOGUE_COMPUTE_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/compute_tools_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/system_info_tools.json
    ETHERVOX_CATALOGUE_SYSTEM_INFO_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/system_info_tools_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/weather_tools.json
    ETHERVOX_CATALOGUE_WEATHER_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/weather_tools_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/context_tools.json
    ETHERVOX_CATALOGUE_CONTEXT_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/context_tools_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/meta_tools.json
    ETHERVOX_CATALOGUE_META_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/meta_tools_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/startup_prompt_tools.json
    ETHERVOX_CATALOGUE_STARTUP_PROMPT_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/startup_prompt_tools_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/timer_tools.json
    ETHERVOX_CATALOGUE_TIMER_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/timer_tools_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/unit_conversion.json
    ETHERVOX_CATALOGUE_UNIT_CONVERSION_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/unit_conversion_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/memory_tools.json
    ETHERVOX_CATALOGUE_MEMORY_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/memory_tools_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/file_tools.json
    ETHERVOX_CATALOGUE_FILE_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/file_tools_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/conversation_tools.json
    ETHERVOX_CATALOGUE_CONVERSATION_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/conversation_tools_catalogue.h
)
ethervox_embed_catalogue(
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/catalogue/workspace_tools.json
    ETHERVOX_CATALOGUE_WORKSPACE_TOOLS_JSON
    ${ETHERVOX_CATALOGUE_GENERATED_DIR}/workspace_tools_catalogue.h
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

# Exclude files that use system() on iOS (system() is not available on iOS)
if(TARGET_PLATFORM STREQUAL "IOS" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(FILTER ETHERVOXAI_CORE_SOURCES EXCLUDE REGEX ".*/dialogue/integration_tests\\.c$")
    list(FILTER ETHERVOXAI_CORE_SOURCES EXCLUDE REGEX ".*/dialogue/llm_tool_tests\\.c$")
    message(STATUS "Excluded files that use system() calls on iOS")
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
# TASK-C2.6b: gated on ETHERVOX_FEATURE_FILE_TOOLS (OFF for EDGE/WORKSPACE, see
# cmake/EthervoxFeatures.cmake) so an EDGE/WORKSPACE binary contains no
# file_tools code or catalogue contract, not just an unreached registration
# call - previously this flag only skipped the main.c call site, while the
# plugin (and its embedded catalogue JSON) still linked into every profile.
if(NOT WIN32 AND ETHERVOX_FEATURE_FILE_TOOLS)
    file(GLOB FILE_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/file_tools/*.c")
    list(APPEND ETHERVOXAI_CORE_SOURCES ${FILE_TOOLS_SOURCES})
    message(STATUS "File tools plugin sources included")
elseif(WIN32)
    message(STATUS "File tools plugin SKIPPED on Windows (requires PATH_MAX)")
else()
    message(STATUS "File tools plugin SKIPPED (ETHERVOX_FEATURE_FILE_TOOLS=OFF)")
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
    # TASK-C1.0: Exclude train_pronunciation.c - depends on removed phonemizer
    file(GLOB CONVERSATION_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/conversation_tools/*.c")
    list(REMOVE_ITEM CONVERSATION_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/conversation_tools/train_pronunciation.c")
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
# Desktop: requires libcurl, Android: uses JNI HTTP client, iOS: uses NSURLSession
if(ETHERVOX_FEATURE_WEATHER AND (ANDROID OR TARGET_PLATFORM STREQUAL "IOS" OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR HAVE_LIBCURL OR CURL_FOUND))
    file(GLOB WEATHER_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/weather_tools/*.c")
    if(NOT ANDROID)
        # weather_http_android.c depends on <android/log.h> and <jni.h> and
        # only makes sense on Android - exclude it from non-Android builds
        # (this glob previously pulled it in unconditionally, breaking desktop
        # builds; unrelated to the Granite Speech work but found while
        # verifying this branch compiles).
        list(FILTER WEATHER_TOOLS_SOURCES EXCLUDE REGEX ".*weather_http_android\\.c$")
    endif()
    if(TARGET_PLATFORM STREQUAL "IOS" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # weather_http_ios.m is added explicitly below (Objective-C, not picked
        # up by the *.c glob); nothing to exclude here since there's no
        # weather_http_ios.c counterpart.
        list(APPEND WEATHER_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/weather_tools/weather_http_ios.m")
    endif()
    list(APPEND ETHERVOXAI_CORE_SOURCES ${WEATHER_TOOLS_SOURCES})
    if(ANDROID)
        message(STATUS "Weather tools plugin sources included (Android JNI HTTP)")
    elseif(TARGET_PLATFORM STREQUAL "IOS" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        message(STATUS "Weather tools plugin sources included (iOS NSURLSession)")
    else()
        message(STATUS "Weather tools plugin sources included (libcurl available)")
    endif()
else()
    message(STATUS "Weather tools plugin SKIPPED (ETHERVOX_FEATURE_WEATHER=OFF or libcurl not available)")
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
elseif(TARGET_PLATFORM STREQUAL "IOS" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # ARCHITECTURE CHANGE (Granite Speech / multiplatform voice integration):
    # This branch MUST be checked before "APPLE OR MACOS" below - CMake's
    # APPLE variable is also true when cross-compiling for iOS, so without
    # this explicit branch iOS builds silently fell into the macOS platform
    # sources and src/platform/ios_hal.c was never actually compiled in.
    message(STATUS "Building for iOS - using iOS platform sources")
    set(PLATFORM_SOURCES
        src/audio/platform_ios.c
        src/platform/ios_hal.c
        src/platform/ios_avaudiosession.m  # ObjC shim: AVAudioSession category/mode config for mic capture
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

# Phonemizer removed (TASK-C1.0)

target_include_directories(ethervoxai
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ETHERVOX_CATALOGUE_GENERATED_DIR})

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
        
        # Link Vulkan backend if enabled and available
        if(GGML_VULKAN AND TARGET ggml-vulkan)
            target_link_libraries(ethervoxai PUBLIC ggml-vulkan)
            message(STATUS "Linked ggml-vulkan backend for GPU acceleration")
        endif()
        
        # Link OpenCL backend if enabled and available
        # NOTE: Excluded from APK by default (most devices don't support OpenCL)
        if(GGML_OPENCL AND TARGET ggml-opencl)
            target_link_libraries(ethervoxai PUBLIC ggml-opencl)
            message(STATUS "Linked ggml-opencl backend for GPU acceleration")
        endif()
    else()
        message(WARNING "llama target not found - checking for ggml and llama-base")
        if(TARGET ggml AND TARGET llama-base)
            target_include_directories(ethervoxai PRIVATE 
                ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/include
                ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/src
            )
            target_link_libraries(ethervoxai PUBLIC ggml llama-base)
            message(STATUS "Linked ggml and llama-base libraries to ethervoxai")
            
            # Link Vulkan backend if enabled and available
            if(GGML_VULKAN AND TARGET ggml-vulkan)
                target_link_libraries(ethervoxai PUBLIC ggml-vulkan)
                message(STATUS "Linked ggml-vulkan backend for GPU acceleration")
            endif()
            
            # Link OpenCL backend if enabled and available
            # NOTE: Excluded from APK by default (most devices don't support OpenCL)
            if(GGML_OPENCL AND TARGET ggml-opencl)
                target_link_libraries(ethervoxai PUBLIC ggml-opencl)
                message(STATUS "Linked ggml-opencl backend for GPU acceleration")
            endif()
        else()
            message(WARNING "No llama.cpp targets found - LLM will not be available")
        endif()
    endif()
endif()

# Link Vulkan library on Android when GPU acceleration is enabled
if(ANDROID AND GGML_VULKAN)
    find_library(VULKAN_LIB vulkan REQUIRED)
    target_link_libraries(ethervoxai PUBLIC ${VULKAN_LIB})
    message(STATUS "Linked Vulkan library for GPU acceleration: ${VULKAN_LIB}")
endif()

# Link OpenCL library on Android when OpenCL acceleration is enabled
if(ANDROID AND GGML_OPENCL)
    # On Android, OpenCL is a system library available at runtime
    # Link it directly - symbols will be resolved from system libOpenCL.so if available
    target_link_libraries(ethervoxai PUBLIC OpenCL)
    message(STATUS "Linked OpenCL library for GPU acceleration (runtime resolution)")
    
    # Note: The library is excluded from APK packaging by default
    # since most Android devices don't have OpenCL support
endif()

# Link mtmd (Granite Speech audio support) if available
if(WITH_LLAMA)
    if(TARGET mtmd)
        target_include_directories(ethervoxai PRIVATE
            ${LLAMA_CPP_SOURCE_DIR}/tools/mtmd
        )
        target_link_libraries(ethervoxai PUBLIC mtmd)
        message(STATUS "Linked mtmd library to ethervoxai (Granite Speech STT)")
    else()
        message(WARNING "mtmd target not found - Granite Speech STT will not be available")
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
    
    # ONNX Runtime/Piper TTS removed (TASK-C1.0) - TTS now via ethervox_tts_host_t
    
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

# Add platform HTTP utilities (uses libcurl) - only on non-Android/iOS platforms with curl
# Android uses Kotlin ModelDownloader.kt, iOS uses URLSession for HTTP downloads instead
if(ETHERVOX_FEATURE_HTTP AND NOT ANDROID AND NOT TARGET_PLATFORM STREQUAL "IOS")
    get_target_property(HAVE_CURL ethervoxai COMPILE_DEFINITIONS)
    if(HAVE_CURL MATCHES "HAVE_LIBCURL=1")
        target_sources(ethervoxai PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/common/platform_http.c)
        message(STATUS "✓ Enabled platform_http.c (libcurl available)")
    else()
        message(STATUS "⊘ Skipped platform_http.c (libcurl not available)")
    endif()
else()
    message(STATUS "⊘ Skipped platform_http.c (ETHERVOX_FEATURE_HTTP=OFF, or Android/iOS uses native HTTP)")
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
if(BUILD_EXECUTABLES AND NOT ANDROID AND NOT WIN32)
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
if(BUILD_EXECUTABLES AND NOT ANDROID AND NOT WIN32)
    add_executable(voice_tuner tools/voice_tuner.c)
    target_link_libraries(voice_tuner PRIVATE ethervoxai cjson)
    target_include_directories(voice_tuner PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${cjson_SOURCE_DIR})
    set_target_properties(voice_tuner PROPERTIES OUTPUT_NAME ethervox-tune-voice)
    if(TARGET_PLATFORM STREQUAL "IOS" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        install(TARGETS voice_tuner RUNTIME DESTINATION bin BUNDLE DESTINATION .)
    else()
        install(TARGETS voice_tuner RUNTIME DESTINATION bin)
    endif()
    
    # Emotion tester tool for multi-speaker models
    add_executable(emotion_tester tools/emotion_tester.c)
    target_link_libraries(emotion_tester PRIVATE ethervoxai cjson)
    target_include_directories(emotion_tester PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${cjson_SOURCE_DIR})
    set_target_properties(emotion_tester PROPERTIES OUTPUT_NAME ethervox-test-emotions)
    if(TARGET_PLATFORM STREQUAL "IOS" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        install(TARGETS emotion_tester RUNTIME DESTINATION bin BUNDLE DESTINATION .)
    else()
        install(TARGETS emotion_tester RUNTIME DESTINATION bin)
    endif()
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
if(BUILD_EXECUTABLES AND NOT ANDROID AND NOT WIN32)
    if(TARGET_PLATFORM STREQUAL "IOS" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        install(TARGETS ethervoxai_app 
            RUNTIME DESTINATION bin
            BUNDLE DESTINATION .)
    else()
        install(TARGETS ethervoxai_app RUNTIME DESTINATION bin)
    endif()
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