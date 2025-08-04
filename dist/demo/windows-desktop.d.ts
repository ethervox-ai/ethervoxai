/**
 * 🖥️ EthervoxAI Windows Desktop Demo
 *
 * Sample application demonstrating real-time audio input/output
 * integration with EthervoxAI voice processing modules.
 */
import { EventEmitter } from 'events';
export declare class WindowsDesktopDemo extends EventEmitter {
    private audioConfig;
    private micInstance;
    private speaker;
    private audioManager;
    private currentSession;
    private isListening;
    private isProcessing;
    constructor();
    /**
     * Initialize the Windows desktop demo
     */
    initialize(): Promise<void>;
    /**
     * Load audio configuration
     */
    private loadAudioConfig;
    /**
     * Initialize EthervoxAI core modules
     */
    private initializeCoreModules;
    /**
     * Setup Windows audio system with cross-platform fallbacks
     */
    private setupAudioSystem;
    /**
     * Setup event handlers
     */
    private setupEventHandlers;
    /**
     * Start listening for voice input
     */
    startListening(): void;
    /**
     * Stop listening for voice input
     */
    stopListening(): void;
    /**
     * Handle incoming audio data
     */
    private handleAudioInput;
    /**
     * Start a new voice session
     */
    private startVoiceSession;
    /**
     * End the current voice session
     */
    private endVoiceSession;
    /**
     * Process speech input through EthervoxAI pipeline
     */
    private processSpeechInput;
    /**
     * Request user consent for cloud services
     */
    private requestCloudConsent;
    /**
     * Generate speech output using cross-platform audio manager
     */
    private generateSpeechOutput;
    /**
     * Calculate audio level for simple VAD
     */
    private calculateAudioLevel;
    /**
     * Simulate speech-to-text conversion
     */
    private simulateSpeechToText;
    /**
     * Simulate voice input for demo purposes
     */
    private simulateVoiceInput;
    /**
     * Start privacy dashboard interface
     */
    private startPrivacyDashboard;
    /**
     * Show current status
     */
    showStatus(): void;
    /**
     * Graceful shutdown
     */
    shutdown(): void;
}
export default WindowsDesktopDemo;
//# sourceMappingURL=windows-desktop.d.ts.map