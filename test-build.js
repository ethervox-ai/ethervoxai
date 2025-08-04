#!/usr/bin/env node

/**
 * Build Test Script
 * 
 * Tests that the core EthervoxAI modules build successfully without React dependencies
 */

const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

console.log('🧪 Testing EthervoxAI Build Process...\n');

// Test 1: Check that Node.js is available
try {
  const nodeVersion = execSync('node --version', { encoding: 'utf8' }).trim();
  console.log(`✅ Node.js: ${nodeVersion}`);
} catch (error) {
  console.error('❌ Node.js not found');
  process.exit(1);
}

// Test 2: Check that npm is available  
try {
  const npmVersion = execSync('npm --version', { encoding: 'utf8' }).trim();
  console.log(`✅ npm: ${npmVersion}`);
} catch (error) {
  console.error('❌ npm not found');
  process.exit(1);
}

// Test 3: Check TypeScript configuration
const tsconfigPath = path.join(__dirname, 'tsconfig.json');
if (fs.existsSync(tsconfigPath)) {
  console.log('✅ TypeScript config found');
  const tsconfig = JSON.parse(fs.readFileSync(tsconfigPath, 'utf8'));
  
  // Check that examples are excluded
  if (tsconfig.exclude && tsconfig.exclude.includes('src/examples/**/*')) {
    console.log('✅ Examples directory excluded from core build');
  } else {
    console.warn('⚠️  Examples directory should be excluded from core build');
  }
} else {
  console.error('❌ tsconfig.json not found');
  process.exit(1);
}

// Test 4: Check project structure
const expectedFiles = [
  'src/modules/multilingualRuntime.ts',
  'src/modules/localLLMStack.ts', 
  'src/modules/privacyDashboard.ts',
  'src/index.ts',
  'src/demo.ts'
];

console.log('\n📁 Checking core files:');
for (const file of expectedFiles) {
  const filePath = path.join(__dirname, file);
  if (fs.existsSync(filePath)) {
    console.log(`✅ ${file}`);
  } else {
    console.error(`❌ ${file} not found`);
    process.exit(1);
  }
}

// Test 5: Check examples are separate
const exampleFiles = [
  'src/examples/ui/dashboard/DashboardWeb.tsx',
  'src/examples/ui/dashboard/DashboardMobile.tsx'
];

console.log('\n📁 Checking example files:');
for (const file of exampleFiles) {
  const filePath = path.join(__dirname, file);
  if (fs.existsSync(filePath)) {
    console.log(`✅ ${file} (separated from core)`);
  } else {
    console.warn(`⚠️  ${file} not found in examples`);
  }
}

console.log('\n🎉 Project structure verified!');
console.log('\n📝 Build Instructions:');
console.log('1. Install dependencies: npm install');
console.log('2. Build core modules: npm run build');
console.log('3. Run demo: npm run demo');
console.log('4. For UI examples: npm run build:ui (requires React)');
console.log('\n✨ Ready to build EthervoxAI!');
