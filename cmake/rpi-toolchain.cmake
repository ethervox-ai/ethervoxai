# Raspberry Pi Cross-compilation toolchain file
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Set up cross-compiler (adjust paths as needed)
if(DEFINED ENV{RPI_TOOLCHAIN_PATH})
    set(RPI_TOOLCHAIN_PATH $ENV{RPI_TOOLCHAIN_PATH})
else()
    # Default path for Raspberry Pi toolchain
    set(RPI_TOOLCHAIN_PATH /opt/cross-pi-gcc/bin)
endif()

# Toolchain binaries
set(CMAKE_C_COMPILER ${RPI_TOOLCHAIN_PATH}/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${RPI_TOOLCHAIN_PATH}/arm-linux-gnueabihf-g++)

# Raspberry Pi specific flags
set(CMAKE_C_FLAGS "-mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard" CACHE STRING "C Compiler Base Flags")
set(CMAKE_CXX_FLAGS "-mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard" CACHE STRING "C++ Compiler Base Flags")

# Set the sysroot for Raspberry Pi
if(DEFINED ENV{RPI_SYSROOT})
    set(CMAKE_SYSROOT $ENV{RPI_SYSROOT})
endif()

# Search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)