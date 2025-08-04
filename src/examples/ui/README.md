# EthervoxAI UI Examples

This directory contains example UI implementations that demonstrate how to integrate EthervoxAI core modules with popular frontend frameworks. These are provided as reference implementations and are **not compiled by default** to avoid requiring React dependencies for the core library.

## Available Examples

### Web Dashboard (`DashboardWeb.tsx`)
- **Framework**: React + React DOM
- **Purpose**: Full-featured web browser interface for EthervoxAI
- **Features**: Desktop-optimized layout with comprehensive controls and embedded CSS
- **Dependencies**: Requires React 18.2+ and React DOM

### Web Dashboard Simple (`DashboardWebSimple.tsx`)
- **Framework**: React + React DOM  
- **Purpose**: Simplified web interface using inline styles
- **Features**: Basic dashboard functionality with React inline styles (no external CSS)
- **Dependencies**: Requires React 18.2+ and React DOM

### Mobile Dashboard (`DashboardMobile.tsx`) 
- **Framework**: React Native
- **Purpose**: Mobile app interface for EthervoxAI
- **Features**: Touch-optimized interface for iOS/Android
- **Dependencies**: Requires React Native 0.72+ (includes React 18.2.0)

## Setup Instructions

### Important: React vs React Native Conflict
You cannot install both React DOM and React Native in the same project due to version conflicts. Choose one approach:

### Option 1: Web Dashboard Setup
```bash
# Install React DOM dependencies
npm install react@^18.2.0 react-dom@^18.2.0
npm install --save-dev @types/react @types/react-dom

# Build and type-check web dashboard
npm run build:web
npm run typecheck:web
```

### Option 2: Mobile Dashboard Setup  
```bash
# Install React Native dependencies
npm install react@18.2.0 react-native@^0.72.0
npm install --save-dev @types/react-native

# Build and type-check mobile dashboard
npm run build:mobile
npm run typecheck:mobile
```

### Switching Between Web and Mobile
To switch from one to the other:
```bash
# Remove existing React dependencies
npm uninstall react react-dom react-native @types/react @types/react-dom @types/react-native

# Install the dependencies for your target platform (see above)
```

## Usage

```typescript
// Import the core modules (always available)
import { multilingualRuntime, localLLMStack, privacyDashboard } from '../../../modules';

// Use in your React components
import { DashboardWeb } from './dashboard/DashboardWeb';
import { DashboardWebSimple } from './dashboard/DashboardWebSimple';
import { DashboardMobile } from './dashboard/DashboardMobile';
```

## Customization

These components are designed to be easily customizable:

1. **Copy to your project**: Copy the components to your own React project
2. **Modify styling**: Update the embedded styles or extract to CSS files
3. **Adapt layout**: Adjust the component structure for your needs
4. **Add features**: Extend functionality as required
5. **Port frameworks**: Adapt to Vue, Angular, Svelte, etc.

## Alternative Implementations

You can implement the dashboard in any frontend framework by using the core EthervoxAI modules:

```typescript
import { multilingualRuntime, localLLMStack, privacyDashboard } from '../../modules';

// Use these modules to build your custom UI
const status = privacyDashboard.getPrivacySettings();
const models = localLLMStack.getLocalModels();
const languages = multilingualRuntime.getLanguageProfiles();
```

## Why Examples?

The UI components are in `examples/` because:
- **Optional Dependencies**: Core EthervoxAI works without React
- **Framework Agnostic**: You can use any frontend framework
- **Reference Implementation**: Shows how to integrate with the core modules
- **No Build Conflicts**: Avoids dependency issues when React isn't needed
