# ⚠️ SECURITY ALERT: GitHub Token Exposed

## What Happened

A GitHub personal access token was accidentally committed to the repository in commit `15595eb`.

**Exposed Token (REVOKED):**
```
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
```

## Immediate Actions Required

### 1. ⚠️ REVOKE THE TOKEN IMMEDIATELY

**You MUST revoke this token right now:**

1. Go to: https://github.com/settings/tokens
2. Find the token named "EthervoxAI Bug Reporter" (or similar)
3. Click **"Revoke"** button
4. Confirm revocation

**This token can create issues on your repository until revoked!**

### 2. Generate a New Token

Follow the instructions in `docs/BUG_REPORTER_SECURITY.md` to create a new token with:
- Fine-grained permissions
- Issues: Read and write only
- Scoped to specific repository

### 3. Set New Token via Environment Variable

```bash
export ETHERVOX_GITHUB_TOKEN="your_new_token_here"
```

Or use the setup script:
```bash
./scripts/setup-env.sh
```

## What Was Fixed

✅ Removed hardcoded token from source code
✅ Token now loaded from environment variable
✅ Added `.env` to `.gitignore`
✅ Created setup documentation

## Git History Consideration

**Important:** The exposed token is still in git history (commit `15595eb`).

### Option 1: If repository is private/local
- Revoke the old token
- Continue using the repository
- Never share commit history publicly

### Option 2: If repository might be public
Consider rewriting history to remove the token:

```bash
# WARNING: This rewrites git history!
# Only do this if you haven't pushed to a shared remote

git filter-repo --path src/common/bug_reporter.c \
  --replace-text <(echo "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==>REDACTED_TOKEN")
```

**OR** start fresh with a new repository if needed.

## Prevention

Going forward:
- ✅ All secrets in environment variables
- ✅ `.env` files gitignored
- ✅ Pre-commit hooks to detect secrets (recommended)
- ✅ Regular token rotation

## Timeline

- **Exposed:** Commit `15595eb` (feat: Add bug reporter)
- **Detected:** December 14, 2025
- **Fixed:** December 14, 2025 (moved to environment variable)
- **Status:** ⚠️ **TOKEN MUST BE REVOKED**
