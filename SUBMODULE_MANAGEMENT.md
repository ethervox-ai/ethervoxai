# Submodule Management Guide

## Overview

This project uses git submodules for external dependencies (llama.cpp and whisper.cpp). **Do not commit changes to these submodules directly.**

## Current Submodules

### ethervoxai repository:
- **external/llama.cpp** - LLM inference library
- **external/whisper.cpp** - Speech-to-text library

### granite-finetuning root:
- **ethervoxai** - Main submodule

## Important Rules

⚠️ **NEVER commit changes to llama.cpp or whisper.cpp repositories**

1. Changes within submodules should be contributed upstream to the original repositories
2. Only update the submodule reference (commit hash) when intentionally upgrading
3. The `.gitignore` is configured to ignore submodule contents (not their references)

## Common Operations

### Initialize submodules after cloning
```bash
git submodule update --init --recursive
```

### Update to latest commits
```bash
# Update all submodules to their latest upstream commits
git submodule update --remote --recursive

# After updating, you'll need to commit the new submodule references
git add external/llama.cpp external/whisper.cpp
git commit -m "Update llama.cpp and whisper.cpp to latest versions"
```

### Reset submodules to tracked commits
```bash
# If you accidentally made changes in a submodule
git submodule update --recursive

# Or for a specific submodule
git submodule update external/llama.cpp
```

### Discard local changes in submodules
```bash
# Reset all submodules
git submodule foreach --recursive git reset --hard HEAD
git submodule update --recursive
```

### Check submodule status
```bash
# View current submodule commits
git submodule status

# Check for modifications
git status --short external/
```

## Updating Submodules

When you need to update llama.cpp or whisper.cpp to a newer version:

1. **Pull latest changes in the submodule:**
   ```bash
   cd external/llama.cpp
   git fetch origin
   git checkout master  # or specific branch/tag
   git pull
   cd ../..
   ```

2. **Commit the reference update in the parent repository:**
   ```bash
   git add external/llama.cpp
   git commit -m "Update llama.cpp to version X.Y.Z"
   ```

3. **Test thoroughly before pushing** to ensure the new version works with your code

## Troubleshooting

### "modified: external/llama.cpp (new commits)"

This means the submodule is at a different commit than what the parent repository expects. Reset it:
```bash
git submodule update external/llama.cpp
```

### "modified: external/llama.cpp (modified content)"

This means there are uncommitted changes inside the submodule. To discard:
```bash
cd external/llama.cpp
git reset --hard HEAD
```

### Submodule shows as modified after `.gitignore` changes

The `.gitignore` rules were previously incorrect. After fixing them:
```bash
# Clear the git cache
git rm -r --cached external/llama.cpp external/whisper.cpp
git submodule update --init
```

## Configuration Files

- **`.gitmodules`** - Defines submodule URLs and paths
- **`.gitignore`** - Configured to ignore submodule contents (only track references)
- **`external/llama.cpp/.git`** - Git file pointing to actual repository location

## Best Practices

1. ✅ Always use `git submodule update` commands to manage submodules
2. ✅ Test after updating submodule versions
3. ✅ Document which versions are known to work
4. ❌ Never edit code directly in external/llama.cpp or external/whisper.cpp
5. ❌ Never `git add` files from within the submodule directories
6. ❌ Never commit with `-a` flag if you have modified submodules
