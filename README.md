# 🧠 EthervoxAI: Privacy-First, Multilingual Voice Intelligence for Embedded Devices

## 🎤 EthervoxAI

EthervoxAI is a privacy-first voice assistant platform designed to work entirely offline while providing multilingual support and smart home integration.

## 🌟 Features

- **🌐 Multilingual Support**: Automatic language detection with support for English, Spanish, and Mandarin
- **🧠 Local LLM Stack**: Privacy-preserving AI models that run entirely on your device
- **🔐 Privacy Dashboard**: Complete control over data and cloud interactions
- **📱 Cross-Platform UI**: Web and mobile dashboard interfaces
- **🏠 Smart Home Ready**: Built for seamless integration with IoT devices

## 📁 Project Structure

```
ethervoxai/
├── docs/
│   └── modules/
│       ├── multilingual-runtime.md
│       ├── local-llm-stack.md
│       ├── privacy-dashboard.md
│       └── README.md
├── src/
│   ├── modules/
│   │   ├── multilingualRuntime.ts
│   │   ├── localLLMStack.ts
│   │   └── privacyDashboard.ts
│   ├── examples/
│   │   └── ui/
│   │       └── dashboard/
│   │           ├── DashboardWeb.tsx
│   │           └── DashboardMobile.tsx
│   └── index.ts
├── package.json
├── tsconfig.json
├── tsconfig.ui.json
└── README.md
```

## 🚀 Quick Start

### Prerequisites

- Node.js 16+ (recommended: Node.js 18 or later)
- npm 8+ or yarn 1.22+

### Installation

```bash
# Clone the repository
git clone https://github.com/mkostersitz/ethervoxai.git
cd ethervoxai

# Install dependencies
npm install

# Build the project
npm run build

# Run the demo
npm run demo
```

### Optional UI Dependencies

The core EthervoxAI modules work without any UI dependencies. To use the example dashboard components, install React:

```bash
# For web dashboard
npm install react@^18.2.0 react-dom@^18.2.0
npm install --save-dev @types/react @types/react-dom

# For mobile dashboard (React Native)
npm install react@18.2.0 react-native@^0.72.0
npm install --save-dev @types/react-native
```

### Basic Usage

```typescript
import { EthervoxAI } from './src/index';

// Create and configure EthervoxAI instance
const ai = new EthervoxAI({
  defaultLanguage: 'en-US',
  preferredModel: 'mistral-lite',
  privacyMode: 'balanced',
  enableCloudFallback: false
});

// Initialize the system
await ai.initialize();

// Process text input
const result = await ai.processTextInput('What is the weather like today?');
console.log(`Response: ${result.response}`);
console.log(`Confidence: ${Math.round(result.confidence * 100)}%`);
console.log(`Source: ${result.source}`);
```

### Dashboard Usage (Optional)

The dashboard components are provided as examples in `src/examples/ui/` and require React dependencies:

```typescript
// Import core modules (always available)
import { multilingualRuntime, localLLMStack, privacyDashboard } from 'ethervoxai';

// Import dashboard components (requires React installation)
// Uncomment after installing React dependencies:
// import { DashboardWeb } from './src/examples/ui/dashboard/DashboardWeb';
// import { DashboardMobile } from './src/examples/ui/dashboard/DashboardMobile';

// Use core modules directly
const languages = multilingualRuntime.getLanguageProfiles();
const models = localLLMStack.getLocalModels(); 
const privacy = privacyDashboard.getPrivacySettings();
```

## 🔧 Configuration

### Privacy Modes

- **Strict**: All processing happens locally, user consent required for any cloud access
- **Balanced**: Local processing with optional cloud fallback for complex queries
- **Permissive**: Allows cloud processing for enhanced capabilities

### Supported Languages (MVP)

- English (US/UK)
- Spanish (Latin America)
- Mandarin (Simplified Chinese)

### Available Models

- **mistral-lite**: General conversation and Q&A
- **tinyllama**: Lightweight model for simple queries

## 📚 Module Documentation

Each module has detailed documentation in the `docs/modules/` directory:

- [Multilingual Runtime](docs/modules/multilingual-runtime.md) - Language detection and speech processing
- [Local LLM Stack](docs/modules/local-llm-stack.md) - AI model management and routing
- [Privacy Dashboard](docs/modules/privacy-dashboard.md) - Privacy controls and audit logging

## 🛠️ Development

### Scripts

- `npm run build` - Build the TypeScript project
- `npm run dev` - Watch mode for development
- `npm run start` - Run the compiled application
- `npm run lint` - Lint the codebase
- `npm run test` - Run tests
- `npm run clean` - Clean build artifacts

### Testing

```bash
# Run all tests
npm test

# Run tests with coverage
npm test -- --coverage
```

## 🔐 Privacy & Security

EthervoxAI is designed with privacy as a core principle:

- **Local Processing**: All voice processing happens on-device by default
- **Encrypted Communication**: Any cloud communications are end-to-end encrypted
- **Data Control**: Users have complete control over data retention and deletion
- **Audit Logging**: Comprehensive logging of all system interactions
- **Consent Management**: Granular consent controls for cloud services

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🎯 Roadmap

- [ ] Expand language support to 15+ languages
- [ ] Implement RAG (Retrieval-Augmented Generation) capabilities
- [ ] Add voice cloning and customization
- [ ] Develop plugin ecosystem for third-party integrations
- [ ] Mobile app development
- [ ] Hardware device integration

## 📞 Support

For questions, issues, or contributions, please:

1. Check the [documentation](docs/)
2. Search existing [issues](https://github.com/mkostersitz/ethervoxai/issues)
3. Create a new issue if needed

---

Built with ❤️ for privacy-conscious users who want powerful AI without compromising their data.

*Project licensed under [CC BY-NC-SA 4.0](LICENSE). Commercial use is not permitted without prior approval.*
...
