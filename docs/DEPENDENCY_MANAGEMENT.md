# Dependency Management in EthervoxAI

EthervoxAI uses a hybrid dependency management system that combines the best of git submodules and CMake FetchContent to provide flexibility for different workflows.

## Overview

The project depends on two major external libraries:
- **llama.cpp** - Local LLM inference engine for the Governor
- **whisper.cpp** - Speech-to-text (STT) processing

These are managed automatically to provide the best experience for different user types:
- **Beginners/CI**: Automatic download during first `cmake` configure
- **Advanced developers**: Git submodules for offline work and version control
- **Contributors**: Flexible paths for testing local modifications

## How It Works

### 1. Directory Structure

```
ethervoxai/
├── build/              # CMake build artifacts (can be deleted anytime)
├── external/           # External dependencies (persistent)
│   ├── llama.cpp/      # Downloaded once, reused forever
│   └── whisper.cpp/    # Downloaded once, reused forever
├── .gitmodules         # Git submodule definitions
└── cmake/
    └── FetchDependencies.cmake  # Dependency fetching logic
```

### 2. Acquisition Strategy (Priority Order)

When you run `cmake ..`, for each dependency it:

1. **Checks for custom path**: `-DLLAMA_CPP_CUSTOM_DIR=/path/to/custom/llama.cpp`
2. **Checks for submodule**: Is `external/llama.cpp/CMakeLists.txt` present?
3. **Auto-downloads** (if enabled): Uses CMake FetchContent to clone from GitHub
4. **Fails gracefully**: If disabled, warns and continues without the feature

### 3. Build Time Behavior

| Scenario | Behavior | Time |
|----------|----------|------|
| **First-time build** | Downloads to `external/` | ~2-5 min (one-time) |
| **Clean build** (`rm -rf build/`) | Reuses `external/` | ~10-30 sec |
| **Incremental build** | No CMake run, just compile | ~5-30 sec |
| **Reconfigure** | Checks `external/` exists, returns instantly | <1 sec |

The key: `external/` persists outside `build/`, so even complete rebuilds are fast.

## Usage Methods

### Method 1: Automatic (Default - Recommended for Most Users)

```bash
git clone https://github.com/ethervox-ai/ethervoxai.git
cd ethervoxai
cmake -B build
cmake --build build
```

**What happens:**
- First `cmake -B build`: Downloads llama.cpp and whisper.cpp to `external/` (~2-5 min)
- All subsequent builds: Instant, no downloads

**Advantages:**
- Zero manual steps
- Works on fresh clones and CI
- Dependencies persist across clean builds

### Method 2: Git Submodules (Advanced Users)

```bash
git clone https://github.com/ethervox-ai/ethervoxai.git
cd ethervoxai
git submodule update --init --recursive
cmake -B build
cmake --build build
```

**What happens:**
- `git submodule update`: Clones dependencies to `external/` (~2-5 min)
- `cmake -B build`: Detects existing dependencies, skips download (~10-30 sec)

**Advantages:**
- Offline development after initial clone
- Explicit version control (pinned commits in .gitmodules)
- Easier to contribute changes to llama.cpp/whisper.cpp

### Method 3: Custom Paths (Testing/Development)

```bash
# Clone dependencies to custom locations
git clone https://github.com/ggerganov/llama.cpp.git ~/dev/llama.cpp
git clone https://github.com/ggerganov/whisper.cpp.git ~/dev/whisper.cpp

# Build your changes
cd ~/dev/llama.cpp && make && cd -

# Use custom paths
cmake -B build \
  -DLLAMA_CPP_CUSTOM_DIR=~/dev/llama.cpp \
  -DWHISPER_CPP_CUSTOM_DIR=~/dev/whisper.cpp
cmake --build build
```

**Advantages:**
- Test local modifications to dependencies
- Use pre-installed versions
- Multiple projects can share one dependency clone

## Configuration Options

### CMake Options

```bash
# Disable automatic downloads (requires manual setup)
cmake -B build -DETHERVOX_AUTO_FETCH_DEPS=OFF

# Use full clones instead of shallow (for development on dependencies)
cmake -B build -DETHERVOX_FETCH_SHALLOW=OFF

# Disable llama.cpp integration
cmake -B build -DWITH_LLAMA=OFF

# Disable whisper.cpp integration
cmake -B build -DWITH_WHISPER=OFF

# Use custom dependency locations
cmake -B build \
  -DLLAMA_CPP_CUSTOM_DIR=/path/to/llama.cpp \
  -DWHISPER_CPP_CUSTOM_DIR=/path/to/whisper.cpp
```

### Environment Variables

None required - all configuration is via CMake options.

## Troubleshooting

### "llama.cpp not found" Warning

**Problem**: CMake can't find llama.cpp

**Solutions:**
```bash
# Option 1: Enable auto-fetch (default)
cmake -B build -DETHERVOX_AUTO_FETCH_DEPS=ON

# Option 2: Manual submodule init
git submodule update --init external/llama.cpp

# Option 3: Provide custom path
cmake -B build -DLLAMA_CPP_CUSTOM_DIR=/path/to/llama.cpp
```

### Network Issues During First Build

**Problem**: CMake configure hangs or fails downloading

**Solutions:**
```bash
# Use submodules instead (can retry git clone on failure)
git submodule update --init --recursive

# Or download manually
git clone https://github.com/ggerganov/llama.cpp.git external/llama.cpp
git clone https://github.com/ggerganov/whisper.cpp.git external/whisper.cpp
```

### Dependency Version Mismatch

**Problem**: Need specific version of llama.cpp

**Solutions:**
```bash
# Method 1: Update .gitmodules commit hash (recommended)
cd external/llama.cpp
git checkout <desired-commit-or-tag>
cd ../..
git add external/llama.cpp
git commit -m "Update llama.cpp to version X"

# Method 2: Use custom directory with specific version
git clone https://github.com/ggerganov/llama.cpp.git ~/llama-v1.2.3
cd ~/llama-v1.2.3 && git checkout v1.2.3
cmake -B build -DLLAMA_CPP_CUSTOM_DIR=~/llama-v1.2.3
```

### Clean Dependency Cache

**Problem**: Want to re-download dependencies

**Solution:**
```bash
# Remove cached dependencies
rm -rf external/

# Next cmake will re-download
cmake -B build
```

## For Contributors

### Modifying External Dependencies

If you need to modify llama.cpp or whisper.cpp:

```bash
# Use submodules for version control
git submodule update --init external/llama.cpp
cd external/llama.cpp

# Make changes, commit to your fork
git checkout -b my-feature
# ... make changes ...
git commit -m "Add feature X"
git push origin my-feature

# Test with EthervoxAI
cd ../..
cmake -B build
cmake --build build
```

### Updating Dependency Versions

To update the pinned version:

```bash
cd external/llama.cpp
git fetch origin
git checkout <new-tag-or-commit>
cd ../..

# Update submodule reference
git add external/llama.cpp
git commit -m "chore: Update llama.cpp to vX.Y.Z"
```

### CI/CD Integration

The system works seamlessly with CI:

```yaml
# .github/workflows/build.yml
steps:
  - uses: actions/checkout@v3
  
  - name: Configure
    run: cmake -B build  # Auto-downloads dependencies
    
  - name: Build
    run: cmake --build build -j 4
```

No need for `submodule: recursive` in checkout - dependencies download automatically.

## Performance Characteristics

### First Build (Cold Cache)

```
git clone                    ~30 seconds
cmake -B build (downloads)   ~2-5 minutes (network-dependent)
cmake --build build          ~5-15 minutes (CPU-dependent)
Total: ~7-20 minutes
```

### Clean Rebuild

```
rm -rf build/
cmake -B build              ~10-30 seconds (external/ cached)
cmake --build build         ~5-15 minutes
Total: ~5-15 minutes
```

### Incremental Development Build

```
# Edit src/main.c
cmake --build build         ~5-30 seconds (only changed files)
Total: ~5-30 seconds
```

### Subsequent Configures (No Changes)

```
cmake -B build              <1 second (FETCHCONTENT_UPDATES_DISCONNECTED=ON)
```

## Implementation Details

### FetchContent Configuration

From `cmake/FetchDependencies.cmake`:

```cmake
# Optimization: Skip git update checks after first download
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# Use shallow clones for faster downloads (50% faster)
GIT_SHALLOW TRUE

# Store in external/ (not build/_deps/)
SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp"
```

This ensures:
- ✅ Fast reconfigures (no network checks)
- ✅ Persistent storage (survives clean builds)
- ✅ Minimal bandwidth (shallow clones)

### Git Submodules Configuration

From `.gitmodules`:

```ini
[submodule "external/llama.cpp"]
    path = external/llama.cpp
    url = https://github.com/ggerganov/llama.cpp.git
    
[submodule "external/whisper.cpp"]
    path = external/whisper.cpp
    url = https://github.com/ggerganov/whisper.cpp.git
```

This allows:
- ✅ Version pinning (specific commits)
- ✅ Offline development
- ✅ Explicit git history

## Best Practices

### For End Users

- Use **automatic download** (default) for simplest setup
- Run `git submodule update --init` if working offline frequently
- Keep `external/` directory in place for fast rebuilds

### For Contributors

- Use **git submodules** to track dependency versions
- Test with `ETHERVOX_AUTO_FETCH_DEPS=OFF` to ensure submodules work
- Update dependency versions explicitly via submodule commits

### For Packagers

- Pre-populate `external/` in source tarballs
- Or disable auto-fetch: `-DETHERVOX_AUTO_FETCH_DEPS=OFF`
- Use system packages if available via `*_CUSTOM_DIR` options

## See Also

- [CMake FetchContent Documentation](https://cmake.org/cmake/help/latest/module/FetchContent.html)
- [Git Submodules Documentation](https://git-scm.com/book/en/v2/Git-Tools-Submodules)
- [llama.cpp Repository](https://github.com/ggerganov/llama.cpp)
- [whisper.cpp Repository](https://github.com/ggerganov/whisper.cpp)
