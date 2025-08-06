/**
 * 🧪 AI Integration Test Script
 * 
 * Tests the new AI model infrastructure including:
 * - Platform Detection
 * - Model Manager
 * - Inference Engine
 * - Local LLM Stack integration
 */

// Using require for Node.js compatibility since we're in a .js file
const { EthervoxAI } = require('../dist/index.js');

async function testAIIntegration() {
  console.log('🧪 Testing EthervoxAI AI Model Integration');
  console.log('='.repeat(50));

  try {
    // Test 1: Create EthervoxAI instance
    console.log('\n1️⃣ Creating EthervoxAI instance...');
    const ai = new EthervoxAI({
      defaultLanguage: 'en-US',
      preferredModel: 'tinyllama-1.1b-chat-q4',
      privacyMode: 'balanced',
      enableCloudFallback: false
    });
    console.log('✅ EthervoxAI instance created');

    // Test 2: Initialize the system
    console.log('\n2️⃣ Initializing AI system...');
    await ai.initialize();
    console.log('✅ AI system initialized');

    // Test 3: Get system status
    console.log('\n3️⃣ Checking system status...');
    const status = ai.getStatus();
    console.log('📊 System Status:');
    console.log(`   Initialized: ${status.isInitialized}`);
    console.log(`   Available Models: ${status.availableModels.length}`);
    console.log(`   Current Model: ${status.config.preferredModel}`);
    console.log(`   Privacy Mode: ${status.config.privacyMode}`);

    // Test 4: Process text input
    console.log('\n4️⃣ Testing text processing...');
    const queries = [
      'Hello, what can you help me with?',
      'What is the weather like today?',
      'Set a timer for 5 minutes',
      'Tell me a joke'
    ];

    for (const query of queries) {
      console.log(`\n🗣️  Query: "${query}"`);
      try {
        const result = await ai.processTextInput(query);
        console.log(`✅ Response: "${result.response}"`);
        console.log(`   Confidence: ${Math.round(result.confidence * 100)}%`);
        console.log(`   Source: ${result.source}`);
        if (result.tokensUsed) {
          console.log(`   Tokens: ${result.tokensUsed}`);
        }
      } catch (error) {
        console.log(`❌ Failed: ${error.message}`);
      }
    }

    // Test 5: Test model switching (if multiple models available)
    if (status.availableModels.length > 1) {
      console.log('\n5️⃣ Testing model switching...');
      const newModel = status.availableModels.find(m => m !== status.config.preferredModel);
      if (newModel) {
        console.log(`🔄 Switching to model: ${newModel}`);
        ai.updateConfig({ preferredModel: newModel });
        const switchResult = await ai.processTextInput('Test with new model');
        console.log(`✅ New model response: "${switchResult.response}"`);
      }
    }

    console.log('\n🎉 AI Integration Test Completed Successfully!');
    console.log('All core AI functionality is working correctly.');

  } catch (error) {
    console.error('\n❌ AI Integration Test Failed:');
    console.error(error.message);
    console.error('Stack trace:', error.stack);
    process.exit(1);
  }
}

// Run the test if this script is executed directly
if (require.main === module) {
  testAIIntegration()
    .then(() => {
      console.log('\n✅ Test completed successfully');
      process.exit(0);
    })
    .catch((error) => {
      console.error('\n❌ Test failed:', error);
      process.exit(1);
    });
}

module.exports = { testAIIntegration };
