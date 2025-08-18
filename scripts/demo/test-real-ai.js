#!/usr/bin/env node

/**
 * Test Real AI Processing in Voice Demo
 */

const path = require('path');
const { LocalLLMStack } = require(path.join(__dirname, '..', '..', 'dist', 'modules', 'localLLMStack'));

async function testRealAI() {
    console.log('🧠 Testing Real AI Processing');
    console.log('==============================');
    
    const llm = new LocalLLMStack();
    await llm.initialize();
    
    const testQueries = [
        "What time is it?",
        "Tell me a joke about computers",
        "How are you feeling today?",
        "What can you help me with?",
        "Can you explain artificial intelligence?"
    ];
    
    for (const query of testQueries) {
        console.log(`\n🔍 Query: "${query}"`);
        
        try {
            const response = await llm.processQuery(query, false);
            console.log(`✅ AI Response: "${response.text}"`);
            console.log(`   Source: ${response.source}`);
            console.log(`   Model: ${response.model}`);
            console.log(`   Tokens: ${response.tokensUsed}`);
            console.log(`   Speed: ${response.inferenceStats?.tokensPerSecond?.toFixed(1)} tok/s`);
        } catch (error) {
            console.log(`❌ Error: ${error.message}`);
        }
        
        // Small delay between queries
        await new Promise(resolve => setTimeout(resolve, 500));
    }
}

testRealAI().catch(console.error);
