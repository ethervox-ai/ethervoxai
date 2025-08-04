"use strict";
/**
 * 🌐 Multilingual Runtime Module
 *
 * Enables EthervoxAI to detect, interpret, and respond to spoken input
 * in multiple languages seamlessly without manual switching.
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.multilingualRuntime = exports.MultilingualRuntime = void 0;
class MultilingualRuntime {
    constructor() {
        this.supportedLanguages = ['en-US', 'en-GB', 'es-419', 'zh-CN'];
        this.languageProfiles = new Map();
        this.sttEngines = new Map();
        this.ttsEngines = new Map();
        this.fallbackLanguage = 'en-US';
        this.initializeDefaultProfiles();
    }
    /**
     * Initialize default language profiles for MVP languages
     */
    initializeDefaultProfiles() {
        const defaultProfiles = [
            { code: 'en-US', name: 'English (US)', isDefault: true, customVocabulary: [] },
            { code: 'en-GB', name: 'English (UK)', isDefault: false, customVocabulary: [] },
            { code: 'es-419', name: 'Spanish (LatAm)', isDefault: false, customVocabulary: [] },
            { code: 'zh-CN', name: 'Mandarin (Simplified)', isDefault: false, customVocabulary: [] }
        ];
        defaultProfiles.forEach(profile => {
            this.languageProfiles.set(profile.code, profile);
        });
    }
    /**
     * Detect language from audio input
     */
    async detectLanguage(audioBuffer) {
        // Placeholder for actual language detection logic
        // In production, this would use ML models or cloud services
        return {
            detectedLanguage: this.fallbackLanguage,
            confidence: 0.95,
            alternativeLanguages: [
                { language: 'es-419', confidence: 0.75 },
                { language: 'zh-CN', confidence: 0.65 }
            ]
        };
    }
    /**
     * Process speech input with automatic language detection
     */
    async processSpeechInput(audioBuffer) {
        const detection = await this.detectLanguage(audioBuffer);
        const sttEngine = this.sttEngines.get(detection.detectedLanguage);
        if (!sttEngine) {
            throw new Error(`No STT engine available for language: ${detection.detectedLanguage}`);
        }
        const transcription = await sttEngine.transcribe(audioBuffer);
        return {
            transcription,
            detectedLanguage: detection.detectedLanguage,
            confidence: detection.confidence
        };
    }
    /**
     * Generate speech output in specified language
     */
    async generateSpeechOutput(text, language) {
        const ttsEngine = this.ttsEngines.get(language);
        if (!ttsEngine) {
            // Fallback to default language
            const fallbackEngine = this.ttsEngines.get(this.fallbackLanguage);
            if (!fallbackEngine) {
                throw new Error('No TTS engine available');
            }
            return fallbackEngine.synthesize(text);
        }
        return ttsEngine.synthesize(text);
    }
    /**
     * Add or update language profile
     */
    updateLanguageProfile(profile) {
        this.languageProfiles.set(profile.code, profile);
    }
    /**
     * Get all configured language profiles
     */
    getLanguageProfiles() {
        return Array.from(this.languageProfiles.values());
    }
    /**
     * Set fallback language
     */
    setFallbackLanguage(languageCode) {
        if (this.supportedLanguages.includes(languageCode)) {
            this.fallbackLanguage = languageCode;
        }
    }
    /**
     * Register STT engine for a language
     */
    registerSTTEngine(language, engine) {
        this.sttEngines.set(language, engine);
    }
    /**
     * Register TTS engine for a language
     */
    registerTTSEngine(language, engine) {
        this.ttsEngines.set(language, engine);
    }
    /**
     * Get supported languages
     */
    getSupportedLanguages() {
        return [...this.supportedLanguages];
    }
}
exports.MultilingualRuntime = MultilingualRuntime;
exports.multilingualRuntime = new MultilingualRuntime();
//# sourceMappingURL=multilingualRuntime.js.map