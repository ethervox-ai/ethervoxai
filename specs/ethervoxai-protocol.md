# 🌐 EthervoxAI Cross-Language Protocol Specification

## Overview
This document defines the protocol interfaces that enable EthervoxAI to run consistently across TypeScript/Node.js, Python, MicroPython, and C++ implementations while maintaining core functionality and interoperability.

## Core Principles
1. **Protocol-First Design**: Define interfaces before implementations
2. **Language Neutrality**: Specs work across all target languages
3. **Platform Awareness**: Each implementation optimizes for its platform
4. **Binary Compatibility**: Shared model formats and data structures
5. **Minimal Duplication**: Logic in specs, platform specifics in implementations

---

## 📊 **Platform Detection Protocol**

### Interface Specification
```json
{
  "platformDetection": {
    "systemCapabilities": {
      "hardware": {
        "totalMemory": "number (MB)",
        "availableMemory": "number (MB)", 
        "cpuCores": "number",
        "architecture": "string (x64|arm64|arm32|riscv)"
      },
      "platform": {
        "os": "string (windows|linux|darwin|freertos|micropython)",
        "deviceType": "string (desktop|mobile|embedded|microcontroller)",
        "isRaspberryPi": "boolean",
        "raspberryPiModel": "string?"
      },
      "aiCapabilities": {
        "hasGPU": "boolean",
        "hasNeuralEngine": "boolean",
        "hasAVX2": "boolean", 
        "hasNEON": "boolean",
        "hasDSP": "boolean"
      },
      "performanceTier": "string (ultra|high|medium|low|micro)",
      "constraints": {
        "maxModelSize": "number (MB)",
        "maxContextLength": "number (tokens)",
        "recommendedThreads": "number",
        "useMemoryMapping": "boolean"
      }
    }
  }
}
```

### Implementation Requirements
- **Node.js**: Use `os` module and `/proc` filesystem
- **Python**: Use `psutil`, `platform`, `/proc` access
- **MicroPython**: Use `machine`, `gc.mem_info()`, board-specific APIs
- **C++**: Use system calls, board SDKs, FreeRTOS APIs

---

## 🗃️ **Model Management Protocol**

### Model Catalog Format
```json
{
  "models": [
    {
      "id": "tinyllama-1.1b-q4",
      "name": "TinyLlama 1.1B Q4",
      "architecture": "llama",
      "quantization": "q4_0",
      "sizeBytes": 701685760,
      "requiredMemory": 1073741824,
      "contextLength": 2048,
      "supportedPlatforms": ["desktop", "mobile", "embedded"],
      "minPerformanceTier": "low",
      "downloadUrls": {
        "primary": "https://huggingface.co/...",
        "mirror": "https://..."
      },
      "checksum": {
        "sha256": "abc123...",
        "md5": "def456..."
      },
      "metadata": {
        "license": "Apache-2.0",
        "tags": ["chat", "lightweight", "embedded"],
        "description": "Lightweight model for resource-constrained devices"
      }
    }
  ]
}
```

### Download Protocol
```json
{
  "downloadRequest": {
    "modelId": "string",
    "targetPath": "string",
    "resumeSupported": "boolean",
    "progressCallback": "function"
  },
  "downloadProgress": {
    "modelId": "string",
    "status": "string (downloading|verifying|complete|error)",
    "bytesDownloaded": "number",
    "totalBytes": "number",
    "percentage": "number",
    "speedBytesPerSecond": "number",
    "etaSeconds": "number",
    "error": "string?"
  }
}
```

---

## 🚀 **Inference Engine Protocol**

### Inference Request/Response
```json
{
  "inferenceRequest": {
    "modelId": "string",
    "prompt": "string",
    "parameters": {
      "maxTokens": "number",
      "temperature": "number",
      "topP": "number",
      "stopSequences": ["string"]
    },
    "streaming": "boolean"
  },
  "inferenceResponse": {
    "text": "string",
    "tokensGenerated": "number",
    "tokensPerSecond": "number",
    "finished": "boolean",
    "finishReason": "string (length|stop|error)",
    "timings": {
      "promptEvalTime": "number (ms)",
      "generateTime": "number (ms)",
      "totalTime": "number (ms)"
    }
  }
}
```

### Streaming Protocol
```json
{
  "streamingToken": {
    "token": "string",
    "tokenId": "number",
    "logProb": "number?",
    "finished": "boolean"
  }
}
```

---

## 🔄 **Inter-Process Communication**

### Message Protocol (JSON-RPC Style)
```json
{
  "jsonrpc": "2.0",
  "method": "string",
  "params": "object",
  "id": "string|number"
}
```

### Transport Layers
- **Desktop**: Unix sockets, Named pipes, HTTP
- **Embedded**: UART, I2C, SPI, WiFi/BLE
- **Mobile**: Intent system, IPC mechanisms

---

## 🔐 **Privacy & Audit Protocol**

### Audit Log Format
```json
{
  "auditEntry": {
    "timestamp": "ISO8601",
    "sessionId": "string",
    "eventType": "string (query|response|model_load|privacy_change)",
    "data": "object",
    "privacyLevel": "string (local|cloud|external)",
    "userId": "string?",
    "deviceId": "string"
  }
}
```

---

## 📁 **File System Protocol**

### Directory Structure
```
.ethervoxai/
├── models/           # Downloaded models
│   ├── cache.json   # Model metadata
│   └── *.gguf       # Model files
├── config/          # Configuration files
│   ├── platform.json
│   └── privacy.json
├── logs/            # Audit and debug logs
└── temp/            # Temporary files
```

### Configuration Format
```json
{
  "platform": {
    "detectedCapabilities": "SystemCapabilities",
    "userOverrides": "object",
    "lastDetection": "ISO8601"
  },
  "privacy": {
    "localProcessingOnly": "boolean",
    "auditingEnabled": "boolean",
    "dataRetentionDays": "number"
  }
}
```

---

## 🎯 **Implementation Targets**

### Target Platforms
1. **Desktop/Server** (Node.js, Python)
   - Full feature set
   - Large model support
   - Web interfaces

2. **Embedded Linux** (Python, C++)
   - Raspberry Pi, Jetson
   - Medium models
   - GPIO integration

3. **Microcontrollers** (C++, MicroPython)
   - ESP32, Pico, STM32
   - Tiny models or edge inference
   - IoT integration

4. **Mobile** (Platform-specific)
   - React Native wrapper
   - iOS/Android native

### Capability Matrix
| Platform | Models | Inference | UI | Network |
|----------|--------|-----------|----|---------| 
| Desktop | Full | Full | Web | Full |
| Embedded | Medium | Local | Mobile | WiFi |
| MCU | Tiny | Edge | LED/LCD/Audio | BLE/WiFi |
| Mobile | Medium | Hybrid | Native | Cellular |

---

## 🔧 **Implementation Strategy**

### Phase 1: Protocol Definition (Current)
- [x] Define core interfaces
- [x] Create specification documents
- [x] Design message protocols

### Phase 2: Reference Implementation (Node.js - Done)
- [x] TypeScript implementation with full features
- [x] Comprehensive testing
- [x] Documentation and examples

### Phase 3: Python Implementation
- [ ] Desktop Python version
- [ ] Raspberry Pi optimizations
- [ ] Cross-platform compatibility

### Phase 4: C++ Implementation  
- [ ] Desktop C++ version
- [ ] Embedded Linux support
- [ ] Real-time optimizations

### Phase 5: MicroPython Implementation
- [ ] ESP32/Pico support
- [ ] Memory-optimized inference
- [ ] IoT integration features

### Phase 6: Platform Integration
- [ ] Mobile React Native wrappers
- [ ] Hardware-specific optimizations
- [ ] Multi-language orchestration

---

## 🧪 **Testing & Validation**

### Cross-Language Test Suite
- Protocol compliance tests
- Performance benchmarks
- Interoperability validation
- Platform-specific testing

### Continuous Integration
- Multi-platform builds
- Cross-language compatibility
- Performance regression testing
- Documentation synchronization

---

This protocol specification ensures that all EthervoxAI implementations maintain compatibility while optimizing for their specific platforms and use cases.
