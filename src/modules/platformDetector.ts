/**
 * 🔍 Platform Detection Module
 * 
 * Detects hardware capabilities, OS features, and system constraints
 * to optimize AI model selection and performance parameters.
 */

import { cpus, totalmem, freemem, arch, platform } from 'os';
import { existsSync, readFileSync } from 'fs';
import { join } from 'path';

export interface SystemCapabilities {
  // Hardware
  totalMemory: number;        // MB
  availableMemory: number;    // MB
  cpuCores: number;
  architecture: string;       // x64, arm64, etc.
  
  // Platform
  platform: 'windows' | 'linux' | 'darwin' | 'other';
  isRaspberryPi: boolean;
  raspberryPiModel?: string;
  
  // AI Capabilities
  hasGPU: boolean;
  hasNeuralEngine: boolean;   // Apple Silicon
  hasAVX2: boolean;          // x86 vector extensions
  hasNEON: boolean;          // ARM vector extensions
  
  // Performance tier
  performanceTier: 'low' | 'medium' | 'high' | 'ultra';
  
  // Recommended constraints
  maxModelSize: number;       // MB
  maxContextLength: number;   // tokens
  recommendedThreads: number;
  useMemoryMapping: boolean;
}

export interface ModelCompatibility {
  modelName: string;
  isCompatible: boolean;
  requiredMemory: number;
  expectedPerformance: 'poor' | 'fair' | 'good' | 'excellent';
  optimizationFlags: string[];
  warnings: string[];
}

export class PlatformDetector {
  private _capabilities: SystemCapabilities | null = null;
  private _detectionTime: number = 0;

  /**
   * Get system capabilities (cached after first detection)
   */
  async getCapabilities(): Promise<SystemCapabilities> {
    if (this._capabilities && Date.now() - this._detectionTime < 60000) {
      return this._capabilities;
    }

    console.log('🔍 Detecting system capabilities...');
    
    const capabilities: SystemCapabilities = {
      // Basic system info
      totalMemory: Math.round(totalmem() / 1024 / 1024),
      availableMemory: Math.round(freemem() / 1024 / 1024),
      cpuCores: cpus().length,
      architecture: arch(),
      
      // Platform detection
      platform: this.detectPlatform(),
      isRaspberryPi: await this.detectRaspberryPi(),
      raspberryPiModel: await this.getRaspberryPiModel(),
      
      // Hardware acceleration
      hasGPU: await this.detectGPU(),
      hasNeuralEngine: this.detectNeuralEngine(),
      hasAVX2: this.detectAVX2(),
      hasNEON: this.detectNEON(),
      
      // Performance calculations
      performanceTier: 'medium', // Will be calculated
      maxModelSize: 0,           // Will be calculated
      maxContextLength: 0,       // Will be calculated
      recommendedThreads: 0,     // Will be calculated
      useMemoryMapping: false    // Will be calculated
    };

    // Calculate performance tier and recommendations
    this.calculatePerformanceMetrics(capabilities);
    
    this._capabilities = capabilities;
    this._detectionTime = Date.now();
    
    this.logCapabilities(capabilities);
    
    return capabilities;
  }

  /**
   * Check model compatibility with current system
   */
  async checkModelCompatibility(
    modelName: string, 
    modelSizeMB: number,
    minMemoryMB: number = 0,
    preferredMemoryMB: number = 0
  ): Promise<ModelCompatibility> {
    const caps = await this.getCapabilities();
    
    const requiredMemory = Math.max(modelSizeMB * 1.5, minMemoryMB); // 1.5x for overhead
    const isCompatible = caps.availableMemory >= requiredMemory;
    
    let expectedPerformance: ModelCompatibility['expectedPerformance'] = 'fair';
    if (caps.performanceTier === 'ultra') expectedPerformance = 'excellent';
    else if (caps.performanceTier === 'high') expectedPerformance = 'good';
    else if (caps.performanceTier === 'low') expectedPerformance = 'poor';
    
    const optimizationFlags: string[] = [];
    const warnings: string[] = [];
    
    // Add platform-specific optimizations
    if (caps.hasAVX2) optimizationFlags.push('AVX2');
    if (caps.hasNEON) optimizationFlags.push('NEON');
    if (caps.useMemoryMapping) optimizationFlags.push('MMAP');
    
    // Add warnings for potential issues
    if (requiredMemory > caps.availableMemory * 0.8) {
      warnings.push('High memory usage - may cause system slowdown');
    }
    
    if (caps.isRaspberryPi && modelSizeMB > 1000) {
      warnings.push('Large model on Raspberry Pi - expect slower performance');
    }
    
    if (caps.totalMemory < 4096 && modelSizeMB > 500) {
      warnings.push('Limited system memory - consider smaller model variant');
    }

    return {
      modelName,
      isCompatible,
      requiredMemory,
      expectedPerformance,
      optimizationFlags,
      warnings
    };
  }

  /**
   * Get recommended models for current system
   */
  async getRecommendedModels(): Promise<Array<{name: string, size: string, reason: string}>> {
    const caps = await this.getCapabilities();
    const recommendations = [];

    if (caps.performanceTier === 'ultra' && caps.totalMemory >= 16384) {
      recommendations.push({
        name: 'llama2-13b-chat-q4',
        size: '7.3GB',
        reason: 'High-performance system can handle larger models'
      });
    }
    
    if (caps.performanceTier >= 'high' && caps.totalMemory >= 8192) {
      recommendations.push({
        name: 'llama2-7b-chat-q4',
        size: '3.9GB',
        reason: 'Good balance of capability and performance'
      });
    }
    
    if (caps.performanceTier >= 'medium' || caps.totalMemory >= 4096) {
      recommendations.push({
        name: 'mistral-7b-instruct-v0.1-q4',
        size: '4.1GB',
        reason: 'Excellent instruction following, efficient'
      });
    }
    
    // Always include lightweight options
    recommendations.push({
      name: 'tinyllama-1.1b-chat-q4',
      size: '669MB',
      reason: 'Lightweight, works on any system'
    });

    if (caps.isRaspberryPi) {
      recommendations.push({
        name: 'phi-2-2.7b-q4',
        size: '1.6GB',
        reason: 'Optimized for ARM processors, good for RPi'
      });
    }

    return recommendations;
  }

  // ================== Private Methods ==================

  private detectPlatform(): SystemCapabilities['platform'] {
    switch (platform()) {
      case 'win32': return 'windows';
      case 'linux': return 'linux';
      case 'darwin': return 'darwin';
      default: return 'other';
    }
  }

  private async detectRaspberryPi(): Promise<boolean> {
    if (platform() !== 'linux') return false;
    
    try {
      // Check device tree model
      if (existsSync('/proc/device-tree/model')) {
        const model = readFileSync('/proc/device-tree/model', 'utf8');
        if (model.toLowerCase().includes('raspberry pi')) return true;
      }
      
      // Check CPU info
      if (existsSync('/proc/cpuinfo')) {
        const cpuInfo = readFileSync('/proc/cpuinfo', 'utf8');
        return cpuInfo.includes('BCM') || cpuInfo.includes('Raspberry Pi');
      }
    } catch (error) {
      // Ignore errors, fall through to false
    }
    
    return false;
  }

  private async getRaspberryPiModel(): Promise<string | undefined> {
    if (!(await this.detectRaspberryPi())) return undefined;
    
    try {
      if (existsSync('/proc/device-tree/model')) {
        const model = readFileSync('/proc/device-tree/model', 'utf8')
          .replace(/\0/g, '')
          .trim();
        return model;
      }
    } catch (error) {
      // Ignore errors
    }
    
    return 'Unknown Raspberry Pi';
  }

  private async detectGPU(): Promise<boolean> {
    // This is a basic implementation - in production you'd use more sophisticated detection
    try {
      if (platform() === 'win32') {
        // Could check for NVIDIA/AMD/Intel GPU via WMI
        return false; // Placeholder
      } else if (platform() === 'linux') {
        // Could check /proc/driver/nvidia/version or lspci
        return false; // Placeholder
      }
    } catch (error) {
      // Ignore errors
    }
    
    return false;
  }

  private detectNeuralEngine(): boolean {
    // Apple Silicon Neural Engine detection
    if (platform() === 'darwin' && arch() === 'arm64') {
      const cpuList = cpus();
      return cpuList.some((cpu: any) => cpu.model.includes('Apple'));
    }
    return false;
  }

  private detectAVX2(): boolean {
    // x86/x64 AVX2 support detection
    if (arch() === 'x64' || arch() === 'ia32') {
      const cpuList = cpus();
      // This is simplified - in production you'd check CPU flags
      return cpuList.length > 0; // Placeholder - assume modern x64 has AVX2
    }
    return false;
  }

  private detectNEON(): boolean {
    // ARM NEON support detection
    if (arch().startsWith('arm')) {
      // Most modern ARM processors have NEON
      return true;
    }
    return false;
  }

  private calculatePerformanceMetrics(caps: SystemCapabilities): void {
    // Determine performance tier
    const memoryScore = caps.totalMemory >= 16384 ? 4 : 
                       caps.totalMemory >= 8192 ? 3 :
                       caps.totalMemory >= 4096 ? 2 : 1;
    
    const coreScore = caps.cpuCores >= 8 ? 4 :
                     caps.cpuCores >= 4 ? 3 :
                     caps.cpuCores >= 2 ? 2 : 1;
    
    const archScore = caps.hasNeuralEngine ? 4 :
                     caps.hasAVX2 ? 3 :
                     caps.hasNEON ? 2 : 1;
    
    const totalScore = memoryScore + coreScore + archScore;
    
    if (totalScore >= 10) caps.performanceTier = 'ultra';
    else if (totalScore >= 8) caps.performanceTier = 'high';
    else if (totalScore >= 6) caps.performanceTier = 'medium';
    else caps.performanceTier = 'low';
    
    // Calculate model constraints
    const memoryForModels = Math.floor(caps.availableMemory * 0.6); // 60% of available memory
    caps.maxModelSize = Math.max(memoryForModels, 512); // At least 512MB
    
    // Context length based on available memory
    caps.maxContextLength = Math.min(
      Math.floor(caps.availableMemory / 4), // Rough estimate: 4MB per 1k tokens
      8192 // Reasonable maximum
    );
    
    // Thread count (leave some cores for system)
    caps.recommendedThreads = Math.max(
      Math.floor(caps.cpuCores * 0.75),
      1
    );
    
    // Use memory mapping for larger systems
    caps.useMemoryMapping = caps.totalMemory >= 4096 && !caps.isRaspberryPi;
  }

  private logCapabilities(caps: SystemCapabilities): void {
    console.log('📊 System Capabilities Detected:');
    console.log(`   💾 Memory: ${caps.availableMemory}MB available / ${caps.totalMemory}MB total`);
    console.log(`   🖥️  CPU: ${caps.cpuCores} cores (${caps.architecture})`);
    console.log(`   🏗️  Platform: ${caps.platform}${caps.isRaspberryPi ? ` (${caps.raspberryPiModel})` : ''}`);
    console.log(`   ⚡ Performance Tier: ${caps.performanceTier.toUpperCase()}`);
    console.log(`   🧠 Max Model Size: ${caps.maxModelSize}MB`);
    console.log(`   📝 Max Context: ${caps.maxContextLength} tokens`);
    console.log(`   🧵 Recommended Threads: ${caps.recommendedThreads}`);
    
    const features = [];
    if (caps.hasGPU) features.push('GPU');
    if (caps.hasNeuralEngine) features.push('Neural Engine');
    if (caps.hasAVX2) features.push('AVX2');
    if (caps.hasNEON) features.push('NEON');
    
    if (features.length > 0) {
      console.log(`   🚀 Hardware Acceleration: ${features.join(', ')}`);
    }
  }
}

// Export singleton instance
export const platformDetector = new PlatformDetector();
