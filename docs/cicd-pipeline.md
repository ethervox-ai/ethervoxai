# EtherVoxAI CI/CD Pipeline Documentation

# Overview

This document outlines the comprehensive CI/CD pipeline setup for EtherVoxAI, providing automated testing, building,
and release management across all supported platforms.

## Pipeline Architecture

## Core Workflows

### 1. Build and Test (`build-and-test.yml`)

- **Triggers**: Push to main/develop/feature/hotfix branches, PRs to main/develop
- **Platforms**: Windows, Linux, macOS, Raspberry Pi (Zero & 4)
- **Features**:
  - Cross-platform builds with CMake
  - Automated testing with CTest
  - Code coverage with Codecov
  - Artifact generation for all platforms

### 2. ESP32 Build (`esp32-build.yml`)

- **Triggers**: Push/PR to main branches, manual dispatch
- **Targets**: ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6
- **Features**:
  - ESP-IDF v5.1.2 compilation
  - Firmware size analysis
  - Flash instruction generation
  - Security configuration validation

### 3. Dashboard CI/CD (`dashboard.yml`)

- **Triggers**: Changes to dashboard/ directory
- **Features**:
  - Node.js 18 + pnpm build system
  - TypeScript type checking
  - ESLint + Prettier formatting
  - Unit and component testing
  - E2E testing with Playwright
  - Lighthouse performance audits
  - Netlify preview deployments

### 4. Code Quality (`code-quality.yml`)

- **Triggers**: All pushes and PRs
- **Features**:
  - C++ formatting (clang-format)
  - Static analysis (clang-tidy, cppcheck)
  - Security scanning (Semgrep, TruffleHog)
  - Dependency vulnerability checks
  - Documentation quality checks
  - License compliance validation
  - Code metrics analysis

### 5. Release (`release.yml`)

- **Triggers**: Git tags (v*), manual dispatch
- **Features**:
  - Multi-platform release builds
  - ESP32 firmware packaging
  - Automated release notes generation
  - GitHub Releases creation
  - Documentation updates

## Platform Support Matrix

| Platform | Build | Test | Release | Notes |
|----------|-------|------|---------|-------|
| Windows x64 | | | | Full featured |
| Linux x64 | | | | Full featured |
| macOS x64 | | | | Full featured |
| Linux ARM64 | | | | Cross-compile only |
| Raspberry Pi Zero | | | | Cross-compile only |
| Raspberry Pi 4/5 | | | | Cross-compile only |
| ESP32 | | | | Firmware only |
| ESP32-S2/S3/C3/C6 | | | | Firmware only |

## Configuration Files

## Code Quality

- `.clang-format` - C++ code formatting (Google style + modifications)
- `.clang-tidy` - Static analysis rules for C++
- `.markdownlint.json` - Markdown linting configuration
- `.markdown-link-check.json` - Link validation settings

## Dashboard

- `dashboard/.auditci.json` - NPM audit configuration
- `dashboard/lighthouserc.js` - Lighthouse performance thresholds

## Required Secrets

For full functionality, configure these GitHub repository secrets:

## Code Coverage & Security

- `CODECOV_TOKEN` - Code coverage reporting
- `SAFETY_API_KEY` - Python dependency security scanning

## Lighthouse & Preview Deployments

- `LHCI_GITHUB_APP_TOKEN` - Lighthouse CI integration
- `NETLIFY_AUTH_TOKEN` - Netlify deployment authentication
- `NETLIFY_SITE_ID` - Netlify site identifier

## Release Automation

- `GITHUB_TOKEN` - Automatically provided, used for releases

## Workflow Triggers

## Automatic Triggers

```yaml

# Main workflows

on:
  push:
    branches: [ main, develop, 'feature/**', 'hotfix/**' ]
  pull_request:
    branches: [ main, develop ]

# Release workflow

on:
  push:
    tags: [ 'v*' ]
```text

## Manual Triggers

- **ESP32 Build**: Manual dispatch with target selection
- **Release**: Manual dispatch with version and prerelease options

## Build Artifacts

## Desktop Builds

- `ethervoxai-{platform}-{sha}` - Platform binaries + dashboard
- Archives: `.zip` (Windows), `.tar.gz` (Unix)

## Embedded Builds

- `ethervoxai-esp32-{target}-{sha}` - Firmware binaries + flash scripts
- `ethervoxai-{rpi-target}-{sha}` - Cross-compiled binaries

## Release Assets

- Multi-platform release packages
- ESP32 firmware with installation instructions
- Comprehensive documentation

## Performance & Optimization

## Build Performance

- **Caching**: ESP-IDF tools, npm dependencies, CMake builds
- **Parallelization**: Multi-core builds (`-j$(nproc)`)
- **Conditional Execution**: Path-based triggers for dashboard changes

## Test Performance

- **Coverage**: Linux-only to avoid redundancy
- **E2E Tests**: Playwright with browser caching
- **Timeouts**: Reasonable limits for all jobs

## Monitoring & Alerts

## Build Status

- GitHub status checks on all PRs
- Artifact retention (15-30 days)
- Failed build notifications

## Quality Gates

- Code coverage thresholds
- Security vulnerability blocking
- Performance regression detection

## Development Workflow

## Feature Development

1. Create feature branch from `develop`
1. Push triggers build-and-test + code-quality
1. Create PR triggers full validation
1. Dashboard changes trigger preview deployment

## Release Process

1. Create release tag (e.g., `v1.0.0`)
1. Automated multi-platform builds
1. Release notes generation
1. GitHub Release creation
1. Documentation updates

## Maintenance

## Regular Updates

- ESP-IDF version updates (quarterly)
- Node.js LTS version updates
- Security scanner updates
- Dependency updates via Dependabot

## Monitoring

- Build success rates
- Test coverage trends
- Security vulnerability reports
- Performance metrics

## Troubleshooting

## Common Issues

- **ESP-IDF Setup**: Check toolchain installation logs
- **Cross-compilation**: Verify toolchain files in `cmake/toolchains/`
- **Dashboard Tests**: Check Node.js version compatibility
- **Release Failures**: Validate version format and permissions

## Debug Actions

- Enable debug logging in workflows
- Check artifact uploads for build outputs
- Review workflow run logs for detailed error information

---

*Last Updated: September 2025*
*Pipeline Version: v1.0*
