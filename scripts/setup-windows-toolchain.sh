#!/usr/bin/env bash
# Setup Windows cross-compilation toolchain
set -euo pipefail

# Get the project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Configuration
SYSROOT_DIR="${PROJECT_ROOT}/sysroot/windows"
MINGW_PREFIX="x86_64-w64-mingw32"

echo "=========================================="
echo "  Windows Cross-Compilation Setup"
echo "=========================================="
echo "Project root: ${PROJECT_ROOT}"
echo "Sysroot location: ${SYSROOT_DIR}"
echo "MinGW prefix: ${MINGW_PREFIX}"
echo ""

# Check if running on Linux
if [[ "$(uname -s)" != "Linux" ]]; then
    echo "Error: This script must be run on a Linux system"
    exit 1
fi

# Update package lists
echo "Updating package lists..."
sudo apt-get update

# Install MinGW cross-compiler
echo "Installing MinGW cross-compiler..."
sudo apt-get install -y \
    mingw-w64 \
    gcc-mingw-w64-x86-64 \
    g++-mingw-w64-x86-64 \
    binutils-mingw-w64-x86-64 \
    mingw-w64-tools \
    mingw-w64-common \
    wget \
    unzip

# Verify MinGW installation
echo "Verifying MinGW installation..."
if ! command -v ${MINGW_PREFIX}-gcc &> /dev/null; then
    echo "Error: MinGW GCC not found in PATH"
    exit 1
fi

MINGW_VERSION=$(${MINGW_PREFIX}-gcc --version | head -n1)
echo "MinGW GCC installed: ${MINGW_VERSION}"

# Create sysroot directory structure
echo "Creating sysroot directory structure..."
mkdir -p "${SYSROOT_DIR}"/{lib,include,bin}

# Download Windows development libraries
echo "Setting up Windows libraries in sysroot..."

# Create a temporary directory for downloads
TEMP_DIR=$(mktemp -d)
cd "${TEMP_DIR}"

# Note: MinGW already includes basic Windows libraries, but we can add more if needed
# Copy MinGW runtime libraries to sysroot
MINGW_LIB_PATH="/usr/${MINGW_PREFIX}/lib"
MINGW_INCLUDE_PATH="/usr/${MINGW_PREFIX}/include"

if [ -d "${MINGW_LIB_PATH}" ]; then
    echo "Copying MinGW libraries to sysroot..."
    cp -r "${MINGW_LIB_PATH}"/* "${SYSROOT_DIR}/lib/" 2>/dev/null || true
fi

if [ -d "${MINGW_INCLUDE_PATH}" ]; then
    echo "Copying MinGW headers to sysroot..."
    cp -r "${MINGW_INCLUDE_PATH}"/* "${SYSROOT_DIR}/include/" 2>/dev/null || true
fi

# Download and setup OpenSSL for Windows (if using HTTPS)
echo "Downloading OpenSSL for Windows..."
OPENSSL_VERSION="1.1.1w"
OPENSSL_URL="https://github.com/openssl/openssl/archive/refs/tags/OpenSSL_${OPENSSL_VERSION//./_}.tar.gz"

if wget -q --spider "${OPENSSL_URL}"; then
    wget "${OPENSSL_URL}" -O openssl.tar.gz
    tar -xzf openssl.tar.gz
    
    # Note: Building OpenSSL for Windows is complex, you may want to use pre-built binaries
    # For now, we'll just note where to get them
    echo "Note: For OpenSSL, consider using pre-built Windows binaries from:"
    echo "  https://slproweb.com/products/Win32OpenSSL.html"
fi

# Download libcurl for Windows (optional)
echo "Note: For libcurl, consider using pre-built Windows binaries from:"
echo "  https://curl.se/windows/"

# Cleanup
cd "${PROJECT_ROOT}"
rm -rf "${TEMP_DIR}"

# Create CMake toolchain file if it doesn't exist
TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/windows-toolchain.cmake"
if [ ! -f "${TOOLCHAIN_FILE}" ]; then
    echo "Creating CMake toolchain file..."
    mkdir -p "${PROJECT_ROOT}/cmake"
    
    cat > "${TOOLCHAIN_FILE}" << 'EOF'
# Windows cross-compilation toolchain file
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross compiler
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Define platform macros
add_definitions(-DETHERVOX_PLATFORM_WINDOWS=1)
add_definitions(-DETHERVOX_PLATFORM_DESKTOP=1)

# Sysroot for libraries and headers
set(CMAKE_SYSROOT ${CMAKE_CURRENT_LIST_DIR}/../sysroot/windows)

# Where to find libraries and headers
set(CMAKE_FIND_ROOT_PATH 
    /usr/x86_64-w64-mingw32
    ${CMAKE_SYSROOT}
)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Static linking for MinGW runtime (optional - makes binaries more portable)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
EOF
    
    echo "Created ${TOOLCHAIN_FILE}"
fi

# Create a helper script for downloading pre-built Windows libraries
DEPS_SCRIPT="${PROJECT_ROOT}/scripts/download-windows-deps.sh"
cat > "${DEPS_SCRIPT}" << 'EOF'
#!/usr/bin/env bash
# Download pre-built Windows dependencies
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SYSROOT_DIR="${PROJECT_ROOT}/sysroot/windows"

echo "Downloading pre-built Windows libraries..."
echo "Sysroot: ${SYSROOT_DIR}"

# Create temp directory
TEMP_DIR=$(mktemp -d)
cd "${TEMP_DIR}"

# Download OpenSSL for Windows (pre-built)
echo "Note: Download OpenSSL from https://slproweb.com/products/Win32OpenSSL.html"
echo "Extract to ${SYSROOT_DIR}"

# Download libcurl for Windows
echo "Note: Download libcurl from https://curl.se/windows/"
echo "Extract to ${SYSROOT_DIR}"

# Cleanup
rm -rf "${TEMP_DIR}"

echo ""
echo "Please manually download and extract Windows libraries to:"
echo "  ${SYSROOT_DIR}"
EOF

chmod +x "${DEPS_SCRIPT}"

# Summary
echo ""
echo "=========================================="
echo "  Windows Toolchain Setup Complete!"
echo "=========================================="
echo ""
echo "MinGW compiler: ${MINGW_PREFIX}-gcc"
echo "Sysroot: ${SYSROOT_DIR}"
echo "Toolchain file: ${TOOLCHAIN_FILE}"
echo ""
echo "Next steps:"
echo "  1. (Optional) Run ./scripts/download-windows-deps.sh for additional libraries"
echo "  2. Build for Windows: make build-windows"
echo "  3. Find binary in: build-windows/ethervoxai.exe"
echo ""
echo "Note: For OpenSSL and libcurl, you may need to download pre-built Windows binaries:"
echo "  - OpenSSL: https://slproweb.com/products/Win32OpenSSL.html"
echo "  - libcurl: https://curl.se/windows/"
echo ""