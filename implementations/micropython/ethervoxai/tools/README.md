# 🛠️ EthervoxAI MicroPython Tools

This directory contains development and deployment tools for EthervoxAI on microcontrollers.

## 📋 Available Tools

### 1. 🔄 `model_converter.py` - AI Model Converter
Convert and optimize AI models for microcontroller deployment.

**Features:**
- Model quantization (4-bit, 8-bit, 16-bit)
- Memory footprint optimization
- Platform-specific optimization
- Compatibility validation
- Deployment package generation

**Usage:**
```bash
# Convert TensorFlow Lite model for Raspberry Pi Pico
python model_converter.py --input wake_word.tflite --target pico --quantize 4bit

# Convert ONNX model for ESP32 with auto quantization
python model_converter.py --input voice_classifier.onnx --target esp32 --quantize auto

# Convert with custom output directory
python model_converter.py --input model.tflite --target pico_w --output ./converted_models/
```

**Supported Formats:**
- TensorFlow Lite (.tflite)
- ONNX (.onnx)  
- PyTorch (.pth, .pt)
- Custom binary (.bin)

**Target Platforms:**
- `pico` - Raspberry Pi Pico (264KB SRAM)
- `pico_w` - Raspberry Pi Pico W (264KB SRAM + WiFi)
- `esp32` - ESP32 (520KB SRAM)
- `esp32s2` - ESP32-S2 (320KB SRAM)
- `esp32s3` - ESP32-S3 (512KB SRAM)

### 2. 🚀 `deployment_tool.py` - Deployment Automation
Deploy EthervoxAI applications to microcontrollers automatically.

**Features:**
- Multiple deployment methods (USB, WiFi, Bluetooth)
- Automatic file transfer
- Dependency management
- Deployment verification
- Device discovery and information

**Usage:**
```bash
# Deploy core components via USB
python deployment_tool.py --target /dev/ttyUSB0 --components core examples

# Deploy to specific COM port on Windows
python deployment_tool.py --target COM3 --components core examples tools

# List available devices
python deployment_tool.py --list-devices

# Get device information
python deployment_tool.py --target /dev/ttyUSB0 --device-info

# Deploy with verification
python deployment_tool.py --target /dev/ttyUSB0 --verify
```

**Deployment Methods:**
- **USB/Serial**: mpremote, ampy, Thonny
- **WiFi**: WebREPL, FTP, HTTP (coming soon)
- **Bluetooth**: ESP32 Bluetooth transfer (coming soon)

**Components:**
- `core` - Essential EthervoxAI modules
- `examples` - Demo applications and examples
- `tools` - Development and utility tools
- `models` - Pre-trained AI models

## 🔧 Tool Requirements

### System Dependencies

#### For Model Converter:
```bash
# Python packages (install with pip)
pip install numpy
pip install tensorflow  # For .tflite models
pip install onnx        # For .onnx models  
pip install torch       # For .pth/.pt models
```

#### For Deployment Tool:
```bash
# Option 1: mpremote (recommended)
pip install mpremote

# Option 2: ampy (alternative)
pip install adafruit-ampy

# Option 3: Thonny IDE (GUI option)
# Download from: https://thonny.org/
```

### Hardware Requirements

#### USB/Serial Deployment:
- USB cable connected to microcontroller
- Working serial drivers
- Microcontroller in normal mode (not bootloader)

#### WiFi Deployment (future):
- ESP32 or Pico W with WiFi
- Both devices on same network
- WebREPL enabled on microcontroller

## 📊 Model Conversion Guidelines

### Memory Constraints by Platform

| Platform | Total RAM | Available for Models | Recommended Quantization |
|----------|-----------|---------------------|-------------------------|
| Pico | 264KB | ~180KB | 4-bit |
| Pico W | 264KB | ~150KB | 4-bit |
| ESP32 | 520KB | ~400KB | 8-bit |
| ESP32-S2 | 320KB | ~200KB | 8-bit |
| ESP32-S3 | 512KB | ~320KB | 8-bit |

### Quantization Trade-offs

| Quantization | Size Reduction | Speed Improvement | Accuracy Impact |
|--------------|----------------|-------------------|-----------------|
| 4-bit | 75% smaller | 3-4x faster | Medium (5-15%) |
| 8-bit | 50% smaller | 2x faster | Low (1-5%) |
| 16-bit | 25% smaller | 1.5x faster | Minimal (<1%) |

### Recommended Model Sizes

#### Wake Word Detection:
- **Pico**: <10KB (4-bit quantized)
- **ESP32**: <20KB (8-bit quantized)

#### Voice Command Classification:
- **Pico**: <50KB (4-bit quantized)
- **ESP32**: <100KB (8-bit quantized)

#### Simple Language Models:
- **Pico**: <80KB (4-bit quantized)
- **ESP32**: <200KB (8-bit quantized)

## 🚀 Deployment Workflows

### Development Workflow

1. **Develop** on computer with full Python/MicroPython environment
2. **Test** core functionality with simulators or local hardware
3. **Convert** AI models for target platform
4. **Deploy** to microcontroller for testing
5. **Verify** functionality on actual hardware
6. **Optimize** based on performance metrics

### Production Deployment

1. **Prepare** final deployment package
2. **Validate** all dependencies and configurations
3. **Deploy** to target devices (batch deployment for multiple units)
4. **Test** functionality on deployed devices
5. **Monitor** performance and memory usage

### Continuous Integration

```bash
# Example CI/CD pipeline
#!/bin/bash

# 1. Convert models
python tools/model_converter.py --input models/wake_word.tflite --target pico --output dist/

# 2. Run tests
python -m pytest tests/

# 3. Deploy to test device
python tools/deployment_tool.py --target /dev/ttyUSB0 --components core examples --verify

# 4. Run hardware tests
python examples/audio_io_test.py
```

## 🔍 Troubleshooting

### Model Conversion Issues

#### "Model too large for platform"
- Try more aggressive quantization (4-bit instead of 8-bit)
- Remove unnecessary model components
- Use smaller base model architecture
- Consider model pruning techniques

#### "Quantization failed"
- Verify input model format compatibility
- Check model architecture (some layers don't quantize well)
- Try different quantization methods
- Use pre-quantized models when available

### Deployment Issues

#### "No deployment tool available"
```bash
# Install mpremote (recommended)
pip install mpremote

# Or install ampy (alternative)  
pip install adafruit-ampy
```

#### "Device not found"
- Check USB cable and connection
- Verify correct port/device path
- Try different USB port
- Check device drivers (Windows)
- Ensure device is not in bootloader mode

#### "Permission denied" (Linux/macOS)
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Or use sudo for deployment
sudo python deployment_tool.py --target /dev/ttyUSB0
```

#### "Upload failed"
- Check available flash storage space
- Verify file permissions
- Try smaller deployment batches
- Reset device and retry

### Memory Issues

#### "Out of memory during deployment"
- Deploy components separately (`--components core` then `--components examples`)
- Clear device flash before deployment
- Use smaller models or more aggressive quantization
- Monitor memory usage during deployment

#### "Model loading failed on device"
- Check model size vs. available RAM
- Verify model conversion completed successfully
- Test with smaller models first
- Check model file integrity

## 📚 Advanced Usage

### Custom Model Conversion

```python
from tools.model_converter import ModelConverter

converter = ModelConverter()

# Custom conversion with specific parameters
result = converter.convert_model(
    input_path='my_model.tflite',
    target_platform='pico',
    quantization='4bit',
    output_dir='./models/'
)

if result['success']:
    print(f"Converted: {result['original_size_kb']}KB -> {result['converted_size_kb']}KB")
```

### Automated Deployment

```python
from tools.deployment_tool import DeploymentTool

deployer = DeploymentTool()

# Deploy with custom configuration
config = {
    'verify_deployment': True,
    'backup_existing': True,
    'restart_after_deploy': True
}

result = deployer.deploy(
    target='/dev/ttyUSB0',
    method='usb',
    components=['core', 'examples'],
    config=config
)
```

### Batch Deployment

```bash
# Deploy to multiple devices
for device in /dev/ttyUSB*; do
    echo "Deploying to $device"
    python deployment_tool.py --target $device --components core --verify
done
```

## 🔮 Future Enhancements

### Planned Features

- **WiFi Deployment**: Over-the-air updates via WebREPL/HTTP
- **Bluetooth Deployment**: Wireless deployment for ESP32
- **Model Optimization**: Advanced pruning and compression
- **Cloud Integration**: Download models from cloud repositories
- **GUI Tools**: Graphical interface for non-technical users
- **Monitoring**: Real-time performance monitoring and debugging

### Contributing

To contribute to the tools:

1. Follow existing code structure and documentation style
2. Add comprehensive error handling and logging
3. Include unit tests for new functionality
4. Update this README with new features
5. Test on multiple platforms and devices

## 📄 License

These tools are part of the EthervoxAI project and follow the same license terms.
