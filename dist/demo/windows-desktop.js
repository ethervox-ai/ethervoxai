"use strict";
/**
 * 🖥️ EthervoxAI Windows Desktop Demo
 *
 * Sample application demonstrating real-time audio input/output
 * integration with EthervoxAI voice processing modules.
 */
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.WindowsDesktopDemo = void 0;
const events_1 = require("events");
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
// Import EthervoxAI core modules
const multilingualRuntime_1 = require("../modules/multilingualRuntime");
const localLLMStack_1 = require("../modules/localLLMStack");
const privacyDashboard_1 = require("../modules/privacyDashboard");
const crossPlatformAudio_1 = require("../modules/crossPlatformAudio");
// Audio processing imports (will be installed separately)
let mic, Speaker, wav;
try {
    mic = require('mic');
    Speaker = require('speaker');
    wav = require('wav');
}
catch (error) {
    console.warn('⚠️ Advanced audio dependencies not installed.');
    console.warn('   Using cross-platform audio alternatives instead.');
}
class WindowsDesktopDemo extends events_1.EventEmitter {
    constructor() {
        super();
        this.currentSession = null;
        this.isListening = false;
        this.isProcessing = false;
        this.audioConfig = this.loadAudioConfig();
        this.audioManager = new crossPlatformAudio_1.CrossPlatformAudioManager({
            enableLogging: true,
            preferredOutput: 'native'
        });
        this.setupEventHandlers();
    }
    /**
     * Initialize the Windows desktop demo
     */
    async initialize() {
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
        }
        catch (error) {
            console.error('❌ Failed to initialize desktop demo:', error);
            this.emit('error', error);
        }
    }
    /**
     * Load audio configuration
     */
    loadAudioConfig() {
        const configPath = path.join(__dirname, '../config/audio.json');
        // Default configuration
        const defaultConfig = {
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
        }
        catch (error) {
            console.warn('⚠️ Could not load audio config, using defaults');
        }
        return defaultConfig;
    }
    /**
     * Initialize EthervoxAI core modules
     */
    async initializeCoreModules() {
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
    async setupAudioSystem() {
        console.log('🔧 Setting up audio system...');
        // Always log the cross-platform audio manager status
        const audioStatus = this.audioManager.getStatus();
        console.log('🎵 Available audio outputs:', audioStatus.availableOutputs.length);
        audioStatus.availableOutputs.forEach((output) => {
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
            }
            catch (error) {
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
    setupEventHandlers() {
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
    startListening() {
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
        micInputStream.on('data', (data) => {
            this.handleAudioInput(data);
        });
        micInputStream.on('error', (error) => {
            console.error('🎤 Microphone error:', error);
            this.emit('audioError', error);
        });
        this.micInstance.start();
        this.emit('listeningStarted');
    }
    /**
     * Stop listening for voice input
     */
    stopListening() {
        if (!this.isListening)
            return;
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
    async handleAudioInput(audioData) {
        if (!this.isListening || this.isProcessing)
            return;
        try {
            // Start new voice session if needed
            if (!this.currentSession) {
                this.startVoiceSession();
            }
            // Add audio data to current session
            this.currentSession.audioBuffer.push(audioData);
            // Simple voice activity detection (VAD)
            const audioLevel = this.calculateAudioLevel(audioData);
            if (audioLevel > this.audioConfig.vadThreshold) {
                // Process speech if we have enough audio data
                if (this.currentSession.audioBuffer.length > 5) {
                    await this.processSpeechInput();
                }
            }
        }
        catch (error) {
            console.error('❌ Error handling audio input:', error);
        }
    }
    /**
     * Start a new voice session
     */
    startVoiceSession() {
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
    endVoiceSession() {
        if (!this.currentSession)
            return;
        const session = this.currentSession;
        this.currentSession = null;
        console.log(`🏁 Ended voice session: ${session.id}`);
        this.emit('sessionEnded', session);
    }
    /**
     * Process speech input through EthervoxAI pipeline
     */
    async processSpeechInput() {
        if (!this.currentSession || this.isProcessing)
            return;
        this.isProcessing = true;
        console.log('🎯 Processing speech input...');
        try {
            // Combine audio buffers
            const audioBuffer = Buffer.concat(this.currentSession.audioBuffer);
            // Log privacy event
            const queryId = privacyDashboard_1.privacyDashboard.logCloudQuery('Speech processing request', 'local-stt', true, ['audio-buffer'], true);
            // Simulate speech-to-text conversion
            const transcription = await this.simulateSpeechToText(audioBuffer);
            console.log(`📝 Transcription: "${transcription}"`);
            // Detect language (convert string to ArrayBuffer for the API)
            const textBuffer = new TextEncoder().encode(transcription);
            const detectedLanguage = await multilingualRuntime_1.multilingualRuntime.detectLanguage(textBuffer.buffer);
            console.log(`🌐 Detected language: ${detectedLanguage.detectedLanguage} (${Math.round(detectedLanguage.confidence * 100)}% confidence)`);
            // Update session language
            this.currentSession.language = detectedLanguage.detectedLanguage;
            // Process through local LLM
            const intentResult = await localLLMStack_1.localLLMStack.parseIntent(transcription);
            console.log(`🧠 Intent: ${intentResult.intent} (${Math.round(intentResult.confidence * 100)}% confidence)`);
            // Generate response
            let response;
            if (intentResult.requiresExternalLLM) {
                // Check privacy consent for external LLM
                const consentGranted = await this.requestCloudConsent(intentResult.intent);
                if (consentGranted) {
                    const llmResponse = await localLLMStack_1.localLLMStack.routeToExternalLLM(transcription, intentResult, true);
                    response = llmResponse ? llmResponse.text : 'Sorry, external service is unavailable.';
                    console.log('☁️ Response from external LLM');
                }
                else {
                    const llmResponse = await localLLMStack_1.localLLMStack.generateLocalResponse(transcription, intentResult);
                    response = llmResponse.text;
                    console.log('🏠 Response from local LLM');
                }
            }
            else {
                const llmResponse = await localLLMStack_1.localLLMStack.generateLocalResponse(transcription, intentResult);
                response = llmResponse.text;
                console.log('🏠 Response from local LLM');
            }
            console.log(`💬 Response: "${response}"`);
            // Convert response to speech
            await this.generateSpeechOutput(response, this.currentSession.language);
            // Clear audio buffer for next input
            this.currentSession.audioBuffer = [];
        }
        catch (error) {
            console.error('❌ Error processing speech:', error);
        }
        finally {
            this.isProcessing = false;
        }
    }
    /**
     * Request user consent for cloud services
     */
    async requestCloudConsent(intent) {
        console.log(`🤔 Requesting consent for cloud processing: ${intent}`);
        // In a real app, this would show a UI dialog
        // For demo, we'll check privacy settings
        const privacySettings = privacyDashboard_1.privacyDashboard.getPrivacySettings();
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
    async generateSpeechOutput(text, language) {
        console.log(`🔊 Generating speech output in ${language}...`);
        try {
            // Use the cross-platform audio manager for reliable output
            await this.audioManager.playAudio(text);
            console.log('🔊 Audio output completed');
            // Also try to get native audio if available
            try {
                const audioOutput = await multilingualRuntime_1.multilingualRuntime.generateSpeechOutput(text, language);
                if (this.speaker && audioOutput && audioOutput.byteLength > 0) {
                    // Use native speaker if available (for higher quality)
                    const audioBuffer = Buffer.from(audioOutput);
                    this.speaker.write(audioBuffer);
                    console.log('🔊 Enhanced audio output played');
                }
            }
            catch (nativeError) {
                // Native audio failed, but cross-platform audio already handled it
                console.log('🔊 Using cross-platform audio (native unavailable)');
            }
            this.emit('speechGenerated', { text, language });
        }
        catch (error) {
            console.error('❌ Error generating speech output:', error);
            console.log(`🔊 [FALLBACK] Text: "${text}"`);
        }
    }
    /**
     * Calculate audio level for simple VAD
     */
    calculateAudioLevel(buffer) {
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
    async simulateSpeechToText(audioBuffer) {
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
    simulateVoiceInput() {
        console.log('🎤 [SIMULATION] Voice input active - simulating speech...');
        this.isListening = true;
        // Simulate periodic voice input
        const simulateInput = async () => {
            if (!this.isListening)
                return;
            this.startVoiceSession();
            // Simulate audio data
            const dummyAudioBuffer = Buffer.alloc(1024);
            this.currentSession.audioBuffer.push(dummyAudioBuffer);
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
    startPrivacyDashboard() {
        console.log('🔐 Privacy Dashboard Status:');
        const settings = privacyDashboard_1.privacyDashboard.getPrivacySettings();
        console.log(`   Cloud Access: ${settings.cloudAccessEnabled ? 'Enabled' : 'Disabled'}`);
        console.log(`   Per-Query Consent: ${settings.cloudAccessPerQuery ? 'Required' : 'Not Required'}`);
        console.log(`   Audit Logging: ${settings.auditLoggingEnabled ? 'Enabled' : 'Disabled'}`);
        console.log(`   Data Retention: ${settings.dataRetentionDays} days`);
        // Show device status
        const devices = privacyDashboard_1.privacyDashboard.getDeviceStatuses();
        console.log(`   Registered Devices: ${devices.length}`);
    }
    /**
     * Show current status
     */
    showStatus() {
        console.log('\n📊 EthervoxAI Desktop Demo Status:');
        console.log(`   Listening: ${this.isListening ? '🎤 Active' : '🔇 Inactive'}`);
        console.log(`   Processing: ${this.isProcessing ? '⚡ Active' : '💤 Idle'}`);
        console.log(`   Current Session: ${this.currentSession ? `🗣️ ${this.currentSession.id}` : '❌ None'}`);
        const languages = multilingualRuntime_1.multilingualRuntime.getLanguageProfiles();
        console.log(`   Supported Languages: ${languages.length}`);
        const models = localLLMStack_1.localLLMStack.getLocalModels();
        console.log(`   Local Models: ${models.length}`);
        const queries = privacyDashboard_1.privacyDashboard.getCloudQueryHistory(5);
        console.log(`   Recent Queries: ${queries.length}\n`);
    }
    /**
     * Graceful shutdown
     */
    shutdown() {
        console.log('🛑 Shutting down...');
        this.stopListening();
        if (this.speaker) {
            this.speaker.end();
        }
        console.log('✅ Shutdown complete');
        process.exit(0);
    }
}
exports.WindowsDesktopDemo = WindowsDesktopDemo;
// Export for use as module
exports.default = WindowsDesktopDemo;
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
//# sourceMappingURL=windows-desktop.js.map