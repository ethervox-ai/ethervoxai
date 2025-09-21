#!/usr/bin/env node

/**
 * 🧪 Quick Test for Voice Interaction Pipeline
 * Tests the complete pipeline without requiring wake word detection
 */

const path = require('path');

// Import the demo class
const { VoiceInteractionDemo } = require('./voice-interaction-demo');

async function testPipeline() {
    console.log('🧪 Testing Voice Interaction Pipeline');
    console.log('=====================================');
    
    const demo = new VoiceInteractionDemo();
    await demo.initialize();
    
    console.log('\n📝 Testing common voice commands...');
    
    const testCommands = [
        'What time is it?',
        'Tell me a joke',
        'How are you doing?',
        'What can you do for me?'
    ];
    
    for (let i = 0; i < testCommands.length; i++) {
        const command = testCommands[i];
        console.log(`\n--- Test ${i + 1}/${testCommands.length}: "${command}" ---`);
        
        try {
            await demo.processSimulatedCommand(command);
        } catch (error) {
            console.error(`❌ Test failed:`, error.message);
        }
        
        // Short pause between tests
        await demo.sleep(1000);
    }
    
    console.log('\n📊 Test Results:');
    demo.showStats();
    
    console.log('\n✅ Pipeline test completed!');
}

if (require.main === module) {
    testPipeline()
        .catch(error => {
            console.error('Test failed:', error);
            process.exit(1);
        });
}

module.exports = { testPipeline };
