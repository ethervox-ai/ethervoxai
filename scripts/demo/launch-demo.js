/**
 * Simple Node.js launcher for EthervoxAI Windows Desktop Demo
 * This handles the demo initialization and provides better error handling
 */

const { exec } = require('child_process');
const { promisify } = require('util');
const fs = require('fs');
const execAsync = promisify(exec);

/**
 * Check audio capabilities with platform-specific detection
 */
async function checkAudioCapabilities() {
  const status = {
    microphone: '❌',
    speaker: '❌', 
    wav: '❌',
    tts: '❌',
    platform: 'generic'
  };
  
  // Check basic Node.js packages
  try { require('mic'); status.microphone = '✅'; } catch(e) { status.microphone = '❌'; }
  try { require('speaker'); status.speaker = '✅'; } catch(e) { status.speaker = '❌'; }
  try { require('wav'); status.wav = '✅'; } catch(e) { status.wav = '❌'; }
  try { require('say'); status.tts = '✅'; } catch(e) { status.tts = '❌'; }
  
  // Detect Raspberry Pi
  if (process.platform === 'linux' && process.arch === 'arm64') {
    try {
      if (fs.existsSync('/proc/device-tree/model')) {
        const model = fs.readFileSync('/proc/device-tree/model', 'utf8');
        if (model.includes('Raspberry Pi')) {
          status.platform = 'raspberry-pi';
          
          // Check Raspberry Pi specific TTS engines
          try {
            await execAsync('which espeak');
            status.espeak = '✅';
          } catch(e) { status.espeak = '❌'; }
          
          try {
            await execAsync('which pico2wave');
            status.pico2wave = '✅';
          } catch(e) { status.pico2wave = '❌'; }
          
          try {
            await execAsync('which festival');
            status.festival = '✅';
          } catch(e) { status.festival = '❌'; }
          
          try {
            await execAsync('which aplay');
            status.alsa = '✅';
          } catch(e) { status.alsa = '❌'; }
          
          // Check for Bluetooth audio
          try {
            const { stdout } = await execAsync('pactl list sinks short');
            if (stdout.includes('bluez') || stdout.includes('bluetooth')) {
              status.bluetooth = '✅';
            } else {
              status.bluetooth = '❌';
            }
          } catch(e) { status.bluetooth = '❌'; }
          
          // Override speaker status if we have working TTS engines
          if (status.espeak === '✅' || status.pico2wave === '✅' || status.festival === '✅') {
            status.speaker = '✅ (TTS)';
            status.tts = '✅';
          }
        }
      }
    } catch(e) {
      // Not a Raspberry Pi or can't detect
    }
  }
  
  return status;
}

console.log('🚀 EthervoxAI Cross-Platform Demo');
console.log('===================================');
console.log();

async function runDemo() {
  try {
    console.log('Loading demo modules...');
    
    // Load the WindowsDesktopDemo class
    const { WindowsDesktopDemo } = require('../../dist/demo/windows-desktop');
    
    console.log('✅ Demo modules loaded successfully');
    console.log();
    
    // Create and initialize the demo
    const demo = new WindowsDesktopDemo();
    
    console.log('🔧 Initializing demo...');
    await demo.initialize();
    
    console.log();
    console.log('=====================================');
    console.log('Commands available during demo:');
    console.log('  start  - Start voice listening');
    console.log('  stop   - Stop voice listening');
    console.log('  status - Show current status');
    console.log('  quit   - Exit application');
    console.log('=====================================');
    console.log();
    
    // Set up command line interface
    const readline = require('readline');
    const rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout,
      prompt: 'EthervoxAI> '
    });
    
    rl.prompt();
    
    rl.on('line', async (input) => {
      const command = input.trim().toLowerCase();
      
      switch (command) {
        case 'start':
          console.log('🎤 Starting voice listening...');
          try {
            await demo.startListening();
            console.log('✅ Voice listening started');
          } catch (error) {
            console.log('⚠️  Using simulated audio:', error.message);
          }
          break;
          
        case 'stop':
          console.log('🛑 Stopping voice listening...');
          await demo.stopListening();
          console.log('✅ Voice listening stopped');
          break;
          
        case 'status':
          console.log('📊 Current status:');
          console.log('  - Listening:', demo.isListening ? '✅' : '❌');
          console.log('  - Processing:', demo.isProcessing ? '✅' : '❌');
          
          // Check audio library status with platform-specific detection
          const audioStatus = await checkAudioCapabilities();
          
          console.log('  - Audio Libraries:');
          console.log(`    • Microphone (mic): ${audioStatus.microphone}`);
          console.log(`    • Speaker output: ${audioStatus.speaker}`);
          console.log(`    • WAV processing: ${audioStatus.wav}`);
          console.log(`    • Text-to-Speech: ${audioStatus.tts}`);
          
          if (audioStatus.platform === 'raspberry-pi') {
            console.log('  🍓 Raspberry Pi audio engines:');
            console.log(`    • eSpeak: ${audioStatus.espeak}`);
            console.log(`    • Pico2Wave: ${audioStatus.pico2wave}`);
            console.log(`    • Festival: ${audioStatus.festival}`);
            console.log(`    • ALSA/PulseAudio: ${audioStatus.alsa}`);
            
            if (audioStatus.bluetooth) {
              console.log(`    • Bluetooth Audio: ${audioStatus.bluetooth}`);
            }
          }
          
          if (audioStatus.speaker === '❌' && audioStatus.platform !== 'raspberry-pi') {
            console.log('  ⚠️  Audio output simulated (speaker package not available)');
            console.log('     This is common on ARM64 Windows systems.');
          }
          
          console.log(`  - System: ${process.platform} ${process.arch}, Node.js ${process.version}`);
          break;
          
        case 'quit':
        case 'exit':
          console.log('👋 Goodbye!');
          process.exit(0);
          break;
          
        case 'help':
          console.log('Available commands: start, stop, status, quit, help');
          break;
          
        default:
          if (command) {
            console.log(`❓ Unknown command: ${command}. Type 'help' for available commands.`);
          }
          break;
      }
      
      rl.prompt();
    });
    
    rl.on('close', () => {
      console.log('👋 Demo ended.');
      process.exit(0);
    });
    
  } catch (error) {
    console.error('❌ Failed to start demo:', error.message);
    console.error();
    console.error('This might be due to:');
    console.error('1. Missing dependencies - run: npm install');
    console.error('2. Build not completed - run: npm run build');
    console.error('3. Audio libraries not installed (optional)');
    console.error();
    console.error('For basic demo functionality, only the core dependencies are required.');
    console.error('Audio features will be simulated if audio libraries are not available.');
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

// Run the demo
runDemo();
