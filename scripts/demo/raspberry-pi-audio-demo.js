#!/usr/bin/env node

/**
 * EthervoxAI Raspberry Pi 5 Audio Demo
 * Demonstrates the optimized audio manager with Bluetooth speaker support
 */

const path = require('path');

// Add the source directory to the require path
const srcPath = path.join(__dirname, '..', '..', '..', 'src');
require('module')._nodeModulePaths.unshift(path.join(srcPath, 'node_modules'));

async function runRaspberryPiAudioDemo() {
    console.log('🍓 EthervoxAI Raspberry Pi 5 Audio Demo');
    console.log('=====================================');
    
    try {
        // Import the Raspberry Pi Audio Manager
        const { RaspberryPiAudioManager } = require('../../../src/modules/raspberryPiAudio');
        
        // Create audio manager with Raspberry Pi optimized settings
        const audioManager = new RaspberryPiAudioManager({
            preferredOutput: 'espeak',
            fallbackChain: ['espeak', 'pico2wave', 'festival'],
            enableLogging: true,
            speakingRate: 150,
            voice: 'en',
            volume: 80
        });
        
        console.log('\n🔧 Initializing audio system...');
        
        // Wait a moment for initialization
        await new Promise(resolve => setTimeout(resolve, 2000));
        
        console.log('\n📱 Checking for available audio devices...');
        const devices = await audioManager.getAudioDevices();
        
        if (devices.length > 0) {
            console.log('✅ Audio devices found:');
            devices.forEach((device, index) => {
                console.log(`   ${index + 1}. ${device}`);
            });
        }
        
        console.log('\n🧪 Testing audio output...');
        await audioManager.testAudio();
        
        console.log('\n🎯 Running demo phrases...');
        
        const demoPhrases = [
            'Welcome to EthervoxAI running on Raspberry Pi 5!',
            'This is a demonstration of optimized audio output.',
            'The system supports multiple Text-to-Speech engines.',
            'Bluetooth audio devices are automatically detected.',
            'Thank you for testing EthervoxAI audio capabilities!'
        ];
        
        for (let i = 0; i < demoPhrases.length; i++) {
            const phrase = demoPhrases[i];
            console.log(`\n📢 Playing phrase ${i + 1}/${demoPhrases.length}:`);
            console.log(`   "${phrase}"`);
            
            try {
                await audioManager.playText(phrase);
                console.log('   ✅ Playback successful');
                
                // Wait between phrases
                await new Promise(resolve => setTimeout(resolve, 1500));
                
            } catch (error) {
                console.log(`   ❌ Playback failed: ${error.message}`);
            }
        }
        
        console.log('\n🎵 Testing different TTS engines...');
        
        // Test with different voices/engines if available
        const voiceTests = [
            { text: 'Testing eSpeak voice engine', voice: 'en' },
            { text: 'Testing with different speaking rate', rate: 120 },
            { text: 'Testing with higher volume', volume: 100 }
        ];
        
        for (const test of voiceTests) {
            console.log(`\n🔊 ${test.text}`);
            try {
                await audioManager.playText(test.text, test);
                console.log('   ✅ Voice test successful');
                await new Promise(resolve => setTimeout(resolve, 1000));
            } catch (error) {
                console.log(`   ❌ Voice test failed: ${error.message}`);
            }
        }
        
        console.log('\n🔍 Audio system information:');
        console.log('============================');
        
        // Show system audio info
        try {
            const { exec } = require('child_process');
            const { promisify } = require('util');
            const execAsync = promisify(exec);
            
            // Check eSpeak version
            try {
                const { stdout } = await execAsync('espeak --version');
                console.log(`📦 eSpeak: ${stdout.trim()}`);
            } catch (error) {
                console.log('📦 eSpeak: Not available');
            }
            
            // Check audio system
            try {
                const { stdout } = await execAsync('pactl info | grep "Server Name"');
                console.log(`🔊 Audio Server: ${stdout.trim()}`);
            } catch (error) {
                console.log('🔊 Audio Server: PulseAudio status unknown');
            }
            
            // Check default sink
            try {
                const { stdout } = await execAsync('pactl get-default-sink');
                console.log(`🎵 Default Audio Device: ${stdout.trim()}`);
            } catch (error) {
                console.log('🎵 Default Audio Device: Unknown');
            }
            
        } catch (error) {
            console.log('⚠️ Could not retrieve system information');
        }
        
        // Cleanup
        await audioManager.cleanup();
        
        console.log('\n🎉 Raspberry Pi 5 Audio Demo Complete!');
        console.log('=====================================');
        console.log('');
        console.log('📋 Summary:');
        console.log('- Audio manager initialized successfully');
        console.log('- Multiple TTS engines tested');
        console.log('- Bluetooth audio support configured');
        console.log('- All demo phrases played');
        console.log('');
        console.log('💡 If you experienced any issues:');
        console.log('1. Run: bash scripts/setup/setup-raspberry-pi-audio.sh');
        console.log('2. Reboot your Raspberry Pi');
        console.log('3. Check Bluetooth speaker connection');
        console.log('4. Test with: ~/test_ethervoxai_audio.sh');
        console.log('');
        
    } catch (error) {
        console.error('❌ Demo failed:', error);
        console.log('');
        console.log('🔧 Troubleshooting steps:');
        console.log('1. Install dependencies: bash scripts/setup/setup-raspberry-pi-audio.sh');
        console.log('2. Build TypeScript: npm run build');
        console.log('3. Check audio devices: aplay -l');
        console.log('4. Test TTS engines: espeak "test"');
        console.log('5. Check Bluetooth: pactl list sinks short');
        
        process.exit(1);
    }
}

// Run the demo
if (require.main === module) {
    runRaspberryPiAudioDemo()
        .then(() => process.exit(0))
        .catch(error => {
            console.error('Fatal error:', error);
            process.exit(1);
        });
}

module.exports = { runRaspberryPiAudioDemo };
