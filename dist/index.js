"use strict";
/**
 * EthervoxAI - Private Multilingual Voice Assistant
 *
 * Main entry point that integrates all modules and provides
 * a unified interface for the voice assistant functionality.
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.ethervoxai = exports.privacyDashboard = exports.localLLMStack = exports.multilingualRuntime = exports.EthervoxAI = void 0;
const multilingualRuntime_1 = require("./modules/multilingualRuntime");
const localLLMStack_1 = require("./modules/localLLMStack");
const privacyDashboard_1 = require("./modules/privacyDashboard");
class EthervoxAI {
    constructor(config = {}) {
        this.isInitialized = false;
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
    async initialize() {
        if (this.isInitialized) {
            return;
        }
        console.log('🎤 Initializing EthervoxAI...');
        // Set up multilingual runtime
        if (this.config.defaultLanguage) {
            multilingualRuntime_1.multilingualRuntime.setFallbackLanguage(this.config.defaultLanguage);
        }
        // Configure LLM stack
        if (this.config.preferredModel) {
            localLLMStack_1.localLLMStack.setCurrentModel(this.config.preferredModel);
        }
        // Configure privacy settings
        privacyDashboard_1.privacyDashboard.updatePrivacySettings({
            cloudAccessEnabled: this.config.enableCloudFallback || false,
            cloudAccessPerQuery: this.config.privacyMode === 'strict'
        });
        this.isInitialized = true;
        console.log('✅ EthervoxAI initialized successfully');
    }
    /**
     * Process voice input through the complete pipeline
     */
    async processVoiceInput(audioBuffer) {
        if (!this.isInitialized) {
            throw new Error('EthervoxAI not initialized. Call initialize() first.');
        }
        // Step 1: Process speech input with language detection
        const speechResult = await multilingualRuntime_1.multilingualRuntime.processSpeechInput(audioBuffer);
        // Step 2: Generate response using LLM stack
        const llmResponse = await localLLMStack_1.localLLMStack.processQuery(speechResult.transcription, this.config.enableCloudFallback);
        // Step 3: Generate speech output in detected language
        const responseAudio = await multilingualRuntime_1.multilingualRuntime.generateSpeechOutput(llmResponse.text, speechResult.detectedLanguage);
        // Log interaction for privacy dashboard
        if (llmResponse.source === 'external') {
            privacyDashboard_1.privacyDashboard.logCloudQuery(speechResult.transcription, llmResponse.model, true, // encrypted
            ['transcription', 'response'], this.config.enableCloudFallback || false);
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
    async processTextInput(text, language) {
        if (!this.isInitialized) {
            throw new Error('EthervoxAI not initialized. Call initialize() first.');
        }
        const llmResponse = await localLLMStack_1.localLLMStack.processQuery(text, this.config.enableCloudFallback);
        if (llmResponse.source === 'external') {
            privacyDashboard_1.privacyDashboard.logCloudQuery(text, llmResponse.model, true, ['text', 'response'], this.config.enableCloudFallback || false);
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
    getStatus() {
        return {
            isInitialized: this.isInitialized,
            config: this.config,
            supportedLanguages: multilingualRuntime_1.multilingualRuntime.getSupportedLanguages(),
            availableModels: localLLMStack_1.localLLMStack.getLocalModels().map(m => m.name),
            privacySettings: privacyDashboard_1.privacyDashboard.getPrivacySettings()
        };
    }
    /**
     * Update configuration
     */
    updateConfig(updates) {
        this.config = { ...this.config, ...updates };
        if (updates.defaultLanguage) {
            multilingualRuntime_1.multilingualRuntime.setFallbackLanguage(updates.defaultLanguage);
        }
        if (updates.preferredModel) {
            localLLMStack_1.localLLMStack.setCurrentModel(updates.preferredModel);
        }
        if (updates.enableCloudFallback !== undefined || updates.privacyMode) {
            privacyDashboard_1.privacyDashboard.updatePrivacySettings({
                cloudAccessEnabled: updates.enableCloudFallback ?? this.config.enableCloudFallback,
                cloudAccessPerQuery: (updates.privacyMode ?? this.config.privacyMode) === 'strict'
            });
        }
    }
    /**
     * Shutdown the system gracefully
     */
    async shutdown() {
        console.log('🔄 Shutting down EthervoxAI...');
        // Export user data before shutdown (if requested)
        const userData = privacyDashboard_1.privacyDashboard.exportUserData();
        console.log(`📊 Exported ${userData.cloudQueries.length} cloud queries and ${userData.auditLog.length} audit entries`);
        this.isInitialized = false;
        console.log('✅ EthervoxAI shutdown complete');
    }
}
exports.EthervoxAI = EthervoxAI;
// Export modules for direct access
var multilingualRuntime_2 = require("./modules/multilingualRuntime");
Object.defineProperty(exports, "multilingualRuntime", { enumerable: true, get: function () { return multilingualRuntime_2.multilingualRuntime; } });
var localLLMStack_2 = require("./modules/localLLMStack");
Object.defineProperty(exports, "localLLMStack", { enumerable: true, get: function () { return localLLMStack_2.localLLMStack; } });
var privacyDashboard_2 = require("./modules/privacyDashboard");
Object.defineProperty(exports, "privacyDashboard", { enumerable: true, get: function () { return privacyDashboard_2.privacyDashboard; } });
// Export dashboard components (optional - requires React dependencies)
// Uncomment these lines if you have React installed:
// export { DashboardWeb } from './ui/dashboard/DashboardWeb';
// export { DashboardMobile } from './ui/dashboard/DashboardMobile';
// Create default instance
exports.ethervoxai = new EthervoxAI();
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
//# sourceMappingURL=index.js.map