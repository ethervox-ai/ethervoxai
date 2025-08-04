/**
 * Simple Node.js launcher for EthervoxAI Windows Desktop Demo
 * This handles the demo initialization and provides better error handling
 */

console.log('🚀 EthervoxAI Windows Desktop Demo');
console.log('=====================================');
console.log();

async function runDemo() {
  try {
    console.log('Loading demo modules...');
    
    // Load the WindowsDesktopDemo class
    const { WindowsDesktopDemo } = require('./dist/demo/windows-desktop');
    
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
          console.log('  - Audio libraries:', typeof require === 'function' ? '⚠️  Simulated' : '✅');
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
