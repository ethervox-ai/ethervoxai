/**
 * Audio Library Verification Script
 * Tests if the native audio libraries are properly installed and working
 */

console.log('🔍 EthervoxAI Audio Library Verification');
console.log('========================================');
console.log();

// Test each audio library individually
const libraries = [
  { name: 'mic', description: 'Microphone input' },
  { name: 'speaker', description: 'Audio output' },
  { name: 'wav', description: 'WAV file processing' },
  { name: 'say', description: 'Text-to-Speech' }
];

let allWorking = true;

for (const lib of libraries) {
  try {
    console.log(`Testing ${lib.name} (${lib.description})...`);
    
    const module = require(lib.name);
    console.log(`✅ ${lib.name}: Available`);
    
    // Basic functionality test
    if (lib.name === 'mic') {
      // Test mic module
      const micInstance = module({
        rate: 16000,
        channels: 1,
        debug: false,
        exitOnSilence: 6
      });
      console.log(`   - Microphone instance created successfully`);
    } else if (lib.name === 'speaker') {
      // Test speaker module  
      const Speaker = module;
      const speaker = new Speaker({
        channels: 2,
        bitDepth: 16,
        sampleRate: 44100
      });
      console.log(`   - Speaker instance created successfully`);
      speaker.end(); // Clean up
    } else if (lib.name === 'wav') {
      // Test wav module
      const Reader = module.Reader;
      const Writer = module.Writer;
      console.log(`   - WAV Reader and Writer available`);
    } else if (lib.name === 'say') {
      // Test say module (this should always work)
      console.log(`   - TTS engine available`);
    }
    
  } catch (error) {
    console.log(`❌ ${lib.name}: ${error.message}`);
    
    if (lib.name !== 'say') { // say is optional for basic functionality
      allWorking = false;
    }
  }
  console.log();
}

console.log('========================================');
if (allWorking) {
  console.log('🎉 All audio libraries are working correctly!');
  console.log('   The demo will use real audio input/output.');
} else {
  console.log('⚠️  Some audio libraries are missing or not working.');
  console.log('   The demo will run with simulated audio.');
  console.log();
  console.log('To fix this:');
  console.log('1. Ensure Visual Studio Build Tools are installed');
  console.log('2. Run: install-audio-libraries.bat');
  console.log('3. Restart your command prompt as Administrator if needed');
}

console.log();
console.log('System Information:');
console.log(`Node.js: ${process.version}`);
console.log(`Platform: ${process.platform} ${process.arch}`);
console.log(`Working Directory: ${process.cwd()}`);
