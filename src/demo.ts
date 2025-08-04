/**
 * EthervoxAI Demo
 * 
 * This demo shows how the integrated modules work together
 * to provide a complete voice assistant experience.
 */

// Import the main EthervoxAI class and modules
import { EthervoxAI, multilingualRuntime, localLLMStack, privacyDashboard } from './index';

async function runDemo() {
  console.log('🎤 EthervoxAI Integration Demo');
  console.log('=' .repeat(50));

  // 1. Initialize EthervoxAI with custom configuration
  console.log('\n🚀 Initializing EthervoxAI...');
  const ai = new EthervoxAI({
    defaultLanguage: 'en-US',
    preferredModel: 'mistral-lite',
    privacyMode: 'balanced',
    enableCloudFallback: false
  });

  await ai.initialize();
  console.log('✅ EthervoxAI initialized successfully');

  // 2. Test multilingual capabilities
  console.log('\n🌐 Testing Multilingual Runtime...');
  const languages = multilingualRuntime.getSupportedLanguages();
  console.log(`Supported languages: ${languages.join(', ')}`);
  
  const profiles = multilingualRuntime.getLanguageProfiles();
  profiles.forEach(profile => {
    console.log(`  - ${profile.name} (${profile.code})${profile.isDefault ? ' [DEFAULT]' : ''}`);
  });

  // 3. Test Local LLM Stack
  console.log('\n🧠 Testing Local LLM Stack...');
  const models = localLLMStack.getLocalModels();
  console.log(`Available models: ${models.map(m => m.name).join(', ')}`);
  
  const currentModel = localLLMStack.getCurrentModel();
  console.log(`Current model: ${currentModel?.name} (${currentModel?.type})`);

  // Test various queries
  const testQueries = [
    'What is the weather like today?',
    'Set a timer for 5 minutes',
    'Play some relaxing music',
    'Turn on the living room lights',
    'Tell me a joke'
  ];

  console.log('\n💬 Testing Query Processing...');
  for (const query of testQueries) {
    console.log(`\nQuery: "${query}"`);
    try {
      const result = await ai.processTextInput(query);
      console.log(`  Response: ${result.response}`);
      console.log(`  Confidence: ${Math.round(result.confidence * 100)}%`);
      console.log(`  Source: ${result.source}`);
    } catch (error) {
      console.log(`  Error: ${error}`);
    }
  }

  // 4. Test Privacy Dashboard
  console.log('\n🔐 Testing Privacy Dashboard...');
  const privacySettings = privacyDashboard.getPrivacySettings();
  console.log('Privacy Settings:');
  console.log(`  - Cloud access enabled: ${privacySettings.cloudAccessEnabled}`);
  console.log(`  - Consent per query: ${privacySettings.cloudAccessPerQuery}`);
  console.log(`  - Encryption enabled: ${privacySettings.encryptionEnabled}`);
  console.log(`  - Data retention: ${privacySettings.dataRetentionDays} days`);

  const cloudQueries = privacyDashboard.getCloudQueryHistory(5);
  console.log(`\nRecent cloud queries: ${cloudQueries.length}`);

  const deviceStatuses = privacyDashboard.getDeviceStatuses();
  console.log(`\nDevice statuses: ${deviceStatuses.length} devices`);
  deviceStatuses.forEach(device => {
    console.log(`  - ${device.name}: ${device.isOnline ? 'Online' : 'Offline'} (${device.privacyMode} mode)`);
  });

  // 5. Test system status
  console.log('\n📊 System Status...');
  const status = ai.getStatus();
  console.log(`Initialized: ${status.isInitialized}`);
  console.log(`Configuration:`);
  console.log(`  - Default Language: ${status.config.defaultLanguage}`);
  console.log(`  - Preferred Model: ${status.config.preferredModel}`);
  console.log(`  - Privacy Mode: ${status.config.privacyMode}`);
  console.log(`  - Cloud Fallback: ${status.config.enableCloudFallback}`);

  // 6. Test configuration updates
  console.log('\n⚙️ Testing Configuration Updates...');
  ai.updateConfig({
    privacyMode: 'strict',
    enableCloudFallback: false
  });
  
  const updatedStatus = ai.getStatus();
  console.log(`Updated privacy mode: ${updatedStatus.config.privacyMode}`);

  // 7. Export user data (privacy feature)
  console.log('\n📦 Testing Data Export...');
  const userData = privacyDashboard.exportUserData();
  console.log(`Exported data contains:`);
  console.log(`  - Privacy settings: ✓`);
  console.log(`  - Cloud queries: ${userData.cloudQueries.length}`);
  console.log(`  - Audit log entries: ${userData.auditLog.length}`);
  console.log(`  - Device statuses: ${userData.devices.length}`);

  // 8. Simulate voice processing pipeline
  console.log('\n🎙️ Simulating Voice Processing Pipeline...');
  console.log('(Note: This would normally process actual audio data)');
  
  // Simulate audio buffer (normally from microphone)
  const simulatedAudioBuffer = new ArrayBuffer(1024);
  
  try {
    // This would normally process actual audio
    console.log('Step 1: Audio capture → Language detection');
    console.log('Step 2: Speech-to-text conversion');
    console.log('Step 3: Intent parsing and LLM processing');
    console.log('Step 4: Text-to-speech generation');
    console.log('Step 5: Audio output');
    
    // For demo purposes, we'll just process the text
    const textResult = await ai.processTextInput('Hello, what can you help me with today?');
    console.log(`Pipeline result: ${textResult.response}`);
  } catch (error) {
    console.log(`Pipeline error: ${error}`);
  }

  // 9. Cleanup
  console.log('\n🔄 Shutting down...');
  await ai.shutdown();
  
  console.log('\n✅ Demo completed successfully!');
  console.log('\n📋 Summary:');
  console.log('- Multilingual runtime: Language detection and speech processing');
  console.log('- Local LLM stack: Privacy-preserving AI inference');
  console.log('- Privacy dashboard: Complete data control and audit logging');
  console.log('- Integrated UI: Web and mobile dashboard interfaces');
  console.log('\n🚀 EthervoxAI is ready for deployment!');
}

// Run the demo
if (require.main === module) {
  runDemo().catch(console.error);
}

export { runDemo };
