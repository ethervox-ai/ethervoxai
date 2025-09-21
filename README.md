# 🧠 EthervoxAI: Privacy-First, Multilingual Voice Intelligence for Embedded Devices

## 🎤 About EthervoxAI

EthervoxAI is a privacy-first voice assistant platform designed to work entirely offline while providing multilingual support and smart home integration. Built with real AI capabilities, cross-platform compatibility, and comprehensive audio processing.

## 🌟 Key Features

- **🌐 Multilingual Support**: Automatic language detection with support for English, Spanish, and Mandarin
- **🧠 Local LLM Stack**: Real AI models (TinyLlama, Phi-2, Mistral, Llama2) that run entirely on your device
- **🔐 Privacy Dashboard**: Complete control over data and cloud interactions with audit logging
- **📱 Cross-Platform Implementation**: TypeScript/Node.js, Python, C++, and MicroPython implementations
- **🎵 Advanced Audio Processing**: Multi-method audio I/O with comprehensive testing suite
- **🏠 Smart Home Ready**: Built for seamless integration with IoT devices and embedded systems
- **⚡ Platform Intelligence**: Automatic hardware detection and performance optimization
- **🔧 Development Tools**: Comprehensive testing, debugging, and development utilities

## 📁 Project Structure

```
ethervoxai/
├── 📚 docs/                          # Documentation and specifications
│   ├── modules/                      # Module-specific documentation
│   ├── AUDIO_ALTERNATIVES.md        # Audio system alternatives guide
│   ├── AUDIO-TESTING.md             # Audio testing procedures
│   └── README_WINDOWS_DEMO.md       # Windows demo documentation
├── 🛠️ implementations/               # Multi-language implementations
│   ├── python/                      # Python implementation with examples
│   ├── cpp/                         # C++ implementation for embedded systems
│   │   └── esp32/                   # ESP32 microcontroller support
│   ├── micropython/                 # MicroPython for microcontrollers
│   └── shared/                      # Cross-language shared resources
├── 🎯 src/                          # Main TypeScript/Node.js implementation
│   ├── modules/                     # Core AI and audio modules
│   │   ├── multilingualRuntime.ts   # Language detection and processing
│   │   ├── localLLMStack.ts         # AI model management
│   │   ├── privacyDashboard.ts      # Privacy controls and audit logging
│   │   ├── modelManager.ts          # AI model downloading and caching
│   │   ├── inferenceEngine.ts       # AI inference and streaming
│   │   ├── platformDetector.ts      # Hardware detection and optimization
│   │   └── crossPlatformAudio.ts    # Multi-method audio I/O
│   ├── demo/                        # Demo applications
│   │   └── windows-desktop.ts       # Windows desktop demo with audio I/O
│   ├── examples/                    # Example implementations
│   │   └── ui/                      # Optional web and mobile UI examples
│   └── index.ts                     # Main entry point and API
├── 🧪 tests/                        # Comprehensive testing suite
│   └── audio-input-output/          # Interactive audio testing console
├── 📦 scripts/                      # Build, setup, and demo scripts
│   ├── demo/                        # Demo launchers
│   ├── setup/                       # Installation and setup scripts
│   └── testing/                     # Testing and verification utilities
├── 📋 specs/                        # Protocol and interface specifications
│   └── ethervoxai-protocol.md       # Cross-language protocol definitions
├── ⚙️ config/                       # Configuration files
│   ├── audio.json                   # Audio system configuration
│   └── privacy.json                 # Privacy settings
├── 📄 package.json                  # Node.js dependencies and scripts
├── 🔧 tsconfig*.json                # TypeScript build configurations
└── 📖 README.md                     # This file
```

## 🚀 Quick Start

### Prerequisites

- **Node.js 16+** (recommended: Node.js 18 or later)
- **npm 8+** or yarn 1.22+
- **Windows 10/11** (for full demo functionality)

### Core Installation

```bash
# Clone the repository
git clone https://github.com/ethervox-ai/ethervoxai.git
cd ethervoxai

# Install core dependencies (no React conflicts)
npm install

# Build the project
npm run build

# Run the main demo
npm run demo

# Test audio capabilities (interactive console)
npm run test:audio
```

### Available Demo Options

**🎤 Main Demo (Core Functionality)**
```bash
npm run demo                    # Core AI and voice processing demo
```

**🖥️ Windows Desktop Demo (Advanced)**
```bash
npm run demo:windows           # Windows-specific audio integration demo
```

**🧪 Audio Testing Suite**
```bash
npm run test:audio             # Interactive audio input/output testing
```

**🤖 AI Integration Demo**
```bash
npm run demo:ai                # Test local AI model integration
```

### Platform-Specific Setup

**For Production Audio (Windows)**
```bash
# Install advanced audio dependencies
cd scripts/setup
.\install-audio-libraries.bat
```

**For ESP32/Embedded Development**
```bash
# Navigate to C++ implementation
cd implementations/cpp/esp32
# Follow ESP32-specific build instructions
```

### Optional UI Development

The UI examples are provided as optional components to avoid dependency conflicts:

**Web Dashboard (React)**
```bash
# Install React dependencies for web dashboard
npm install react@^18.2.0 react-dom@^18.2.0
npm install --save-dev @types/react @types/react-dom

# Build web examples
npm run build:web

# Run UI demo
npm run demo:ui
```

**Mobile Dashboard (React Native)**
```bash
# Install React Native (cannot coexist with React DOM)
npm install react@18.2.0 react-native@^0.72.0
npm install --save-dev @types/react-native

# Build mobile examples
npm run build:mobile
```

**Note**: React DOM and React Native cannot be installed simultaneously due to version conflicts. Choose based on your target platform.

### Basic Usage

```typescript
import { EthervoxAI } from 'ethervoxai';

// Create and configure EthervoxAI instance
const ai = new EthervoxAI({
  defaultLanguage: 'en-US',
  preferredModel: 'tinyllama-1.1b-chat-q4',
  privacyMode: 'balanced',
  enableCloudFallback: false
});

// Initialize the system (downloads models if needed)
await ai.initialize();

// Process text input with local AI
const result = await ai.processTextInput('What is the weather like today?');
console.log(`Response: ${result.response}`);
console.log(`Confidence: ${Math.round(result.confidence * 100)}%`);
console.log(`Model: ${result.model}`);

// Test audio capabilities
const audioManager = ai.getAudioManager();
await audioManager.testOutputMethods();
```

### Module Usage (Direct Access)

```typescript
// Import individual modules for fine-grained control
import { 
  multilingualRuntime, 
  localLLMStack, 
  privacyDashboard,
  modelManager,
  platformDetector,
  crossPlatformAudio 
} from 'ethervoxai';

// Detect platform capabilities
const capabilities = await platformDetector.getCapabilities();
console.log(`Platform: ${capabilities.platform} (${capabilities.performanceTier})`);

// Get recommended models for your hardware
const models = await modelManager.getRecommendedModels();
console.log('Recommended models:', models.map(m => m.name));

// Test audio output methods
const audioManager = new crossPlatformAudio.CrossPlatformAudioManager();
await audioManager.initialize();
await audioManager.playText('Hello from EthervoxAI!');
```

## 🔧 Configuration & Capabilities

### Hardware Detection & Optimization

EthervoxAI automatically detects your hardware and optimizes performance:

```typescript
import { platformDetector } from 'ethervoxai';

const caps = await platformDetector.getCapabilities();
// Returns: {
//   platform: 'windows' | 'linux' | 'darwin' | 'raspberry-pi',
//   performanceTier: 'low' | 'medium' | 'high' | 'ultra',
//   totalMemory: number,
//   cpuCores: number,
//   hasGPU: boolean,
//   vectorExtensions: ['AVX2', 'NEON', etc.],
//   isRaspberryPi: boolean,
//   recommendedThreads: number
// }
```

### Audio System Configuration

Multiple audio output methods with automatic fallback:

```typescript
import { CrossPlatformAudioManager } from 'ethervoxai';

const audioManager = new CrossPlatformAudioManager({
  preferredOutput: 'native',
  fallbackChain: ['native', 'wav-player', 'play-sound', 'tts-only'],
  enableLogging: true
});

// Test all available audio methods
await audioManager.testOutputMethods();
```

**Available Audio Methods:**
- **Native TTS**: Windows SAPI, macOS Speech Synthesis, Linux espeak
- **WAV Player**: Pure JavaScript audio playback
- **Play-Sound**: Cross-platform sound wrapper  
- **Node Speaker**: High-quality audio streaming
- **PowerShell TTS**: Windows PowerShell speech synthesis

### AI Model Configuration

EthervoxAI supports real local AI models with automatic hardware optimization:

**Available Models:**
- **tinyllama-1.1b-chat-q4**: Lightweight (669MB) - Perfect for Raspberry Pi and low-end systems
- **phi-2-2.7b-q4**: Microsoft's efficient model (1.6GB) - Optimized for ARM processors  
- **mistral-7b-instruct-v0.1-q4**: High-quality instruction model (4.1GB) - Excellent for conversations
- **llama2-7b-chat-q4**: Meta's popular chat model (3.9GB) - General purpose conversations
- **llama2-13b-chat-q4**: Larger model for high-end systems (7.3GB) - Enhanced capabilities

```typescript
import { modelManager } from 'ethervoxai';

// Get models recommended for your hardware
const recommended = await modelManager.getRecommendedModels();

// Download and cache a specific model
const modelPath = await modelManager.getModelPath('tinyllama-1.1b-chat-q4');

// Check model compatibility
const isCompatible = await modelManager.isModelCompatible('mistral-7b-instruct-v0.1-q4');
```

## 📚 Documentation & Development

### Module Documentation

Each module has comprehensive documentation in the `docs/modules/` directory:

- **[Multilingual Runtime](docs/modules/multilingual-runtime.md)** - Language detection and speech processing
- **[Local LLM Stack](docs/modules/local-llm-stack.md)** - AI model management and routing  
- **[Privacy Dashboard](docs/modules/privacy-dashboard.md)** - Privacy controls and audit logging

### Additional Documentation

- **[Audio Alternatives Guide](docs/AUDIO_ALTERNATIVES.md)** - Comprehensive audio system options
- **[Audio Testing Procedures](docs/AUDIO-TESTING.md)** - Testing and troubleshooting audio
- **[Windows Demo Guide](docs/README_WINDOWS_DEMO.md)** - Windows-specific demo instructions
- **[Installation Guide](INSTALLATION.md)** - Detailed setup and dependency management
- **[Multi-Language Strategy](MULTI-LANGUAGE-STRATEGY.md)** - Cross-platform implementation guide
- **[AI Integration Summary](AI-INTEGRATION-SUMMARY.md)** - Local AI model integration details

### Development Scripts

**Build & Development:**
```bash
npm run build              # Build TypeScript core
npm run build:web          # Build web dashboard (requires React)
npm run build:mobile       # Build mobile dashboard (requires React Native)
npm run build:all          # Build core + web components
npm run dev                # Watch mode for development
npm run typecheck          # Type checking only
```

**Testing & Quality:**
```bash
npm run test               # Run all tests
npm run test:watch         # Run tests in watch mode
npm run test:coverage      # Run tests with coverage report
npm run test:audio         # Interactive audio testing console
npm run lint               # Lint and fix code
npm run lint:core          # Lint core modules only
```

**Demos & Examples:**
```bash
npm run demo               # Main EthervoxAI demo
npm run demo:windows       # Windows desktop audio demo  
npm run demo:ai            # AI integration testing
npm run demo:ui            # Web UI demo (requires React)
```

## 🔐 Privacy & Security

EthervoxAI is designed with privacy as a core principle:

- **🏠 Local Processing**: All voice and AI processing happens on-device by default
- **🔒 Encrypted Communication**: Any optional cloud communications are end-to-end encrypted  
- **👤 Data Control**: Users have complete control over data retention and deletion
- **📋 Audit Logging**: Comprehensive logging of all system interactions with privacy dashboard
- **✅ Consent Management**: Granular consent controls for cloud services and data sharing
- **🌍 Cross-Platform Privacy**: Consistent privacy controls across all implementation languages
- **🔍 No Telemetry**: No automatic data collection or telemetry by default
- **⚡ Offline Capable**: Full functionality without internet connection

### Privacy Dashboard Features

- Real-time privacy status monitoring
- Data retention policy management  
- Cloud service consent controls
- Audit log viewer and export
- Model download and caching controls
- Cross-language implementation privacy sync

### Privacy Modes & Language Support

**Privacy Modes:**
- **Strict**: All processing happens locally, user consent required for any cloud access
- **Balanced**: Local processing with optional cloud fallback for complex queries  
- **Permissive**: Allows cloud processing for enhanced capabilities

**Supported Languages (MVP):**
- English (US/UK)
- Spanish (Latin America)
- Mandarin (Simplified Chinese)

**Multi-Platform Implementations:**
- **TypeScript/Node.js**: Full-featured reference implementation
- **Python**: Core functionality with examples
- **C++**: Embedded systems and ESP32 support
- **MicroPython**: Microcontroller optimization

## 🚀 Platform Support & Implementations

### Primary Implementation (TypeScript/Node.js)
- **Full Feature Set**: Complete AI integration with all modules
- **Cross-Platform**: Windows 10/11, macOS, Linux, Raspberry Pi
- **Audio I/O**: 5+ audio methods with automatic fallback
- **Development Tools**: Comprehensive testing and debugging suite
- **Performance Tiers**: Automatic optimization for hardware capabilities

### Additional Language Implementations
- **🐍 Python**: Core functionality with examples (`implementations/python/`)
- **⚡ C++**: Embedded systems and ESP32 support (`implementations/cpp/`)  
- **🔬 MicroPython**: Microcontroller optimization (`implementations/micropython/`)
- **🔄 Shared Protocols**: Consistent APIs across all implementations

### Hardware Compatibility
- **💻 Desktop/Laptop**: Full functionality on Windows/macOS/Linux
- **🍓 Raspberry Pi**: Optimized models and audio processing  
- **📱 ESP32**: C++ implementation for microcontrollers
- **🖥️ ARM Systems**: Native support with vector optimization
- **☁️ Cloud/Server**: Headless operation with REST APIs

## 🎯 Roadmap & Future Development

### Current Status (✅ Completed)
- ✅ Real local AI model integration (TinyLlama, Phi-2, Mistral, Llama2)
- ✅ Multi-platform audio processing with 5+ output methods
- ✅ Hardware detection and automatic optimization
- ✅ Privacy dashboard with audit logging
- ✅ Cross-platform implementations (Python, C++, MicroPython)
- ✅ Comprehensive testing and development tools
- ✅ ESP32 and embedded system support

### Near-term Goals (🔄 In Progress)
- 🔄 Expand language support to 15+ languages
- 🔄 Voice cloning and customization capabilities
- 🔄 RAG (Retrieval-Augmented Generation) integration
- 🔄 Plugin ecosystem for third-party integrations
- 🔄 Mobile app development (React Native)

### Long-term Vision (📋 Planned)
- 📋 Hardware device integration and manufacturing
- 📋 Distributed AI model sharing (privacy-preserving)
- 📋 Advanced voice synthesis and emotion detection
- 📋 Multi-modal AI (vision, audio, text) integration
- 📋 Commercial licensing and enterprise features

## 🤝 Contributing to EthervoxAI

*By contributing, you agree your work is released under [CC BY-NC-SA 4.0](LICENSE). Commercial redistribution of your work is prohibited without approval.*

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## 📄 License & Legal

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

**Key License Points:**
- ✅ Commercial use permitted
- ✅ Modification and distribution allowed
- ✅ Private use encouraged
- ⚠️ No warranty provided
- 📋 License and copyright notice required

**Additional Legal Documentation:**
- [Code of Conduct](CODE_OF_CONDUCT.md)
- [Security Policy](SECURITY.md)  
- [Legal Documentation](docs/legal.md)
- [License Comparison](docs/license-comparison.md)

## 📞 Support & Community

For questions, issues, or contributions, please:

1. **📖 Check Documentation**: Review [docs/](docs/) and module-specific guides
2. **🔍 Search Issues**: Check existing [GitHub issues](https://github.com/ethervox-ai/ethervoxai/issues)
3. **🧪 Test Audio**: Run `npm run test:audio` for audio-related issues
4. **🆕 Create Issue**: File a new issue with detailed information
5. **💬 Discussions**: Join community discussions for feature requests

### Getting Help

**For Installation Issues:**
- Review [INSTALLATION.md](INSTALLATION.md) for dependency troubleshooting
- Check [Audio Alternatives Guide](docs/AUDIO_ALTERNATIVES.md) for audio problems
- Run the audio test suite: `npm run test:audio`

**For Development Questions:**
- See [Multi-Language Strategy](MULTI-LANGUAGE-STRATEGY.md) for implementation guidance
- Check [AI Integration Summary](AI-INTEGRATION-SUMMARY.md) for model-related questions
- Review module documentation in [docs/modules/](docs/modules/)

**For Hardware-Specific Issues:**
- Test hardware detection: `npm run demo` and check platform detection output
- Review ESP32 documentation: [implementations/cpp/esp32/README.md](implementations/cpp/esp32/README.md)
- Check Raspberry Pi optimization guidelines

---

**Built with ❤️ for privacy-conscious developers and users who want powerful AI without compromising their data.**

*EthervoxAI - Where Privacy Meets Intelligence* 🧠🔐
