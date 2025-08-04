/**
 * 🧠 Local LLM Stack Module
 *
 * Modular local LLM stack for intent parsing, response generation,
 * and optional routing to external models while preserving privacy.
 */
export interface LLMModel {
    name: string;
    type: 'local' | 'external';
    capabilities: string[];
    memoryLimit: number;
    tokenLimit: number;
}
export interface IntentParseResult {
    intent: string;
    confidence: number;
    entities: Record<string, any>;
    requiresExternalLLM: boolean;
}
export interface LLMResponse {
    text: string;
    confidence: number;
    source: 'local' | 'external';
    model: string;
    tokensUsed: number;
}
export interface RoutingRule {
    condition: string;
    action: 'local' | 'external' | 'fallback';
    modelPreference?: string;
}
export declare class LocalLLMStack {
    private localModels;
    private externalPlugins;
    private routingRules;
    private intentParser;
    private currentModel;
    constructor();
    /**
     * Initialize default local models for MVP
     */
    private initializeDefaultModels;
    /**
     * Initialize default routing rules
     */
    private initializeDefaultRoutingRules;
    /**
     * Parse intent from text input using hybrid approach
     */
    parseIntent(text: string): Promise<IntentParseResult>;
    /**
     * Extract entities from text based on intent
     */
    private extractEntities;
    /**
     * Generate response using local LLM
     */
    generateLocalResponse(text: string, intent: IntentParseResult): Promise<LLMResponse>;
    /**
     * Route query to external LLM if needed
     */
    routeToExternalLLM(text: string, intent: IntentParseResult, userConsent: boolean): Promise<LLMResponse | null>;
    /**
     * Determine if query should be routed to external LLM
     */
    private shouldRouteExternal;
    /**
     * Process complete query through the LLM stack
     */
    processQuery(text: string, userConsent?: boolean): Promise<LLMResponse>;
    /**
     * Register external LLM plugin
     */
    registerExternalPlugin(name: string, plugin: any): void;
    /**
     * Update routing rules
     */
    updateRoutingRules(rules: RoutingRule[]): void;
    /**
     * Set current local model
     */
    setCurrentModel(modelName: string): void;
    /**
     * Get available local models
     */
    getLocalModels(): LLMModel[];
    /**
     * Get current model info
     */
    getCurrentModel(): LLMModel | undefined;
}
export declare const localLLMStack: LocalLLMStack;
//# sourceMappingURL=localLLMStack.d.ts.map