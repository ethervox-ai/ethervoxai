/**
 * EthervoxAI - Private Multilingual Voice Assistant
 *
 * Main entry point that integrates all modules and provides
 * a unified interface for the voice assistant functionality.
 */
export interface EthervoxAIConfig {
    defaultLanguage?: string;
    preferredModel?: string;
    privacyMode?: 'strict' | 'balanced' | 'permissive';
    enableCloudFallback?: boolean;
}
export declare class EthervoxAI {
    private config;
    private isInitialized;
    constructor(config?: EthervoxAIConfig);
    /**
     * Initialize the EthervoxAI system
     */
    initialize(): Promise<void>;
    /**
     * Process voice input through the complete pipeline
     */
    processVoiceInput(audioBuffer: ArrayBuffer): Promise<{
        transcription: string;
        detectedLanguage: string;
        response: string;
        confidence: number;
        source: 'local' | 'external';
    }>;
    /**
     * Process text input (for testing/debugging)
     */
    processTextInput(text: string, language?: string): Promise<{
        response: string;
        confidence: number;
        source: 'local' | 'external';
    }>;
    /**
     * Get system status
     */
    getStatus(): {
        isInitialized: boolean;
        config: EthervoxAIConfig;
        supportedLanguages: string[];
        availableModels: string[];
        privacySettings: any;
    };
    /**
     * Update configuration
     */
    updateConfig(updates: Partial<EthervoxAIConfig>): void;
    /**
     * Shutdown the system gracefully
     */
    shutdown(): Promise<void>;
}
export { multilingualRuntime } from './modules/multilingualRuntime';
export { localLLMStack } from './modules/localLLMStack';
export { privacyDashboard } from './modules/privacyDashboard';
export type { LanguageProfile, STTEngine, TTSEngine, LanguageDetectionResult } from './modules/multilingualRuntime';
export type { LLMModel, IntentParseResult, LLMResponse, RoutingRule } from './modules/localLLMStack';
export type { CloudQuery, PrivacySettings, DeviceStatus, AuditLogEntry } from './modules/privacyDashboard';
export declare const ethervoxai: EthervoxAI;
//# sourceMappingURL=index.d.ts.map