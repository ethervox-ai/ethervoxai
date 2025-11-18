# EthervoxAI Scripts

This folder contains utility scripts for building, deploying, and setting up cross-compilation environments for EthervoxAI on various platforms. Below is a description of each script and example usage.

---

## build.sh
**Purpose:** Cross-platform build orchestrator for EthervoxAI.

**Usage:**
```bash
./scripts/build.sh [platform] [options]
```
**Platforms:**
- `desktop` (default)
- `debug` (coverage)
- `esp32`, `esp32-s3`, `esp32-c3`, `all-esp32`
- `rpi`, `rpi-zero`, `all-rpi`
- `all` (all supported)

**Options:**
- `--clean` (clean build)
- `--test` (run tests)
- `--verbose` (verbose output)
- `--help` (show help)

**Examples:**
```bash
./scripts/build.sh           # Build for desktop
./scripts/build.sh esp32-s3  # Build for ESP32-S3
./scripts/build.sh rpi       # Build for Raspberry Pi
```

---

## deploy-rpi.sh
**Purpose:** Deploys the built EthervoxAI binary and configs to a Raspberry Pi over SSH.

**Usage:**
```bash
./scripts/deploy-rpi.sh
```
**Environment variables:**
- `RPI_USER` (default: pi)
- `RPI_HOST` (default: raspberrypi.local)
- `RPI_DIR` (default: /home/pi/ethervoxai)

**Example:**
```bash
RPI_USER=pi RPI_HOST=192.168.1.42 ./scripts/deploy-rpi.sh
```

---

## download-windows-deps.sh
**Purpose:** Guides manual download of pre-built Windows libraries (OpenSSL, libcurl) for cross-compilation.

**Usage:**
```bash
./scripts/download-windows-deps.sh
```
**Note:** Follow the printed instructions to download and extract libraries to `sysroot/windows`.

---

## fix_build_errors.sh
**Purpose:** Checks for duplicate type definitions and common build errors in headers.

**Usage:**
```bash
./scripts/fix_build_errors.sh
```

---

## setup-esp32-toolchain.sh
**Purpose:** Installs ESP-IDF, sets up the ESP32 toolchain, and prepares the project structure for ESP32 builds.

**Usage:**
```bash
./scripts/setup-esp32-toolchain.sh
```
**Actions:**
- Installs ESP-IDF (if missing)
- Sets up environment
- Creates `esp32-project` with symlinks to `src` and `include`
- Verifies installation

---

## setup-rpi-toolchain.sh
**Purpose:** Installs ARM cross-compiler and cross-compiles required libraries for Raspberry Pi builds.

**Usage:**
```bash
./scripts/setup-rpi-toolchain.sh
```
**Actions:**
- Installs ARM toolchain
- Cross-compiles `bcm2835` library
- Downloads and prepares ARM versions of `curl`, `openssl` for sysroot

---

## setup-windows-toolchain.sh
**Purpose:** Installs MinGW cross-compiler and sets up sysroot for Windows cross-compilation.

**Usage:**
```bash
./scripts/setup-windows-toolchain.sh
```
**Actions:**
- Installs MinGW toolchain
- Creates sysroot structure
- Guides setup of Windows development libraries

---

## General Notes
- All scripts should be run from the project root unless otherwise specified.
- Some scripts require `sudo` for installing packages.
- For platform-specific builds, ensure the corresponding toolchain setup script has been run first.
