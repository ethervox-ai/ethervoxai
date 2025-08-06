#!/usr/bin/env node

/**
 * EthervoxAI UI Demo Launcher
 * 
 * Launches the local web server to demonstrate both web and mobile UI interfaces
 */

const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');

console.log('🚀 EthervoxAI UI Demo Launcher');
console.log('==============================');
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

if (missingPackages.length > 0) {
  console.log('📦 Installing required packages...');
  console.log(`Missing: ${missingPackages.join(', ')}`);
  console.log();
  
  // Use cmd /k to ensure Node.js environment is loaded
  const installCommand = process.platform === 'win32' 
    ? ['cmd', '/c', '"C:\\Program Files\\nodejs\\nodevars.bat" && npm', 'install', ...missingPackages]
    : ['npm', 'install', ...missingPackages];
  
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
      console.log('Please run: npm install express cors');
      process.exit(1);
    }
  });
} else {
  startDemo();
}

function startDemo() {
  console.log('🎬 Starting EthervoxAI UI Demo...');
  console.log();
  
  // Start the demo server
  const serverPath = path.join(__dirname, 'server', 'demo-server.js');
  
  if (!fs.existsSync(serverPath)) {
    console.error('❌ Demo server not found:', serverPath);
    process.exit(1);
  }
  
  // For Windows, ensure Node.js environment is loaded
  const nodeCommand = process.platform === 'win32'
    ? ['cmd', '/c', '"C:\\Program Files\\nodejs\\nodevars.bat" && node', serverPath]
    : ['node', serverPath];
  
  const server = spawn(nodeCommand[0], nodeCommand.slice(1), {
    stdio: 'inherit',
    shell: true
  });
  
  server.on('error', (error) => {
    console.error('❌ Failed to start demo server:', error.message);
    process.exit(1);
  });
  
  server.on('close', (code) => {
    console.log(`Demo server exited with code ${code}`);
  });
  
  // Handle graceful shutdown
  process.on('SIGINT', () => {
    console.log('\n🛑 Shutting down UI Demo...');
    server.kill('SIGINT');
    process.exit(0);
  });
  
  process.on('SIGTERM', () => {
    console.log('\n🛑 Shutting down UI Demo...');
    server.kill('SIGTERM');
    process.exit(0);
  });
}
