# UI Components

This directory contains example UI implementations for the EthervoxAI dashboard. These are provided as reference implementations and can be adapted for your specific frontend framework.

## Dashboard Components

### Web Dashboard (React)
- **File**: `DashboardWeb.tsx`
- **Framework**: React 18+
- **Description**: Full-featured web dashboard with comprehensive controls

### Mobile Dashboard (React Native)
- **File**: `DashboardMobile.tsx` 
- **Framework**: React Native 0.72+
- **Description**: Mobile-optimized dashboard interface

## Installation

These UI components are optional and require additional dependencies:

### For Web Dashboard
```bash
npm install react@^18.2.0 react-dom@^18.2.0
npm install --save-dev @types/react@^18.2.0 @types/react-dom@^18.2.0
```

### For Mobile Dashboard
```bash
npm install react@18.2.0 react-native@^0.72.0
npm install --save-dev @types/react-native@^0.72.0
```

## Usage

```typescript
// Web Dashboard
import { DashboardWeb } from './ui/dashboard/DashboardWeb';

// Mobile Dashboard  
import { DashboardMobile } from './ui/dashboard/DashboardMobile';
```

## Customization

These components are designed to be easily customizable:

1. **Styling**: Modify the embedded styles or extract to CSS files
2. **Layout**: Adjust the component structure for your needs
3. **Functionality**: Add or remove features as required
4. **Framework**: Port to other frameworks like Vue, Angular, or Svelte

## Alternative Implementations

You can implement the dashboard in any frontend framework by using the core EthervoxAI modules:

```typescript
import { multilingualRuntime, localLLMStack, privacyDashboard } from '../modules';

// Use these modules to build your custom UI
const status = privacyDashboard.getPrivacySettings();
const models = localLLMStack.getLocalModels();
const languages = multilingualRuntime.getLanguageProfiles();
```
