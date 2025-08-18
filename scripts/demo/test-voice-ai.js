#!/usr/bin/env node

/**
 * Direct Test of Voice Demo AI Processing
 */

const path = require('path');

// Load the voice interaction demo
const VoiceInteractionDemo = require(path.join(__dirname, 'voice-interaction-demo.js'));

async function testDirectAI() {
    console.log('🎤 Testing Voice Demo AI Processing');
    console.log('===================================');
    
    const demo = new VoiceInteractionDemo.VoiceInteractionDemo();
    await demo.initialize();
    
    const testQueries = [
        "What time is it?",
        "Tell me a joke",
        "How are you?",
        "What can you help me with?"
    ];
    
    for (const query of testQueries) {
        console.log(`\n🔍 Testing: "${query}"`);
        
        try {
            const response = await demo.processAudioWithAI(query, 'unknown');
            console.log(`✅ Response: "${response}"`);
        } catch (error) {
            console.log(`❌ Error: ${error.message}`);
        }
    }
}

testDirectAI().catch(console.error);
