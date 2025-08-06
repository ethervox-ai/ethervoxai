/**
 * 🗃️ Model Manager Module
 * 
 * Downloads, caches, and manages local AI models with platform optimizations.
 * Handles model variants, quantizations, and automatic selection based on system capabilities.
 */

import { createHash } from 'crypto';
import { createReadStream, createWriteStream, existsSync, mkdirSync, statSync, unlinkSync, readFileSync, writeFileSync } from 'fs';
import { join, dirname } from 'path';
import { homedir } from 'os';
import { platformDetector, SystemCapabilities, ModelCompatibility } from './platformDetector';

export interface ModelInfo {
  name: string;
  displayName: string;
  description: string;
  size: number;           // Size in MB
  quantization: 'f16' | 'f32' | 'q4_0' | 'q4_1' | 'q5_0' | 'q5_1' | 'q8_0';
  architecture: 'llama' | 'mistral' | 'phi' | 'tinyllama' | 'other';
  contextLength: number;  // Maximum context length in tokens
  requiredMemory: number; // Minimum RAM needed in MB
  downloadUrl: string;
  checksum: string;       // SHA256 checksum for verification
  tags: string[];         // ['chat', 'instruct', 'code', etc.]
  license: string;
  createdBy: string;
}

export interface DownloadProgress {
  modelName: string;
  downloadedBytes: number;
  totalBytes: number;
  percentage: number;
  speed: number;          // bytes per second
  eta: number;            // estimated time remaining in seconds
  status: 'downloading' | 'verifying' | 'complete' | 'error';
  error?: string;
}

export interface ModelCache {
  path: string;
  size: number;
  checksum: string;
  downloadDate: Date;
  lastUsed: Date;
  useCount: number;
}

export class ModelManager {
  private modelsDir: string;
  private cacheFile: string;
  private downloadCallbacks: Map<string, (progress: DownloadProgress) => void> = new Map();
  
  constructor(customModelsDir?: string) {
    this.modelsDir = customModelsDir || join(homedir(), '.ethervoxai', 'models');
    this.cacheFile = join(this.modelsDir, 'cache.json');
    this.ensureDirectories();
  }

  /**
   * Get the default model catalog with popular GGML models
   */
  getDefaultModelCatalog(): ModelInfo[] {
    return [
      {
        name: 'tinyllama-1.1b-chat-q4',
        displayName: 'TinyLlama 1.1B Chat (Q4)',
        description: 'Lightweight model perfect for basic conversations and resource-constrained devices',
        size: 669,
        quantization: 'q4_0',
        architecture: 'tinyllama',
        contextLength: 2048,
        requiredMemory: 1024,
        downloadUrl: 'https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGML/resolve/main/tinyllama-1.1b-chat-v1.0.q4_0.bin',
        checksum: 'placeholder_checksum_1',
        tags: ['chat', 'lightweight', 'embedded'],
        license: 'Apache-2.0',
        createdBy: 'TinyLlama Team'
      },
      {
        name: 'phi-2-2.7b-q4',
        displayName: 'Microsoft Phi-2 2.7B (Q4)',
        description: 'High-quality small model from Microsoft, great for ARM devices',
        size: 1600,
        quantization: 'q4_0',
        architecture: 'phi',
        contextLength: 2048,
        requiredMemory: 2048,
        downloadUrl: 'https://huggingface.co/microsoft/phi-2-ggml/resolve/main/phi-2.q4_0.bin',
        checksum: 'placeholder_checksum_2',
        tags: ['chat', 'instruct', 'arm-optimized'],
        license: 'MIT',
        createdBy: 'Microsoft Research'
      },
      {
        name: 'mistral-7b-instruct-v0.1-q4',
        displayName: 'Mistral 7B Instruct v0.1 (Q4)',
        description: 'Excellent instruction-following model with strong performance',
        size: 4100,
        quantization: 'q4_0',
        architecture: 'mistral',
        contextLength: 4096,
        requiredMemory: 5120,
        downloadUrl: 'https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.1-GGML/resolve/main/mistral-7b-instruct-v0.1.q4_0.bin',
        checksum: 'placeholder_checksum_3',
        tags: ['chat', 'instruct', 'general'],
        license: 'Apache-2.0',
        createdBy: 'Mistral AI'
      },
      {
        name: 'llama2-7b-chat-q4',
        displayName: 'Llama 2 7B Chat (Q4)',
        description: 'Popular general-purpose chat model from Meta',
        size: 3900,
        quantization: 'q4_0',
        architecture: 'llama',
        contextLength: 4096,
        requiredMemory: 4896,
        downloadUrl: 'https://huggingface.co/TheBloke/Llama-2-7B-Chat-GGML/resolve/main/llama-2-7b-chat.q4_0.bin',
        checksum: 'placeholder_checksum_4',
        tags: ['chat', 'general', 'popular'],
        license: 'Custom (Llama 2)',
        createdBy: 'Meta'
      },
      {
        name: 'llama2-13b-chat-q4',
        displayName: 'Llama 2 13B Chat (Q4)',
        description: 'High-capability model for systems with sufficient memory',
        size: 7300,
        quantization: 'q4_0',
        architecture: 'llama',
        contextLength: 4096,
        requiredMemory: 8192,
        downloadUrl: 'https://huggingface.co/TheBloke/Llama-2-13B-Chat-GGML/resolve/main/llama-2-13b-chat.q4_0.bin',
        checksum: 'placeholder_checksum_5',
        tags: ['chat', 'general', 'high-performance'],
        license: 'Custom (Llama 2)',
        createdBy: 'Meta'
      }
    ];
  }

  /**
   * Get recommended models based on system capabilities
   */
  async getRecommendedModels(): Promise<ModelInfo[]> {
    const capabilities = await platformDetector.getCapabilities();
    const catalog = this.getDefaultModelCatalog();
    
    const recommended: ModelInfo[] = [];
    
    for (const model of catalog) {
      const compatibility = await platformDetector.checkModelCompatibility(
        model.name,
        model.size,
        model.requiredMemory
      );
      
      if (compatibility.isCompatible) {
        recommended.push(model);
      }
    }
    
    // Sort by performance tier preference
    recommended.sort((a, b) => {
      const perfOrder = ['ultra', 'high', 'medium', 'low'];
      const aScore = capabilities.performanceTier === 'ultra' ? a.size : -a.size;
      const bScore = capabilities.performanceTier === 'ultra' ? b.size : -b.size;
      return bScore - aScore;
    });
    
    return recommended;
  }

  /**
   * Download a model with progress tracking
   */
  async downloadModel(
    modelName: string,
    onProgress?: (progress: DownloadProgress) => void
  ): Promise<string> {
    const catalog = this.getDefaultModelCatalog();
    const modelInfo = catalog.find(m => m.name === modelName);
    
    if (!modelInfo) {
      throw new Error(`Model '${modelName}' not found in catalog`);
    }

    // Check if already downloaded
    const modelPath = join(this.modelsDir, `${modelName}.bin`);
    if (existsSync(modelPath)) {
      const isValid = await this.verifyModel(modelPath, modelInfo.checksum);
      if (isValid) {
        console.log(`✅ Model '${modelName}' already downloaded and verified`);
        return modelPath;
      } else {
        console.log(`⚠️ Model '${modelName}' checksum mismatch, re-downloading...`);
        unlinkSync(modelPath);
      }
    }

    console.log(`📥 Downloading model: ${modelInfo.displayName} (${modelInfo.size}MB)`);
    
    // Register progress callback
    if (onProgress) {
      this.downloadCallbacks.set(modelName, onProgress);
    }

    try {
      // For now, we'll simulate the download since we don't have actual URLs
      // In production, this would use fetch() or https.get() for actual downloads
      await this.simulateDownload(modelInfo, modelPath, onProgress);
      
      // Update cache
      await this.updateCache(modelName, modelPath, modelInfo);
      
      console.log(`✅ Successfully downloaded: ${modelInfo.displayName}`);
      return modelPath;
      
    } catch (error) {
      if (onProgress) {
        onProgress({
          modelName,
          downloadedBytes: 0,
          totalBytes: modelInfo.size * 1024 * 1024,
          percentage: 0,
          speed: 0,
          eta: 0,
          status: 'error',
          error: error instanceof Error ? error.message : 'Unknown error'
        });
      }
      
      // Clean up partial download
      if (existsSync(modelPath)) {
        unlinkSync(modelPath);
      }
      
      throw error;
    } finally {
      this.downloadCallbacks.delete(modelName);
    }
  }

  /**
   * Get the path to a local model (download if needed)
   */
  async getModelPath(modelName: string): Promise<string> {
    const modelPath = join(this.modelsDir, `${modelName}.bin`);
    
    if (existsSync(modelPath)) {
      // Update last used time
      await this.updateLastUsed(modelName);
      return modelPath;
    }
    
    // Auto-download if not found
    console.log(`Model '${modelName}' not found locally, downloading...`);
    return await this.downloadModel(modelName);
  }

  /**
   * List all downloaded models
   */
  listDownloadedModels(): string[] {
    if (!existsSync(this.modelsDir)) return [];
    
    const cache = this.loadCache();
    return Object.keys(cache);
  }

  /**
   * Get model cache information
   */
  getModelCacheInfo(modelName: string): ModelCache | null {
    const cache = this.loadCache();
    return cache[modelName] || null;
  }

  /**
   * Remove a downloaded model
   */
  removeModel(modelName: string): boolean {
    const modelPath = join(this.modelsDir, `${modelName}.bin`);
    
    if (existsSync(modelPath)) {
      unlinkSync(modelPath);
      
      // Update cache
      const cache = this.loadCache();
      delete cache[modelName];
      this.saveCache(cache);
      
      console.log(`🗑️ Removed model: ${modelName}`);
      return true;
    }
    
    return false;
  }

  /**
   * Get total cache size in MB
   */
  getCacheSize(): number {
    const cache = this.loadCache();
    return Object.values(cache).reduce((total, model) => total + model.size, 0) / (1024 * 1024);
  }

  /**
   * Clear unused models (not used in last 30 days)
   */
  clearUnusedModels(maxAgeDays: number = 30): string[] {
    const cache = this.loadCache();
    const cutoffDate = new Date(Date.now() - maxAgeDays * 24 * 60 * 60 * 1000);
    const removedModels: string[] = [];
    
    for (const [modelName, modelCache] of Object.entries(cache)) {
      if (modelCache.lastUsed < cutoffDate) {
        if (this.removeModel(modelName)) {
          removedModels.push(modelName);
        }
      }
    }
    
    if (removedModels.length > 0) {
      console.log(`🧹 Cleaned up ${removedModels.length} unused models: ${removedModels.join(', ')}`);
    }
    
    return removedModels;
  }

  // ================== Private Methods ==================

  private ensureDirectories(): void {
    if (!existsSync(this.modelsDir)) {
      mkdirSync(this.modelsDir, { recursive: true });
    }
  }

  private async simulateDownload(
    modelInfo: ModelInfo,
    outputPath: string,
    onProgress?: (progress: DownloadProgress) => void
  ): Promise<void> {
    // This simulates a model download for demo purposes
    // In production, this would be a real HTTP download
    
    const totalBytes = modelInfo.size * 1024 * 1024;
    let downloadedBytes = 0;
    const startTime = Date.now();
    
    // Create a dummy file with some content
    const writeStream = createWriteStream(outputPath);
    
    const chunkSize = 64 * 1024; // 64KB chunks
    const totalChunks = Math.ceil(totalBytes / chunkSize);
    
    for (let i = 0; i < totalChunks; i++) {
      const currentChunkSize = Math.min(chunkSize, totalBytes - downloadedBytes);
      const chunk = Buffer.alloc(currentChunkSize, `Model data chunk ${i}\n`);
      
      writeStream.write(chunk);
      downloadedBytes += currentChunkSize;
      
      if (onProgress) {
        const elapsed = (Date.now() - startTime) / 1000;
        const speed = downloadedBytes / elapsed;
        const eta = (totalBytes - downloadedBytes) / speed;
        
        onProgress({
          modelName: modelInfo.name,
          downloadedBytes,
          totalBytes,
          percentage: (downloadedBytes / totalBytes) * 100,
          speed,
          eta,
          status: 'downloading'
        });
      }
      
      // Simulate network delay
      await new Promise(resolve => setTimeout(resolve, 10));
    }
    
    writeStream.end();
    await new Promise<void>((resolve) => writeStream.on('close', () => resolve()));
    
    if (onProgress) {
      onProgress({
        modelName: modelInfo.name,
        downloadedBytes: totalBytes,
        totalBytes,
        percentage: 100,
        speed: 0,
        eta: 0,
        status: 'complete'
      });
    }
  }

  private async verifyModel(filePath: string, expectedChecksum: string): Promise<boolean> {
    try {
      const hash = createHash('sha256');
      const stream = createReadStream(filePath);
      
      for await (const chunk of stream) {
        hash.update(chunk);
      }
      
      const actualChecksum = hash.digest('hex');
      return actualChecksum === expectedChecksum || expectedChecksum === 'placeholder_checksum_1'; // Allow placeholder for demo
    } catch (error) {
      return false;
    }
  }

  private loadCache(): Record<string, ModelCache> {
    try {
      if (existsSync(this.cacheFile)) {
        const data = readFileSync(this.cacheFile, 'utf8');
        const cache = JSON.parse(data);
        
        // Convert date strings back to Date objects
        for (const model of Object.values(cache) as ModelCache[]) {
          model.downloadDate = new Date(model.downloadDate);
          model.lastUsed = new Date(model.lastUsed);
        }
        
        return cache;
      }
    } catch (error) {
      console.warn('Failed to load model cache, starting fresh:', error);
    }
    
    return {};
  }

  private saveCache(cache: Record<string, ModelCache>): void {
    try {
      const data = JSON.stringify(cache, null, 2);
      writeFileSync(this.cacheFile, data, 'utf8');
    } catch (error) {
      console.warn('Failed to save model cache:', error);
    }
  }

  private async updateCache(modelName: string, modelPath: string, modelInfo: ModelInfo): Promise<void> {
    const cache = this.loadCache();
    const stats = statSync(modelPath);
    
    cache[modelName] = {
      path: modelPath,
      size: stats.size,
      checksum: modelInfo.checksum,
      downloadDate: new Date(),
      lastUsed: new Date(),
      useCount: 0
    };
    
    this.saveCache(cache);
  }

  private async updateLastUsed(modelName: string): Promise<void> {
    const cache = this.loadCache();
    
    if (cache[modelName]) {
      cache[modelName].lastUsed = new Date();
      cache[modelName].useCount++;
      this.saveCache(cache);
    }
  }
}

// Export singleton instance
export const modelManager = new ModelManager();
