# Feature flags for optional network / mutating-file subsystems (TASK-C1.0).
# Profile-based feature composition (TASK-C1.2).
#
# ETHERVOX_PROFILE defines a named feature set for a target platform class.
# Setting a profile automatically configures all feature flags. Individual
# flags can still be overridden manually after profile selection.
#
# Profiles (TASK-C1.2, §2 of docs/07-BACKEND-CHANGES.md):
#   EDGE      - ESP32-S3/P4: HAL, audio I/O, VAD, wake word, ethervox_link client, no llama.cpp (~200KB)
#   MOBILE    - iOS, Android, RPi: + inference, tools, ASR, memory, networked tools
#   DESKTOP   - Friend'O'Mine desktop/hub: + model pool, vision, embeddings, session forking
#   WORKSPACE - Workspace shells: DESKTOP minus network code (HTTP, downloader, bug reporter, weather, built-in file tools)
#
# A feature guarded here must never "succeed silently" when OFF: callers get
# ETHERVOX_ERROR_FEATURE_DISABLED, not empty/stubbed data.

set(ETHERVOX_PROFILE "DESKTOP" CACHE STRING "Feature profile: EDGE|MOBILE|DESKTOP|WORKSPACE")
set_property(CACHE ETHERVOX_PROFILE PROPERTY STRINGS "EDGE" "MOBILE" "DESKTOP" "WORKSPACE")

# Validate profile
if(NOT ETHERVOX_PROFILE MATCHES "^(EDGE|MOBILE|DESKTOP|WORKSPACE)$")
    message(FATAL_ERROR "Invalid ETHERVOX_PROFILE='${ETHERVOX_PROFILE}'. Must be EDGE, MOBILE, DESKTOP, or WORKSPACE.")
endif()

message(STATUS "ETHERVOX_PROFILE: ${ETHERVOX_PROFILE}")

# Configure feature flags based on profile (can be overridden after this block)
if(ETHERVOX_PROFILE STREQUAL "EDGE")
    # ESP32: no inference, minimal feature set
    set(ETHERVOX_FEATURE_HTTP       OFF CACHE BOOL "HTTP disabled for EDGE profile" FORCE)
    set(ETHERVOX_FEATURE_DOWNLOADER OFF CACHE BOOL "Downloader disabled for EDGE profile" FORCE)
    set(ETHERVOX_FEATURE_BUG_REPORT OFF CACHE BOOL "Bug reporter disabled for EDGE profile" FORCE)
    set(ETHERVOX_FEATURE_WEATHER    OFF CACHE BOOL "Weather disabled for EDGE profile" FORCE)
    set(ETHERVOX_FEATURE_FILE_TOOLS OFF CACHE BOOL "File tools disabled for EDGE profile" FORCE)
elseif(ETHERVOX_PROFILE STREQUAL "MOBILE")
    # iOS, Android, RPi: full voice line feature set including network
    option(ETHERVOX_FEATURE_HTTP       "Enable generic platform HTTP client (src/common/platform_http.c)" ON)
    option(ETHERVOX_FEATURE_DOWNLOADER "Enable the model downloader"                                       ON)
    option(ETHERVOX_FEATURE_BUG_REPORT "Enable the GitHub-issue bug reporter"                               ON)
    option(ETHERVOX_FEATURE_WEATHER    "Enable the Open-Meteo weather tool"                                 ON)
    option(ETHERVOX_FEATURE_FILE_TOOLS "Enable the file_tools plugin (read, and read-write if selected)"    ON)
elseif(ETHERVOX_PROFILE STREQUAL "DESKTOP")
    # Friend'O'Mine desktop: same as MOBILE for now (pool/vision/embeddings/forking are future work)
    option(ETHERVOX_FEATURE_HTTP       "Enable generic platform HTTP client (src/common/platform_http.c)" ON)
    option(ETHERVOX_FEATURE_DOWNLOADER "Enable the model downloader"                                       ON)
    option(ETHERVOX_FEATURE_BUG_REPORT "Enable the GitHub-issue bug reporter"                               ON)
    option(ETHERVOX_FEATURE_WEATHER    "Enable the Open-Meteo weather tool"                                 ON)
    option(ETHERVOX_FEATURE_FILE_TOOLS "Enable the file_tools plugin (read, and read-write if selected)"    ON)
elseif(ETHERVOX_PROFILE STREQUAL "WORKSPACE")
    # Workspace: no network, no mutating file tools (AGENTS.md I1, I2)
    set(ETHERVOX_FEATURE_HTTP       OFF CACHE BOOL "HTTP disabled for WORKSPACE profile" FORCE)
    set(ETHERVOX_FEATURE_DOWNLOADER OFF CACHE BOOL "Downloader disabled for WORKSPACE profile" FORCE)
    set(ETHERVOX_FEATURE_BUG_REPORT OFF CACHE BOOL "Bug reporter disabled for WORKSPACE profile" FORCE)
    set(ETHERVOX_FEATURE_WEATHER    OFF CACHE BOOL "Weather disabled for WORKSPACE profile" FORCE)
    set(ETHERVOX_FEATURE_FILE_TOOLS OFF CACHE BOOL "Built-in file tools disabled for WORKSPACE profile" FORCE)
endif()

# Convert feature flags to compile definitions
if(ETHERVOX_FEATURE_HTTP)
    add_definitions(-DETHERVOX_FEATURE_HTTP=1)
else()
    add_definitions(-DETHERVOX_FEATURE_HTTP=0)
endif()
if(ETHERVOX_FEATURE_DOWNLOADER)
    add_definitions(-DETHERVOX_FEATURE_DOWNLOADER=1)
else()
    add_definitions(-DETHERVOX_FEATURE_DOWNLOADER=0)
endif()
if(ETHERVOX_FEATURE_BUG_REPORT)
    add_definitions(-DETHERVOX_FEATURE_BUG_REPORT=1)
else()
    add_definitions(-DETHERVOX_FEATURE_BUG_REPORT=0)
endif()
if(ETHERVOX_FEATURE_WEATHER)
    add_definitions(-DETHERVOX_FEATURE_WEATHER=1)
else()
    add_definitions(-DETHERVOX_FEATURE_WEATHER=0)
endif()
if(ETHERVOX_FEATURE_FILE_TOOLS)
    add_definitions(-DETHERVOX_FEATURE_FILE_TOOLS=1)
else()
    add_definitions(-DETHERVOX_FEATURE_FILE_TOOLS=0)
endif()

# Define profile macro
add_definitions(-DETHERVOX_PROFILE_${ETHERVOX_PROFILE}=1)

message(STATUS "Feature flags: HTTP=${ETHERVOX_FEATURE_HTTP} DOWNLOADER=${ETHERVOX_FEATURE_DOWNLOADER} "
               "BUG_REPORT=${ETHERVOX_FEATURE_BUG_REPORT} WEATHER=${ETHERVOX_FEATURE_WEATHER} "
               "FILE_TOOLS=${ETHERVOX_FEATURE_FILE_TOOLS}")

# Generate ethervox_features.h with ETHERVOX_HAS_* macros (TASK-C1.2)
set(FEATURES_HEADER "${CMAKE_BINARY_DIR}/include/ethervox_features.h")
file(WRITE "${FEATURES_HEADER}" "// Generated by cmake/EthervoxFeatures.cmake - DO NOT EDIT\n")
file(APPEND "${FEATURES_HEADER}" "#ifndef ETHERVOX_FEATURES_H\n")
file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_FEATURES_H\n\n")
file(APPEND "${FEATURES_HEADER}" "// Profile: ${ETHERVOX_PROFILE}\n")
file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_PROFILE_${ETHERVOX_PROFILE} 1\n\n")
file(APPEND "${FEATURES_HEADER}" "// Feature flags\n")
if(ETHERVOX_FEATURE_HTTP)
    file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_HAS_HTTP 1\n")
else()
    file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_HAS_HTTP 0\n")
endif()
if(ETHERVOX_FEATURE_DOWNLOADER)
    file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_HAS_DOWNLOADER 1\n")
else()
    file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_HAS_DOWNLOADER 0\n")
endif()
if(ETHERVOX_FEATURE_BUG_REPORT)
    file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_HAS_BUG_REPORT 1\n")
else()
    file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_HAS_BUG_REPORT 0\n")
endif()
if(ETHERVOX_FEATURE_WEATHER)
    file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_HAS_WEATHER 1\n")
else()
    file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_HAS_WEATHER 0\n")
endif()
if(ETHERVOX_FEATURE_FILE_TOOLS)
    file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_HAS_FILE_TOOLS 1\n")
else()
    file(APPEND "${FEATURES_HEADER}" "#define ETHERVOX_HAS_FILE_TOOLS 0\n")
endif()
file(APPEND "${FEATURES_HEADER}" "\n#endif // ETHERVOX_FEATURES_H\n")

message(STATUS "Generated ${FEATURES_HEADER}")

# Make generated header available
include_directories("${CMAKE_BINARY_DIR}/include")
