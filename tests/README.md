# EthervoxAI Test Suite

This directory contains comprehensive tests for the EthervoxAI platform, organized into unit tests and integration tests.

## Structure

```text
tests/
├── unit/                    # Unit tests for individual components
│   ├── test_audio_core.c   # Audio subsystem unit tests
│   ├── test_config.c       # Configuration system tests
│   └── test_plugin_manager.c # Plugin management tests
├── integration/             # Integration tests across components
│   └── test_end_to_end.c   # End-to-end system tests
├── CMakeLists.txt          # Test build configuration
└── README.md               # This file
```text

## Running Tests

## Prerequisites

- CMake 3.16 or higher
- C compiler (GCC, Clang, or MSVC)
- Platform-specific dependencies (ALSA on Linux, etc.)

## Building and Running

```bash

# From project root

mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug

# Build tests

cmake --build . --target all

# Run all tests

ctest --output-on-failure --verbose

# Run specific test

./tests/test_audio_core
./tests/test_config
./tests/test_plugin_manager
./tests/test_end_to_end_integration
```text

## Test Coverage

For code coverage analysis (Linux/macOS with gcov):

```bash

# Configure with coverage

cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON

# Build and run tests

cmake --build .
ctest

# Generate coverage report

cmake --build . --target coverage
```text

## Test Categories

## Unit Tests

Unit tests focus on individual components in isolation:

- **test_audio_core.c**: Tests audio configuration, runtime initialization, and buffer operations
- **test_config.c**: Tests platform detection, version constants, and feature configuration
- **test_plugin_manager.c**: Tests plugin type conversions, manager initialization, and error handling

## Integration Tests

Integration tests verify component interactions:

- **test_end_to_end.c**: Tests complete system initialization, configuration consistency, and error handling chains

## Test Philosophy

## Expected Failures in CI

Some tests may fail in CI environments due to missing hardware or models:

- Audio initialization may fail without audio devices
- Dialogue engine may fail without language models
- These failures are expected and logged as warnings

## Cross-Platform Testing

Tests are designed to work across all supported platforms:

- Windows (MSVC, MinGW)
- Linux (GCC, Clang)
- macOS (Clang)
- Raspberry Pi (ARM GCC)
- ESP32 (Xtensa GCC)

## Test Data

Tests use minimal synthetic data and avoid external dependencies where possible. When external resources are needed, graceful fallbacks are implemented.

## Adding New Tests

## Unit Test Template

```c
#include <assert.h>
#include <stdio.h>
#include "ethervox/your_module.h"

void test_your_function() {
    printf("Testing your_function...\n");

    // Test implementation
    assert(your_function() == expected_value);

    printf("✓ Your function test passed\n");
}

int main() {
    printf("Running Your Module Unit Tests\n");
    printf("==============================\n");

    test_your_function();

    printf("==============================\n");
    printf("All tests completed!\n");
    return 0;
}
```text

## Integration Test Template

Integration tests should verify interactions between components and handle expected failures gracefully.

## Continuous Integration

Tests are automatically run in GitHub Actions across multiple platforms:

- Ubuntu (latest)
- Windows (latest)
- macOS (latest)
- Cross-compilation for Raspberry Pi
- Cross-compilation for ESP32

See `.github/workflows/build-and-test.yml` for complete CI configuration.
