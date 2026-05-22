# GitHub Actions CI/CD Configuration

## Setting Up GitHub Secrets

The bug reporter requires a GitHub token to submit issues. This token must be stored as a **GitHub Secret**.

### Step 1: Create a GitHub Token

1. Go to https://github.com/settings/tokens?type=beta
2. Click **"Generate new token"** → **"Fine-grained token"**
3. Configure the token:
   - **Name**: `EthervoxAI CI Bug Reporter`
   - **Expiration**: 90 days (recommended)
   - **Repository access**: 
     - Select: **Only select repositories**
     - Choose: `ethervox-ai/ethervoxai-android` (or your repo)
   - **Permissions**:
     - Repository permissions → **Issues** → **Read and write**
4. Click **"Generate token"**
5. **Copy the token immediately** (you won't see it again)

### Step 2: Add Tokens to GitHub Secrets

You need to add **two separate tokens** for Android and desktop platforms:

#### For Android App Repository

1. Go to: `https://github.com/YOUR_USERNAME/ethervoxai-android`
2. Click **Settings** → **Secrets and variables** → **Actions**
3. Click **"New repository secret"**
4. Configure:
   - **Name**: `ETHERVOX_GITHUB_TOKEN_ANDROID`
   - **Value**: Paste the Android token
5. Click **"Add secret"**

#### For Desktop CLI Repository

1. Go to: `https://github.com/YOUR_USERNAME/ethervoxai`
2. Click **Settings** → **Secrets and variables** → **Actions**
3. Click **"New repository secret"**
4. Configure:
   - **Name**: `ETHERVOX_GITHUB_TOKEN_DESKTOP`
   - **Value**: Paste the desktop token
5. Click **"Add secret"**

#### Generic Fallback (Optional)

Add a generic token that works for both:
   - **Name**: `ETHERVOX_GITHUB_TOKEN`
   - **Value**: Paste a token with access to both repositories

**Recommendation:** Use separate tokens for better security and tracking.

### Step 3: Verify Configuration

The tokens are now available in GitHub Actions workflows via:

```yaml
env:
  # Platform-specific (recommended)
  ETHERVOX_GITHUB_TOKEN_ANDROID: ${{ secrets.ETHERVOX_GITHUB_TOKEN_ANDROID }}
  ETHERVOX_GITHUB_TOKEN_DESKTOP: ${{ secrets.ETHERVOX_GITHUB_TOKEN_DESKTOP }}
  
  # Generic fallback
  ETHERVOX_GITHUB_TOKEN: ${{ secrets.ETHERVOX_GITHUB_TOKEN }}
```

## Current Workflow Configuration

The token is configured in:
- `.github/workflows/build-and-test.yml` (test coverage job)

### Example Usage in Workflow

```yaml
- name: Run tests with coverage
  working-directory: build-coverage
  env:
    ETHERVOX_GITHUB_TOKEN_DESKTOP: ${{ secrets.ETHERVOX_GITHUB_TOKEN_DESKTOP }}
    ETHERVOX_GITHUB_TOKEN: ${{ secrets.ETHERVOX_GITHUB_TOKEN }}
  run: |
    ctest --output-on-failure
```

## Security Best Practices

### ✅ DO:

- **Use fine-grained tokens** with minimal permissions
- **Set expiration dates** (rotate every 90 days)
- **Use GitHub Secrets** for all CI/CD pipelines
- **Limit repository access** to only what's needed
- **Monitor token usage** in GitHub settings

### ❌ DON'T:

- ❌ Commit tokens to repository
- ❌ Use tokens with excessive permissions
- ❌ Share tokens between environments
- ❌ Use personal account tokens for organizations
- ❌ Print secrets in logs (`echo $SECRET`)

## Token Permissions Explained

**Why "Issues: Read and write"?**
- Creates bug reports anonymously
- Allows users to submit issues from the app
- Cannot modify code or access private data
- Scoped to a single public repository

**What the token CANNOT do:**
- ❌ Push code or create branches
- ❌ Access private repositories
- ❌ Modify repository settings
- ❌ Access other GitHub resources
- ❌ Impersonate users

## Troubleshooting

### Token Not Working in CI

**Check:**
1. Secret name matches platform: `ETHERVOX_GITHUB_TOKEN_ANDROID` or `ETHERVOX_GITHUB_TOKEN_DESKTOP`
2. Secret is set at repository level (not organization)
3. Workflow has `secrets` context enabled
4. Token hasn't expired

**Test in workflow:**
```yaml
- name: Verify token is set
  run: |
    if [ -z "$ETHERVOX_GITHUB_TOKEN_DESKTOP" ] && [ -z "$ETHERVOX_GITHUB_TOKEN" ]; then
      echo "❌ No GitHub token configured"
      exit 1
    else
      echo "✅ Token is configured"
    fi
  env:
    ETHERVOX_GITHUB_TOKEN_DESKTOP: ${{ secrets.ETHERVOX_GITHUB_TOKEN_DESKTOP }}
    ETHERVOX_GITHUB_TOKEN: ${{ secrets.ETHERVOX_GITHUB_TOKEN }}
```

### Token Expired

When token expires:
1. Generate a new token (follow Step 1 above)
2. Update the secret (follow Step 2 above)
3. Re-run failed workflows

### Permission Denied Errors

If you see "403 Forbidden" or "401 Unauthorized":
1. Verify token has "Issues: Read and write" permission
2. Check repository access includes target repo
3. Ensure token hasn't been revoked

## Multiple Environments

### Development
```bash
export ETHERVOX_GITHUB_TOKEN="your_dev_token"
```

### Staging (GitHub Actions - Pull Requests)
```yaml
env:
  ETHERVOX_GITHUB_TOKEN: ${{ secrets.ETHERVOX_GITHUB_TOKEN_STAGING }}
```

### Production (GitHub Actions - Main Branch)
```yaml
env:
  ETHERVOX_GITHUB_TOKEN: ${{ secrets.ETHERVOX_GITHUB_TOKEN }}
```

### Fork Protection

For pull requests from forks, secrets are **not exposed** by default (security feature).

If needed, explicitly allow:
```yaml
# .github/workflows/build-and-test.yml
on:
  pull_request_target:  # Has access to secrets
```

⚠️ **Warning**: Only use `pull_request_target` if you trust fork contributors and validate PR code first.

## Token Rotation Schedule

**Recommended rotation:**
- **Every 90 days** for production
- **On security incidents** immediately
- **When team members leave** projects
- **After accidental exposure**

**Rotation checklist:**
1. ✅ Generate new token
2. ✅ Update GitHub Secret
3. ✅ Revoke old token
4. ✅ Verify CI/CD still works
5. ✅ Document in change log

## Monitoring

**Check token usage:**
1. Go to https://github.com/settings/tokens
2. View your fine-grained tokens
3. Click on token name
4. Review "Last used" timestamp

**Setup alerts:**
- Monitor workflow failures
- Enable GitHub notifications
- Review security advisories

## Emergency Response

**If token is compromised:**

1. **Immediately revoke token**
   - https://github.com/settings/tokens → Find token → Revoke
   
2. **Regenerate new token**
   - Follow Step 1 above
   
3. **Update secret**
   - Follow Step 2 above
   
4. **Review audit logs**
   - Check for unauthorized API calls
   - Review recent issues created
   
5. **Notify team**
   - Document incident
   - Update security procedures

## Reference Links

- **GitHub Secrets Documentation**: https://docs.github.com/en/actions/security-guides/encrypted-secrets
- **Fine-grained Tokens**: https://github.blog/2022-10-18-introducing-fine-grained-personal-access-tokens/
- **Token Security**: https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens
