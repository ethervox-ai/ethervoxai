#!/usr/bin/env bash
# Setup Raspberry Pi cross-compilation toolchain
set -euo pipefail

# Get the project root directory (where this script is located)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SYSROOT_DIR="${PROJECT_ROOT}/sysroot/rpi/usr"

echo "Project root: ${PROJECT_ROOT}"
echo "Sysroot directory: ${SYSROOT_DIR}"

# Install the ARM cross-compiler
echo "Installing ARM cross-compiler..."
sudo apt update
sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# Verify installation
echo "Verifying cross-compiler installation..."
arm-linux-gnueabihf-gcc --version

# Create a directory for cross-compiled dependencies
echo "Creating sysroot directories..."
mkdir -p "${SYSROOT_DIR}/include"
mkdir -p "${SYSROOT_DIR}/lib"

# Download bcm2835 library
echo "Downloading bcm2835 library..."
TEMP_DIR=$(mktemp -d)
cd "${TEMP_DIR}"
wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.73.tar.gz
tar xzf bcm2835-1.73.tar.gz
cd bcm2835-1.73

# Cross-compile for ARM
echo "Cross-compiling bcm2835 library..."
./configure --host=arm-linux-gnueabihf --prefix="${SYSROOT_DIR}"
make
make install

# Download ARM libraries from Debian repository
echo "Downloading ARM libraries (curl, openssl)..."
cd "${TEMP_DIR}"

# Use Debian bookworm (stable) repository
DEBIAN_MIRROR="http://ftp.debian.org/debian/pool/main"

# Download libcurl for ARM (find latest version)
echo "Fetching libcurl packages..."
wget -q "${DEBIAN_MIRROR}/c/curl/" -O curl_index.html
LIBCURL_PKG=$(grep -oP 'libcurl4_[0-9.-]+\+deb12u[0-9]+_armhf\.deb' curl_index.html | sort -V | tail -1)
LIBCURL_DEV_PKG=$(grep -oP 'libcurl4-openssl-dev_[0-9.-]+\+deb12u[0-9]+_armhf\.deb' curl_index.html | sort -V | tail -1)

if [ -n "$LIBCURL_PKG" ]; then
    echo "Downloading $LIBCURL_PKG..."
    wget "${DEBIAN_MIRROR}/c/curl/${LIBCURL_PKG}"
fi
if [ -n "$LIBCURL_DEV_PKG" ]; then
    echo "Downloading $LIBCURL_DEV_PKG..."
    wget "${DEBIAN_MIRROR}/c/curl/${LIBCURL_DEV_PKG}"
fi

# Download OpenSSL for ARM (find latest version)
echo "Fetching OpenSSL packages..."
wget -q "${DEBIAN_MIRROR}/o/openssl/" -O openssl_index.html
LIBSSL_PKG=$(grep -oP 'libssl3[a-z]*_[0-9.-]+~deb12u[0-9]+_armhf\.deb' openssl_index.html | sort -V | tail -1)
LIBSSL_DEV_PKG=$(grep -oP 'libssl-dev_[0-9.-]+~deb12u[0-9]+_armhf\.deb' openssl_index.html | sort -V | tail -1)

if [ -n "$LIBSSL_PKG" ]; then
    echo "Downloading $LIBSSL_PKG..."
    wget "${DEBIAN_MIRROR}/o/openssl/${LIBSSL_PKG}"
fi
if [ -n "$LIBSSL_DEV_PKG" ]; then
    echo "Downloading $LIBSSL_DEV_PKG..."
    wget "${DEBIAN_MIRROR}/o/openssl/${LIBSSL_DEV_PKG}"
fi

# Clean up index files
rm -f curl_index.html openssl_index.html

# Extract libraries to sysroot
echo "Extracting libraries to sysroot..."
for deb in *.deb; do
    if [ -f "$deb" ]; then
        echo "Extracting $deb..."
        dpkg-deb -x "$deb" "${SYSROOT_DIR}/../"
    fi
done

# Cleanup
echo "Cleaning up temporary files..."
cd "${PROJECT_ROOT}"
rm -rf "${TEMP_DIR}"

echo "✓ Raspberry Pi toolchain setup complete!"
echo "Sysroot location: ${SYSROOT_DIR}"