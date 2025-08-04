# UI Examples

This directory contains example UI implementations for the EthervoxAI dashboard. These are provided as reference implementations and are **not compiled by default** to avoid requiring React dependencies for the core library.

## Dashboard Components

### Web Dashboard (React)
- **File**: `dashboard/DashboardWeb.tsx`
- **Framework**: React 18+
- **Description**: Full-featured web dashboard with comprehensive controls

### Mobile Dashboard (React Native)
- **File**: `dashboard/DashboardMobile.tsx` 
- **Framework**: React Native 0.72+
- **Description**: Mobile-optimized dashboard interface

## Building Examples

The examples are excluded from the default build process. To compile them:

### 1. Install React Dependencies
```bash
# For Web Dashboard
npm install react@^18.2.0 react-dom@^18.2.0
npm install --save-dev @types/react@^18.2.0 @types/react-dom@^18.2.0

# For Mobile Dashboard (additional)
npm install react-native@^0.72.0
npm install --save-dev @types/react-native@^0.72.0
```

### 2. Build Examples
```bash
# Type check examples
npm run typecheck:ui

# Build examples (after installing React)
npm run build:ui

# Build everything (core + examples)
npm run build:all
```

## Usage

```typescript
// Import the core modules (always available)
import { multilingualRuntime, localLLMStack, privacyDashboard } from '../../../modules';

// Use in your React components
import { DashboardWeb } from './dashboard/DashboardWeb';
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
