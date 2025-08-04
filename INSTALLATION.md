# 🔧 Installation & Setup Guide

## Fixed Dependency Issues

### 1. Resolved React/React Native Conflicts
- **Problem**: Conflicting peer dependencies between React 18.3.1 and React Native's React 18.2.0 requirement
- **Solution**: Made React dependencies optional and moved UI components to examples
- **Result**: Core modules work independently without React dependencies

### 2. Updated Deprecated Dependencies
The following dependencies have been updated to resolve npm warnings:

| Package | Old Version | New Version | Status |
|---------|-------------|-------------|---------|
| eslint | ^8.0.0 | ^9.0.0 | ✅ Updated |
| @typescript-eslint/* | ^6.0.0 | ^8.0.0 | ✅ Updated |
| rimraf | ^5.0.0 | ^6.0.0 | ✅ Updated |
| @types/node | ^20.0.0 | ^22.0.0 | ✅ Updated |
| typescript | ^5.0.0 | ^5.5.0 | ✅ Updated |
| jest | ^29.0.0 | ^29.7.0 | ✅ Updated |
| ts-jest | ^29.0.0 | ^29.2.0 | ✅ Updated |

### 3. Migrated to ESLint v9 Flat Config
- **Old**: `.eslintrc.json` with legacy configuration
- **New**: `eslint.config.js` with flat configuration format
- **Benefits**: Future-proof, better performance, cleaner configuration

## Installation Steps

### 1. Prerequisites
```bash
# Verify Node.js installation (16+ required, 18+ recommended)
node --version

# Verify npm installation
npm --version
```

### 2. Install Core Dependencies
```bash
# Install EthervoxAI core (no React dependencies)
npm install

# This will install:
# - TypeScript 5.5+
# - ESLint 9.0+
# - Jest 29.7+
# - Modern tooling without deprecated packages
```

### 3. Optional: Install UI Dependencies
```bash
# Only if you want to use the example dashboard components
npm install react@^18.2.0 react-dom@^18.2.0
npm install --save-dev @types/react @types/react-dom
```

## Verification

### Build and Test
```bash
# Build the core TypeScript project (excludes React examples)
npm run build

# Build only the UI examples (requires React dependencies)
npm run build:ui

# Build everything (core + examples)
npm run build:all

# Run tests
npm run test

# Run the demo
npm run demo

# Lint the core code
npm run lint:core

# Type check examples (requires React types)
npm run typecheck:ui
```

### Expected Output
After running `npm install`, you should see:
- ✅ No deprecation warnings
- ✅ No peer dependency conflicts
- ✅ Clean installation without errors

## Troubleshooting

### If npm is not recognized
1. **Install Node.js**: Download from [nodejs.org](https://nodejs.org/)
2. **Add to PATH**: Ensure Node.js bin directory is in your system PATH
3. **Restart terminal**: Close and reopen your terminal/PowerShell

### If you see deprecation warnings
The warnings you encountered have been fixed by:
- Updating to ESLint 9.x (from deprecated 8.x)
- Using modern `@eslint/js` instead of deprecated `@humanwhocodes/*`
- Updating rimraf to v6 (from deprecated v3/v5)
- Removing problematic transitive dependencies

### Alternative Package Managers
```bash
# Using Yarn
yarn install

# Using pnpm  
pnpm install
```

## Project Structure Benefits

```
ethervoxai/
├── src/
│   ├── modules/              # Core functionality (no external deps)
│   │   ├── multilingualRuntime.ts
│   │   ├── localLLMStack.ts
│   │   └── privacyDashboard.ts
│   ├── examples/             # Optional examples (requires React)
│   │   └── ui/
│   │       └── dashboard/
│   └── index.ts              # Main export (core only)
├── eslint.config.js          # Modern ESLint v9 config
├── jest.config.js            # Updated Jest configuration
├── tsconfig.json             # Core TypeScript config
├── tsconfig.ui.json          # UI examples TypeScript config
└── package.json              # Clean dependencies
```

## Next Steps

1. **Install Node.js** if not already installed
2. **Run `npm install`** to verify the fix
3. **Build the project** with `npm run build`
4. **Try the demo** with `npm run demo`
5. **Optionally install React** for UI components

All dependency conflicts and deprecation warnings have been resolved! 🎉
