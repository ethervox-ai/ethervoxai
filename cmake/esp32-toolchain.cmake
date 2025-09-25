# ESP32 Cross-compilation toolchain file
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

# ESP-IDF configuration
if(DEFINED ENV{IDF_PATH})
    set(IDF_PATH $ENV{IDF_PATH})
else()
    message(FATAL_ERROR "IDF_PATH environment variable not set")
endif()

# Toolchain paths
set(CMAKE_C_COMPILER ${IDF_PATH}/tools/xtensa-esp32s3-elf/esp-2022r1-11.2.0/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-gcc)
set(CMAKE_CXX_COMPILER ${IDF_PATH}/tools/xtensa-esp32s3-elf/esp-2022r1-11.2.0/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-g++)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})

# ESP32 specific flags
set(CMAKE_C_FLAGS "-mlongcalls -Wno-frame-address" CACHE STRING "C Compiler Base Flags")
set(CMAKE_CXX_FLAGS "-mlongcalls -Wno-frame-address" CACHE STRING "C++ Compiler Base Flags")

# Prevent CMake from testing the compiler
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)