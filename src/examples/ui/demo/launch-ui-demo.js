#!/usr/bin/env node

/**
 * EthervoxAI UI Demo Launcher - Cross-Platform Edition
 * 
 * Launches the local web server to demonstrate both web and mobile UI interfaces
 * Supports Windows, Linux, and Raspberry Pi with platform-specific optimizations
 */

const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');

// Platform detection
const isWindows = process.platform === 'win32';
const isLinux = process.platform === 'linux';
const isMacOS = process.platform === 'darwin';

// Raspberry Pi detection
const isRaspberryPi = (() => {
  if (!isLinux) return false;
  
  try {
    // Check for RPi-specific files
    if (fs.existsSync('/proc/device-tree/model')) {
      const model = fs.readFileSync('/proc/device-tree/model', 'utf8');
      return model.toLowerCase().includes('raspberry pi');
    }
    
    // Fallback: check CPU architecture
    const cpuInfo = fs.readFileSync('/proc/cpuinfo', 'utf8');
    return cpuInfo.includes('BCM') || cpuInfo.includes('ARM');
  } catch {
    return false;
  }
})();

// System info
const getSystemInfo = () => {
  const info = {
    platform: process.platform,
    arch: process.arch,
    nodeVersion: process.version,
    totalMemory: Math.round(os.totalmem() / 1024 / 1024), // MB
    freeMemory: Math.round(os.freemem() / 1024 / 1024)    // MB
  };
  
  if (isRaspberryPi) {
    try {
      const model = fs.readFileSync('/proc/device-tree/model', 'utf8').replace(/\0/g, '');
      info.raspberryPiModel = model;
    } catch {}
  }
  
  return info;
};

console.log('🚀 EthervoxAI UI Demo Launcher - Cross-Platform Edition');
console.log('========================================================');

const systemInfo = getSystemInfo();
console.log(`🖥️  Platform: ${systemInfo.platform} (${systemInfo.arch})`);
console.log(`⚡ Node.js: ${systemInfo.nodeVersion}`);
console.log(`💾 Memory: ${systemInfo.freeMemory}MB free / ${systemInfo.totalMemory}MB total`);

if (isRaspberryPi) {
  console.log(`🍓 Raspberry Pi detected: ${systemInfo.raspberryPiModel || 'Unknown model'}`);
  
  // Apply RPi-specific optimizations
  if (systemInfo.totalMemory < 1024) {
    console.log('🔧 Applying memory optimizations for low-memory system');
    process.env.NODE_OPTIONS = '--max-old-space-size=512';
  }
  
  if (systemInfo.arch === 'armv6l') {
    console.log('📱 Raspberry Pi Zero/1 detected - enabling lightweight mode');
    process.env.ETHERVOX_LIGHTWEIGHT = 'true';
  }
}

console.log();

// Check if required dependencies are installed
const requiredPackages = ['express', 'cors'];
const missingPackages = [];

for (const pkg of requiredPackages) {
  try {
    require.resolve(pkg);
  } catch (error) {
    missingPackages.push(pkg);
  }
}

for (const pkg of requiredPackages) {
  try {
    require.resolve(pkg);
  } catch (error) {
    missingPackages.push(pkg);
  }
}

if (missingPackages.length > 0) {
  console.log('📦 Installing required packages...');
  console.log(`Missing: ${missingPackages.join(', ')}`);
  console.log();
  
  // Platform-specific npm installation
  let installCommand;
  
  if (isWindows) {
    // Windows - load Node.js environment first
    installCommand = ['cmd', '/c', '"C:\\Program Files\\nodejs\\nodevars.bat" && npm', 'install', ...missingPackages];
  } else {
    // Linux/macOS - direct npm call with RPi optimizations
    const npmArgs = ['install', ...missingPackages];
    
    if (isRaspberryPi) {
      // Use fewer concurrent downloads on RPi to avoid memory issues
      npmArgs.push('--production', '--no-optional', '--maxsockets=1');
    }
    
    installCommand = ['npm', ...npmArgs];
  }
  
  console.log(`Running: ${installCommand.join(' ')}`);
  
  const npmInstall = spawn(installCommand[0], installCommand.slice(1), {
    stdio: 'inherit',
    shell: true,
    cwd: path.join(__dirname, '../../../../../')
  });
  
  npmInstall.on('close', (code) => {
    if (code === 0) {
      console.log('✅ Dependencies installed successfully');
      startDemo();
    } else {
      console.error('❌ Failed to install dependencies');
      console.error(`Exit code: ${code}`);
      
      if (isRaspberryPi) {
        console.log('\n🍓 Raspberry Pi troubleshooting:');
        console.log('  - Ensure you have enough free space: df -h');
        console.log('  - Check memory usage: free -m');
        console.log('  - Try: sudo npm install --unsafe-perm');
      }
      
      process.exit(1);
    }
  });
} else {
  startDemo();
}

function startDemo() {
  console.log('🚀 Starting EthervoxAI UI Demo Server...');
  console.log();
  
  // Start the demo server
  const serverPath = path.join(__dirname, 'server', 'demo-server.js');
  
  if (!fs.existsSync(serverPath)) {
    console.error('❌ Demo server not found:', serverPath);
    process.exit(1);
  }
  
  // Platform-specific Node.js execution
  let nodeCommand;
  
  if (isWindows) {
    // Windows - ensure Node.js environment is loaded
    nodeCommand = ['cmd', '/c', '"C:\\Program Files\\nodejs\\nodevars.bat" && node', serverPath];
  } else {
    // Linux/macOS - direct node execution with RPi optimizations
    const nodeArgs = [serverPath];
    
    if (isRaspberryPi) {
      // Apply memory constraints for Raspberry Pi
      if (systemInfo.totalMemory < 1024) {
        console.log('🔧 Applying Raspberry Pi memory optimizations...');
        process.env.NODE_OPTIONS = '--max-old-space-size=512';
      }
      
      // Set environment variables for RPi-specific behavior
      process.env.ETHERVOX_PLATFORM = 'raspberry-pi';
      process.env.ETHERVOX_ARCH = systemInfo.arch;
    }
    
    nodeCommand = ['node', ...nodeArgs];
  }
  
  console.log(`Platform: ${isRaspberryPi ? '🍓 Raspberry Pi' : (isWindows ? '🪟 Windows' : '🐧 Linux')}`);
  console.log('Starting server...');
  
  const server = spawn(nodeCommand[0], nodeCommand.slice(1), {
    stdio: 'inherit',
    shell: true,
    env: { ...process.env } // Pass through all environment variables
  });
  
  server.on('error', (error) => {
    console.error('❌ Failed to start demo server:', error.message);
    
    if (isRaspberryPi) {
      console.log('\n🍓 Raspberry Pi troubleshooting:');
      console.log('  - Check available memory: free -m');
      console.log('  - Verify Node.js installation: node --version');
      console.log('  - Try restarting if system is low on resources');
    }
    
    process.exit(1);
  });
  
  server.on('close', (code) => {
    if (code === 0) {
      console.log('✅ Demo server shut down cleanly');
    } else {
      console.log(`Demo server exited with code ${code}`);
    }
  });
  
  // Handle graceful shutdown
  const shutdown = (signal) => {
    console.log(`\n🛑 Received ${signal} - shutting down UI Demo...`);
    server.kill(signal);
    setTimeout(() => {
      console.log('Forcefully terminating...');
      process.exit(1);
    }, 5000);
  };
  
  process.on('SIGINT', () => shutdown('SIGINT'));
  process.on('SIGTERM', () => shutdown('SIGTERM'));
}
