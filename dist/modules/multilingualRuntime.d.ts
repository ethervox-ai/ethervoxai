/**
 * 🌐 Multilingual Runtime Module
 *
 * Enables EthervoxAI to detect, interpret, and respond to spoken input
 * in multiple languages seamlessly without manual switching.
 */
export interface LanguageProfile {
    code: string;
    name: string;
    dialect?: string;
    isDefault: boolean;
    customVocabulary: string[];
}
export interface STTEngine {
    language: string;
    confidence: number;
    transcribe(audioBuffer: ArrayBuffer): Promise<string>;
}
export interface TTSEngine {
    language: string;
    voice: string;
    synthesize(text: string): Promise<ArrayBuffer>;
}
export interface LanguageDetectionResult {
    detectedLanguage: string;
    confidence: number;
    alternativeLanguages: {
        language: string;
        confidence: number;
    }[];
}
export declare class MultilingualRuntime {
    private supportedLanguages;
    private languageProfiles;
    private sttEngines;
    private ttsEngines;
    private fallbackLanguage;
    constructor();
    /**
     * Initialize default language profiles for MVP languages
     */
    private initializeDefaultProfiles;
    /**
     * Detect language from audio input
     */
    detectLanguage(audioBuffer: ArrayBuffer): Promise<LanguageDetectionResult>;
    /**
     * Process speech input with automatic language detection
     */
    processSpeechInput(audioBuffer: ArrayBuffer): Promise<{
        transcription: string;
        detectedLanguage: string;
        confidence: number;
    }>;
    /**
     * Generate speech output in specified language
     */
    generateSpeechOutput(text: string, language: string): Promise<ArrayBuffer>;
    /**
     * Add or update language profile
     */
    updateLanguageProfile(profile: LanguageProfile): void;
    /**
     * Get all configured language profiles
     */
    getLanguageProfiles(): LanguageProfile[];
    /**
     * Set fallback language
     */
    setFallbackLanguage(languageCode: string): void;
    /**
     * Register STT engine for a language
     */
    registerSTTEngine(language: string, engine: STTEngine): void;
    /**
     * Register TTS engine for a language
     */
    registerTTSEngine(language: string, engine: TTSEngine): void;
    /**
     * Get supported languages
     */
    getSupportedLanguages(): string[];
}
export declare const multilingualRuntime: MultilingualRuntime;
//# sourceMappingURL=multilingualRuntime.d.ts.map