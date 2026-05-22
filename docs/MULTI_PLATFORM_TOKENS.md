# Multi-Platform Token Configuration

## Overview

EthervoxAI now supports **separate GitHub tokens** for Android and Desktop platforms, allowing you to:

✅ Use different tokens for different repositories  
✅ Track usage separately per platform  
✅ Revoke one platform without affecting the other  
✅ Apply different rate limits/permissions per platform  

## Token Priority (Fallback Chain)

When bug reporter looks for a token, it checks in this order:

```
1. Platform-specific token
   ├─ Android:  ETHERVOX_GITHUB_TOKEN_ANDROID
   └─ Desktop:  ETHERVOX_GITHUB_TOKEN_DESKTOP
   
2. Generic fallback
   └─ ETHERVOX_GITHUB_TOKEN
```

## Local Development Setup

### Option 1: Platform-Specific (Recommended)

```bash
# Add to ~/.zshrc or ~/.bashrc
export ETHERVOX_GITHUB_TOKEN_ANDROID="github_pat_YOUR_ANDROID_TOKEN"
export ETHERVOX_GITHUB_TOKEN_DESKTOP="github_pat_YOUR_DESKTOP_TOKEN"
```

### Option 2: Generic Fallback

```bash
# Single token for both platforms
export ETHERVOX_GITHUB_TOKEN="github_pat_YOUR_TOKEN"
```

### Option 3: Mixed Approach

```bash
# Specific token for Android, generic for Desktop
export ETHERVOX_GITHUB_TOKEN_ANDROID="github_pat_ANDROID_TOKEN"
export ETHERVOX_GITHUB_TOKEN="github_pat_GENERIC_TOKEN"
```

## GitHub Actions Setup

### Android Repository

Set these secrets in your Android repo settings:

```
ETHERVOX_GITHUB_TOKEN_ANDROID  →  Token scoped to Android repo
ETHERVOX_GITHUB_TOKEN          →  (Optional) Generic fallback
```

### Desktop Repository

Set these secrets in your Desktop repo settings:

```
ETHERVOX_GITHUB_TOKEN_DESKTOP  →  Token scoped to Desktop repo
ETHERVOX_GITHUB_TOKEN          →  (Optional) Generic fallback
```

### Workflow Configuration

The workflow automatically sets both:

```yaml
env:
  ETHERVOX_GITHUB_TOKEN_DESKTOP: ${{ secrets.ETHERVOX_GITHUB_TOKEN_DESKTOP }}
  ETHERVOX_GITHUB_TOKEN: ${{ secrets.ETHERVOX_GITHUB_TOKEN }}
```

## Use Cases

### Separate Repositories

**Scenario:** Android app reports to `owner/ethervoxai-android`, Desktop reports to `owner/ethervoxai`

```bash
# Token 1: Scoped to owner/ethervoxai-android
export ETHERVOX_GITHUB_TOKEN_ANDROID="github_pat_TOKEN1"

# Token 2: Scoped to owner/ethervoxai
export ETHERVOX_GITHUB_TOKEN_DESKTOP="github_pat_TOKEN2"
```

### Single Repository

**Scenario:** Both platforms report to same repository

```bash
# One token with access to both
export ETHERVOX_GITHUB_TOKEN="github_pat_SINGLE_TOKEN"
```

### Development + Production

**Scenario:** Different tokens for dev vs prod environments

```bash
# Development
export ETHERVOX_GITHUB_TOKEN="github_pat_DEV_TOKEN"

# Production (CI/CD)
# Set ETHERVOX_GITHUB_TOKEN_ANDROID and _DESKTOP in GitHub Secrets
```

## Security Benefits

### Blast Radius Limitation

If one token is compromised:
- Only affects that platform
- Other platform continues working
- Easier to identify source of leak

### Audit Trail

Separate tokens provide:
- Per-platform usage metrics
- Better tracking of API calls
- Clearer security logs

### Permission Isolation

Different permissions per platform:
- Android token: Write to Android repo only
- Desktop token: Write to Desktop repo only
- No cross-contamination

## Migration Guide

### From Single Token

**Before:**
```bash
export ETHERVOX_GITHUB_TOKEN="github_pat_OLD_TOKEN"
```

**After (no changes needed):**
```bash
# Still works! Generic fallback is supported
export ETHERVOX_GITHUB_TOKEN="github_pat_OLD_TOKEN"
```

**Or migrate to platform-specific:**
```bash
# Create two new tokens, then:
export ETHERVOX_GITHUB_TOKEN_ANDROID="github_pat_NEW_ANDROID"
export ETHERVOX_GITHUB_TOKEN_DESKTOP="github_pat_NEW_DESKTOP"

# Revoke old token
```

## Quick Setup Script

Run the interactive setup:

```bash
./scripts/setup-env.sh
```

This will prompt you for:
1. Android token (optional)
2. Desktop token (optional)
3. Generic fallback token (optional)

And save them to `.env` file.

## Troubleshooting

### "Bug reporting not configured"

**Check priority order:**
```bash
# 1. Platform-specific set?
echo $ETHERVOX_GITHUB_TOKEN_DESKTOP  # Should output token

# 2. Generic fallback set?
echo $ETHERVOX_GITHUB_TOKEN  # Should output token

# 3. Both empty = error!
```

### Token not recognized in CI

**For Android repo**, ensure secret is named:
- `ETHERVOX_GITHUB_TOKEN_ANDROID` ✅
- NOT `ETHERVOX_ANDROID_TOKEN` ❌

**For Desktop repo**, ensure secret is named:
- `ETHERVOX_GITHUB_TOKEN_DESKTOP` ✅
- NOT `ETHERVOX_DESKTOP_TOKEN` ❌

## Files Modified

- `src/common/bug_reporter.c` - Token priority logic
- `.env.example` - Template with all token options
- `.github/workflows/build-and-test.yml` - Desktop token in CI
- `scripts/setup-env.sh` - Interactive multi-token setup
- `docs/BUG_REPORTER_SECURITY.md` - Updated documentation
- `docs/GITHUB_ACTIONS_SECRETS.md` - CI/CD configuration guide

## See Also

- [Bug Reporter Security](BUG_REPORTER_SECURITY.md) - Local token setup
- [GitHub Actions Secrets](GITHUB_ACTIONS_SECRETS.md) - CI/CD configuration
- [Security Incident](SECURITY_INCIDENT.md) - Recent security fixes
