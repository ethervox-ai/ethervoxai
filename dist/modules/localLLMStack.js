"use strict";
/**
 * 🧠 Local LLM Stack Module
 *
 * Modular local LLM stack for intent parsing, response generation,
 * and optional routing to external models while preserving privacy.
 *
 * Updated to integrate with ModelManager and InferenceEngine
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.localLLMStack = exports.LocalLLMStack = void 0;
const modelManager_1 = require("./modelManager");
const inferenceEngine_1 = require("./inferenceEngine");
const platformDetector_1 = require("./platformDetector");
class LocalLLMStack {
    constructor() {
        this.localModels = new Map();
        this.externalPlugins = new Map();
        this.routingRules = [];
        this.currentModel = 'mistral-7b-instruct-v0.1-q4'; // Updated to use real model name
        this.isInitialized = false;
        this.initializeDefaultModels();
        this.initializeDefaultRoutingRules();
    }
    /**
     * Initialize the Local LLM Stack with real AI models
     */
    async initialize() {
        if (this.isInitialized)
            return;
        console.log('🚀 Initializing Local LLM Stack...');
        try {
            // Get system capabilities and recommended models
            const capabilities = await platformDetector_1.platformDetector.getCapabilities();
            const recommendedModels = await modelManager_1.modelManager.getRecommendedModels();
            console.log(`📊 System Performance Tier: ${capabilities.performanceTier}`);
            console.log(`💾 Available Memory: ${capabilities.availableMemory}MB`);
            console.log(`🧠 Recommended Models: ${recommendedModels.map(m => m.name).join(', ')}`);
            // Use the best recommended model as default
            if (recommendedModels.length > 0) {
                this.currentModel = recommendedModels[0].name;
                console.log(`🎯 Selected default model: ${this.currentModel}`);
            }
            // Initialize inference engine with selected model
            await inferenceEngine_1.inferenceEngine.initialize(this.currentModel);
            // Update local models registry with real models
            await this.updateLocalModelsFromCatalog();
            console.log('✅ Local LLM Stack initialized successfully');
            this.isInitialized = true;
        }
        catch (error) {
            console.error('❌ Failed to initialize Local LLM Stack:', error);
            throw error;
        }
    }
    /**
     * Initialize default local models for MVP
     */
    initializeDefaultModels() {
        const defaultModels = [
            {
                name: 'mistral-lite',
                type: 'local',
                capabilities: ['conversation', 'qa', 'summarization'],
                memoryLimit: 2048,
                tokenLimit: 4096
            },
            {
                name: 'tinyllama',
                type: 'local',
                capabilities: ['simple-qa', 'classification'],
                memoryLimit: 1024,
                tokenLimit: 2048
            }
        ];
        defaultModels.forEach(model => {
            this.localModels.set(model.name, model);
        });
    }
    /**
     * Initialize default routing rules
     */
    initializeDefaultRoutingRules() {
        this.routingRules = [
            {
                condition: 'confidence < 0.7',
                action: 'external',
                modelPreference: 'gpt-4'
            },
            {
                condition: 'intent = unknown',
                action: 'fallback'
            },
            {
                condition: 'default',
                action: 'local'
            }
        ];
    }
    /**
     * Parse intent from text input using hybrid approach
     */
    async parseIntent(text) {
        // Placeholder for actual intent parsing logic
        // Would use rule-based system + ML model in production
        const commonIntents = [
            'weather', 'timer', 'music', 'smart_home', 'conversation', 'unknown'
        ];
        // Simple keyword-based classification for MVP
        let detectedIntent = 'unknown';
        let confidence = 0.5;
        if (text.toLowerCase().includes('weather')) {
            detectedIntent = 'weather';
            confidence = 0.9;
        }
        else if (text.toLowerCase().includes('timer') || text.toLowerCase().includes('alarm')) {
            detectedIntent = 'timer';
            confidence = 0.85;
        }
        else if (text.toLowerCase().includes('music') || text.toLowerCase().includes('play')) {
            detectedIntent = 'music';
            confidence = 0.8;
        }
        else if (text.toLowerCase().includes('light') || text.toLowerCase().includes('temperature')) {
            detectedIntent = 'smart_home';
            confidence = 0.75;
        }
        else if (text.length > 50) {
            detectedIntent = 'conversation';
            confidence = 0.6;
        }
        return {
            intent: detectedIntent,
            confidence,
            entities: this.extractEntities(text, detectedIntent),
            requiresExternalLLM: confidence < 0.7 || detectedIntent === 'unknown'
        };
    }
    /**
     * Extract entities from text based on intent
     */
    extractEntities(text, intent) {
        const entities = {};
        switch (intent) {
            case 'weather':
                // Extract location if mentioned
                const locationMatch = text.match(/in\s+([A-Za-z\s]+)/);
                if (locationMatch) {
                    entities.location = locationMatch[1].trim();
                }
                break;
            case 'timer':
                // Extract duration
                const durationMatch = text.match(/(\d+)\s*(minute|hour|second)/);
                if (durationMatch) {
                    entities.duration = {
                        value: parseInt(durationMatch[1]),
                        unit: durationMatch[2]
                    };
                }
                break;
            case 'music':
                // Extract song/artist
                const songMatch = text.match(/play\s+(.+)/i);
                if (songMatch) {
                    entities.query = songMatch[1].trim();
                }
                break;
        }
        return entities;
    }
    /**
     * Generate response using local LLM
     */
    async generateLocalResponse(text, intent) {
        if (!this.isInitialized) {
            await this.initialize();
        }
        const model = this.localModels.get(this.currentModel);
        if (!model) {
            throw new Error(`Local model ${this.currentModel} not found`);
        }
        try {
            console.log(`🧠 Generating local response for: "${text}"`);
            // Use the real inference engine for generation
            const inferenceResponse = await inferenceEngine_1.inferenceEngine.complete(text, {
                temperature: 0.7,
                contextLength: 2048
            });
            return {
                text: inferenceResponse.text,
                confidence: intent.confidence,
                source: 'local',
                model: this.currentModel,
                tokensUsed: inferenceResponse.tokensGenerated,
                inferenceStats: {
                    tokensPerSecond: inferenceResponse.tokensPerSecond,
                    totalTime: inferenceResponse.timings.totalTime,
                    promptTokens: inferenceResponse.promptTokens
                }
            };
        }
        catch (error) {
            console.warn('❌ Real inference failed, falling back to demo responses:', error);
            // Fallback to demo responses if real inference fails
            return this.generateFallbackResponse(text, intent);
        }
    }
    /**
     * Generate fallback response when real inference is unavailable
     */
    generateFallbackResponse(text, intent) {
        let responseText = '';
        let tokensUsed = 0;
        switch (intent.intent) {
            case 'greeting':
                responseText = 'Hello! I\'m EthervoxAI, your privacy-focused voice assistant running locally on your device.';
                tokensUsed = 18;
                break;
            case 'question':
                responseText = 'I\'m here to help answer your questions while keeping your data private and secure.';
                tokensUsed = 16;
                break;
            case 'weather':
                responseText = `I'll help you check the weather${intent.entities.location ? ` in ${intent.entities.location}` : ''}.`;
                tokensUsed = 15;
                break;
            case 'timer':
                if (intent.entities.duration) {
                    responseText = `Setting a timer for ${intent.entities.duration.value} ${intent.entities.duration.unit}s.`;
                }
                else {
                    responseText = 'How long would you like me to set the timer for?';
                }
                tokensUsed = 12;
                break;
            case 'music':
                responseText = intent.entities.query
                    ? `Playing ${intent.entities.query}.`
                    : 'What would you like me to play?';
                tokensUsed = 10;
                break;
            case 'smart_home':
                responseText = 'I can help you control your smart home devices.';
                tokensUsed = 11;
                break;
            default:
                responseText = 'I understand you want to have a conversation. How can I help you today?';
                tokensUsed = 16;
        }
        return {
            text: responseText,
            confidence: intent.confidence,
            source: 'local',
            model: this.currentModel,
            tokensUsed
        };
    }
    /**
     * Route query to external LLM if needed
     */
    async routeToExternalLLM(text, intent, userConsent) {
        if (!userConsent) {
            return null;
        }
        // Determine routing based on rules
        const shouldRoute = this.shouldRouteExternal(intent);
        if (!shouldRoute) {
            return null;
        }
        // Placeholder for external LLM integration
        // In production, this would call OpenAI, Anthropic, etc.
        return {
            text: 'This response would come from an external LLM service.',
            confidence: 0.9,
            source: 'external',
            model: 'gpt-4',
            tokensUsed: 25
        };
    }
    /**
     * Determine if query should be routed to external LLM
     */
    shouldRouteExternal(intent) {
        for (const rule of this.routingRules) {
            if (rule.condition === 'default') {
                return rule.action === 'external';
            }
            if (rule.condition.includes('confidence') && intent.confidence < 0.7) {
                return rule.action === 'external';
            }
            if (rule.condition.includes('unknown') && intent.intent === 'unknown') {
                return rule.action === 'fallback' || rule.action === 'external';
            }
        }
        return false;
    }
    /**
     * Process complete query through the LLM stack
     */
    async processQuery(text, userConsent = false) {
        const intent = await this.parseIntent(text);
        // Try local LLM first
        if (!intent.requiresExternalLLM) {
            return this.generateLocalResponse(text, intent);
        }
        // Route to external LLM if needed and consented
        const externalResponse = await this.routeToExternalLLM(text, intent, userConsent);
        if (externalResponse) {
            return externalResponse;
        }
        // Fallback to local LLM
        return this.generateLocalResponse(text, intent);
    }
    /**
     * Register external LLM plugin
     */
    registerExternalPlugin(name, plugin) {
        this.externalPlugins.set(name, plugin);
    }
    /**
     * Update local models registry from ModelManager catalog
     */
    async updateLocalModelsFromCatalog() {
        const catalog = modelManager_1.modelManager.getDefaultModelCatalog();
        const recommendedModels = await modelManager_1.modelManager.getRecommendedModels();
        // Clear existing demo models
        this.localModels.clear();
        // Add real models from catalog
        for (const modelInfo of catalog) {
            const isRecommended = recommendedModels.some(r => r.name === modelInfo.name);
            const llmModel = {
                name: modelInfo.name,
                type: 'local',
                capabilities: modelInfo.tags.includes('chat') ? ['conversation', 'qa'] :
                    modelInfo.tags.includes('instruct') ? ['instruction-following', 'qa', 'summarization'] :
                        modelInfo.tags.includes('code') ? ['code-generation', 'code-review'] :
                            ['general-purpose', 'qa'],
                memoryLimit: modelInfo.requiredMemory,
                tokenLimit: modelInfo.contextLength,
                modelInfo: modelInfo
            };
            this.localModels.set(modelInfo.name, llmModel);
            // Set the first recommended model as current
            if (isRecommended && !this.currentModel.includes('-instruct-v0.1-q4')) {
                this.currentModel = modelInfo.name;
            }
        }
        console.log(`📋 Updated local models registry: ${this.localModels.size} models available`);
        console.log(`🎯 Current model: ${this.currentModel}`);
    }
    /**
     * Update routing rules
     */
    updateRoutingRules(rules) {
        this.routingRules = rules;
    }
    /**
     * Set current local model
     */
    setCurrentModel(modelName) {
        if (this.localModels.has(modelName)) {
            this.currentModel = modelName;
        }
    }
    /**
     * Get available local models
     */
    getLocalModels() {
        return Array.from(this.localModels.values());
    }
    /**
     * Get current model info
     */
    getCurrentModel() {
        return this.localModels.get(this.currentModel);
    }
}
exports.LocalLLMStack = LocalLLMStack;
exports.localLLMStack = new LocalLLMStack();
//# sourceMappingURL=localLLMStack.js.map