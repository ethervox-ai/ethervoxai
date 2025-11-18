# CMake toolchain file for Raspberry Pi cross-compilation

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross compiler
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Where to find the target environment
# Use CMAKE_CURRENT_LIST_DIR to make it portable
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CMAKE_SYSROOT "${PROJECT_ROOT}/sysroot/rpi/usr")

set(CMAKE_FIND_ROOT_PATH 
    /usr/arm-linux-gnueabihf
    ${CMAKE_SYSROOT}
)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Explicitly add include and library paths
include_directories(SYSTEM ${CMAKE_SYSROOT}/include)
link_directories(${CMAKE_SYSROOT}/lib)

# Define platform
add_definitions(-DETHERVOX_PLATFORM_RPI)

# Raspberry Pi specific flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")