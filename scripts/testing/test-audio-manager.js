/**
 * Test the new Cross-Platform Audio Manager
 */

// Import the audio manager (after building)
const path = require('path');

console.log('🎵 Testing Cross-Platform Audio Manager');
console.log('======================================');

async function testAudioManager() {
  try {
    // Import the compiled module
    const { CrossPlatformAudioManager } = require('./dist/modules/crossPlatformAudio');
    
    console.log('✅ Audio manager loaded successfully');
    
    // Create audio manager instance
    const audioManager = new CrossPlatformAudioManager({
      enableLogging: true,
      preferredOutput: 'native'
    });
    
    console.log('\n📊 Audio Manager Status:');
    const status = audioManager.getStatus();
    console.log(JSON.stringify(status, null, 2));
    
    console.log('\n🎤 Testing text-to-speech...');
    await audioManager.playAudio('Hello from EthervoxAI! This is a test of the cross-platform audio system.');
    
    console.log('✅ Audio test completed');
    
  } catch (error) {
    console.error('❌ Audio manager test failed:', error.message);
    console.error('\nThis might be because:');
    console.error('1. The project needs to be built first: npm run build');
    console.error('2. Audio dependencies are missing');
    console.error('3. The audio manager needs debugging');
  }
}

testAudioManager();
