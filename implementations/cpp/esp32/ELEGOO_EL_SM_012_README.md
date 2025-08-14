# 🔧 ELEGOO EL-SM-012 Module Configuration

This directory contains the configuration and optimization files for the **ELEGOO EL-SM-012** ESP32 development module, specifically tailored for EthervoxAI applications.

## 📋 Module Overview

The ELEGOO EL-SM-012 is an ESP32-based development module optimized for AI and IoT applications. This configuration provides:

- **Hardware-specific optimizations** for the EL-SM-012 module
- **Custom partition table** optimized for AI model storage
- **Pin mapping** configuration for module-specific peripherals
- **Performance tuning** for AI inference workloads
- **Power management** optimized for the module's capabilities

## 🚀 Quick Start

### 1. Build for ELEGOO EL-SM-012

```bash
# Use the dedicated build script
.\build_elegoo.bat

# Or build directly with PlatformIO
pio run -e elegoo_el_sm_012
```

### 2. Upload to Module

```bash
# Upload firmware
pio run -e elegoo_el_sm_012 -t upload

# Monitor serial output
pio device monitor -e elegoo_el_sm_012
```

### 3. Run Tests

```bash
# Run ELEGOO-specific tests
pio test -e elegoo_el_sm_012 -f test_elegoo_el_sm_012
```

## 📁 Configuration Files

### Core Configuration
- **`platformio.ini`** - Contains `[env:elegoo_el_sm_012]` environment
- **`elegoo_el_sm_012_config.h`** - Hardware-specific definitions
- **`partitions_elegoo.csv`** - Custom partition table for AI workloads

### Test Files
- **`test_elegoo_el_sm_012.cpp`** - Comprehensive module testing
- **`build_elegoo.bat`** - Automated build and test script

## ⚙️ Hardware Specifications

### Module Capabilities
```cpp
// Board identification
#define BOARD_NAME "ELEGOO EL-SM-012"
#define BOARD_VERSION "1.0"
#define BOARD_VENDOR "ELEGOO"

// Hardware features
#define HAS_WIFI true
#define HAS_BLUETOOTH true
#define HAS_PSRAM false          // Adjust based on actual module
#define FLASH_SIZE_MB 4          // Typical ESP32 configuration
#define CPU_FREQ_MHZ 240         // Full performance mode
```

### Pin Assignments
```cpp
// LED indicators
#define LED_BUILTIN 2
#define LED_RED     25
#define LED_GREEN   26
#define LED_BLUE    27

// Communication interfaces
#define SDA_PIN     21           // I2C Data
#define SCL_PIN     22           // I2C Clock
#define SPI_MOSI    23           // SPI Master Out
#define SPI_MISO    19           // SPI Master In
#define SPI_SCK     18           // SPI Clock
#define SPI_CS      5            // SPI Chip Select
```

## 🧠 AI Optimization Settings

### Memory Configuration
```cpp
// AI model constraints
#define MAX_MODEL_SIZE_BYTES (512 * 1024)      // 512KB max model
#define TENSOR_ARENA_SIZE_BYTES (8 * 1024)     // 8KB tensor workspace
#define AUDIO_BUFFER_SIZE_SAMPLES 1024         // Audio processing
```

### Performance Settings
```cpp
// CPU and memory optimization
#define OPTIMIZE_FOR_SPEED true
#define ENABLE_WATCHDOG true
#define STACK_GUARD_SIZE 512
```

## 📊 Partition Table Layout

The custom partition table (`partitions_elegoo.csv`) is optimized for AI workloads:

| Partition | Type | Size | Purpose |
|-----------|------|------|---------|
| `app0` | Application | 1.6MB | Primary firmware |
| `app1` | Application | 1.6MB | OTA update slot |
| `model` | Data (FAT) | 512KB | AI model storage |
| `config` | Data (NVS) | 64KB | Configuration data |
| `logs` | Data (FAT) | 64KB | Runtime logs |
| `spiffs` | Data (SPIFFS) | 64KB | File system |

## 🔬 Testing Framework

### Comprehensive Tests
The `test_elegoo_el_sm_012.cpp` file includes:

- **Module Detection** - Verify correct board identification
- **GPIO Functionality** - Test all configured pins
- **Communication Buses** - I2C, SPI, UART validation
- **Memory Performance** - RAM and flash testing
- **CPU Performance** - Computation benchmarks
- **Connectivity** - WiFi and Bluetooth capability tests
- **Power Management** - Power mode and frequency scaling

### Running Tests
```bash
# Run all ELEGOO tests
pio test -e elegoo_el_sm_012 -f test_elegoo_el_sm_012

# Run with verbose output
pio test -e elegoo_el_sm_012 -f test_elegoo_el_sm_012 -v
```

## 🔧 Customization

### Hardware Variations
If your ELEGOO EL-SM-012 module has different specifications:

1. **Update hardware definitions** in `elegoo_el_sm_012_config.h`
2. **Adjust memory settings** for your specific module variant
3. **Modify pin assignments** based on your module's pinout
4. **Update partition sizes** if different flash capacity

### Example Customization
```cpp
// In elegoo_el_sm_012_config.h
#define FLASH_SIZE_MB 8          // If your module has 8MB flash
#define HAS_PSRAM true           // If your module includes PSRAM
#define PSRAM_SIZE_KB 4096       // 4MB PSRAM configuration
```

## 🐛 Troubleshooting

### Common Issues

**Build Errors:**
- Ensure PlatformIO is updated: `pio upgrade`
- Clean build directory: `pio run -e elegoo_el_sm_012 -t clean`

**Upload Failures:**
- Check COM port in `platformio.ini`
- Verify module connection and drivers
- Try different upload speeds (115200, 921600)

**Test Failures:**
- Verify hardware connections
- Check pin assignments in config file
- Ensure proper power supply

### Debug Configuration
```ini
# Enable detailed debugging
build_flags = 
    ${env:elegoo_el_sm_012.build_flags}
    -DCORE_DEBUG_LEVEL=5
    -DDEBUG_ESP_PORT=Serial
```

## 📚 Additional Resources

- **[EthervoxAI Documentation](../README.md)** - Main project documentation
- **[ESP32 Platform Guide](../docs/)** - General ESP32 implementation guide
- **[ELEGOO Official Support](https://www.elegoo.com/pages/support-center)** - Module documentation

## 🤝 Contributing

When contributing ELEGOO EL-SM-012 specific improvements:

1. Test changes with the dedicated test suite
2. Update configuration files as needed
3. Document hardware-specific features
4. Maintain compatibility with other ESP32 variants

## 📞 Support

For ELEGOO EL-SM-012 specific issues:
- Check the test output for hardware validation
- Verify pin assignments match your module variant
- Consult ELEGOO documentation for module specifications
- Open an issue with test results and module details
