"use strict";
/**
 * Cross-Platform Audio Manager for EthervoxAI
 * Provides audio output using multiple fallback strategies for maximum compatibility
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.CrossPlatformAudioManager = void 0;
const events_1 = require("events");
class CrossPlatformAudioManager extends events_1.EventEmitter {
    constructor(config) {
        super();
        this.availableOutputs = new Map();
        this.currentOutput = null;
        this.config = {
            preferredOutput: 'native',
            fallbackChain: ['native', 'wav-player', 'play-sound', 'tts-only'],
            enableLogging: true,
            ...config
        };
        this.initializeOutputMethods();
    }
    /**
     * Initialize all available audio output methods
     */
    initializeOutputMethods() {
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
    tryInitializeNativeTTS() {
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
        }
        catch (error) {
            this.log('❌ Native TTS failed:', error?.message || 'Unknown error');
        }
    }
    tryInitializeWavPlayer() {
        try {
            const wavPlayer = require('node-wav-player');
            this.availableOutputs.set('wav-player', {
                type: 'wav-file',
                player: wavPlayer,
                playAudio: this.playWithWavPlayer.bind(this),
                description: 'WAV file player (pure JS)'
            });
            this.log('✅ WAV player available');
        }
        catch (error) {
            this.log('❌ WAV player failed:', error?.message || 'Unknown error');
        }
    }
    tryInitializePlaySound() {
        try {
            const player = require('play-sound')();
            this.availableOutputs.set('play-sound', {
                type: 'general-audio',
                player: player,
                playAudio: this.playWithPlaySound.bind(this),
                description: 'Cross-platform sound player'
            });
            this.log('✅ Play-sound available');
        }
        catch (error) {
            this.log('❌ Play-sound failed:', error?.message || 'Unknown error');
        }
    }
    tryInitializeSay() {
        try {
            const say = require('say');
            this.availableOutputs.set('tts-only', {
                type: 'text-to-speech',
                tts: say,
                playAudio: this.playWithSay.bind(this),
                description: 'Text-to-Speech (say package)'
            });
            this.log('✅ Say TTS available');
        }
        catch (error) {
            this.log('❌ Say TTS failed:', error?.message || 'Unknown error');
        }
    }
    tryInitializePowerShellTTS() {
        try {
            if (process.platform === 'win32') {
                const ps = require('node-powershell');
                this.availableOutputs.set('powershell-tts', {
                    type: 'powershell-tts',
                    ps: ps,
                    playAudio: this.playWithPowerShellTTS.bind(this),
                    description: 'PowerShell TTS'
                });
                this.log('✅ PowerShell TTS available');
            }
        }
        catch (error) {
            this.log('❌ PowerShell TTS failed:', error?.message || 'Unknown error');
        }
    }
    selectBestOutput() {
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
     * Play audio using the best available method
     */
    async playAudio(input, options) {
        if (!this.currentOutput) {
            this.log('⚠️ No audio output available - simulating playback');
            return;
        }
        const output = this.availableOutputs.get(this.currentOutput);
        try {
            await output.playAudio(input, options);
            this.emit('audioPlayed', { method: this.currentOutput, success: true });
        }
        catch (error) {
            this.log(`❌ Audio playback failed with ${this.currentOutput}:`, error?.message || 'Unknown error');
            // Try fallback
            await this.tryFallback(input, options);
        }
    }
    async tryFallback(input, options) {
        const currentIndex = this.config.fallbackChain.indexOf(this.currentOutput);
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
                }
                catch (error) {
                    this.log(`❌ Fallback ${fallbackType} also failed:`, error?.message || 'Unknown error');
                }
            }
        }
        this.log('❌ All audio output methods failed');
        this.emit('audioFailed', { input, error: 'All methods failed' });
    }
    // Specific playback methods
    async playWithNativeTTS(text) {
        if (typeof text !== 'string') {
            throw new Error('Native TTS requires text input');
        }
        const { exec } = require('child_process');
        const command = `powershell -Command "Add-Type -AssemblyName System.Speech; $synth = New-Object System.Speech.Synthesis.SpeechSynthesizer; $synth.Speak('${text.replace(/'/g, "''")}');"`;
        return new Promise((resolve, reject) => {
            exec(command, (error, stdout, stderr) => {
                if (error)
                    reject(error);
                else
                    resolve();
            });
        });
    }
    async playWithWavPlayer(audioBuffer, options) {
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
            fs.writeFile(tempFile, audioBuffer, (err) => {
                if (err)
                    return reject(err);
                wavPlayer.play({ path: tempFile })
                    .then(() => {
                    // Clean up temp file
                    fs.unlink(tempFile, () => { });
                    resolve();
                })
                    .catch(reject);
            });
        });
    }
    async playWithPlaySound(input) {
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
            fs.writeFile(tempFile, input, (err) => {
                if (err)
                    return reject(err);
                player.play(tempFile, (err) => {
                    // Clean up temp file
                    fs.unlink(tempFile, () => { });
                    if (err)
                        reject(err);
                    else
                        resolve();
                });
            });
        });
    }
    async playWithSay(text, options) {
        if (typeof text !== 'string') {
            throw new Error('Say TTS requires text input');
        }
        const say = this.availableOutputs.get('tts-only').tts;
        return new Promise((resolve, reject) => {
            say.speak(text, null, null, (err) => {
                if (err)
                    reject(err);
                else
                    resolve();
            });
        });
    }
    async playWithPowerShellTTS(text) {
        if (typeof text !== 'string') {
            throw new Error('PowerShell TTS requires text input');
        }
        const ps = new (this.availableOutputs.get('powershell-tts').ps)({
            executionPolicy: 'Bypass',
            noProfile: true
        });
        try {
            ps.addCommand(`Add-Type -AssemblyName System.Speech`);
            ps.addCommand(`$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer`);
            ps.addCommand(`$synth.Speak("${text.replace(/"/g, '`"')}")`);
            await ps.invoke();
        }
        finally {
            ps.dispose();
        }
    }
    /**
     * Get status of all available audio outputs
     */
    getStatus() {
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
    log(...args) {
        if (this.config.enableLogging) {
            console.log('[AudioManager]', ...args);
        }
    }
}
exports.CrossPlatformAudioManager = CrossPlatformAudioManager;
exports.default = CrossPlatformAudioManager;
//# sourceMappingURL=crossPlatformAudio.js.map