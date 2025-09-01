/**
 * 🖥️ EthervoxAI Windows Desktop Demo
 * 
 * Sample application demonstrating real-time audio input/output
 * integration with EthervoxAI voice processing modules.
 */

import { EventEmitter } from 'events';
import * as path from 'path';
import * as fs from 'fs';

// Import EthervoxAI core modules
import { multilingualRuntime } from '../modules/multilingualRuntime';
import { localLLMStack } from '../modules/localLLMStack';
import { privacyDashboard } from '../modules/privacyDashboard';
import { CrossPlatformAudioManager } from '../modules/crossPlatformAudio';

// Audio processing imports (will be installed separately)
let mic: any, Speaker: any, wav: any;

try {
  mic = require('mic');
  Speaker = require('speaker');  
  wav = require('wav');
} catch (error) {
  console.warn('⚠️ Advanced audio dependencies not installed.');
  console.warn('   Using cross-platform audio alternatives instead.');
}

interface AudioConfig {
  input: {
    sampleRate: number;
    channels: number;
    bitDepth: number;
    device: string;
  };
  output: {
    sampleRate: number;
    channels: number;
    bitDepth: number; 
    device: string;
  };
  bufferSize: number;
  vadThreshold: number;
}

interface VoiceSession {
  id: string;
  startTime: Date;
  language: string;
  isActive: boolean;
  audioBuffer: Buffer[];
}

export class WindowsDesktopDemo extends EventEmitter {
  private audioConfig: AudioConfig;
  private micInstance: any;
  private speaker: any;
  private audioManager: CrossPlatformAudioManager;
  private currentSession: VoiceSession | null = null;
  private isListening: boolean = false;
  private isProcessing: boolean = false;

  constructor() {
    super();
    this.audioConfig = this.loadAudioConfig();
    this.audioManager = new CrossPlatformAudioManager({
      enableLogging: true,
      preferredOutput: 'native'
    });
    // Allow override via env for troubleshooting (e.g., EVX_AUDIO_OUT=tts-only|powershell-tts|wav-player|play-sound|native)
    const preferred = process.env.EVX_AUDIO_OUT as any;
    if (preferred) {
      try {
        this.audioManager.setPreferredOutput(preferred);
      } catch {}
    }
    this.setupEventHandlers();
  }

  /**
   * Initialize the Windows desktop demo
   */
  async initialize(): Promise<void> {
    console.log('🚀 Initializing EthervoxAI Windows Desktop Demo...');
    
    try {
      // Initialize core modules
      await this.initializeCoreModules();
      
      // Setup audio system
      await this.setupAudioSystem();
      
      // Start privacy dashboard
      this.startPrivacyDashboard();
      
      console.log('✅ Desktop demo initialized successfully!');
      console.log('💬 Say "Hello EthervoxAI" to start voice interaction');
      
      this.emit('initialized');
    } catch (error) {
      console.error('❌ Failed to initialize desktop demo:', error);
      this.emit('error', error);
    }
  }

  /**
   * Load audio configuration
   */
  private loadAudioConfig(): AudioConfig {
    const configPath = path.join(__dirname, '../config/audio.json');
    
    // Default configuration
    const defaultConfig: AudioConfig = {
      input: {
        sampleRate: 16000,
        channels: 1,
        bitDepth: 16,
        device: 'default'
      },
      output: {
        sampleRate: 22050,
        channels: 2,
        bitDepth: 16,
        device: 'default'
      },
      bufferSize: 1024,
      vadThreshold: 0.3
    };

    try {
      if (fs.existsSync(configPath)) {
        const userConfig = JSON.parse(fs.readFileSync(configPath, 'utf8'));
        return { ...defaultConfig, ...userConfig };
      }
    } catch (error) {
      console.warn('⚠️ Could not load audio config, using defaults');
    }

    return defaultConfig;
  }

  /**
   * Initialize EthervoxAI core modules
   */
  private async initializeCoreModules(): Promise<void> {
    console.log('🌐 Setting up multilingual runtime...');
    // Multilingual runtime is automatically initialized
    
    console.log('🧠 Initializing local LLM stack...');
    // Local LLM stack initialization handled by module
    
    console.log('🔐 Configuring privacy dashboard...');
    // Privacy dashboard handles its own initialization
  }

  /**
   * Setup Windows audio system with cross-platform fallbacks
   */
  private async setupAudioSystem(): Promise<void> {
    console.log('🔧 Setting up audio system...');
    
    // Always log the cross-platform audio manager status
    const audioStatus = this.audioManager.getStatus();
    console.log('🎵 Available audio outputs:', audioStatus.availableOutputs.length);
    audioStatus.availableOutputs.forEach((output: any) => {
      console.log(`  • ${output.description} (${output.type})`);
    });
    
    // Try to setup advanced audio (mic/speaker) if available
    let advancedAudioAvailable = false;
    
    if (mic && Speaker) {
      try {
        // Initialize microphone
        this.micInstance = mic({
          rate: this.audioConfig.input.sampleRate,
          channels: this.audioConfig.input.channels,
          debug: false,
          exitOnSilence: 0
        });

        // Initialize speaker (optional, we have cross-platform fallback)
        this.speaker = new Speaker({
          channels: this.audioConfig.output.channels,
          bitDepth: this.audioConfig.output.bitDepth,
          sampleRate: this.audioConfig.output.sampleRate
        });

        advancedAudioAvailable = true;
        console.log('🎤 Advanced audio system configured successfully');
      } catch (error) {
        console.warn('⚠️ Advanced audio setup failed, using cross-platform alternatives');
      }
    }
    
    if (!advancedAudioAvailable) {
      console.log('🎵 Using cross-platform audio system');
      console.log(`   Primary method: ${audioStatus.currentOutput}`);
    }
  }

  /**
   * Setup event handlers
   */
  private setupEventHandlers(): void {
    // Handle process termination gracefully
    process.on('SIGINT', () => {
      console.log('\n👋 Shutting down EthervoxAI desktop demo...');
      this.shutdown();
    });

    process.on('uncaughtException', (error) => {
      console.error('💥 Uncaught exception:', error);
      this.shutdown();
    });
  }

  /**
   * Start listening for voice input
   */
  startListening(): void {
    if (this.isListening) {
      console.log('🎤 Already listening...');
      return;
    }

    if (!this.micInstance) {
      console.log('🎤 [SIMULATION] Starting voice input simulation...');
      this.simulateVoiceInput();
      return;
    }

    console.log('🎤 Starting voice input...');
    this.isListening = true;

    const micInputStream = this.micInstance.getAudioStream();
    
    micInputStream.on('data', (data: Buffer) => {
      this.handleAudioInput(data);
    });

    micInputStream.on('error', (error: Error) => {
      console.error('🎤 Microphone error:', error);
      this.emit('audioError', error);
    });

    this.micInstance.start();
    this.emit('listeningStarted');
  }

  /**
   * Stop listening for voice input
   */
  stopListening(): void {
    if (!this.isListening) return;

    console.log('🔇 Stopping voice input...');
    this.isListening = false;

    if (this.micInstance) {
      this.micInstance.stop();
    }

    if (this.currentSession) {
      this.endVoiceSession();
    }

    this.emit('listenerStopped');
  }

  /**
   * Handle incoming audio data
   */
  private async handleAudioInput(audioData: Buffer): Promise<void> {
    if (!this.isListening || this.isProcessing) return;

    try {
      // Start new voice session if needed
      if (!this.currentSession) {
        this.startVoiceSession();
      }

      // Add audio data to current session
      this.currentSession!.audioBuffer.push(audioData);

      // Simple voice activity detection (VAD)
      const audioLevel = this.calculateAudioLevel(audioData);
      
      if (audioLevel > this.audioConfig.vadThreshold) {
        // Process speech if we have enough audio data
        if (this.currentSession!.audioBuffer.length > 5) {
          await this.processSpeechInput();
        }
      }
    } catch (error) {
      console.error('❌ Error handling audio input:', error);
    }
  }

  /**
   * Start a new voice session
   */
  private startVoiceSession(): void {
    this.currentSession = {
      id: `session_${Date.now()}`,
      startTime: new Date(),
      language: 'en',
      isActive: true,
      audioBuffer: []
    };

    console.log(`🗣️ Started voice session: ${this.currentSession.id}`);
    this.emit('sessionStarted', this.currentSession);
  }

  /**
   * End the current voice session
   */
  private endVoiceSession(): void {
    if (!this.currentSession) return;

    const session = this.currentSession;
    this.currentSession = null;

    console.log(`🏁 Ended voice session: ${session.id}`);
    this.emit('sessionEnded', session);
  }

  /**
   * Process speech input through EthervoxAI pipeline
   */
  private async processSpeechInput(): Promise<void> {
    if (!this.currentSession || this.isProcessing) return;

    this.isProcessing = true;
    console.log('🎯 Processing speech input...');

    try {
      // Combine audio buffers
      const audioBuffer = Buffer.concat(this.currentSession.audioBuffer);
      
      // Log privacy event
      const queryId = privacyDashboard.logCloudQuery(
        'Speech processing request',
        'local-stt',
        true,
        ['audio-buffer'],
        true
      );

      // Simulate speech-to-text conversion
      const transcription = await this.simulateSpeechToText(audioBuffer);
      console.log(`📝 Transcription: "${transcription}"`);

      // Detect language (convert string to ArrayBuffer for the API)
      const textBuffer = new TextEncoder().encode(transcription);
      const detectedLanguage = await multilingualRuntime.detectLanguage(textBuffer.buffer);
      console.log(`🌐 Detected language: ${detectedLanguage.detectedLanguage} (${Math.round(detectedLanguage.confidence * 100)}% confidence)`);

      // Update session language
      this.currentSession.language = detectedLanguage.detectedLanguage;

      // Process through local LLM
      const intentResult = await localLLMStack.parseIntent(transcription);
      console.log(`🧠 Intent: ${intentResult.intent} (${Math.round(intentResult.confidence * 100)}% confidence)`);

      // Generate response
      let response: string;
      if (intentResult.requiresExternalLLM) {
        // Check privacy consent for external LLM
        const consentGranted = await this.requestCloudConsent(intentResult.intent);
        if (consentGranted) {
          const llmResponse = await localLLMStack.routeToExternalLLM(transcription, intentResult, true);
          response = llmResponse ? llmResponse.text : 'Sorry, external service is unavailable.';
          console.log('☁️ Response from external LLM');
        } else {
          const llmResponse = await localLLMStack.generateLocalResponse(transcription, intentResult);
          response = llmResponse.text;
          console.log('🏠 Response from local LLM');
        }
      } else {
        const llmResponse = await localLLMStack.generateLocalResponse(transcription, intentResult);
        response = llmResponse.text;
        console.log('🏠 Response from local LLM');
      }

      console.log(`💬 Response: "${response}"`);

      // Convert response to speech
      await this.generateSpeechOutput(response, this.currentSession.language);

      // Clear audio buffer for next input
      this.currentSession.audioBuffer = [];

    } catch (error) {
      console.error('❌ Error processing speech:', error);
    } finally {
      this.isProcessing = false;
    }
  }

  /**
   * Request user consent for cloud services
   */
  private async requestCloudConsent(intent: string): Promise<boolean> {
    console.log(`🤔 Requesting consent for cloud processing: ${intent}`);
    
    // In a real app, this would show a UI dialog
    // For demo, we'll check privacy settings
    const privacySettings = privacyDashboard.getPrivacySettings();
    
    if (!privacySettings.cloudAccessEnabled) {
      console.log('🚫 Cloud access disabled in privacy settings');
      return false;
    }

    if (privacySettings.cloudAccessPerQuery) {
      console.log('✅ Per-query consent granted (simulated)');
      return true;
    }

    return privacySettings.cloudAccessEnabled;
  }

  /**
   * Generate speech output using cross-platform audio manager
   */
  private async generateSpeechOutput(text: string, language: string): Promise<void> {
    console.log(`🔊 Generating speech output in ${language}...`);

    try {
      // Use the cross-platform audio manager for reliable output
      await this.audioManager.playAudio(text);
      console.log('🔊 Audio output completed');
      
      // Also try to get native audio if available
      try {
        const audioOutput = await multilingualRuntime.generateSpeechOutput(text, language);
        
        if (this.speaker && audioOutput && audioOutput.byteLength > 0) {
          // Use native speaker if available (for higher quality)
          const audioBuffer = Buffer.from(audioOutput);
          this.speaker.write(audioBuffer);
          console.log('🔊 Enhanced audio output played');
        }
      } catch (nativeError) {
        // Native audio failed, but cross-platform audio already handled it
        console.log('🔊 Using cross-platform audio (native unavailable)');
      }

      this.emit('speechGenerated', { text, language });
    } catch (error) {
      console.error('❌ Error generating speech output:', error);
      console.log(`🔊 [FALLBACK] Text: "${text}"`);
    }
  }

  /**
   * Calculate audio level for simple VAD
   */
  private calculateAudioLevel(buffer: Buffer): number {
    let sum = 0;
    for (let i = 0; i < buffer.length; i += 2) {
      const sample = buffer.readInt16LE(i);
      sum += Math.abs(sample);
    }
    return sum / (buffer.length / 2) / 32768;
  }

  /**
   * Simulate speech-to-text conversion
   */
  private async simulateSpeechToText(audioBuffer: Buffer): Promise<string> {
    // In a real implementation, this would use actual STT
    const simulatedPhrases = [
      'Hello EthervoxAI',
      'What is the weather today',
      'Set a timer for 5 minutes',
      'Change language to Spanish',
      'Show privacy settings',
      'Tell me a joke',
      'What time is it',
      'Stop listening'
    ];

    // Simulate processing delay
    await new Promise(resolve => setTimeout(resolve, 500));

    return simulatedPhrases[Math.floor(Math.random() * simulatedPhrases.length)];
  }

  /**
   * Simulate voice input for demo purposes
   */
  private simulateVoiceInput(): void {
    console.log('🎤 [SIMULATION] Voice input active - simulating speech...');
    this.isListening = true;

    // Simulate periodic voice input
    const simulateInput = async () => {
      if (!this.isListening) return;

      this.startVoiceSession();
      
      // Simulate audio data
      const dummyAudioBuffer = Buffer.alloc(1024);
      this.currentSession!.audioBuffer.push(dummyAudioBuffer);
      
      await this.processSpeechInput();
      this.endVoiceSession();

      // Schedule next simulation
      setTimeout(simulateInput, 10000 + Math.random() * 10000);
    };

    // Start first simulation after delay
    setTimeout(simulateInput, 3000);
  }

  /**
   * Start privacy dashboard interface
   */
  private startPrivacyDashboard(): void {
    console.log('🔐 Privacy Dashboard Status:');
    const settings = privacyDashboard.getPrivacySettings();
    console.log(`   Cloud Access: ${settings.cloudAccessEnabled ? 'Enabled' : 'Disabled'}`);
    console.log(`   Per-Query Consent: ${settings.cloudAccessPerQuery ? 'Required' : 'Not Required'}`);
    console.log(`   Audit Logging: ${settings.auditLoggingEnabled ? 'Enabled' : 'Disabled'}`);
    console.log(`   Data Retention: ${settings.dataRetentionDays} days`);

    // Show device status
    const devices = privacyDashboard.getDeviceStatuses();
    console.log(`   Registered Devices: ${devices.length}`);
  }

  /**
   * Show current status
   */
  showStatus(): void {
    console.log('\n📊 EthervoxAI Desktop Demo Status:');
    console.log(`   Listening: ${this.isListening ? '🎤 Active' : '🔇 Inactive'}`);
    console.log(`   Processing: ${this.isProcessing ? '⚡ Active' : '💤 Idle'}`);
    console.log(`   Current Session: ${this.currentSession ? `🗣️ ${this.currentSession.id}` : '❌ None'}`);
    
    const languages = multilingualRuntime.getLanguageProfiles();
    console.log(`   Supported Languages: ${languages.length}`);
    
    const models = localLLMStack.getLocalModels();
    console.log(`   Local Models: ${models.length}`);
    
    const queries = privacyDashboard.getCloudQueryHistory(5);
    console.log(`   Recent Queries: ${queries.length}\n`);
  }

  /**
   * Graceful shutdown
   */
  shutdown(): void {
    console.log('🛑 Shutting down...');
    
    this.stopListening();
    
    if (this.speaker) {
      this.speaker.end();
    }

    console.log('✅ Shutdown complete');
    process.exit(0);
  }
}

// Export for use as module
export default WindowsDesktopDemo;

// CLI interface when run directly
if (require.main === module) {
  const demo = new WindowsDesktopDemo();

  // Setup CLI commands
  process.stdin.setEncoding('utf8');
  process.stdin.on('readable', () => {
    const chunk = process.stdin.read();
    if (chunk) {
      const command = chunk.trim().toLowerCase();
      
      switch (command) {
        case 'start':
        case 'listen':
          demo.startListening();
          break;
        case 'stop':
          demo.stopListening();
          break;
        case 'status':
          demo.showStatus();
          break;
        case 'quit':
        case 'exit':
          demo.shutdown();
          break;
        default:
          console.log('Commands: start, stop, status, quit');
      }
    }
  });

  // Initialize and start demo
  demo.initialize().then(() => {
    console.log('\n💡 Commands available:');
    console.log('   start  - Start voice listening');
    console.log('   stop   - Stop voice listening');
    console.log('   status - Show current status');
    console.log('   quit   - Exit application\n');
    
    demo.startListening();
  }).catch((error) => {
    console.error('Failed to start demo:', error);
    process.exit(1);
  });
}
