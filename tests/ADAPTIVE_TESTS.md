# Adaptive Configuration Tests

## Overview

Comprehensive test suite for device profiling and GGUF-based adaptive configuration.

## Test Files

### 1. Device Profile Tests (`test_device_profile.c`)

Tests the runtime hardware detection and tier-based configuration system.

**What It Tests**:
- ✅ CPU core detection via `sysconf(_SC_NPROCESSORS_ONLN)`
- ✅ RAM detection via `sysinfo()`
- ✅ Device tier classification (LOW/MEDIUM/HIGH/ULTRA)
- ✅ Optimal thread count selection (2-6 threads)
- ✅ Optimal batch size selection (256/512/1024)
- ✅ KV cache type selection (F16/Q8_0/Q4_0)
- ✅ Flash attention decision
- ✅ Tier-specific settings consistency
- ✅ Re-initialization safety
- ✅ Settings appropriateness for hardware

**Key Validations**:
- Thread count doesn't exceed CPU cores
- Batch size is appropriate for RAM
- Low-RAM devices (<4GB) use smaller batches
- Settings are consistent within each tier

### 2. GGUF Config Tests (`test_gguf_config.c`)

Tests GGUF metadata extraction and optimal configuration calculation.

**What It Tests**:
- ✅ Metadata extraction (stub mode with safe defaults)
- ✅ KV cache memory estimation
- ✅ Context support checking based on RAM
- ✅ Optimal config calculation (context, batch, threads, KV type)
- ✅ Tier-based context scaling
- ✅ Config consistency with device profile
- ✅ Memory-constrained scenarios
- ✅ NULL pointer handling
- ✅ Small model optimization

**Key Validations**:
- KV cache scales linearly with context length
- Context length is capped based on device tier
- Total memory usage (model + KV + reserved) fits in RAM
- Config matches device profile recommendations
- Small models use their full context length

## Building and Running

### Build Tests

```bash
cd ethervox_core/src/main/cpp/build
cmake ..
make test_device_profile test_gguf_config
```

### Run Individual Tests

```bash
# Device profiling tests
./tests/test_device_profile

# GGUF config tests
./tests/test_gguf_config
```

### Run All Adaptive Tests

```bash
ctest -L adaptive -V
```

### Expected Output

**Device Profile Tests**:
```
=== Device Profile Tests ===

Testing device_profile_init...
  ✓ Device profile initialization works
Testing CPU core detection...
  ✓ Detected 8 CPU cores
Testing RAM detection...
  ✓ Detected 16384 MB total RAM
Testing device tier classification...
  ✓ Device classified as: ULTRA tier
Testing optimal thread count...
  ✓ Optimal threads: 6
Testing optimal batch size...
  ✓ Optimal batch size: 1024
Testing optimal KV cache type...
  ✓ Optimal KV cache type: F16 (enum 1)
Testing flash attention decision...
  ✓ Flash attention: ENABLED
...

=== All Device Profile Tests Passed ===
```

**GGUF Config Tests**:
```
=== GGUF Config Helper Tests ===

Testing GGUF metadata extraction (stub mode)...
  ✓ Stub mode returns safe defaults
Testing KV cache memory estimation...
  ✓ KV cache estimation: 2K=512 MB, 4K=1024 MB, 8K=2048 MB
Testing context support checking...
  ✓ Context support: 2K=YES, 32K=NO
Testing optimal config calculation...
  ✓ Calculated config: context=8192, batch=1024, threads=6, kv_type=1
...

=== All GGUF Config Helper Tests Passed ===
```

## Test Categories

### Tier-Specific Tests

Tests verify correct settings for each device tier:

| Tier | RAM | Cores | Threads | Batch | Flash |
|------|-----|-------|---------|-------|-------|
| LOW | <4GB | <4 | 2 | 256 | NO |
| MEDIUM | 4-6GB | 4+ | 4 | 512 | YES |
| HIGH | 6-8GB | 6+ | 4 | 1024 | YES |
| ULTRA | >8GB | 8+ | 6 | 1024 | YES |

### Memory Constraint Tests

Tests verify that configurations respect device memory limits:
- Model size + KV cache + 1GB reserved ≤ Total RAM
- Context is reduced automatically if needed
- Low-RAM devices get smaller batch sizes

## Android Testing

### On-Device Testing

```bash
# Build for Android
./gradlew :ethervox_core:assembleDebug

# Push and run tests on device
adb push build/test_device_profile /data/local/tmp/
adb shell /data/local/tmp/test_device_profile

adb push build/test_gguf_config /data/local/tmp/
adb shell /data/local/tmp/test_gguf_config
```

### Check Logs

```bash
# Device profiling logs
adb logcat | grep "Device Profile"

# Example output:
# [Device Profile] Initialized:
#   SoC: unknown (blocked by Android security)
#   CPU Cores: 8
#   Total RAM: 8192 MB
#   Available RAM: 5120 MB
#   NEON Support: YES
#   Device Tier: HIGH
```

## Continuous Integration

These tests are part of the CI pipeline and run on every commit:

```yaml
# .github/workflows/test.yml
- name: Run Adaptive Config Tests
  run: |
    cd ethervox_core/src/main/cpp/build
    ctest -L adaptive --output-on-failure
```

## Test Coverage

### What's Tested

- ✅ Hardware detection (CPU, RAM)
- ✅ Tier classification logic
- ✅ Parameter selection algorithms
- ✅ Memory constraint handling
- ✅ Configuration consistency
- ✅ Error handling and edge cases

### What's NOT Tested (Future Work)

- ⏸️ Actual GGUF file parsing (requires llama.cpp integration)
- ⏸️ GPU detection and configuration
- ⏸️ Real model loading with metadata extraction
- ⏸️ Performance benchmarking on different tiers

## Debugging Failed Tests

### Common Issues

**Test Fails with "assertion failed"**:
- Check device specs match tier expectations
- Verify sysconf/sysinfo work on your platform
- Check Android security doesn't block APIs

**Memory calculation failures**:
- Verify your device has sufficient RAM
- Check memory estimate formulas in code
- Test with smaller models/contexts

**Tier classification incorrect**:
- Print actual RAM and CPU values
- Verify tier boundaries in `device_profile.c`
- Check for tier override via system property

## References

- **Implementation**: `src/platform/device_profile.c`
- **API Docs**: `include/ethervox/device_profile.h`
- **Config Helper**: `src/platform/gguf_config_helper.c`
- **Spec**: `docs/implementation/ADAPTIVE_CONFIG_FROM_GGUF.md`
- **Architecture**: `docs/ADAPTIVE_HARDWARE_PROFILING.md`
