/**
 * Raspberry Pi 5 Optimized Audio Manager for EthervoxAI
 * Handles Bluetooth speakers and ALSA audio output with proper TTS conversion
 */

import { EventEmitter } from 'events';
import { spawn, exec } from 'child_process';
import { promises as fs } from 'fs';
import * as path from 'path';
import * as os from 'os';

interface RaspberryPiAudioConfig {
  preferredOutput: 'espeak' | 'pico2wave' | 'festival' | 'alsa-aplay' | 'bluetooth';
  fallbackChain: string[];
  enableLogging: boolean;
  audioDevice?: string; // For Bluetooth or specific ALSA device
  speakingRate?: number; // Words per minute
  voice?: string; // Voice selection
  volume?: number; // 0-100
}

export class RaspberryPiAudioManager extends EventEmitter {
  private availableOutputs: Map<string, any> = new Map();
  private currentOutput: string | null = null;
  private config: RaspberryPiAudioConfig;
  private tempDir: string;

  constructor(config?: Partial<RaspberryPiAudioConfig>) {
    super();
    
    this.config = {
      preferredOutput: 'espeak',
      fallbackChain: ['espeak', 'pico2wave', 'festival', 'alsa-aplay'],
      enableLogging: true,
      speakingRate: 150,
      voice: 'en',
      volume: 80,
      ...config
    };

    this.tempDir = path.join(os.tmpdir(), 'ethervoxai-audio');
    this.initializeOutputMethods();
  }

  /**
   * Initialize Raspberry Pi specific audio output methods
   */
  private async initializeOutputMethods(): Promise<void> {
    this.log('🍓 Initializing Raspberry Pi 5 audio outputs...');

    // Create temp directory for audio files
    try {
      await fs.mkdir(this.tempDir, { recursive: true });
    } catch (error) {
      this.log('⚠️ Could not create temp directory:', error);
    }

    // 1. eSpeak-ng (most compatible TTS for Raspberry Pi)
    await this.tryInitializeESpeak();
    
    // 2. Pico2Wave (high quality TTS)
    await this.tryInitializePico2Wave();
    
    // 3. Festival (fallback TTS)
    await this.tryInitializeFestival();
    
    // 4. Direct ALSA playback
    await this.tryInitializeAlsaAplay();

    // Detect and configure Bluetooth audio
    await this.detectBluetoothAudio();

    this.selectBestOutput();
  }

  private async tryInitializeESpeak(): Promise<void> {
    try {
      // Check if espeak is installed
      await this.execCommand('which espeak');
      
      this.availableOutputs.set('espeak', {
        type: 'text-to-speech',
        playAudio: this.playWithESpeak.bind(this),
        description: 'eSpeak Text-to-Speech'
      });
      this.log('✅ eSpeak available');
    } catch (error: any) {
      this.log('❌ eSpeak not available:', error?.message || 'Not installed');
      this.log('💡 Install with: sudo apt install espeak espeak-ng');
    }
  }

  private async tryInitializePico2Wave(): Promise<void> {
    try {
      // Check if pico2wave is installed
      await this.execCommand('which pico2wave');
      
      this.availableOutputs.set('pico2wave', {
        type: 'text-to-speech',
        playAudio: this.playWithPico2Wave.bind(this),
        description: 'Pico2Wave Text-to-Speech'
      });
      this.log('✅ Pico2Wave available');
    } catch (error: any) {
      this.log('❌ Pico2Wave not available:', error?.message || 'Not installed');
      this.log('💡 Install with: sudo apt install libttspico-utils');
    }
  }

  private async tryInitializeFestival(): Promise<void> {
    try {
      // Check if festival is installed
      await this.execCommand('which festival');
      
      this.availableOutputs.set('festival', {
        type: 'text-to-speech',
        playAudio: this.playWithFestival.bind(this),
        description: 'Festival Text-to-Speech'
      });
      this.log('✅ Festival available');
    } catch (error: any) {
      this.log('❌ Festival not available:', error?.message || 'Not installed');
      this.log('💡 Install with: sudo apt install festival festvox-kallpc16k');
    }
  }

  private async tryInitializeAlsaAplay(): Promise<void> {
    try {
      // Check if aplay is available
      await this.execCommand('which aplay');
      
      this.availableOutputs.set('alsa-aplay', {
        type: 'audio-player',
        playAudio: this.playWithAlsaAplay.bind(this),
        description: 'ALSA Audio Player'
      });
      this.log('✅ ALSA aplay available');
    } catch (error: any) {
      this.log('❌ ALSA aplay not available:', error?.message || 'Not installed');
      this.log('💡 Install with: sudo apt install alsa-utils');
    }
  }

  private async detectBluetoothAudio(): Promise<void> {
    try {
      // Check for connected Bluetooth audio devices
      const result = await this.execCommand('pactl list sinks short');
      const bluetoothSinks = result.split('\n').filter(line => 
        line.includes('bluez') || line.includes('bluetooth')
      );

      if (bluetoothSinks.length > 0) {
        this.log('🔵 Bluetooth audio devices detected:');
        bluetoothSinks.forEach(sink => {
          const sinkName = sink.split('\t')[1];
          this.log(`   📱 ${sinkName}`);
        });

        // Set default audio device to Bluetooth if found
        if (bluetoothSinks[0]) {
          const defaultSink = bluetoothSinks[0].split('\t')[1];
          this.config.audioDevice = defaultSink;
          await this.execCommand(`pactl set-default-sink ${defaultSink}`);
          this.log(`🔵 Set default audio device to: ${defaultSink}`);
        }
      }
    } catch (error) {
      this.log('⚠️ Could not detect Bluetooth audio devices');
    }
  }

  private selectBestOutput(): void {
    for (const outputType of this.config.fallbackChain) {
      if (this.availableOutputs.has(outputType)) {
        this.currentOutput = outputType;
        const output = this.availableOutputs.get(outputType);
        this.log(`🎵 Selected audio output: ${output.description}`);
        return;
      }
    }

    this.log('⚠️ No audio output methods available - running in silent mode');
    this.currentOutput = null;
  }

  /**
   * Play text as speech using the best available method
   */
  public async playText(text: string, options?: any): Promise<void> {
    if (!this.currentOutput) {
      throw new Error('No audio output method available');
    }

    const output = this.availableOutputs.get(this.currentOutput);
    this.log(`🔊 Playing text with ${output.description}: "${text.substring(0, 50)}..."`);

    try {
      await output.playAudio(text, options);
      this.emit('playback-success', { text, method: this.currentOutput });
    } catch (error) {
      this.log(`❌ Playback failed with ${this.currentOutput}:`, error);
      
      // Try fallback
      await this.tryFallbackPlayback(text, options);
    }
  }

  private async tryFallbackPlayback(text: string, options?: any): Promise<void> {
    const currentIndex = this.config.fallbackChain.indexOf(this.currentOutput!);
    
    for (let i = currentIndex + 1; i < this.config.fallbackChain.length; i++) {
      const fallbackOutput = this.config.fallbackChain[i];
      
      if (this.availableOutputs.has(fallbackOutput)) {
        this.log(`🔄 Trying fallback: ${fallbackOutput}`);
        
        try {
          const output = this.availableOutputs.get(fallbackOutput);
          await output.playAudio(text, options);
          this.emit('playback-success', { text, method: fallbackOutput });
          return;
        } catch (error) {
          this.log(`❌ Fallback ${fallbackOutput} also failed:`, error);
        }
      }
    }

    throw new Error('All audio output methods failed');
  }

  /**
   * TTS implementations for Raspberry Pi
   */
  private async playWithESpeak(text: string, options?: any): Promise<void> {
    const rate = options?.rate || this.config.speakingRate;
    const voice = options?.voice || this.config.voice;
    const volume = options?.volume || this.config.volume;

    // Build espeak command
    let command = `espeak -v${voice} -s${rate} -a${volume}`;
    
    // If Bluetooth is configured, pipe through paplay
    if (this.config.audioDevice) {
      command += ` --stdout | paplay --device=${this.config.audioDevice}`;
    }

    command += ` "${text.replace(/"/g, '\\"')}"`;

    await this.execCommand(command);
  }

  private async playWithPico2Wave(text: string, options?: any): Promise<void> {
    const tempFile = path.join(this.tempDir, `pico_${Date.now()}.wav`);
    
    try {
      // Generate audio file with pico2wave
      await this.execCommand(`pico2wave -w "${tempFile}" "${text.replace(/"/g, '\\"')}"`);
      
      // Play the generated file
      let playCommand = `aplay "${tempFile}"`;
      
      // Use PulseAudio for Bluetooth
      if (this.config.audioDevice) {
        playCommand = `paplay --device=${this.config.audioDevice} "${tempFile}"`;
      }

      await this.execCommand(playCommand);
      
    } finally {
      // Clean up temp file
      try {
        await fs.unlink(tempFile);
      } catch (error) {
        // Ignore cleanup errors
      }
    }
  }

  private async playWithFestival(text: string, options?: any): Promise<void> {
    const tempFile = path.join(this.tempDir, `festival_${Date.now()}.wav`);
    
    try {
      // Generate audio with festival
      const festivalCommand = `echo "${text.replace(/"/g, '\\"')}" | festival --tts --otype riff --o "${tempFile}"`;
      await this.execCommand(festivalCommand);
      
      // Play the generated file
      let playCommand = `aplay "${tempFile}"`;
      
      if (this.config.audioDevice) {
        playCommand = `paplay --device=${this.config.audioDevice} "${tempFile}"`;
      }

      await this.execCommand(playCommand);
      
    } finally {
      // Clean up temp file
      try {
        await fs.unlink(tempFile);
      } catch (error) {
        // Ignore cleanup errors
      }
    }
  }

  private async playWithAlsaAplay(audioFile: string): Promise<void> {
    // This method is for playing existing audio files
    let command = `aplay "${audioFile}"`;
    
    if (this.config.audioDevice) {
      command = `paplay --device=${this.config.audioDevice} "${audioFile}"`;
    }

    await this.execCommand(command);
  }

  /**
   * Utility methods
   */
  private async execCommand(command: string): Promise<string> {
    return new Promise((resolve, reject) => {
      exec(command, { timeout: 30000 }, (error, stdout, stderr) => {
        if (error) {
          reject(new Error(`Command failed: ${command}\nError: ${error.message}\nStderr: ${stderr}`));
        } else {
          resolve(stdout);
        }
      });
    });
  }

  private log(message: string, ...args: any[]): void {
    if (this.config.enableLogging) {
      console.log(`[RaspberryPiAudio] ${message}`, ...args);
    }
  }

  /**
   * Test audio output
   */
  public async testAudio(): Promise<void> {
    this.log('🧪 Testing audio output...');
    
    try {
      await this.playText('Hello from EthervoxAI running on Raspberry Pi 5!');
      this.log('✅ Audio test successful');
    } catch (error) {
      this.log('❌ Audio test failed:', error);
      throw error;
    }
  }

  /**
   * Get available audio devices
   */
  public async getAudioDevices(): Promise<string[]> {
    try {
      const alsaDevices = await this.execCommand('aplay -l');
      const pulseDevices = await this.execCommand('pactl list sinks short');
      
      this.log('🔊 Available audio devices:');
      this.log('ALSA Devices:', alsaDevices);
      this.log('PulseAudio Sinks:', pulseDevices);
      
      return pulseDevices.split('\n').filter(line => line.trim().length > 0);
    } catch (error) {
      this.log('⚠️ Could not enumerate audio devices:', error);
      return [];
    }
  }

  /**
   * Configure Bluetooth audio device
   */
  public async setBluetoothDevice(deviceName: string): Promise<void> {
    try {
      await this.execCommand(`pactl set-default-sink ${deviceName}`);
      this.config.audioDevice = deviceName;
      this.log(`🔵 Bluetooth audio device set to: ${deviceName}`);
    } catch (error) {
      this.log('❌ Failed to set Bluetooth device:', error);
      throw error;
    }
  }

  /**
   * Cleanup resources
   */
  public async cleanup(): Promise<void> {
    try {
      // Clean up temp directory
      const files = await fs.readdir(this.tempDir);
      for (const file of files) {
        await fs.unlink(path.join(this.tempDir, file));
      }
      await fs.rmdir(this.tempDir);
      this.log('🧹 Cleaned up temporary files');
    } catch (error) {
      // Ignore cleanup errors
    }
  }
}

// Export for easy use
export default RaspberryPiAudioManager;
