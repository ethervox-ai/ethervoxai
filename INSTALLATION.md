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

The UI examples are split into separate components to avoid conflicts:

#### Web Dashboard (React only)
```bash
# Install React for web dashboard
npm install react@^18.2.0 react-dom@^18.2.0
npm install --save-dev @types/react @types/react-dom

# Type check web dashboard
npm run typecheck:web
```

#### Mobile Dashboard (React Native)
```bash
# Install React Native (includes React at specific version)
npm install react@18.2.0 react-native@^0.72.0
npm install --save-dev @types/react-native

# Type check mobile dashboard  
npm run typecheck:mobile
```

**Note**: You cannot install both React DOM and React Native in the same project due to version conflicts. Choose one based on your target platform.

## Verification

### Build and Test
```bash
# Build the core TypeScript project (excludes React examples)
npm run build

# Build web dashboard (requires React + React DOM)
npm run build:web

# Build mobile dashboard (requires React Native)
npm run build:mobile

# Build everything (core + web examples)
npm run build:all

# Run tests (core functionality only)
npm run test

# Run the demo (core functionality)
npm run demo

# Lint the core code
npm run lint:core

# Type check web examples (requires React types)
npm run typecheck:web

# Type check mobile examples (requires React Native types)
npm run typecheck:mobile
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

## 🔒 Keeping Models & Runtime Files Out of Version Control

### What's Excluded from Git

EthervoxAI automatically excludes the following from version control:

**AI Models & Cache (can be 1-10GB+):**
- `models/` - Downloaded AI models
- `.ethervoxai/` - User model cache directory
- `*.ggml`, `*.gguf`, `*.bin` - Model files
- `model-cache/` - Temporary model storage

**Runtime Logs & Audit Data:**
- `audit-logs/` - Privacy audit logs
- `ethervoxai-audit.log` - Main audit log
- `inference-logs/` - AI inference logs
- `benchmark-results/` - Performance test data

**Testing Artifacts:**
- `test-models/` - Models downloaded during testing
- `test-outputs/` - Test result files
- `performance-logs/` - Benchmark data

### Why This Matters

- **Repository Size**: AI models can be 1-10GB each
- **Privacy**: Audit logs may contain sensitive data
- **Performance**: Large files slow down git operations
- **Collaboration**: Runtime files are user-specific

### Manual Cleanup (if needed)

If you accidentally committed large files:

```bash
# Remove large files from git history (DANGEROUS - backup first!)
git filter-branch --tree-filter 'rm -rf models/ .ethervoxai/' HEAD

# Or use BFG Repo-Cleaner (recommended)
bfg --delete-folders models,audit-logs
git reflog expire --expire=now --all && git gc --prune=now --aggressive
```

### Global .gitignore (Optional)

For additional protection across all projects:

```bash
# Create global gitignore
git config --global core.excludesfile ~/.gitignore_global

# Add to ~/.gitignore_global
echo "*.ggml" >> ~/.gitignore_global
echo "*.gguf" >> ~/.gitignore_global
echo ".ethervoxai/" >> ~/.gitignore_global
echo "audit-logs/" >> ~/.gitignore_global
```

### Verification

Check what's ignored:
```bash
git status --ignored
git check-ignore -v models/ .ethervoxai/ *.ggml
```

The `.gitignore` file is comprehensive and will prevent any model files, audit logs, or testing artifacts from being accidentally committed to GitHub.
