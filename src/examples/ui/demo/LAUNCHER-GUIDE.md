# EthervoxAI UI Demo - Launcher Options Summary

## 🚀 Available Launchers

The EthervoxAI UI Demo provides multiple ways to start the demo server, each designed for different scenarios and user preferences. **All batch files now automatically detect PowerShell and switch to CMD for optimal compatibility.**

### 1. **launch-ui-demo-master.bat** ⭐ **(RECOMMENDED)**
- **Purpose**: Complete automated setup and launch
- **Features**: 
  - ✅ Automatically detects PowerShell and switches to CMD
  - ✅ Automatically loads Node.js environment
  - ✅ Installs required dependencies (express, cors)
  - ✅ Comprehensive error checking
  - ✅ Step-by-step progress reporting
- **Usage**: `.\launch-ui-demo-master.bat`
- **Best for**: First-time users, complete automation
- **Works in**: PowerShell (auto-switches), CMD, Windows Terminal

### 2. **run-ui-demo.bat**
- **Purpose**: Node.js environment wrapper
- **Features**:
  - ✅ PowerShell detection and auto-switch to CMD
  - ✅ Loads Node.js environment using cmd /c
  - ✅ Runs the demo with proper PATH
  - ✅ Error handling and troubleshooting tips
- **Usage**: `.\run-ui-demo.bat`  
- **Best for**: Users who already have dependencies installed
- **Works in**: PowerShell (auto-switches), CMD, Windows Terminal

### 3. **start-ui-demo.bat**
- **Purpose**: Basic launcher with environment check
- **Features**:
  - ✅ PowerShell detection and auto-switch to CMD
  - ✅ Loads Node.js environment
  - ✅ Basic error checking
  - ✅ Version reporting
- **Usage**: `.\start-ui-demo.bat`
- **Best for**: Standard usage when environment is set up
- **Works in**: PowerShell (auto-switches), CMD, Windows Terminal

### 4. **launch-ui-demo.js**
- **Purpose**: Node.js launcher script
- **Features**:
  - ✅ Cross-platform compatibility
  - ✅ Dependency checking and installation
  - ✅ Graceful shutdown handling
- **Usage**: `node launch-ui-demo.js`
- **Best for**: Direct Node.js execution, cross-platform use

### 5. **NPM Script**
- **Purpose**: Package.json integration
- **Features**:
  - ✅ Integrated with project build system
  - ✅ Consistent with other npm commands
- **Usage**: `npm run demo:ui` (from project root)
- **Best for**: Developers familiar with npm workflows

## 🔧 Node.js Environment Loading

All launchers properly handle the Node.js environment by calling:
```batch
"C:\Program Files\nodejs\nodevars.bat"
```

This ensures that:
- Node.js and npm are in the PATH
- Environment variables are properly set
- Commands execute with correct Node.js installation

## 📋 Dependency Management

The launchers handle these required packages:
- `express` - Web server framework
- `cors` - Cross-origin resource sharing

**Automatic Installation**: 
- `launch-ui-demo-master.bat` - Full automatic installation
- `launch-ui-demo.js` - Checks and auto-installs if missing

**Manual Installation**:
```bash
# From project root, with Node.js environment loaded
npm install express cors
```

## 🌐 Access Points

Once any launcher starts the server successfully:

- **Demo Home**: http://localhost:3000
- **Web Dashboard**: http://localhost:3000/web-demo
- **Mobile Interface**: http://localhost:3000/mobile-demo
- **API Endpoints**: http://localhost:3000/api/*

## 🛠️ Troubleshooting

### Common Issues and Solutions

**1. "Node.js environment not found"**
- Ensure Node.js is installed at `C:\Program Files\nodejs\`
- Download from https://nodejs.org/

**2. "npm command not found"**
- Node.js environment may not be loading properly
- Try running `launch-ui-demo-master.bat` which handles this automatically

**3. "Dependencies not found"**
- Run `launch-ui-demo-master.bat` for automatic installation
- Or manually: `npm install express cors`

**4. "Port already in use"**
- Another service is using port 3000
- Set environment variable: `set PORT=3001`
- Or kill the process using port 3000

**5. PowerShell vs CMD Issues**
- **Solution**: All batch files now automatically detect PowerShell and switch to CMD
- **What happens**: If run in PowerShell, you'll see: "Detected PowerShell environment - switching to CMD for better compatibility..."
- **Why this happens**: Node.js tooling expects CMD environment on Windows for proper PATH handling
- **Manual override**: You can run `cmd.exe` first, then run the batch file if preferred

## 🔄 PowerShell Auto-Switch Feature

**How it works**:
- Detects PowerShell by checking `__PSLockDownPolicy` environment variable (only set when running IN PowerShell)
- Automatically executes: `cmd.exe /c "%~f0" %*` to re-run in CMD
- Preserves all command-line arguments and exit codes
- Seamless user experience - works transparently

**Benefits**:
- ✅ Works in any Windows terminal (PowerShell, CMD, Windows Terminal)
- ✅ No need to manually switch environments
- ✅ Proper Node.js PATH and environment variable handling
- ✅ Maintains compatibility with both shells
- ✅ Accurate PowerShell detection (not just availability)

### Debug Mode

For detailed debugging, run the Node.js launcher directly:
```bash
# Load Node.js environment first
cmd /c ""C:\Program Files\nodejs\nodevars.bat" && node -e "console.log('Node version:', process.version); console.log('PATH:', process.env.PATH.split(';').filter(p => p.includes('node')))""
```

## 🎯 Recommendations

**For first-time users**: Use `launch-ui-demo-master.bat`
**For regular use**: Use `npm run demo:ui` or `run-ui-demo.bat`
**For development**: Use `node launch-ui-demo.js` with nodemon
**For CI/automation**: Use the Node.js script directly

## 🔄 Updates

When updating the demo:
1. Update `demo-server.js` for server functionality
2. Update `launch-ui-demo.js` for launcher logic  
3. Update batch files for Windows-specific improvements
4. Update `package.json` for npm script integration

All launchers are designed to be forward-compatible and will work with future updates to the demo system.
