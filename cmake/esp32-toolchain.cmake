# ESP32 Cross-compilation toolchain file
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

# ESP-IDF configuration
if(DEFINED ENV{IDF_PATH})
    set(IDF_PATH $ENV{IDF_PATH})
else()
    message(FATAL_ERROR "IDF_PATH environment variable not set. Please run: source $IDF_PATH/export.sh")
endif()

# Determine ESP32 target variant
if(NOT DEFINED ESP32_TARGET)
    if(DEFINED ENV{IDF_TARGET})
        set(ESP32_TARGET $ENV{IDF_TARGET})
    else()
        set(ESP32_TARGET "esp32s3")  # Default to ESP32-S3
    endif()
endif()

message(STATUS "ESP32 Target: ${ESP32_TARGET}")

# Set toolchain paths based on ESP32 target
if(ESP32_TARGET STREQUAL "esp32")
    set(TOOLCHAIN_PREFIX "xtensa-esp32-elf")
    set(CMAKE_SYSTEM_PROCESSOR xtensa)
elseif(ESP32_TARGET STREQUAL "esp32s2")
    set(TOOLCHAIN_PREFIX "xtensa-esp32s2-elf") 
    set(CMAKE_SYSTEM_PROCESSOR xtensa)
elseif(ESP32_TARGET STREQUAL "esp32s3")
    set(TOOLCHAIN_PREFIX "xtensa-esp32s3-elf")
    set(CMAKE_SYSTEM_PROCESSOR xtensa)
elseif(ESP32_TARGET STREQUAL "esp32c3")
    set(TOOLCHAIN_PREFIX "riscv32-esp-elf")
    set(CMAKE_SYSTEM_PROCESSOR riscv32)
elseif(ESP32_TARGET STREQUAL "esp32c6") 
    set(TOOLCHAIN_PREFIX "riscv32-esp-elf")
    set(CMAKE_SYSTEM_PROCESSOR riscv32)
else()
    message(FATAL_ERROR "Unsupported ESP32 target: ${ESP32_TARGET}")
endif()

# Find toolchain binaries
find_program(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc
    PATHS ${IDF_PATH}/tools/*/bin
    NO_DEFAULT_PATH
)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++
    PATHS ${IDF_PATH}/tools/*/bin  
    NO_DEFAULT_PATH
)
find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-gcc
    PATHS ${IDF_PATH}/tools/*/bin
    NO_DEFAULT_PATH
)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR "Could not find ${TOOLCHAIN_PREFIX}-gcc. Please ensure ESP-IDF is properly installed and run 'source $IDF_PATH/export.sh'")
endif()

message(STATUS "Using C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Using C++ compiler: ${CMAKE_CXX_COMPILER}")

# ESP32 specific compiler flags
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "xtensa")
    set(CMAKE_C_FLAGS "-mlongcalls -Wno-frame-address -ffunction-sections -fdata-sections" CACHE STRING "C Compiler Base Flags")
    set(CMAKE_CXX_FLAGS "-mlongcalls -Wno-frame-address -ffunction-sections -fdata-sections" CACHE STRING "C++ Compiler Base Flags")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv32")
    set(CMAKE_C_FLAGS "-march=rv32imc -mabi=ilp32 -ffunction-sections -fdata-sections" CACHE STRING "C Compiler Base Flags")
    set(CMAKE_CXX_FLAGS "-march=rv32imc -mabi=ilp32 -ffunction-sections -fdata-sections" CACHE STRING "C++ Compiler Base Flags")
endif()

# Optimization flags
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "C Release Flags")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "C++ Release Flags")
set(CMAKE_C_FLAGS_DEBUG "-Og -g" CACHE STRING "C Debug Flags")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g" CACHE STRING "C++ Debug Flags")

# Linker flags for size optimization
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections -Wl,--cref" CACHE STRING "Linker Flags")

# Prevent CMake from testing the compiler (cross-compilation)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Set find root path for libraries and includes
set(CMAKE_FIND_ROOT_PATH ${IDF_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)