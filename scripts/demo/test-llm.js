#!/usr/bin/env node

/**
 * Test Local LLM Stack
 */

const path = require('path');

// Import the built modules
try {
    const { LocalLLMStack } = require(path.join(__dirname, '..', '..', 'dist', 'modules', 'localLLMStack'));
    
    console.log('🧠 Testing Local LLM Stack...');
    console.log('LocalLLMStack class loaded:', !!LocalLLMStack);
    
    if (LocalLLMStack) {
        // Create an instance
        const localLLMStack = new LocalLLMStack();
        console.log('localLLMStack instance created:', !!localLLMStack);
        console.log('localLLMStack methods:', Object.getOwnPropertyNames(localLLMStack.__proto__));
        
        // Test the processQuery method
        testLLM(localLLMStack);
    } else {
        console.log('❌ LocalLLMStack not available');
    }
    
} catch (error) {
    console.error('❌ Error loading LocalLLMStack:', error.message);
    console.error('Stack:', error.stack);
}

async function testLLM(localLLMStack) {
    try {
        console.log('\n📝 Testing LLM query processing...');
        
        // Initialize the LLM stack first
        console.log('🔧 Initializing LLM stack...');
        await localLLMStack.initialize();
        console.log('✅ LLM stack initialized');
        
        const testQueries = [
            "What time is it?",
            "Tell me a joke",
            "How are you?"
        ];
        
        for (const query of testQueries) {
            console.log(`\n🔍 Testing: "${query}"`);
            
            try {
                const response = await localLLMStack.processQuery(query, false);
                console.log(`✅ Response:`, response);
            } catch (error) {
                console.log(`❌ Error processing "${query}":`, error.message);
            }
        }
        
    } catch (error) {
        console.error('❌ Test failed:', error.message);
    }
}
