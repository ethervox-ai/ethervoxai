# Bug Reporter Security Configuration

## Overview

The bug reporter uses a GitHub personal access token to anonymously submit issues. This token **must never be committed to the repository**.

**Build-Time Token Validation**: As of December 2025, the CMake build system now **requires** a valid GitHub token and will fail the build if:
- The token is not set
- The token is invalid or expired
- The token lacks required permissions
- The token cannot reach the GitHub API

This ensures that only properly configured builds with valid tokens can be deployed.

## Setup Instructions

### 1. Create GitHub Token

1. Go to https://github.com/settings/tokens?type=beta
2. Click "Generate new token" → "Fine-grained token"
3. Configure token:
   - **Name**: EthervoxAI Bug Reporter
   - **Repository access**: Only select repositories → `ethervox-ai/ethervoxai-android`
   - **Permissions**:
     - Repository permissions → Issues → **Read and write**
   - **Expiration**: Set to desired duration (90 days recommended)
4. Click "Generate token" and **copy the token immediately**

### 2. Set Environment Variable

**For development/testing:**

```bash
# Add to your ~/.zshrc or ~/.bashrc

# For Android app development
export ETHERVOX_GITHUB_TOKEN_ANDROID="github_pat_YOUR_ANDROID_TOKEN"

# For desktop CLI development (macOS/Linux/Windows)
export ETHERVOX_GITHUB_TOKEN_DESKTOP="github_pat_YOUR_DESKTOP_TOKEN"

# Generic fallback (optional)
export ETHERVOX_GITHUB_TOKEN="github_pat_YOUR_GENERIC_TOKEN"

# Reload shell or run:
source ~/.zshrc
```

**Token Priority:**
1. Platform-specific token (`_ANDROID` or `_DESKTOP`)
2. Generic fallback token (`ETHERVOX_GITHUB_TOKEN`)

This allows you to:
- Use different tokens for different repositories
- Track usage separately per platform
- Revoke one platform without affecting the other

**For production/CI:**

Set the environment variable in your deployment environment:
- Docker: Use `--env` flag or environment files
- Systemd: Use `Environment=` in service file
- GitHub Actions: Use repository secrets

### 3. Verify Setup

```bash
# Check if token is set
echo $ETHERVOX_GITHUB_TOKEN_DESKTOP

# Should output your token (not empty)
```

**Build-time validation:**

The token is automatically validated when you run CMake configuration:

```bash
cmake -B build
```

If the token is invalid, you'll see an error like:

```
====================================================================
ERROR: GitHub token validation FAILED (HTTP 401 Unauthorized)
====================================================================

The provided token is invalid or has expired.
...
```

If validation succeeds, you'll see:

```
-- GitHub token: Using ETHERVOX_GITHUB_TOKEN_DESKTOP
-- Validating GitHub token...
-- GitHub token validation: SUCCESS (HTTP 200)
-- GitHub token has access to timkos98/ethervoxai repository
```

## Security Best Practices

✅ **DO:**
- Use fine-grained tokens with minimal permissions
- Set token expiration dates
- Rotate tokens periodically
- Use environment variables or secret managers
- Add `.env` to `.gitignore`

❌ **DON'T:**
- Commit tokens to git
- Share tokens in plain text
- Use tokens with excessive permissions
- Hardcode tokens in source files

## Fallback Behavior

If `ETHERVOX_GITHUB_TOKEN` is not set:
- Bug reporting will be disabled
- User will see: "Bug reporting not configured"
- Application will continue to function normally

## Revoking a Token

If a token is accidentally exposed:

1. Go to https://github.com/settings/tokens
2. Find the compromised token
3. Click "Revoke" immediately
4. Generate a new token following steps above
5. Update environment variable

## Token Format

Tokens should start with `github_pat_` and be ~90 characters long.

Example format (fake):
```
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
```
