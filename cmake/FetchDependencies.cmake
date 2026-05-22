# FetchDependencies.cmake
# Automatic dependency fetching for llama.cpp and whisper.cpp
#
# This module handles dependency acquisition with multiple strategies:
# 1. Check if external/<dep> exists (git submodule)
# 2. If not, use FetchContent to download automatically
# 3. Allow manual override via <DEP>_DIR CMake variable
#
# Usage:
#   include(cmake/FetchDependencies.cmake)
#   fetch_llama_cpp()
#   fetch_whisper_cpp()

include(FetchContent)

# Configuration options
option(ETHERVOX_AUTO_FETCH_DEPS "Automatically fetch missing dependencies" ON)
option(ETHERVOX_FETCH_SHALLOW "Use shallow clones for faster downloads" ON)

# Optimization: Skip update checks after first download (speeds up reconfigure)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Skip git update checks for faster reconfigures")

# llama.cpp dependency
function(fetch_llama_cpp)
    set(LLAMA_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp")
    
    # Check if user provided custom path
    if(DEFINED LLAMA_CPP_CUSTOM_DIR)
        set(LLAMA_CPP_DIR "${LLAMA_CPP_CUSTOM_DIR}")
        message(STATUS "Using custom llama.cpp from: ${LLAMA_CPP_DIR}")
    endif()
    
    # Strategy 1: Check if submodule exists
    if(EXISTS "${LLAMA_CPP_DIR}/CMakeLists.txt")
        message(STATUS "✓ llama.cpp found at: ${LLAMA_CPP_DIR}")
        set(LLAMA_CPP_SOURCE_DIR "${LLAMA_CPP_DIR}" PARENT_SCOPE)
        return()
    endif()
    
    # Strategy 2: Auto-fetch if enabled
    if(ETHERVOX_AUTO_FETCH_DEPS)
        message(STATUS "⬇️  llama.cpp not found, downloading automatically...")
        message(STATUS "   This is a one-time download (~50MB)")
        message(STATUS "   Tip: Use 'git submodule update --init' to avoid future downloads")
        
        set(FETCHCONTENT_QUIET OFF)
        
        if(ETHERVOX_FETCH_SHALLOW)
            FetchContent_Declare(
                llama_cpp
                GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
                GIT_TAG        master  # You should pin to a specific tag/commit
                GIT_SHALLOW    TRUE
                GIT_PROGRESS   TRUE
                SOURCE_DIR     "${LLAMA_CPP_DIR}"
            )
        else()
            FetchContent_Declare(
                llama_cpp
                GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
                GIT_TAG        master
                GIT_PROGRESS   TRUE
                SOURCE_DIR     "${LLAMA_CPP_DIR}"
            )
        endif()
        
        FetchContent_GetProperties(llama_cpp)
        if(NOT llama_cpp_POPULATED)
            FetchContent_Populate(llama_cpp)
            
            # Patch llama.cpp's CMakeLists.txt to exclude _XOPEN_SOURCE on Windows
            # llama.cpp incorrectly defines _XOPEN_SOURCE=600 on all platforms including Windows
            # This breaks Windows builds as _XOPEN_SOURCE is POSIX-specific
            if(WIN32)
                set(GGML_CMAKE_FILE "${llama_cpp_SOURCE_DIR}/ggml/src/CMakeLists.txt")
                if(EXISTS "${GGML_CMAKE_FILE}")
                    file(READ "${GGML_CMAKE_FILE}" GGML_CMAKE_CONTENT)
                    # Wrap the _XOPEN_SOURCE logic to exclude Windows
                    string(REPLACE 
                        "# some string functions rely on locale_t availability,\n# which was introduced in POSIX.1-2008, forcing us to go higher\nif (CMAKE_SYSTEM_NAME MATCHES \"OpenBSD\")"
                        "# some string functions rely on locale_t availability,\n# which was introduced in POSIX.1-2008, forcing us to go higher\nif (WIN32)\n    # Skip _XOPEN_SOURCE on Windows (POSIX-specific)\nelseif (CMAKE_SYSTEM_NAME MATCHES \"OpenBSD\")"
                        GGML_CMAKE_CONTENT "${GGML_CMAKE_CONTENT}")
                    file(WRITE "${GGML_CMAKE_FILE}" "${GGML_CMAKE_CONTENT}")
                    message(STATUS "Patched llama.cpp to skip _XOPEN_SOURCE on Windows")
                endif()
            endif()
            
            set(LLAMA_CPP_SOURCE_DIR "${llama_cpp_SOURCE_DIR}" PARENT_SCOPE)
            message(STATUS "✓ llama.cpp downloaded to: ${llama_cpp_SOURCE_DIR}")
        endif()
    else()
        message(WARNING "llama.cpp not found and ETHERVOX_AUTO_FETCH_DEPS=OFF")
        message(WARNING "Please run: git submodule update --init --recursive")
        message(WARNING "Or set: -DETHERVOX_AUTO_FETCH_DEPS=ON")
        set(LLAMA_CPP_SOURCE_DIR "" PARENT_SCOPE)
    endif()
endfunction()

# whisper.cpp dependency
function(fetch_whisper_cpp)
    set(WHISPER_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/whisper.cpp")
    
    # Check if user provided custom path
    if(DEFINED WHISPER_CPP_CUSTOM_DIR)
        set(WHISPER_CPP_DIR "${WHISPER_CPP_CUSTOM_DIR}")
        message(STATUS "Using custom whisper.cpp from: ${WHISPER_CPP_DIR}")
    endif()
    
    # Strategy 1: Check if submodule exists
    if(EXISTS "${WHISPER_CPP_DIR}/CMakeLists.txt")
        message(STATUS "✓ whisper.cpp found at: ${WHISPER_CPP_DIR}")
        set(WHISPER_CPP_SOURCE_DIR "${WHISPER_CPP_DIR}" PARENT_SCOPE)
        return()
    endif()
    
    # Strategy 2: Auto-fetch if enabled
    if(ETHERVOX_AUTO_FETCH_DEPS)
        message(STATUS "⬇️  whisper.cpp not found, downloading automatically...")
        message(STATUS "   This is a one-time download (~30MB)")
        message(STATUS "   Tip: Use 'git submodule update --init' to avoid future downloads")
        
        set(FETCHCONTENT_QUIET OFF)
        
        if(ETHERVOX_FETCH_SHALLOW)
            FetchContent_Declare(
                whisper_cpp
                GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
                GIT_TAG        master  # You should pin to a specific tag/commit
                GIT_SHALLOW    TRUE
                GIT_PROGRESS   TRUE
                SOURCE_DIR     "${WHISPER_CPP_DIR}"
            )
        else()
            FetchContent_Declare(
                whisper_cpp
                GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
                GIT_TAG        master
                GIT_PROGRESS   TRUE
                SOURCE_DIR     "${WHISPER_CPP_DIR}"
            )
        endif()
        
        FetchContent_GetProperties(whisper_cpp)
        if(NOT whisper_cpp_POPULATED)
            FetchContent_Populate(whisper_cpp)
            set(WHISPER_CPP_SOURCE_DIR "${whisper_cpp_SOURCE_DIR}" PARENT_SCOPE)
            message(STATUS "✓ whisper.cpp downloaded to: ${whisper_cpp_SOURCE_DIR}")
        endif()
    else()
        message(WARNING "whisper.cpp not found and ETHERVOX_AUTO_FETCH_DEPS=OFF")
        message(WARNING "Please run: git submodule update --init --recursive")
        message(WARNING "Or set: -DETHERVOX_AUTO_FETCH_DEPS=ON")
        set(WHISPER_CPP_SOURCE_DIR "" PARENT_SCOPE)
    endif()
endfunction()

# Helper function to print dependency status
function(print_dependency_status)
    message(STATUS "")
    message(STATUS "=== Dependency Acquisition Summary ===")
    message(STATUS "Auto-fetch enabled: ${ETHERVOX_AUTO_FETCH_DEPS}")
    message(STATUS "Shallow clones: ${ETHERVOX_FETCH_SHALLOW}")
    
    if(LLAMA_CPP_SOURCE_DIR)
        message(STATUS "✓ llama.cpp: ${LLAMA_CPP_SOURCE_DIR}")
    else()
        message(STATUS "✗ llama.cpp: NOT AVAILABLE")
    endif()
    
    if(WHISPER_CPP_SOURCE_DIR)
        message(STATUS "✓ whisper.cpp: ${WHISPER_CPP_SOURCE_DIR}")
    else()
        message(STATUS "✗ whisper.cpp: NOT AVAILABLE")
    endif()
    
    message(STATUS "=====================================")
    message(STATUS "")
endfunction()
