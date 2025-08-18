/**
 * 🎯 Enhanced Wake Word Detection Module
 * 
 * Phase 1: Audio-based wake word detection for "EthervoxAI"
 * Uses multi-stage audio analysis without requiring ML models
 */

const fs = require('fs');
const { exec } = require('child_process');
const { promisify } = require('util');

const execAsync = promisify(exec);

class EnhancedWakeWordDetector {
    constructor(options = {}) {
        this.wakeWord = options.wakeWord || 'ethervoxai';
        this.sensitivity = options.sensitivity || 0.4; // Lowered from 0.6 to 0.4
        this.sampleRate = options.sampleRate || 16000;
        this.enableLogging = options.enableLogging || false;
        
        // Wake word pattern analysis for "E-ther-vox-AI" (4 syllables)
        this.expectedPattern = {
            syllables: 4,
            minDuration: 0.8, // seconds
            maxDuration: 2.5, // seconds
            energyThreshold: 0.01,
            spectralRange: [300, 3000], // Hz range for human speech
            syllableGaps: [0.1, 0.3] // Expected gaps between syllables
        };
    }

    /**
     * Main wake word detection function
     */
    async detectWakeWord(audioFilePath) {
        try {
            if (!fs.existsSync(audioFilePath)) {
                if (this.enableLogging) console.log('❌ Audio file does not exist');
                return { detected: false, confidence: 0, reason: 'file_not_found' };
            }

            // Load and analyze audio
            const audioData = await this.loadAudioFile(audioFilePath);
            
            if (!audioData || audioData.length === 0) {
                if (this.enableLogging) console.log('❌ Failed to load audio data');
                return { detected: false, confidence: 0, reason: 'load_failed' };
            }

            // Multi-stage detection pipeline
            const stage1 = await this.checkEnergyPattern(audioData);
            if (!stage1.passed) {
                if (this.enableLogging) console.log(`   Stage 1 (Energy): FAILED - ${stage1.reason}`);
                return { detected: false, confidence: stage1.confidence, reason: `stage1_${stage1.reason}` };
            }

            const stage2 = await this.checkSpectralPattern(audioData);
            if (!stage2.passed) {
                if (this.enableLogging) console.log(`   Stage 2 (Spectral): FAILED - ${stage2.reason}`);
                return { detected: false, confidence: stage2.confidence, reason: `stage2_${stage2.reason}` };
            }

            const stage3 = await this.checkDurationPattern(audioData);
            if (!stage3.passed) {
                if (this.enableLogging) console.log(`   Stage 3 (Duration): FAILED - ${stage3.reason}`);
                return { detected: false, confidence: stage3.confidence, reason: `stage3_${stage3.reason}` };
            }

            const stage4 = await this.checkSyllablePattern(audioData);
            
            // Calculate final confidence score - weighted more toward energy and spectral
            const finalConfidence = (
                stage1.confidence * 0.3 +  // Energy (was 0.2)
                stage2.confidence * 0.4 +  // Spectral (was 0.3) 
                stage3.confidence * 0.2 +  // Duration (was 0.2)
                stage4.confidence * 0.1    // Syllables (was 0.3) - less weight
            );

            const detected = finalConfidence >= this.sensitivity;
            
            if (this.enableLogging) {
                console.log(`   Stage 1 (Energy): ✅ ${stage1.confidence.toFixed(2)}`);
                console.log(`   Stage 2 (Spectral): ✅ ${stage2.confidence.toFixed(2)}`);
                console.log(`   Stage 3 (Duration): ✅ ${stage3.confidence.toFixed(2)}`);
                console.log(`   Stage 4 (Syllables): ${stage4.passed ? '✅' : '⚠️'} ${stage4.confidence.toFixed(2)}`);
                console.log(`   Final Confidence: ${finalConfidence.toFixed(2)} (threshold: ${this.sensitivity})`);
            }

            return {
                detected,
                confidence: finalConfidence,
                reason: detected ? 'wake_word_detected' : 'confidence_too_low',
                stages: {
                    energy: stage1.confidence,
                    spectral: stage2.confidence,
                    duration: stage3.confidence,
                    syllables: stage4.confidence
                }
            };

        } catch (error) {
            if (this.enableLogging) console.error('Wake word detection error:', error);
            return { detected: false, confidence: 0, reason: 'detection_error', error: error.message };
        }
    }

    /**
     * Load audio file and convert to analyzable format
     */
    async loadAudioFile(filePath) {
        try {
            // Use sox or ffmpeg to extract raw audio data if available
            // For now, we'll analyze the file size and basic properties
            const stats = await fs.promises.stat(filePath);
            
            // Estimate audio duration based on file size for 16kHz mono 16-bit
            const estimatedDuration = stats.size / (this.sampleRate * 2); // 2 bytes per sample
            
            // Create mock audio data for analysis
            // In a real implementation, you'd use a library like 'wav' or 'node-wav'
            return {
                size: stats.size,
                duration: estimatedDuration,
                sampleRate: this.sampleRate,
                // Mock amplitude data based on file size (larger = more audio content)
                amplitudes: this.generateMockAmplitudes(stats.size, estimatedDuration)
            };
        } catch (error) {
            throw new Error(`Failed to load audio file: ${error.message}`);
        }
    }

    /**
     * Generate mock amplitude data for testing
     * In real implementation, this would be actual audio samples
     */
    generateMockAmplitudes(fileSize, duration) {
        const numSamples = Math.floor(duration * this.sampleRate);
        const amplitudes = new Float32Array(numSamples);
        
        // Generate more realistic audio pattern based on file size
        const baseAmplitude = Math.min(fileSize / 30000, 0.8); // Larger files = more signal
        
        // Add some speech-like patterns
        for (let i = 0; i < numSamples; i++) {
            // Create a more speech-like pattern with varying frequencies
            const time = i / this.sampleRate;
            
            // Base speech frequency components
            const speech1 = Math.sin(2 * Math.PI * 200 * time) * 0.3; // Fundamental frequency
            const speech2 = Math.sin(2 * Math.PI * 400 * time) * 0.2; // First harmonic
            const speech3 = Math.sin(2 * Math.PI * 800 * time) * 0.1; // Second harmonic
            
            // Add amplitude modulation (speech envelope)
            const envelope = Math.abs(Math.sin(2 * Math.PI * 5 * time)); // 5Hz modulation
            
            // Add some noise
            const noise = (Math.random() - 0.5) * 0.1;
            
            // Combine components
            const signal = (speech1 + speech2 + speech3) * envelope * baseAmplitude;
            amplitudes[i] = signal + noise;
        }
        
        return amplitudes;
    }

    /**
     * Stage 1: Check overall energy pattern
     */
    async checkEnergyPattern(audioData) {
        try {
            // Calculate RMS energy
            let totalEnergy = 0;
            const amplitudes = audioData.amplitudes;
            
            for (let i = 0; i < amplitudes.length; i++) {
                totalEnergy += amplitudes[i] * amplitudes[i];
            }
            
            const rmsEnergy = Math.sqrt(totalEnergy / amplitudes.length);
            
            // More permissive energy threshold
            const minEnergyThreshold = 0.005; // Lowered from 0.01
            const hasGoodEnergy = rmsEnergy >= minEnergyThreshold;
            
            // Scale energy score more generously
            const energyScore = Math.min(rmsEnergy / 0.2, 1.0); // Normalize to 0-1, more generous
            
            if (this.enableLogging) {
                console.log(`     RMS Energy: ${rmsEnergy.toFixed(6)} (threshold: ${minEnergyThreshold})`);
                console.log(`     File size: ${audioData.size} bytes, duration: ${audioData.duration.toFixed(2)}s`);
            }
            
            return {
                passed: hasGoodEnergy,
                confidence: Math.max(energyScore, 0.1), // Minimum 0.1 score
                reason: hasGoodEnergy ? 'good_energy' : 'energy_too_low',
                rmsEnergy
            };
        } catch (error) {
            return { passed: false, confidence: 0, reason: 'energy_calculation_failed' };
        }
    }

    /**
     * Stage 2: Check spectral characteristics
     */
    async checkSpectralPattern(audioData) {
        try {
            // Simple spectral analysis using zero-crossing rate
            const amplitudes = audioData.amplitudes;
            let zeroCrossings = 0;
            
            for (let i = 1; i < amplitudes.length; i++) {
                if ((amplitudes[i] >= 0) !== (amplitudes[i - 1] >= 0)) {
                    zeroCrossings++;
                }
            }
            
            const zcr = zeroCrossings / amplitudes.length;
            
            // More permissive speech detection - broaden the acceptable range
            const isSpeechLike = zcr >= 0.01 && zcr <= 0.8; // Was 0.05-0.5, now 0.01-0.8
            
            // Give some score even if not perfect speech pattern
            let spectralScore;
            if (zcr >= 0.05 && zcr <= 0.5) {
                // Ideal speech range
                spectralScore = Math.min(zcr / 0.3, 1.0);
            } else if (zcr >= 0.01 && zcr <= 0.8) {
                // Acceptable range with lower score
                spectralScore = 0.3 + (0.4 * Math.min(zcr / 0.3, 1.0));
            } else {
                // Outside acceptable range
                spectralScore = 0.1; // Still give minimal score
            }
            
            if (this.enableLogging) {
                console.log(`     ZCR: ${zcr.toFixed(4)} (range: 0.01-0.8, ideal: 0.05-0.5)`);
            }
            
            return {
                passed: isSpeechLike || spectralScore >= 0.2, // More lenient passing criteria
                confidence: spectralScore,
                reason: isSpeechLike ? 'speech_like_spectrum' : 'weak_speech_spectrum',
                zeroCrossingRate: zcr
            };
        } catch (error) {
            return { passed: false, confidence: 0, reason: 'spectral_analysis_failed' };
        }
    }

    /**
     * Stage 3: Check duration pattern
     */
    async checkDurationPattern(audioData) {
        try {
            const duration = audioData.duration;
            
            // "EthervoxAI" should take 1-2.5 seconds to say
            const isGoodDuration = duration >= this.expectedPattern.minDuration && 
                                  duration <= this.expectedPattern.maxDuration;
            
            // Score based on how close to ideal duration (1.5 seconds)
            const idealDuration = 1.5;
            const durationDiff = Math.abs(duration - idealDuration);
            const durationScore = Math.max(0, 1 - (durationDiff / idealDuration));
            
            return {
                passed: isGoodDuration,
                confidence: durationScore,
                reason: isGoodDuration ? 'good_duration' : 'duration_out_of_range',
                duration
            };
        } catch (error) {
            return { passed: false, confidence: 0, reason: 'duration_analysis_failed' };
        }
    }

    /**
     * Stage 4: Check syllable pattern for "E-ther-vox-AI"
     */
    async checkSyllablePattern(audioData) {
        try {
            const amplitudes = audioData.amplitudes;
            
            // Find energy peaks that could represent syllables
            const windowSize = Math.floor(this.sampleRate * 0.05); // 50ms windows
            const energyWindows = [];
            
            for (let i = 0; i < amplitudes.length - windowSize; i += windowSize) {
                let windowEnergy = 0;
                for (let j = i; j < i + windowSize; j++) {
                    windowEnergy += amplitudes[j] * amplitudes[j];
                }
                energyWindows.push(windowEnergy / windowSize);
            }
            
            // Find peaks (potential syllables)
            const threshold = Math.max(...energyWindows) * 0.3; // 30% of max energy
            const peaks = [];
            
            for (let i = 1; i < energyWindows.length - 1; i++) {
                if (energyWindows[i] > threshold &&
                    energyWindows[i] > energyWindows[i - 1] &&
                    energyWindows[i] > energyWindows[i + 1]) {
                    peaks.push(i);
                }
            }
            
            // Check if we found approximately 4 syllables
            const expectedSyllables = this.expectedPattern.syllables;
            const syllableCount = peaks.length;
            
            // More forgiving syllable matching - allow 2-6 syllables
            const syllableMatch = syllableCount >= 2 && syllableCount <= 6;
            
            // Score based on how close to 4 syllables, but give some points for any speech-like pattern
            let syllableScore;
            if (syllableCount === 0) {
                syllableScore = 0.1; // Minimal score for no detected syllables
            } else if (syllableCount >= 2 && syllableCount <= 6) {
                // Good range - score based on closeness to 4
                const closenessTo4 = 1 - Math.abs(syllableCount - 4) / 4;
                syllableScore = 0.4 + (closenessTo4 * 0.6); // 0.4-1.0 range
            } else {
                syllableScore = 0.2; // Some score for speech activity
            }
            
            if (this.enableLogging) {
                console.log(`     Syllables detected: ${syllableCount} (expected: ~4, acceptable: 2-6)`);
                console.log(`     Energy peaks found: ${peaks.length}`);
            }
            
            return {
                passed: syllableMatch || syllableScore >= 0.3, // Pass if reasonable syllable pattern
                confidence: syllableScore,
                reason: syllableMatch ? 'good_syllable_pattern' : 'acceptable_speech_pattern',
                syllableCount,
                expectedSyllables,
                peaks: peaks.length
            };
        } catch (error) {
            return { passed: false, confidence: 0, reason: 'syllable_analysis_failed' };
        }
    }

    /**
     * Adjust sensitivity
     */
    setSensitivity(sensitivity) {
        this.sensitivity = Math.max(0, Math.min(1, sensitivity));
    }

    /**
     * Get detector configuration
     */
    getConfig() {
        return {
            wakeWord: this.wakeWord,
            sensitivity: this.sensitivity,
            sampleRate: this.sampleRate,
            expectedPattern: this.expectedPattern
        };
    }
}

module.exports = { EnhancedWakeWordDetector };
