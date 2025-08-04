# Scripts Directory

This directory contains organized build, setup, demo, and testing scripts for EthervoxAI.

## Directory Structure

```
scripts/
├── build.bat              # Main build script
├── demo/                  # Demo and launch scripts
│   ├── launch-demo.js     # Demo launcher
│   ├── run-demo.bat       # Demo runner (batch)
│   ├── start-windows-demo.bat         # Windows desktop demo
│   ├── start-windows-demo-alt.bat     # Alternative Windows demo
│   └── test-demo.js       # Demo testing
├── setup/                 # Installation and setup scripts
│   ├── install-alternatives.bat      # Install audio alternatives
│   ├── install-audio-libraries.bat   # Install audio libraries
│   ├── install-remaining.bat         # Install remaining packages
│   ├── install-speaker-arm64.bat     # ARM64 speaker installation
│   └── setup-audio-advanced.bat      # Advanced audio setup
└── testing/               # Legacy testing scripts
    ├── check-audio.bat    # Audio system check
    ├── run-verify.bat     # Verification runner
    ├── test-alternatives.bat         # Test audio alternatives
    ├── test-audio-mgr.bat            # Test audio manager
    ├── test-audio-alternatives.js    # Audio alternatives test
    ├── test-audio-manager.js         # Audio manager test
    ├── test-build.js      # Build test
    ├── test-simple.js     # Simple test
    └── verify-audio.js    # Audio verification
```

## Quick Reference

### Building the Project
```bash
# From root directory
.\scripts\build.bat

# Or use npm script
npm run build
```

### Running Demos
```bash
# Main demo launcher
npm run demo

# Windows desktop demo
.\scripts\demo\start-windows-demo.bat

# Alternative Windows demo
.\scripts\demo\start-windows-demo-alt.bat
```

### Audio Testing
```bash
# New comprehensive audio test suite (recommended)
npm run test:audio

# Or run directly
.\tests\audio-input-output\run-audio-test.bat
```

### Setup and Installation
```bash
# Install audio alternatives (ARM64 compatible)
.\scripts\setup\install-alternatives.bat

# Advanced audio setup
.\scripts\setup\setup-audio-advanced.bat

# ARM64 specific speaker installation (legacy)
.\scripts\setup\install-speaker-arm64.bat
```

## Migration Notes

The following files have been moved from the root directory:

### Moved to `scripts/demo/`
- `launch-demo.js`
- `run-demo.bat`
- `start-windows-demo.bat`
- `start-windows-demo-alt.bat`
- `test-demo.js`

### Moved to `scripts/setup/`
- `install-alternatives.bat`
- `install-audio-libraries.bat`
- `install-remaining.bat`
- `install-speaker-arm64.bat`
- `setup-audio-advanced.bat`

### Moved to `scripts/testing/` (Legacy)
- `check-audio.bat`
- `run-verify.bat`
- `test-alternatives.bat`
- `test-audio-mgr.bat`
- `test-audio-alternatives.js`
- `test-audio-manager.js`
- `test-build.js`
- `test-simple.js`
- `verify-audio.js`

### Moved to `tests/audio-input-output/`
- `run-audio-test.bat`

## Recommendations

1. **For new audio testing**: Use the comprehensive test suite at `tests/audio-input-output/`
2. **For legacy testing**: Scripts in `scripts/testing/` are preserved for compatibility
3. **For demos**: Use npm scripts or the organized scripts in `scripts/demo/`
4. **For setup**: Use scripts in `scripts/setup/` for initial project setup

## NPM Scripts Integration

The following npm scripts have been updated to work with the new file locations:

- `npm run demo` - Runs the demo launcher from `scripts/demo/`
- `npm run test:audio` - Runs the comprehensive audio test suite
- `npm run build` - Uses the main build process (no change needed)

All other existing npm scripts continue to work as before.
