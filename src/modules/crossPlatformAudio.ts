/**
 * Cross-Platform Audio Manager for EthervoxAI
 * Provides audio output using multiple fallback strategies for maximum compatibility
 */

import { EventEmitter } from 'events';

interface AudioManagerConfig {
  preferredOutput: 'native' | 'wav-player' | 'play-sound' | 'tts-only' | 'powershell-tts';
  fallbackChain: string[];
  enableLogging: boolean;
}

export class CrossPlatformAudioManager extends EventEmitter {
  private availableOutputs: Map<string, any> = new Map();
  private currentOutput: string | null = null;
  private config: AudioManagerConfig;

  constructor(config?: Partial<AudioManagerConfig>) {
    super();
    
    this.config = {
  preferredOutput: 'native',
  fallbackChain: ['native', 'powershell-tts', 'wav-player', 'play-sound', 'tts-only'],
      enableLogging: true,
      ...config
    };

    this.initializeOutputMethods();
  }

  /**
   * Initialize all available audio output methods
   */
  private initializeOutputMethods(): void {
    this.log('🔧 Initializing cross-platform audio outputs...');

    // 1. Native Windows TTS (most compatible on Windows)
    this.tryInitializeNativeTTS();
    
    // 2. WAV Player (pure JavaScript, no native deps)
    this.tryInitializeWavPlayer();
    
    // 3. Play-sound (cross-platform wrapper)
    this.tryInitializePlaySound();
    
    // 4. Say package (TTS fallback)
    this.tryInitializeSay();

    // 5. Native PowerShell TTS (Windows only)
    this.tryInitializePowerShellTTS();

    this.selectBestOutput();
  }

  private tryInitializeNativeTTS(): void {
    try {
      if (process.platform === 'win32') {
        const { exec } = require('child_process');
        this.availableOutputs.set('native', {
          type: 'native-tts',
          playAudio: this.playWithNativeTTS.bind(this),
          description: 'Windows built-in TTS'
        });
        this.log('✅ Native Windows TTS available');
      }
    } catch (error: any) {
      this.log('❌ Native TTS failed:', error?.message || 'Unknown error');
    }
  }

  private tryInitializeWavPlayer(): void {
    try {
      const wavPlayer = require('node-wav-player');
      this.availableOutputs.set('wav-player', {
        type: 'wav-file',
        player: wavPlayer,
        playAudio: this.playWithWavPlayer.bind(this),
        description: 'WAV file player (pure JS)'
      });
      this.log('✅ WAV player available');
    } catch (error: any) {
      this.log('❌ WAV player failed:', error?.message || 'Unknown error');
    }
  }

  private tryInitializePlaySound(): void {
    try {
      const player = require('play-sound')();
      this.availableOutputs.set('play-sound', {
        type: 'general-audio',
        player: player,
        playAudio: this.playWithPlaySound.bind(this),
        description: 'Cross-platform sound player'
      });
      this.log('✅ Play-sound available');
    } catch (error: any) {
      this.log('❌ Play-sound failed:', error?.message || 'Unknown error');
    }
  }

  private tryInitializeSay(): void {
    try {
      const say = require('say');
      this.availableOutputs.set('tts-only', {
        type: 'text-to-speech',
        tts: say,
        playAudio: this.playWithSay.bind(this),
        description: 'Text-to-Speech (say package)'
      });
      this.log('✅ Say TTS available');
    } catch (error: any) {
      this.log('❌ Say TTS failed:', error?.message || 'Unknown error');
    }
  }

  private tryInitializePowerShellTTS(): void {
    try {
      if (process.platform === 'win32') {
        this.availableOutputs.set('powershell-tts', {
          type: 'powershell-tts',
          playAudio: this.playWithPowerShellTTS.bind(this),
          description: 'PowerShell TTS'
        });
        this.log('✅ PowerShell TTS available');
      }
    } catch (error: any) {
      this.log('❌ PowerShell TTS failed:', error?.message || 'Unknown error');
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
   * Force a specific preferred output method if available
   */
  public setPreferredOutput(output: 'native' | 'wav-player' | 'play-sound' | 'tts-only' | 'powershell-tts'): boolean {
    if (this.availableOutputs.has(output)) {
      this.currentOutput = output;
      const meta = this.availableOutputs.get(output);
      this.log(`🎯 Preferred audio output set to: ${meta.description}`);
      return true;
    }
    this.log(`⚠️ Requested output "${output}" not available on this platform`);
    return false;
  }

  /**
   * Play audio using the best available method
   */
  async playAudio(input: string | Buffer, options?: any): Promise<void> {
    if (!this.currentOutput) {
      this.log('⚠️ No audio output available - simulating playback');
      return;
    }

    const output = this.availableOutputs.get(this.currentOutput);
    try {
      await output.playAudio(input, options);
      this.emit('audioPlayed', { method: this.currentOutput, success: true });
    } catch (error: any) {
      this.log(`❌ Audio playback failed with ${this.currentOutput}:`, error?.message || 'Unknown error');
      
      // Try fallback
      await this.tryFallback(input, options);
    }
  }

  private async tryFallback(input: string | Buffer, options?: any): Promise<void> {
    const currentIndex = this.config.fallbackChain.indexOf(this.currentOutput!);
    
    for (let i = currentIndex + 1; i < this.config.fallbackChain.length; i++) {
      const fallbackType = this.config.fallbackChain[i];
      
      if (this.availableOutputs.has(fallbackType)) {
        this.log(`🔄 Trying fallback: ${fallbackType}`);
        this.currentOutput = fallbackType;
        
        try {
          const fallbackOutput = this.availableOutputs.get(fallbackType);
          await fallbackOutput.playAudio(input, options);
          this.emit('audioPlayed', { method: fallbackType, success: true, fallback: true });
          return;
        } catch (error: any) {
          this.log(`❌ Fallback ${fallbackType} also failed:`, error?.message || 'Unknown error');
        }
      }
    }

    this.log('❌ All audio output methods failed');
    this.emit('audioFailed', { input, error: 'All methods failed' });
  }

  // Specific playback methods

  private async playWithNativeTTS(text: string): Promise<void> {
    if (typeof text !== 'string') {
      throw new Error('Native TTS requires text input');
    }
    const { spawn } = require('child_process');
    const vol = Number(process.env.EVX_TTS_VOLUME ?? 100);
    const rate = Number(process.env.EVX_TTS_RATE ?? 0);
    const psScript = [
      'Add-Type -AssemblyName System.Speech',
      '$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer',
      '$synth.SetOutputToDefaultAudioDevice()',
      `$synth.Volume = ${isNaN(vol) ? 100 : Math.max(0, Math.min(100, vol))}`,
      `$synth.Rate = ${isNaN(rate) ? 0 : Math.max(-10, Math.min(10, rate))}`,
      `$text = @'`,
      `${text}`,
      `'@`,
      '$synth.Speak($text)'
    ].join('\n');

    return new Promise((resolve, reject) => {
      const ps = spawn('powershell', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', psScript]);
      let stderr = '';
      ps.stderr.on('data', (d: Buffer) => (stderr += d.toString()));
      ps.on('error', (err: any) => reject(err));
      ps.on('close', (code: number) => {
        if (code === 0) resolve();
        else reject(new Error(stderr || `PowerShell exited with code ${code}`));
      });
    });
  }

  private async playWithWavPlayer(audioBuffer: Buffer, options?: any): Promise<void> {
    const wavPlayer = this.availableOutputs.get('wav-player').player;
    
    // If input is text, we need to convert it to audio first
    if (typeof audioBuffer === 'string') {
      throw new Error('WAV player requires audio buffer, not text');
    }

    // Save buffer to temp file and play
    const fs = require('fs');
    const path = require('path');
    const tempFile = path.join(__dirname, 'temp_audio.wav');
    
    return new Promise((resolve, reject) => {
      fs.writeFile(tempFile, audioBuffer, (err: any) => {
        if (err) return reject(err);
        
        wavPlayer.play({ path: tempFile })
          .then(() => {
            // Clean up temp file
            fs.unlink(tempFile, () => {});
            resolve();
          })
          .catch(reject);
      });
    });
  }

  private async playWithPlaySound(input: string | Buffer): Promise<void> {
    const player = this.availableOutputs.get('play-sound').player;
    
    if (typeof input === 'string') {
      // For text input, try to play a sound file instead
      throw new Error('Play-sound requires audio file, not text');
    }

    // Similar to WAV player - save to temp file
    const fs = require('fs');
    const path = require('path');
    const tempFile = path.join(__dirname, 'temp_audio.wav');
    
    return new Promise((resolve, reject) => {
      fs.writeFile(tempFile, input, (err: any) => {
        if (err) return reject(err);
        
        player.play(tempFile, (err: any) => {
          // Clean up temp file
          fs.unlink(tempFile, () => {});
          if (err) reject(err);
          else resolve();
        });
      });
    });
  }

  private async playWithSay(text: string, options?: any): Promise<void> {
    if (typeof text !== 'string') {
      throw new Error('Say TTS requires text input');
    }

    const say = this.availableOutputs.get('tts-only').tts;
    
    return new Promise((resolve, reject) => {
      say.speak(text, null, null, (err: any) => {
        if (err) reject(err);
        else resolve();
      });
    });
  }

  private async playWithPowerShellTTS(text: string): Promise<void> {
    if (typeof text !== 'string') {
      throw new Error('PowerShell TTS requires text input');
    }
    const { spawn } = require('child_process');
    const vol = Number(process.env.EVX_TTS_VOLUME ?? 100);
    const rate = Number(process.env.EVX_TTS_RATE ?? 0);
    const psScript = [
      'Add-Type -AssemblyName System.Speech',
      '$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer',
      '$synth.SetOutputToDefaultAudioDevice()',
      `$synth.Volume = ${isNaN(vol) ? 100 : Math.max(0, Math.min(100, vol))}`,
      `$synth.Rate = ${isNaN(rate) ? 0 : Math.max(-10, Math.min(10, rate))}`,
      `$text = @'`,
      `${text}`,
      `'@`,
      '$synth.Speak($text)'
    ].join('\n');

    return new Promise((resolve, reject) => {
      const ps = spawn('powershell', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', psScript]);
      let stderr = '';
      ps.stderr.on('data', (d: Buffer) => (stderr += d.toString()));
      ps.on('error', (err: any) => reject(err));
      ps.on('close', (code: number) => {
        if (code === 0) resolve();
        else reject(new Error(stderr || `PowerShell exited with code ${code}`));
      });
    });
  }

  /**
   * Get status of all available audio outputs
   */
  getStatus(): any {
    return {
      currentOutput: this.currentOutput,
      availableOutputs: Array.from(this.availableOutputs.entries()).map(([key, value]) => ({
        name: key,
        type: value.type,
        description: value.description
      })),
      platform: process.platform,
      architecture: process.arch
    };
  }

  private log(...args: any[]): void {
    if (this.config.enableLogging) {
      console.log('[AudioManager]', ...args);
    }
  }
}

export default CrossPlatformAudioManager;
