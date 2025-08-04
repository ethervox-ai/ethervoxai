/**
 * Cross-Platform Audio Manager for EthervoxAI
 * Provides audio output using multiple fallback strategies for maximum compatibility
 */
import { EventEmitter } from 'events';
interface AudioManagerConfig {
    preferredOutput: 'native' | 'wav-player' | 'play-sound' | 'tts-only';
    fallbackChain: string[];
    enableLogging: boolean;
}
export declare class CrossPlatformAudioManager extends EventEmitter {
    private availableOutputs;
    private currentOutput;
    private config;
    constructor(config?: Partial<AudioManagerConfig>);
    /**
     * Initialize all available audio output methods
     */
    private initializeOutputMethods;
    private tryInitializeNativeTTS;
    private tryInitializeWavPlayer;
    private tryInitializePlaySound;
    private tryInitializeSay;
    private tryInitializePowerShellTTS;
    private selectBestOutput;
    /**
     * Play audio using the best available method
     */
    playAudio(input: string | Buffer, options?: any): Promise<void>;
    private tryFallback;
    private playWithNativeTTS;
    private playWithWavPlayer;
    private playWithPlaySound;
    private playWithSay;
    private playWithPowerShellTTS;
    /**
     * Get status of all available audio outputs
     */
    getStatus(): any;
    private log;
}
export default CrossPlatformAudioManager;
//# sourceMappingURL=crossPlatformAudio.d.ts.map