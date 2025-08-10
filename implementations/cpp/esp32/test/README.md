# ESP32 Test Directory

This directory contains unit tests for the ESP32 implementation.

## Running Tests

With PlatformIO installed:

```bash
# Run all tests
pio test -e esp32

# Run specific test
pio test -e esp32 -f test_platform

# Run tests with verbose output
pio test -e esp32 --verbose
```

## Test Structure

- `test_platform.cpp` - Platform detection tests
- `test_inference.cpp` - AI inference engine tests  
- `test_memory.cpp` - Memory management tests
- `test_integration.cpp` - Integration tests

## Hardware Requirements

Tests can run on any ESP32 development board. Some tests may require:
- PSRAM (for memory tests)
- SD card (for model loading tests)
- Specific GPIO connections (for audio tests)

## Mock vs Real Testing

Current tests use mock implementations. When real TensorFlow Lite integration is added, tests will be updated to support both mock and real inference modes.
