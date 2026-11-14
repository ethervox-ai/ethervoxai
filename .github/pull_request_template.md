# Pull Request

## Description

<!-- Briefly describe what this PR does and why -->

## Type of Change

<!-- Check all that apply -->

- [ ] Bug fix (non-breaking change fixing an issue)
- [ ] New feature (non-breaking change adding functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update
- [ ] Performance improvement
- [ ] Code cleanup / refactoring
- [ ] Build / CI configuration

## Consumer Impact Statement (ADR-0023 §5)

**This section is mandatory for all PRs that modify public APIs, build configuration, or core behavior.**

For each consumer product that pins this repository, state whether it is affected and what action (if any) is required:

### ethervoxai-apple (Workspace)
<!-- ✅ Not affected | ⚠️ Affected — requires: ... | 🚫 Breaks — must: ... -->

### ethervoxai-android (Friend'O'Mine)
<!-- ✅ Not affected | ⚠️ Affected — requires: ... | 🚫 Breaks — must: ... -->

### ethervoxai-ios
<!-- ✅ Not affected | ⚠️ Affected — requires: ... | 🚫 Breaks — must: ... -->

**Public API changes:**
<!-- List any added, modified, or removed public functions/types. If none, write "None." -->

**Build configuration changes:**
<!-- CMake variables, required dependencies, compiler flags. If none, write "None." -->

**Behavioral changes:**
<!-- Significant changes to existing behavior that consumers depend on. If none, write "None." -->

## Acceptance Criteria

<!-- From the task packet, if applicable. Otherwise, describe what must be true for this PR to be complete. -->

- [ ] Code compiles with zero warnings
- [ ] All four profiles build successfully (EDGE, MOBILE, DESKTOP, WORKSPACE)
- [ ] Tests pass: `ctest --output-on-failure`
- [ ] Consumer impact statement completed (if applicable)
- [ ] Documentation updated (if behavior or API changed)
- [ ] CHANGELOG.md entry added

## Testing

<!-- Describe the testing you performed to verify your changes -->

**Platforms tested:**
- [ ] macOS
- [ ] Linux
- [ ] Windows
- [ ] iOS (if mobile-specific)
- [ ] Android (if mobile-specific)

**Profiles tested:**
- [ ] EDGE
- [ ] MOBILE
- [ ] DESKTOP
- [ ] WORKSPACE

## Verification

<!-- Paste relevant command outputs or screenshots -->

```bash
# Example: Build output, test results, etc.
```

## Related Issues

<!-- Link to related issues using #issue_number -->

Closes #

## Additional Context

<!-- Any other information that reviewers should know -->
