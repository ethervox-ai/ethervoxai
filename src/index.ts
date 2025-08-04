/**
 * EthervoxAI - Private Multilingual Voice Assistant
 * 
 * Main entry point that integrates all modules and provides
 * a unified interface for the voice assistant functionality.
 */

import { multilingualRuntime } from './modules/multilingualRuntime';
import { localLLMStack } from './modules/localLLMStack';
import { privacyDashboard } from './modules/privacyDashboard';

export interface EthervoxAIConfig {
  defaultLanguage?: string;
  preferredModel?: string;
  privacyMode?: 'strict' | 'balanced' | 'permissive';
  enableCloudFallback?: boolean;
}

export class EthervoxAI {
  private config: EthervoxAIConfig;
  private isInitialized = false;

  constructor(config: EthervoxAIConfig = {}) {
    this.config = {
      defaultLanguage: 'en-US',
      preferredModel: 'mistral-lite',
      privacyMode: 'balanced',
      enableCloudFallback: false,
      ...config
    };
  }

  /**
   * Initialize the EthervoxAI system
   */
  async initialize(): Promise<void> {
    if (this.isInitialized) {
      return;
    }

    console.log('🎤 Initializing EthervoxAI...');

    // Set up multilingual runtime
    if (this.config.defaultLanguage) {
      multilingualRuntime.setFallbackLanguage(this.config.defaultLanguage);
    }

    // Configure LLM stack
    if (this.config.preferredModel) {
      localLLMStack.setCurrentModel(this.config.preferredModel);
    }

    // Configure privacy settings
    privacyDashboard.updatePrivacySettings({
      cloudAccessEnabled: this.config.enableCloudFallback || false,
      cloudAccessPerQuery: this.config.privacyMode === 'strict'
    });

    this.isInitialized = true;
    console.log('✅ EthervoxAI initialized successfully');
  }

  /**
   * Process voice input through the complete pipeline
   */
  async processVoiceInput(audioBuffer: ArrayBuffer): Promise<{
    transcription: string;
    detectedLanguage: string;
    response: string;
    confidence: number;
    source: 'local' | 'external';
  }> {
    if (!this.isInitialized) {
      throw new Error('EthervoxAI not initialized. Call initialize() first.');
    }

    // Step 1: Process speech input with language detection
    const speechResult = await multilingualRuntime.processSpeechInput(audioBuffer);
    
    // Step 2: Generate response using LLM stack
    const llmResponse = await localLLMStack.processQuery(
      speechResult.transcription,
      this.config.enableCloudFallback
    );

    // Step 3: Generate speech output in detected language
    const responseAudio = await multilingualRuntime.generateSpeechOutput(
      llmResponse.text,
      speechResult.detectedLanguage
    );

    // Log interaction for privacy dashboard
    if (llmResponse.source === 'external') {
      privacyDashboard.logCloudQuery(
        speechResult.transcription,
        llmResponse.model,
        true, // encrypted
        ['transcription', 'response'],
        this.config.enableCloudFallback || false
      );
    }

    return {
      transcription: speechResult.transcription,
      detectedLanguage: speechResult.detectedLanguage,
      response: llmResponse.text,
      confidence: llmResponse.confidence,
      source: llmResponse.source
    };
  }

  /**
   * Process text input (for testing/debugging)
   */
  async processTextInput(text: string, language?: string): Promise<{
    response: string;
    confidence: number;
    source: 'local' | 'external';
  }> {
    if (!this.isInitialized) {
      throw new Error('EthervoxAI not initialized. Call initialize() first.');
    }

    const llmResponse = await localLLMStack.processQuery(
      text,
      this.config.enableCloudFallback
    );

    if (llmResponse.source === 'external') {
      privacyDashboard.logCloudQuery(
        text,
        llmResponse.model,
        true,
        ['text', 'response'],
        this.config.enableCloudFallback || false
      );
    }

    return {
      response: llmResponse.text,
      confidence: llmResponse.confidence,
      source: llmResponse.source
    };
  }

  /**
   * Get system status
   */
  getStatus(): {
    isInitialized: boolean;
    config: EthervoxAIConfig;
    supportedLanguages: string[];
    availableModels: string[];
    privacySettings: any;
  } {
    return {
      isInitialized: this.isInitialized,
      config: this.config,
      supportedLanguages: multilingualRuntime.getSupportedLanguages(),
      availableModels: localLLMStack.getLocalModels().map(m => m.name),
      privacySettings: privacyDashboard.getPrivacySettings()
    };
  }

  /**
   * Update configuration
   */
  updateConfig(updates: Partial<EthervoxAIConfig>): void {
    this.config = { ...this.config, ...updates };

    if (updates.defaultLanguage) {
      multilingualRuntime.setFallbackLanguage(updates.defaultLanguage);
    }

    if (updates.preferredModel) {
      localLLMStack.setCurrentModel(updates.preferredModel);
    }

    if (updates.enableCloudFallback !== undefined || updates.privacyMode) {
      privacyDashboard.updatePrivacySettings({
        cloudAccessEnabled: updates.enableCloudFallback ?? this.config.enableCloudFallback,
        cloudAccessPerQuery: (updates.privacyMode ?? this.config.privacyMode) === 'strict'
      });
    }
  }

  /**
   * Shutdown the system gracefully
   */
  async shutdown(): Promise<void> {
    console.log('🔄 Shutting down EthervoxAI...');
    
    // Export user data before shutdown (if requested)
    const userData = privacyDashboard.exportUserData();
    console.log(`📊 Exported ${userData.cloudQueries.length} cloud queries and ${userData.auditLog.length} audit entries`);
    
    this.isInitialized = false;
    console.log('✅ EthervoxAI shutdown complete');
  }
}

// Export modules for direct access
export { multilingualRuntime } from './modules/multilingualRuntime';
export { localLLMStack } from './modules/localLLMStack';
export { privacyDashboard } from './modules/privacyDashboard';

// Export types
export type { LanguageProfile, STTEngine, TTSEngine, LanguageDetectionResult } from './modules/multilingualRuntime';
export type { LLMModel, IntentParseResult, LLMResponse, RoutingRule } from './modules/localLLMStack';
export type { CloudQuery, PrivacySettings, DeviceStatus, AuditLogEntry } from './modules/privacyDashboard';

// Export dashboard components (optional - requires React dependencies)
// Uncomment these lines if you have React installed:
// export { DashboardWeb } from './ui/dashboard/DashboardWeb';
// export { DashboardMobile } from './ui/dashboard/DashboardMobile';

// Create default instance
export const ethervoxai = new EthervoxAI();

// Example usage
if (require.main === module) {
  async function demo() {
    console.log('🚀 EthervoxAI Demo');
    
    // Initialize with custom config
    const ai = new EthervoxAI({
      defaultLanguage: 'en-US',
      preferredModel: 'mistral-lite',
      privacyMode: 'balanced',
      enableCloudFallback: false
    });

    await ai.initialize();

    // Test text processing
    console.log('\n💬 Testing text input...');
    const textResult = await ai.processTextInput('What is the weather like today?');
    console.log(`Response: ${textResult.response}`);
    console.log(`Confidence: ${Math.round(textResult.confidence * 100)}%`);
    console.log(`Source: ${textResult.source}`);

    // Show system status
    console.log('\n📊 System Status:');
    const status = ai.getStatus();
    console.log(`Initialized: ${status.isInitialized}`);
    console.log(`Supported Languages: ${status.supportedLanguages.join(', ')}`);
    console.log(`Available Models: ${status.availableModels.join(', ')}`);
    console.log(`Privacy Mode: ${status.config.privacyMode}`);

    await ai.shutdown();
  }

  demo().catch(console.error);
}
