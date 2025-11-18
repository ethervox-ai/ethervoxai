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
