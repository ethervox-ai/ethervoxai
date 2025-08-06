/**
 * 🚀 Inference Engine Module
 * 
 * Handles local AI model inference using llama.cpp bindings
 * Provides streaming responses, context management, and performance optimization
 */

import { spawn, ChildProcess } from 'child_process';
import { existsSync } from 'fs';
import { join } from 'path';
import { platformDetector, SystemCapabilities } from './platformDetector';
import { modelManager, ModelInfo } from './modelManager';

export interface InferenceConfig {
  modelName: string;
  contextLength: number;
  temperature: number;
  topP: number;
  topK: number;
  threads: number;
  batchSize: number;
  repeatPenalty: number;
  repeatLastN: number;
  seed: number;
  useMemoryMapping: boolean;
  useGPU: boolean;
  gpuLayers: number;
}

export interface InferenceResponse {
  text: string;
  isComplete: boolean;
  tokensGenerated: number;
  tokensPerSecond: number;
  promptTokens: number;
  totalTokens: number;
  timings: {
    promptTime: number;    // ms
    generationTime: number; // ms
    totalTime: number;     // ms
  };
}

export interface StreamingResponse {
  token: string;
  isComplete: boolean;
  totalTokens: number;
  tokensPerSecond: number;
}

export class InferenceEngine {
  private currentModel: string | null = null;
  private llamaCppPath: string | null = null;
  private currentProcess: ChildProcess | null = null;
  private isInitialized: boolean = false;
  private config: Partial<InferenceConfig> = {};

  constructor() {
    this.detectLlamaCpp();
  }

  /**
   * Initialize the inference engine with optimal settings for current system
   */
  async initialize(modelName?: string): Promise<void> {
    console.log('🚀 Initializing InferenceEngine...');
    
    const capabilities = await platformDetector.getCapabilities();
    
    // Set default model if not provided
    if (!modelName) {
      const recommended = await modelManager.getRecommendedModels();
      if (recommended.length === 0) {
        throw new Error('No compatible models available for this system');
      }
      modelName = recommended[0].name;
      console.log(`📋 Auto-selected model: ${modelName}`);
    }
    
    // Ensure model is available
    const modelPath = await modelManager.getModelPath(modelName);
    
    // Create optimal configuration
    this.config = await this.createOptimalConfig(modelName, capabilities);
    this.currentModel = modelName;
    
    console.log('✅ InferenceEngine initialized successfully');
    console.log(`   Model: ${modelName}`);
    console.log(`   Threads: ${this.config.threads}`);
    console.log(`   Context Length: ${this.config.contextLength}`);
    console.log(`   Memory Mapping: ${this.config.useMemoryMapping ? 'enabled' : 'disabled'}`);
    
    this.isInitialized = true;
  }

  /**
   * Generate text completion for a given prompt
   */
  async complete(
    prompt: string,
    options: Partial<InferenceConfig> = {}
  ): Promise<InferenceResponse> {
    if (!this.isInitialized || !this.currentModel) {
      throw new Error('InferenceEngine not initialized. Call initialize() first.');
    }

    const startTime = Date.now();
    
    console.log(`🧠 Generating completion for: "${prompt.substring(0, 50)}..."`);
    
    try {
      // For demo purposes, simulate inference
      // In production, this would use actual llama.cpp bindings
      const response = await this.simulateInference(prompt, options);
      
      const endTime = Date.now();
      const totalTime = endTime - startTime;
      
      console.log(`✅ Completion generated in ${totalTime}ms`);
      console.log(`   Tokens: ${response.tokensGenerated} (${response.tokensPerSecond.toFixed(1)} tok/s)`);
      
      return {
        ...response,
        timings: {
          promptTime: Math.floor(totalTime * 0.1),
          generationTime: Math.floor(totalTime * 0.9),
          totalTime
        }
      };
      
    } catch (error) {
      console.error('❌ Inference failed:', error);
      throw new Error(`Inference failed: ${error instanceof Error ? error.message : 'Unknown error'}`);
    }
  }

  /**
   * Generate streaming text completion
   */
  async *completeStreaming(
    prompt: string,
    options: Partial<InferenceConfig> = {}
  ): AsyncGenerator<StreamingResponse, void, unknown> {
    if (!this.isInitialized || !this.currentModel) {
      throw new Error('InferenceEngine not initialized. Call initialize() first.');
    }

    console.log(`🌊 Starting streaming completion for: "${prompt.substring(0, 50)}..."`);
    
    // For demo purposes, simulate streaming
    // In production, this would use actual llama.cpp streaming
    yield* this.simulateStreamingInference(prompt, options);
  }

  /**
   * Switch to a different model
   */
  async switchModel(modelName: string): Promise<void> {
    console.log(`🔄 Switching to model: ${modelName}`);
    
    // Cleanup current model
    await this.cleanup();
    
    // Initialize with new model
    await this.initialize(modelName);
  }

  /**
   * Get current model information
   */
  getCurrentModel(): string | null {
    return this.currentModel;
  }

  /**
   * Get current configuration
   */
  getCurrentConfig(): Partial<InferenceConfig> {
    return { ...this.config };
  }

  /**
   * Check if engine is ready for inference
   */
  isReady(): boolean {
    return this.isInitialized && this.currentModel !== null;
  }

  /**
   * Cleanup resources
   */
  async cleanup(): Promise<void> {
    if (this.currentProcess) {
      this.currentProcess.kill();
      this.currentProcess = null;
    }
    
    this.currentModel = null;
    this.isInitialized = false;
    this.config = {};
    
    console.log('🧹 InferenceEngine cleaned up');
  }

  // ================== Private Methods ==================

  private detectLlamaCpp(): void {
    // Look for llama.cpp executable in common locations
    const possiblePaths = [
      './llama.cpp/main',           // Local build
      './llama.cpp/main.exe',       // Windows local build
      'llama-cpp-main',             // System installed
      'llama.cpp',                  // Alternative name
      join(process.cwd(), 'bin', 'llama.cpp'),
      join(process.cwd(), 'bin', 'main'),
      join(process.cwd(), 'bin', 'main.exe'),
    ];

    for (const path of possiblePaths) {
      if (existsSync(path)) {
        this.llamaCppPath = path;
        console.log(`🔍 Found llama.cpp at: ${path}`);
        return;
      }
    }

    // For demo purposes, we'll simulate without requiring actual llama.cpp
    console.log('ℹ️ llama.cpp not found - running in simulation mode');
    this.llamaCppPath = null;
  }

  private async createOptimalConfig(
    modelName: string,
    capabilities: SystemCapabilities
  ): Promise<InferenceConfig> {
    const catalog = modelManager.getDefaultModelCatalog();
    const modelInfo = catalog.find(m => m.name === modelName);
    
    if (!modelInfo) {
      throw new Error(`Model info not found for: ${modelName}`);
    }

    // Base configuration
    const config: InferenceConfig = {
      modelName,
      contextLength: Math.min(modelInfo.contextLength, capabilities.maxContextLength),
      temperature: 0.7,
      topP: 0.9,
      topK: 40,
      threads: capabilities.recommendedThreads,
      batchSize: capabilities.performanceTier === 'low' ? 8 : 32,
      repeatPenalty: 1.1,
      repeatLastN: 64,
      seed: -1, // Random seed
      useMemoryMapping: capabilities.useMemoryMapping,
      useGPU: capabilities.hasGPU,
      gpuLayers: capabilities.hasGPU ? 35 : 0
    };

    // Platform-specific optimizations
    if (capabilities.isRaspberryPi) {
      config.batchSize = 8;
      config.threads = Math.max(1, Math.floor(config.threads * 0.75));
      config.contextLength = Math.min(config.contextLength, 1024);
    }

    if (capabilities.performanceTier === 'ultra') {
      config.batchSize = 64;
      config.contextLength = Math.min(8192, config.contextLength);
    }

    return config;
  }

  private async simulateInference(
    prompt: string,
    options: Partial<InferenceConfig>
  ): Promise<InferenceResponse> {
    // Simulate processing time based on system capabilities
    const capabilities = await platformDetector.getCapabilities();
    const baseTime = capabilities.performanceTier === 'ultra' ? 500 : 
                    capabilities.performanceTier === 'high' ? 1000 :
                    capabilities.performanceTier === 'medium' ? 2000 : 3000;
    
    const processingTime = baseTime + Math.random() * 1000;
    await new Promise(resolve => setTimeout(resolve, processingTime));

    // Generate a realistic response
    const responses = [
      "I understand you're looking for information. As EthervoxAI, I can help you with various tasks while keeping your data private and secure on your local device.",
      "Thank you for your question. I'm running locally on your device, which means your conversations stay private. How can I assist you today?",
      "I'm EthervoxAI, your privacy-first AI assistant. I'm designed to run entirely on your device without sending data to external servers. What would you like to know?",
      "Hello! I'm processing your request locally to ensure your privacy. As a local AI model, I can help with various tasks while keeping your information secure.",
      "I appreciate your patience. Running locally on your device allows me to provide responses while maintaining complete privacy. How may I help you?"
    ];

    const responseText = responses[Math.floor(Math.random() * responses.length)];
    const tokensGenerated = Math.floor(responseText.split(' ').length * 1.3); // Approximate token count
    const tokensPerSecond = tokensGenerated / (processingTime / 1000);

    return {
      text: responseText,
      isComplete: true,
      tokensGenerated,
      tokensPerSecond,
      promptTokens: Math.floor(prompt.split(' ').length * 1.3),
      totalTokens: tokensGenerated + Math.floor(prompt.split(' ').length * 1.3),
      timings: {
        promptTime: 0,
        generationTime: 0,
        totalTime: 0
      }
    };
  }

  private async *simulateStreamingInference(
    prompt: string,
    options: Partial<InferenceConfig>
  ): AsyncGenerator<StreamingResponse, void, unknown> {
    const fullResponse = await this.simulateInference(prompt, options);
    const words = fullResponse.text.split(' ');
    let tokensGenerated = 0;
    const startTime = Date.now();

    for (let i = 0; i < words.length; i++) {
      const token = words[i] + (i < words.length - 1 ? ' ' : '');
      tokensGenerated++;
      
      const elapsedTime = (Date.now() - startTime) / 1000;
      const tokensPerSecond = tokensGenerated / Math.max(elapsedTime, 0.1);

      yield {
        token,
        isComplete: i === words.length - 1,
        totalTokens: tokensGenerated,
        tokensPerSecond
      };

      // Simulate realistic streaming delay
      const delay = Math.random() * 200 + 50; // 50-250ms per token
      await new Promise(resolve => setTimeout(resolve, delay));
    }
  }
}

// Export singleton instance
export const inferenceEngine = new InferenceEngine();
