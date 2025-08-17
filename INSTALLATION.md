# 🔧 Installation & Setup Guide

## 🖥️ System Prerequisites

### Windows 11

#### Required Software Installation:

**1. Install Node.js & npm**
```powershell
# Option 1: Download from official website
# Visit https://nodejs.org/ and download LTS version (18.x or 20.x)

# Option 2: Using Chocolatey (if installed)
choco install nodejs

# Option 3: Using Windows Package Manager (winget)
winget install OpenJS.NodeJS

# Verify installation
node --version
npm --version
```

**2. Install Git**
```powershell
# Option 1: Download from https://git-scm.com/download/win

# Option 2: Using Chocolatey
choco install git

# Option 3: Using winget
winget install Git.Git

# Verify installation
git --version
```

**3. Install Python (for native modules)**
```powershell
# Option 1: Download from https://python.org

# Option 2: Using Chocolatey
choco install python

# Option 3: Using winget
winget install Python.Python.3.11

# Verify installation
python --version
pip --version
```

**4. Install Build Tools**
```powershell
# Install Visual Studio Build Tools (required for native modules)
# Option 1: Download Visual Studio Installer and install "Build Tools for Visual Studio"

# Option 2: Using Chocolatey
choco install visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools"

# Option 3: Install standalone tools
npm install --global windows-build-tools
```

**5. Optional: Development Tools**
```powershell
# VS Code
winget install Microsoft.VisualStudioCode

# Docker Desktop (for containerized deployments)
winget install Docker.DockerDesktop

# PowerShell 7 (newer than built-in Windows PowerShell)
winget install Microsoft.PowerShell
```

---

### Ubuntu Linux (20.04 LTS, 22.04 LTS, 24.04 LTS)

#### Required Software Installation:

**1. Update System**
```bash
sudo apt update && sudo apt upgrade -y
```

**2. Install Node.js & npm**
```bash
# Option 1: Using NodeSource repository (recommended)
curl -fsSL https://deb.nodesource.com/setup_lts.x | sudo -E bash -
sudo apt-get install -y nodejs

# Option 2: Using snap
sudo snap install node --classic

# Option 3: Using nvm (Node Version Manager)
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
source ~/.bashrc
nvm install --lts
nvm use --lts

# Verify installation
node --version
npm --version
```

**3. Install Development Tools**
```bash
# Essential build tools
sudo apt install -y build-essential curl wget git

# Python and pip (for native modules)
sudo apt install -y python3 python3-pip python3-dev

# Additional development libraries
sudo apt install -y libssl-dev libffi-dev libbz2-dev libreadline-dev libsqlite3-dev
```

**4. Install Optional Tools**
```bash
# VS Code
wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > packages.microsoft.gpg
sudo install -o root -g root -m 644 packages.microsoft.gpg /etc/apt/trusted.gpg.d/
sudo sh -c 'echo "deb [arch=amd64,arm64,armhf signed-by=/etc/apt/trusted.gpg.d/packages.microsoft.gpg] https://packages.microsoft.com/repos/code stable main" > /etc/apt/sources.list.d/vscode.list'
sudo apt update
sudo apt install -y code

# Docker
sudo apt install -y docker.io docker-compose
sudo usermod -aG docker $USER
```

---

### Raspberry Pi OS (Raspbian)

#### Required Software Installation:

**1. Update System**
```bash
sudo apt update && sudo apt upgrade -y
```

**2. Install Node.js & npm (ARM-optimized)**
```bash
# Option 1: Using official ARM builds
curl -fsSL https://deb.nodesource.com/setup_lts.x | sudo -E bash -
sudo apt-get install -y nodejs

# Option 2: Using snap (if available)
sudo snap install node --classic

# For older Raspberry Pi models, you may need to compile or use ARM builds
# Check architecture
uname -m

# Verify installation
node --version
npm --version
```

**3. Install Development Tools**
```bash
# Essential build tools for ARM
sudo apt install -y build-essential curl wget git

# Python development tools
sudo apt install -y python3 python3-pip python3-dev python3-venv

# Additional libraries for ARM compilation
sudo apt install -y libssl-dev libffi-dev libbz2-dev libreadline-dev libsqlite3-dev

# GPIO and hardware libraries (Raspberry Pi specific)
sudo apt install -y python3-rpi.gpio python3-gpiozero
```

**4. Raspberry Pi Specific Setup**
```bash
# Enable I2C, SPI, GPIO (for hardware integration)
sudo raspi-config
# Navigate to Interfacing Options and enable I2C, SPI, GPIO

# Install Pi-specific tools
sudo apt install -y raspberrypi-kernel-headers

# For audio processing (if needed)
sudo apt install -y portaudio19-dev pulseaudio pulseaudio-utils
```

---

### Fedora Linux (38, 39, 40)

#### Required Software Installation:

**1. Update System**
```bash
sudo dnf update -y
```

**2. Install Node.js & npm**
```bash
# Option 1: Using official Fedora repositories
sudo dnf install -y nodejs npm

# Option 2: Using NodeSource repository
sudo dnf install -y curl
curl -fsSL https://rpm.nodesource.com/setup_lts.x | sudo bash -
sudo dnf install -y nodejs

# Option 3: Using nvm
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
source ~/.bashrc
nvm install --lts
nvm use --lts

# Verify installation
node --version
npm --version
```

**3. Install Development Tools**
```bash
# Essential development tools
sudo dnf groupinstall -y "Development Tools" "Development Libraries"
sudo dnf install -y curl wget git

# Python development
sudo dnf install -y python3 python3-pip python3-devel

# Additional libraries
sudo dnf install -y openssl-devel libffi-devel bzip2-devel readline-devel sqlite-devel
```

**4. Install Optional Tools**
```bash
# VS Code
sudo rpm --import https://packages.microsoft.com/keys/microsoft.asc
sudo sh -c 'echo -e "[code]\nname=Visual Studio Code\nbaseurl=https://packages.microsoft.com/yumrepos/vscode\nenabled=1\ngpgcheck=1\ngpgkey=https://packages.microsoft.com/keys/microsoft.asc" > /etc/yum.repos.d/vscode.repo'
sudo dnf install -y code

# Docker
sudo dnf install -y docker docker-compose
sudo systemctl enable docker
sudo systemctl start docker
sudo usermod -aG docker $USER
```

---

### Kali Linux

#### Required Software Installation:

**1. Update System**
```bash
sudo apt update && sudo apt upgrade -y
```

**2. Install Node.js & npm**
```bash
# Option 1: Using official repositories
sudo apt install -y nodejs npm

# Option 2: Using NodeSource repository
curl -fsSL https://deb.nodesource.com/setup_lts.x | sudo -E bash -
sudo apt-get install -y nodejs

# Option 3: Using nvm (recommended for development)
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
source ~/.bashrc
nvm install --lts
nvm use --lts

# Verify installation
node --version
npm --version
```

**3. Install Development Tools**
```bash
# Essential build tools
sudo apt install -y build-essential curl wget git

# Python development (usually pre-installed in Kali)
sudo apt install -y python3 python3-pip python3-dev python3-venv

# Additional development libraries
sudo apt install -y libssl-dev libffi-dev libbz2-dev libreadline-dev libsqlite3-dev

# Security tools integration libraries (Kali-specific)
sudo apt install -y libpcap-dev libnetfilter-queue-dev
```

**4. Kali-Specific Considerations**
```bash
# Ensure non-root user for Node.js development (security best practice)
# If running as root (not recommended for development):
echo 'export npm_config_unsafe_perm=true' >> ~/.bashrc
source ~/.bashrc

# Install additional security-focused tools
sudo apt install -y tor proxychains4 wireshark-common

# For networking and security research
sudo apt install -y nmap netcat-openbsd tcpdump
```

---

## 🔧 Cross-Platform Verification

After installing prerequisites on any system, verify your setup:

```bash
# Check versions
node --version    # Should be 18.x or 20.x
npm --version     # Should be 9.x or 10.x
git --version     # Should be 2.x
python3 --version # Should be 3.8+

# Test Node.js installation
npm config get registry
npm list -g --depth=0

# Test build tools (try compiling a native module)
# Option 1: Install globally with admin privileges (Windows)
# Run PowerShell as Administrator, then:
# npm install -g node-gyp

# Option 2: Install locally in project (recommended)
npm install node-gyp --save-dev

# Option 3: Use npx to run without installing
npx node-gyp --version
```

**Windows-Specific Build Tools Test:**
```powershell
# Run as Administrator if needed
# Check if Visual Studio Build Tools are available
where cl
# Should show path to MSVC compiler

# Test Python availability for node-gyp
python --version
# or
py --version

# Alternative: Test without installing node-gyp globally
npx --yes node-gyp --version
```

**Raspbian/Linux-Specific Build Tools Test:**
```bash
# Option 1: Fix npm permissions (recommended for Raspbian)
# Set npm to use a different directory for global packages
mkdir ~/.npm-global
npm config set prefix '~/.npm-global'
echo 'export PATH=~/.npm-global/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

# Now try installing node-gyp globally
npm install -g node-gyp

# Option 2: Use sudo (not recommended but works)
sudo npm install -g node-gyp

# Option 3: Install locally in project (safest)
npm install node-gyp --save-dev

# Option 4: Use npx without installation
npx node-gyp --version

# Verify build tools are available
gcc --version
make --version
python3 --version

# Check if development headers are installed
dpkg -l | grep python3-dev
dpkg -l | grep build-essential

# Sample Output
```bash
ii  libpython3-dev:arm64                 3.11.2-1+b1                             arm64        header files and a static library for Python (default)
ii  python3-dev                          3.11.2-1+b1                             arm64        header files and a static library for Python (default)
ii  build-essential                      12.9                                    arm64        Informational list of build-essential packages
```

# Test compilation capabilities
node -e "console.log(process.platform, process.arch)"
```

## 📋 Minimum System Requirements

- **CPU**: x86_64 (Intel/AMD) or ARM64 (Apple Silicon, Raspberry Pi 4+)
- **RAM**: 4GB minimum, 8GB recommended
- **Storage**: 2GB free space for development dependencies
- **OS**: 
  - Windows 11 (or Windows 10 with latest updates)
  - Ubuntu 20.04 LTS or newer
  - Raspberry Pi OS (64-bit recommended)
  - Fedora 38 or newer
  - Kali Linux (latest rolling release)

---

## 📥 Getting the Code

### 1. Clone the Repository

**Using HTTPS (recommended for most users):**
```bash
git clone https://github.com/ethervox-ai/ethervoxai.git
```

**Using SSH (if you have SSH keys set up):**
```bash
git clone git@github.com:ethervox-ai/ethervoxai.git
```

**Using GitHub CLI (if installed):**
```bash
gh repo clone ethervox-ai/ethervoxai
```

### 2. Navigate to Project Directory

```bash
cd ethervoxai
```

### 3. Verify Repository Structure

```bash
# List main directories to confirm successful clone
ls -la

# You should see:
# - src/           (Core TypeScript modules)
# - implementations/ (Platform-specific code)
# - package.json   (Node.js dependencies)
# - README.md      (Project documentation)
```

**Windows PowerShell:**
```powershell
# List directories
dir

# Or use ls if you have Git Bash or PowerShell 7
ls
```

---

## Installation Steps

### 1. Prerequisites
See above for detailed pre-requistes

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

### Fixing Current Deprecation Warnings (Raspberry Pi/Linux)

If you're seeing these specific warnings:
```
npm warn deprecated inflight@1.0.6: This module is not supported, and leaks memory
npm warn deprecated glob@7.2.3: Glob versions prior to v9 are no longer supported
```

**Quick Fix - Update Dependencies:**
```bash
# Clear npm cache
npm cache clean --force

# Remove node_modules and package-lock.json
rm -rf node_modules package-lock.json

# Update package.json with fixed versions
npm install rimraf@^6.0.1

# Force update to newer glob version
npm install glob@^10.3.0 --save-dev

# Reinstall everything
npm install
```

**Raspberry Pi Audio Library Alternatives:**
If you're having issues with audio packages on Raspberry Pi, use these alternatives:

```bash
# Replace problematic audio packages with Raspberry Pi optimized versions
npm uninstall node-wav-player play-sound speaker mic

# Install Raspberry Pi compatible alternatives
npm install --save-optional \
  node-aplay@^2.0.0 \
  node-record-lpcm16@^1.0.1 \
  alsa-utils@^1.2.0 \
  node-portaudio@^2.0.0

# For text-to-speech on Raspberry Pi
npm install espeak-tts@^1.0.0

# Verify ALSA is working
aplay -l
amixer scontrols
```

**Complete Clean Installation (if issues persist):**
```bash
# Backup your changes
git stash

# Complete clean slate
rm -rf node_modules package-lock.json ~/.npm/_cacache

# Update npm itself
npm install -g npm@latest

# Fresh install with latest packages
npm install

# Restore your changes
git stash pop
```

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
