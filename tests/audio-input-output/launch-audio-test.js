/**
 * Simple launcher for the Audio Input/Output Test Console
 * This handles loading the compiled modules and starting the test application
 */

console.log('🎵 EthervoxAI Audio Input/Output Test Launcher');
console.log('==============================================');
console.log();

async function launchAudioTest() {
  try {
    console.log('Loading audio test modules...');
    
    // Import the compiled modules
    const { AudioTestConsole } = require('../../dist/tests/audio-input-output/AudioTestConsole');
    
    console.log('✅ Modules loaded successfully');
    console.log();
    
    // Create and start the test console
    const testConsole = new AudioTestConsole();
    await testConsole.start();
    
  } catch (error) {
    console.error('❌ Failed to launch audio test:', error.message);
    console.error();
    console.error('This might be due to:');
    console.error('1. The project needs to be built first: npm run build');
    console.error('2. Audio dependencies are missing');
    console.error('3. Cross-platform audio manager not available');
    console.error();
    console.error('For basic functionality, run: npm install');
    console.error('For advanced audio, run: install-audio-libraries.bat');
    process.exit(1);
  }
}

// Handle uncaught exceptions gracefully
process.on('uncaughtException', (error) => {
  console.error('❌ Unexpected error:', error.message);
  process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
  console.error('❌ Unhandled promise rejection:', reason);
  process.exit(1);
});

// Launch the audio test
launchAudioTest();
