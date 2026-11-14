# FetchDependencies.cmake
# Automatic dependency fetching for llama.cpp (whisper.cpp has been removed -
# STT now runs entirely on Granite Speech via llama.cpp's mtmd library, which
# ships inside the llama.cpp tree itself - see fetch_llama_cpp() below)
#
# This module handles dependency acquisition with multiple strategies:
# 1. Check if external/<dep> exists (git submodule)
# 2. If not, use FetchContent to download automatically
# 3. Allow manual override via <DEP>_DIR CMake variable
#
# Usage:
#   include(cmake/FetchDependencies.cmake)
#   fetch_llama_cpp()

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
        
        # Granite Speech's GGUF is a multimodal (mtmd) pair - main GGUF (LLM +
        # Conformer audio encoder) plus a companion mmproj GGUF (QFormer audio
        # projector). llama.cpp only gained Granite Speech's mtmd support in
        # tools/mtmd/models/granite-speech.cpp as of release b9045; floating on
        # `master` risks a future upstream change silently breaking ASR, so this
        # is pinned to the exact commit that shipped it (verified via the
        # upstream PR "mtmd: add granite-speech support", #22101). Re-pin
        # deliberately if upgrading, and re-verify both BASE and PLUS (SAA)
        # variants still tokenize/decode correctly against the new commit.
        set(LLAMA_CPP_PINNED_COMMIT "a00e47e422bc4e48b8d2cdcfb16b7e55748237c2")  # b9045
        if(ETHERVOX_FETCH_SHALLOW)
            FetchContent_Declare(
                llama_cpp
                GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
                GIT_TAG        ${LLAMA_CPP_PINNED_COMMIT}
                GIT_SHALLOW    TRUE
                GIT_PROGRESS   TRUE
                SOURCE_DIR     "${LLAMA_CPP_DIR}"
            )
        else()
            FetchContent_Declare(
                llama_cpp
                GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
                GIT_TAG        ${LLAMA_CPP_PINNED_COMMIT}
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

# whisper.cpp has been removed entirely - Granite Speech (loaded via
# llama.cpp's mtmd library, fetched as part of fetch_llama_cpp() above)
# replaces it for all STT. No fetch_whisper_cpp() function remains; there is
# no backward-compat path back to Whisper.

# Helper function to print dependency status
function(print_dependency_status)
    message(STATUS "")
    message(STATUS "=== Dependency Acquisition Summary ===")
    message(STATUS "Auto-fetch enabled: ${ETHERVOX_AUTO_FETCH_DEPS}")
    message(STATUS "Shallow clones: ${ETHERVOX_FETCH_SHALLOW}")
    
    if(LLAMA_CPP_SOURCE_DIR)
        message(STATUS "✓ llama.cpp (+ mtmd/Granite Speech): ${LLAMA_CPP_SOURCE_DIR}")
    else()
        message(STATUS "✗ llama.cpp: NOT AVAILABLE")
    endif()
    
    message(STATUS "=====================================")
    message(STATUS "")
endfunction()
