# Patch cJSON CMakeLists.txt to use CMake 3.16
# This script is called during FetchContent PATCH_COMMAND

if(ARGC LESS 1)
    message(FATAL_ERROR "Usage: cmake -P patch_cjson.cmake <source_dir>")
endif()

set(SOURCE_DIR ${CMAKE_ARGV3})
set(CJSON_CMAKE "${SOURCE_DIR}/CMakeLists.txt")

if(EXISTS "${CJSON_CMAKE}")
    file(READ "${CJSON_CMAKE}" CONTENT)
    string(REPLACE "cmake_minimum_required(VERSION 3.0)" 
                   "cmake_minimum_required(VERSION 3.16)" 
                   CONTENT "${CONTENT}")
    file(WRITE "${CJSON_CMAKE}" "${CONTENT}")
    message(STATUS "Patched cJSON CMakeLists.txt to require CMake 3.16")
else()
    message(WARNING "cJSON CMakeLists.txt not found at ${CJSON_CMAKE}")
endif()
