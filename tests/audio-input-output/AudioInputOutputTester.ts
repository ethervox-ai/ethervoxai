/**
 * Audio Input/Output Test Application
 * 
 * Comprehensive testing tool for EthervoxAI audio capabilities:
 * - Select input/output devices and channels
 * - Choose audio processing modules  
 * - Record audio through microphone
 * - Playback through various output methods
 * - Test cross-platform audio manager
 */

import { EventEmitter } from 'events';
import * as fs from 'fs';
import * as path from 'path';
import { CrossPlatformAudioManager } from '../../src/modules/crossPlatformAudio';

interface AudioDevice {
  id: string;
  name: string;
  type: 'input' | 'output';
  channels: number;
  sampleRate: number;
  isDefault: boolean;
}

interface RecordingSession {
  id: string;
  startTime: Date;
  duration: number;
  inputDevice: string;
  inputModule: string;
  audioData: Buffer;
  format: {
    sampleRate: number;
    channels: number;
    bitDepth: number;
  };
}

interface TestConfig {
  inputDevice: string;
  outputDevice: string;  
  inputChannels: number;
  outputChannels: number;
  sampleRate: number;
  bitDepth: number;
  recordingDuration: number; // seconds
  selectedModule: string;
}

export class AudioInputOutputTester extends EventEmitter {
  private audioManager: CrossPlatformAudioManager;
  private micInstance: any = null;
  private speakerInstance: any = null;
  private availableDevices: AudioDevice[] = [];
  private currentRecording: RecordingSession | null = null;
  private recordings: RecordingSession[] = [];
  private isRecording: boolean = false;
  private config: TestConfig;

  // Audio modules
  private audioModules: Map<string, any> = new Map();

  constructor() {
    super();
    
    this.config = {
      inputDevice: 'default',
      outputDevice: 'default',
      inputChannels: 1,
      outputChannels: 2,
      sampleRate: 16000,
      bitDepth: 16,
      recordingDuration: 5,
      selectedModule: 'cross-platform'
    };

    this.audioManager = new CrossPlatformAudioManager({
      enableLogging: true,
      preferredOutput: 'native'
    });

    this.initializeAudioModules();
  }

  /**
   * Initialize all available audio modules
   */
  private initializeAudioModules(): void {
    console.log('🔧 Initializing audio modules...');

    // Cross-platform audio manager
    this.audioModules.set('cross-platform', {
      name: 'Cross-Platform Audio Manager',
      description: 'EthervoxAI unified audio system',
      inputSupported: false,
      outputSupported: true,
      record: null,
      playback: (data: any) => this.audioManager.playAudio(data)
    });

    // Native mic/speaker (if available)
    try {
      const mic = require('mic');
      const Speaker = require('speaker');
      
      this.audioModules.set('native', {
        name: 'Native Audio (mic + speaker)',
        description: 'Direct mic/speaker packages',
        inputSupported: true,
        outputSupported: true,
        record: this.recordWithNative.bind(this),
        playback: this.playbackWithNative.bind(this)
      });
      console.log('✅ Native audio module available');
    } catch (error) {
      console.log('⚠️ Native audio module not available');
    }

    // WAV-based recording/playback
    try {
      const wav = require('wav');
      const wavPlayer = require('node-wav-player');
      
      this.audioModules.set('wav', {
        name: 'WAV Audio Processing',
        description: 'WAV file recording and playback',
        inputSupported: true,
        outputSupported: true,
        record: this.recordWithWav.bind(this),
        playback: this.playbackWithWav.bind(this)
      });
      console.log('✅ WAV audio module available');
    } catch (error) {
      console.log('⚠️ WAV audio module not available');
    }

    // TTS-only module
    try {
      const say = require('say');
      
      this.audioModules.set('tts', {
        name: 'Text-to-Speech Only',
        description: 'TTS output, no recording',
        inputSupported: false,
        outputSupported: true,
        record: null,
        playback: this.playbackWithTTS.bind(this)
      });
      console.log('✅ TTS module available');
    } catch (error) {
      console.log('⚠️ TTS module not available');
    }

    console.log(`🎵 ${this.audioModules.size} audio modules initialized`);
  }

  /**
   * Discover available audio devices
   */
  async discoverAudioDevices(): Promise<AudioDevice[]> {
    console.log('🔍 Discovering audio devices...');
    
    this.availableDevices = [];

    // Add default devices
    this.availableDevices.push({
      id: 'default-input',
      name: 'Default Microphone',
      type: 'input',
      channels: 1,
      sampleRate: 16000,
      isDefault: true
    });

    this.availableDevices.push({
      id: 'default-output', 
      name: 'Default Speakers',
      type: 'output',
      channels: 2,
      sampleRate: 44100,
      isDefault: true
    });

    // Try to discover system audio devices (Windows)
    if (process.platform === 'win32') {
      try {
        await this.discoverWindowsAudioDevices();
      } catch (error) {
        console.log('⚠️ Could not discover Windows audio devices:', error.message);
      }
    }

    console.log(`🎤 Found ${this.availableDevices.filter(d => d.type === 'input').length} input devices`);
    console.log(`🔊 Found ${this.availableDevices.filter(d => d.type === 'output').length} output devices`);
    
    return this.availableDevices;
  }

  /**
   * Discover Windows audio devices using PowerShell
   */
  private async discoverWindowsAudioDevices(): Promise<void> {
    const { exec } = require('child_process');
    
    const command = `powershell -Command "Get-WmiObject -Class Win32_SoundDevice | Select-Object Name, DeviceID | ConvertTo-Json"`;
    
    return new Promise((resolve, reject) => {
      exec(command, { timeout: 5000 }, (error: any, stdout: any, stderr: any) => {
        if (error) {
          reject(error);
          return;
        }

        try {
          const devices = JSON.parse(stdout);
          const deviceArray = Array.isArray(devices) ? devices : [devices];
          
          deviceArray.forEach((device: any, index: number) => {
            if (device.Name) {
              // Add as both input and output (we can't easily distinguish)
              this.availableDevices.push({
                id: `win-input-${index}`,
                name: device.Name,
                type: 'input',
                channels: 1,
                sampleRate: 16000,
                isDefault: false
              });
              
              this.availableDevices.push({
                id: `win-output-${index}`,
                name: device.Name,
                type: 'output', 
                channels: 2,
                sampleRate: 44100,
                isDefault: false
              });
            }
          });
          
          resolve();
        } catch (parseError) {
          reject(parseError);
        }
      });
    });
  }

  /**
   * Start recording audio
   */
  async startRecording(): Promise<void> {
    if (this.isRecording) {
      throw new Error('Already recording');
    }

    const selectedModule = this.audioModules.get(this.config.selectedModule);
    if (!selectedModule || !selectedModule.inputSupported) {
      throw new Error(`Module ${this.config.selectedModule} does not support audio input`);
    }

    console.log(`🎤 Starting recording with ${selectedModule.name}...`);
    console.log(`   Duration: ${this.config.recordingDuration}s`);
    console.log(`   Input: ${this.config.inputDevice}`);
    console.log(`   Channels: ${this.config.inputChannels}`);
    console.log(`   Sample Rate: ${this.config.sampleRate}Hz`);

    this.currentRecording = {
      id: `recording-${Date.now()}`,
      startTime: new Date(),
      duration: this.config.recordingDuration,
      inputDevice: this.config.inputDevice,
      inputModule: this.config.selectedModule,
      audioData: Buffer.alloc(0),
      format: {
        sampleRate: this.config.sampleRate,
        channels: this.config.inputChannels,
        bitDepth: this.config.bitDepth
      }
    };

    this.isRecording = true;
    
    try {
      await selectedModule.record();
      this.emit('recordingStarted', this.currentRecording);
    } catch (error) {
      this.isRecording = false;
      this.currentRecording = null;
      throw error;
    }
  }

  /**
   * Stop recording audio
   */
  async stopRecording(): Promise<RecordingSession | null> {
    if (!this.isRecording || !this.currentRecording) {
      return null;
    }

    console.log('🛑 Stopping recording...');
    
    this.isRecording = false;
    
    // Stop microphone if active
    if (this.micInstance) {
      try {
        this.micInstance.stop();
      } catch (error) {
        console.log('⚠️ Error stopping microphone:', error.message);
      }
    }

    const recording = this.currentRecording;
    this.recordings.push(recording);
    this.currentRecording = null;

    // Save recording to file
    await this.saveRecording(recording);

    console.log(`✅ Recording saved: ${recording.id}`);
    this.emit('recordingStopped', recording);
    
    return recording;
  }

  /**
   * Play back recorded audio
   */
  async playbackRecording(recordingId: string): Promise<void> {
    const recording = this.recordings.find(r => r.id === recordingId);
    if (!recording) {
      throw new Error(`Recording ${recordingId} not found`);
    }

    const selectedModule = this.audioModules.get(this.config.selectedModule);
    if (!selectedModule || !selectedModule.outputSupported) {
      throw new Error(`Module ${this.config.selectedModule} does not support audio output`);
    }

    console.log(`🔊 Playing back recording with ${selectedModule.name}...`);
    console.log(`   Recording: ${recording.id}`);
    console.log(`   Duration: ${recording.duration}s`);
    console.log(`   Output: ${this.config.outputDevice}`);

    try {
      await selectedModule.playback(recording.audioData);
      this.emit('playbackCompleted', recording);
    } catch (error) {
      console.error('❌ Playback failed:', error);
      this.emit('playbackFailed', { recording, error });
      throw error;
    }
  }

  /**
   * Test text-to-speech with current output settings
   */
  async testTextToSpeech(text: string): Promise<void> {
    console.log('🗣️ Testing text-to-speech...');
    console.log(`   Text: "${text}"`);
    console.log(`   Output: ${this.config.outputDevice}`);

    try {
      await this.audioManager.playAudio(text);
      console.log('✅ TTS test completed');
    } catch (error) {
      console.error('❌ TTS test failed:', error);
      throw error;
    }
  }

  // Recording implementations for different modules

  private async recordWithNative(): Promise<void> {
    const mic = require('mic');
    
    this.micInstance = mic({
      rate: this.config.sampleRate,
      channels: this.config.inputChannels,
      debug: false,
      exitOnSilence: 0,
      device: this.config.inputDevice !== 'default-input' ? this.config.inputDevice : undefined
    });

    const micInputStream = this.micInstance.getAudioStream();
    const audioChunks: Buffer[] = [];

    micInputStream.on('data', (data: Buffer) => {
      audioChunks.push(data);
    });

    micInputStream.on('error', (err: Error) => {
      console.error('❌ Microphone error:', err);
      this.emit('recordingError', err);
    });

    this.micInstance.start();

    // Stop recording after specified duration
    setTimeout(() => {
      if (this.isRecording && this.currentRecording) {
        this.currentRecording.audioData = Buffer.concat(audioChunks);
        this.stopRecording();
      }
    }, this.config.recordingDuration * 1000);
  }

  private async recordWithWav(): Promise<void> {
    // For WAV recording, we'll use the native mic but save in WAV format
    await this.recordWithNative();
  }

  // Playback implementations for different modules

  private async playbackWithNative(audioData: Buffer): Promise<void> {
    const Speaker = require('speaker');
    
    const speaker = new Speaker({
      channels: this.config.outputChannels,
      bitDepth: this.config.bitDepth,
      sampleRate: this.config.sampleRate
    });

    return new Promise((resolve, reject) => {
      speaker.on('close', resolve);
      speaker.on('error', reject);
      
      speaker.write(audioData);
      speaker.end();
    });
  }

  private async playbackWithWav(audioData: Buffer): Promise<void> {
    const wavPlayer = require('node-wav-player');
    
    // Save to temp WAV file and play
    const tempFile = path.join(__dirname, 'temp_playback.wav');
    
    // Create WAV header and save file
    const wav = require('wav');
    const writer = new wav.Writer({
      sampleRate: this.config.sampleRate,
      channels: this.config.outputChannels,
      bitDepth: this.config.bitDepth
    });

    return new Promise((resolve, reject) => {
      const fileStream = fs.createWriteStream(tempFile);
      writer.pipe(fileStream);
      
      writer.on('finish', async () => {
        try {
          await wavPlayer.play({ path: tempFile });
          fs.unlinkSync(tempFile); // Clean up
          resolve();
        } catch (error) {
          reject(error);
        }
      });
      
      writer.write(audioData);
      writer.end();
    });
  }

  private async playbackWithTTS(text: any): Promise<void> {
    // Convert buffer to text if needed, or use as-is if it's already text
    const textToSpeak = typeof text === 'string' ? text : 'Playback completed - this was recorded audio';
    
    const say = require('say');
    
    return new Promise((resolve, reject) => {
      say.speak(textToSpeak, null, null, (err: any) => {
        if (err) reject(err);
        else resolve();
      });
    });
  }

  /**
   * Save recording to file
   */
  private async saveRecording(recording: RecordingSession): Promise<void> {
    const recordingsDir = path.join(__dirname, 'recordings');
    
    // Create recordings directory if it doesn't exist
    if (!fs.existsSync(recordingsDir)) {
      fs.mkdirSync(recordingsDir, { recursive: true });
    }

    const filename = `${recording.id}.wav`;
    const filepath = path.join(recordingsDir, filename);

    // Create WAV file with proper header
    const wav = require('wav');
    const writer = new wav.Writer(recording.format);
    const fileStream = fs.createWriteStream(filepath);

    return new Promise((resolve, reject) => {
      writer.pipe(fileStream);
      
      writer.on('finish', () => {
        console.log(`💾 Recording saved to: ${filepath}`);
        resolve();
      });
      
      writer.on('error', reject);
      
      writer.write(recording.audioData);
      writer.end();
    });
  }

  /**
   * Get current configuration
   */
  getConfig(): TestConfig {
    return { ...this.config };
  }

  /**
   * Update configuration
   */
  updateConfig(newConfig: Partial<TestConfig>): void {
    this.config = { ...this.config, ...newConfig };
    console.log('⚙️ Configuration updated:', this.config);
  }

  /**
   * Get available audio modules
   */
  getAvailableModules(): Array<{id: string, name: string, description: string, inputSupported: boolean, outputSupported: boolean}> {
    return Array.from(this.audioModules.entries()).map(([key, module]) => ({
      id: key,
      name: module.name,
      description: module.description,
      inputSupported: module.inputSupported,
      outputSupported: module.outputSupported
    }));
  }

  /**
   * Get recording history
   */
  getRecordings(): RecordingSession[] {
    return [...this.recordings];
  }

  /**
   * Get current status
   */
  getStatus(): any {
    return {
      isRecording: this.isRecording,
      currentRecording: this.currentRecording,
      totalRecordings: this.recordings.length,
      availableDevices: this.availableDevices.length,
      availableModules: this.audioModules.size,
      config: this.config,
      audioManagerStatus: this.audioManager.getStatus()
    };
  }
}

export default AudioInputOutputTester;
