/**
 * EthervoxAI Module Tests
 * 
 * Basic tests to validate module functionality
 */

import { multilingualRuntime } from '../modules/multilingualRuntime';
import { localLLMStack } from '../modules/localLLMStack';
import { privacyDashboard } from '../modules/privacyDashboard';

describe('EthervoxAI Modules', () => {
  describe('Multilingual Runtime', () => {
    test('should have supported languages', () => {
      const languages = multilingualRuntime.getSupportedLanguages();
      expect(languages).toContain('en-US');
      expect(languages).toContain('es-419');
      expect(languages).toContain('zh-CN');
      expect(languages.length).toBeGreaterThan(0);
    });

    test('should have language profiles', () => {
      const profiles = multilingualRuntime.getLanguageProfiles();
      expect(profiles.length).toBeGreaterThan(0);
      
      const defaultProfile = profiles.find(p => p.isDefault);
      expect(defaultProfile).toBeDefined();
    });

    test('should detect language from audio', async () => {
      const audioBuffer = new ArrayBuffer(1024);
      const result = await multilingualRuntime.detectLanguage(audioBuffer);
      
      expect(result.detectedLanguage).toBeDefined();
      expect(result.confidence).toBeGreaterThan(0);
      expect(result.confidence).toBeLessThanOrEqual(1);
      expect(result.alternativeLanguages).toBeInstanceOf(Array);
    });
  });

  describe('Local LLM Stack', () => {
    test('should have available models', () => {
      const models = localLLMStack.getLocalModels();
      expect(models.length).toBeGreaterThan(0);
      
      const mistralModel = models.find(m => m.name === 'mistral-lite');
      expect(mistralModel).toBeDefined();
      expect(mistralModel?.type).toBe('local');
    });

    test('should process queries', async () => {
      const result = await localLLMStack.processQuery('Hello, how are you?');
      
      expect(result.text).toBeDefined();
      expect(result.confidence).toBeGreaterThan(0);
      expect(result.source).toMatch(/^(local|external)$/);
      expect(result.tokensUsed).toBeGreaterThan(0);
    });

    test('should parse intents', async () => {
      const weatherIntent = await localLLMStack.parseIntent('What is the weather like today?');
      expect(weatherIntent.intent).toBe('weather');
      expect(weatherIntent.confidence).toBeGreaterThan(0.8);

      const timerIntent = await localLLMStack.parseIntent('Set a timer for 5 minutes');
      expect(timerIntent.intent).toBe('timer');
      expect(timerIntent.entities.duration).toBeDefined();
    });
  });

  describe('Privacy Dashboard', () => {
    test('should have default privacy settings', () => {
      const settings = privacyDashboard.getPrivacySettings();
      
      expect(settings.cloudAccessEnabled).toBeDefined();
      expect(settings.encryptionEnabled).toBe(true);
      expect(settings.auditLoggingEnabled).toBe(true);
      expect(settings.dataRetentionDays).toBeGreaterThan(0);
    });

    test('should control cloud access', () => {
      expect(privacyDashboard.isCloudAccessAllowed('test-service')).toBe(false);
      
      privacyDashboard.updatePrivacySettings({ cloudAccessEnabled: true });
      expect(privacyDashboard.isCloudAccessAllowed('test-service')).toBe(true);
      
      // Reset to default
      privacyDashboard.updatePrivacySettings({ cloudAccessEnabled: false });
    });

    test('should log cloud queries', () => {
      const queryId = privacyDashboard.logCloudQuery(
        'test query',
        'test-service',
        true,
        ['query', 'response'],
        true
      );
      
      expect(queryId).toBeDefined();
      
      const queries = privacyDashboard.getCloudQueryHistory(1);
      expect(queries.length).toBeGreaterThan(0);
      expect(queries[0].id).toBe(queryId);
    });

    test('should export user data', () => {
      const userData = privacyDashboard.exportUserData();
      
      expect(userData.settings).toBeDefined();
      expect(userData.cloudQueries).toBeInstanceOf(Array);
      expect(userData.auditLog).toBeInstanceOf(Array);
      expect(userData.devices).toBeInstanceOf(Array);
    });

    test('should manage device statuses', () => {
      const devices = privacyDashboard.getDeviceStatuses();
      expect(devices.length).toBeGreaterThan(0);
      
      const device = devices[0];
      expect(device.deviceId).toBeDefined();
      expect(device.name).toBeDefined();
      expect(device.privacyMode).toMatch(/^(strict|balanced|permissive)$/);
    });
  });

  describe('Integration Tests', () => {
    test('should work together in pipeline', async () => {
      // Test the integration between modules
      const testQuery = 'What is the weather like today?';
      
      // 1. Process with LLM
      const llmResponse = await localLLMStack.processQuery(testQuery);
      expect(llmResponse.text).toBeDefined();
      
      // 2. Log if it was a cloud query
      if (llmResponse.source === 'external') {
        const queryId = privacyDashboard.logCloudQuery(
          testQuery,
          llmResponse.model,
          true,
          ['query', 'response'],
          true
        );
        expect(queryId).toBeDefined();
      }
      
      // 3. Check privacy settings allow this
      const settings = privacyDashboard.getPrivacySettings();
      if (llmResponse.source === 'external') {
        expect(settings.cloudAccessEnabled).toBe(true);
      }
    });

    test('should respect privacy settings', async () => {
      // Ensure cloud access is disabled
      privacyDashboard.updatePrivacySettings({ cloudAccessEnabled: false });
      
      const result = await localLLMStack.processQuery('Complex philosophical question about the meaning of life');
      
      // Should use local model even for complex queries when cloud is disabled
      expect(result.source).toBe('local');
    });
  });
});
