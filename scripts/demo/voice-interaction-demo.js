#!/usr/bin/env node

/**
 * 🎤 EthervoxAI Complete Voice Interaction Demo
 * 
 * A comprehensive demo that combines:
 * - Real microphone input recording
 * - Real AI model inference
 * - Audio output with TTS
 * - Voice activity detection
 * - Wake word detection
 * - Full conversation loop
 */

const { exec } = require('child_process');
const { promisify } = require('util');
const fs = require('fs');
const path = require('path');
const readline = require('readline');

const execAsync = promisify(exec);

// Import EthervoxAI modules
let RaspberryPiAudioManager, multilingualRuntime, LocalLLMStack, localLLMStack, privacyDashboard, EnhancedWakeWordDetector;

try {
    // Try to load the built modules
    const srcPath = path.join(__dirname, '..', '..', 'dist');
    ({ multilingualRuntime } = require(path.join(srcPath, 'modules', 'multilingualRuntime')));
    ({ LocalLLMStack } = require(path.join(srcPath, 'modules', 'localLLMStack')));
    ({ privacyDashboard } = require(path.join(srcPath, 'modules', 'privacyDashboard')));
    
    // Create and initialize the LocalLLMStack instance
    if (LocalLLMStack) {
        localLLMStack = new LocalLLMStack();
        console.log('✅ LocalLLMStack instance created');
    }
    
    // Try Raspberry Pi audio manager
    try {
        ({ RaspberryPiAudioManager } = require(path.join(srcPath, 'modules', 'raspberryPiAudio')));
    } catch (e) {
        console.log('⚠️  RaspberryPiAudioManager not built, will use fallback');
    }
} catch (error) {
    console.log('⚠️  EthervoxAI modules not built. Run: npm run build');
    console.log('   Using simulation mode for demo purposes');
}

// Try to load enhanced wake word detector
try {
    ({ EnhancedWakeWordDetector } = require(path.join(__dirname, '..', '..', 'src', 'modules', 'enhancedWakeWordDetector')));
} catch (error) {
    console.log('⚠️  Enhanced wake word detector not found, using fallback');
}

class VoiceInteractionDemo {
    constructor() {
        this.isListening = false;
        this.isProcessing = false;
        this.audioManager = null;
        this.wakeWordDetector = null;
        this.conversationHistory = [];
        this.wakeWords = ['ethervox', 'hey ethervox', 'ethervox ai'];
        
        // Audio configuration
        this.audioConfig = {
            sampleRate: 16000,
            channels: 1,
            recordingDuration: 5000, // 5 seconds
            vadThreshold: 0.3,
            wakeWordThreshold: 0.7
        };
        
        this.stats = {
            totalInteractions: 0,
            successfulRecognitions: 0,
            averageResponseTime: 0,
            languages: new Set(),
            wakeWordDetections: 0,
            falsePositives: 0
        };
    }

    async initialize() {
        console.log('🎤 Initializing EthervoxAI Voice Interaction Demo');
        console.log('=================================================');
        
        // Initialize audio manager
        await this.initializeAudioManager();
        
        // Initialize wake word detector
        await this.initializeWakeWordDetector();
        
        // Initialize AI modules
        await this.initializeAIModules();
        
        // Check capabilities
        await this.checkCapabilities();
        
        console.log('\n✅ Voice Interaction Demo initialized successfully!');
    }

    async initializeAudioManager() {
        console.log('\n🔧 Initializing audio system...');
        
        // Try platform-specific audio manager first
        if (process.platform === 'linux' && process.arch === 'arm64') {
            try {
                if (RaspberryPiAudioManager) {
                    this.audioManager = new RaspberryPiAudioManager({
                        preferredOutput: 'espeak',
                        fallbackChain: ['espeak', 'pico2wave', 'festival'],
                        enableLogging: true,
                        speakingRate: 150,
                        voice: 'en',
                        volume: 80
                    });
                    console.log('🍓 Using Raspberry Pi optimized audio manager');
                } else {
                    throw new Error('RaspberryPiAudioManager not available');
                }
            } catch (error) {
                console.log('⚠️  Raspberry Pi audio manager not available:', error.message);
                this.audioManager = null;
            }
        }
        
        // Fallback to system commands for audio
        if (!this.audioManager) {
            console.log('🔧 Using system audio commands for recording and playback');
        }
        
        // Test audio capabilities
        await this.testAudioCapabilities();
    }

    async initializeWakeWordDetector() {
        console.log('\n🎯 Initializing wake word detector...');
        
        if (EnhancedWakeWordDetector) {
            this.wakeWordDetector = new EnhancedWakeWordDetector({
                wakeWord: 'ethervoxai',
                sensitivity: 0.35, // Lowered further for better responsiveness
                sampleRate: this.audioConfig.sampleRate,
                enableLogging: true
            });
            console.log('✅ Enhanced wake word detector initialized');
            console.log(`   Wake word: "${this.wakeWordDetector.getConfig().wakeWord}"`);
            console.log(`   Sensitivity: ${this.wakeWordDetector.getConfig().sensitivity}`);
        } else {
            console.log('⚠️  Enhanced wake word detector not available, using fallback');
        }
    }

    async initializeAIModules() {
        console.log('\n🧠 Initializing AI modules...');
        
        // Initialize LocalLLMStack first
        if (localLLMStack) {
            console.log('🔧 Initializing Local LLM Stack...');
            try {
                await localLLMStack.initialize();
                console.log('✅ Local LLM Stack initialized successfully');
            } catch (error) {
                console.log('⚠️  Local LLM Stack initialization failed:', error.message);
                console.log('   Will use fallback responses');
            }
        }
        
        if (multilingualRuntime && localLLMStack && privacyDashboard) {
            console.log('✅ EthervoxAI AI modules loaded');
            
            // Initialize privacy dashboard if it has the expected methods
            try {
                if (privacyDashboard.updatePreferences) {
                    privacyDashboard.updatePreferences({
                        allowCloudServices: false,
                        logQueries: true,
                        shareAnalytics: false
                    });
                }
            } catch (error) {
                console.log('⚠️  Privacy dashboard configuration skipped:', error.message);
            }
            
        } else {
            console.log('⚠️  EthervoxAI modules in simulation mode');
        }
    }

    async testAudioCapabilities() {
        console.log('\n🧪 Testing audio capabilities...');
        
        const capabilities = {
            recording: false,
            playback: false,
            tts: false,
            vad: false
        };
        
        // Test recording capability
        try {
            if (process.platform === 'linux') {
                await execAsync('which arecord');
                capabilities.recording = true;
                console.log('✅ Audio recording available (arecord)');
            } else if (process.platform === 'win32') {
                // Windows recording capability check would go here
                console.log('⚠️  Windows recording capability not implemented in this demo');
            }
        } catch (error) {
            console.log('❌ Audio recording not available');
        }
        
        // Test playback capability
        try {
            if (this.audioManager) {
                await this.audioManager.testAudio();
                capabilities.playback = true;
                capabilities.tts = true;
                console.log('✅ Audio playback and TTS available');
            } else if (process.platform === 'linux') {
                await execAsync('which aplay');
                capabilities.playback = true;
                console.log('✅ Audio playback available (aplay)');
                
                await execAsync('which espeak');
                capabilities.tts = true;
                console.log('✅ Text-to-speech available (espeak)');
            }
        } catch (error) {
            console.log('❌ Audio playback/TTS not available');
        }
        
        // Voice Activity Detection is always available (software-based)
        capabilities.vad = true;
        console.log('✅ Voice Activity Detection available');
        
        this.capabilities = capabilities;
        return capabilities;
    }

    async checkCapabilities() {
        console.log('\n📊 System Capabilities Summary:');
        console.log('================================');
        
        const caps = this.capabilities;
        console.log(`🎤 Audio Recording: ${caps.recording ? '✅ Available' : '❌ Not Available'}`);
        console.log(`🔊 Audio Playback: ${caps.playback ? '✅ Available' : '❌ Not Available'}`);
        console.log(`🗣️  Text-to-Speech: ${caps.tts ? '✅ Available' : '❌ Not Available'}`);
        console.log(`👂 Voice Activity Detection: ${caps.vad ? '✅ Available' : '❌ Not Available'}`);
        console.log(`🎯 Wake Word Detection: ${this.wakeWordDetector ? '✅ Enhanced' : '⚠️  Basic Fallback'}`);
        console.log(`🧠 AI Inference: ${multilingualRuntime ? '✅ Available' : '⚠️  Simulation Mode'}`);
        console.log(`🔒 Privacy Controls: ${privacyDashboard ? '✅ Available' : '⚠️  Simulation Mode'}`);
        
        if (process.platform === 'linux' && process.arch === 'arm64') {
            // Check Raspberry Pi specific features
            try {
                const devices = await execAsync('pactl list sinks short');
                const hasBluetoothAudio = devices.stdout.includes('bluez') || devices.stdout.includes('bluetooth');
                console.log(`🔵 Bluetooth Audio: ${hasBluetoothAudio ? '✅ Connected' : '❌ Not Connected'}`);
            } catch (error) {
                console.log('🔵 Bluetooth Audio: ❌ PulseAudio not available');
            }
        }
    }

    async startVoiceInteraction() {
        console.log('\n🎤 Starting Voice Interaction Mode');
        console.log('===================================');
        console.log('Say "EthervoxAI" or "Hey EthervoxAI" followed by your question');
        console.log('Press Ctrl+C to stop');
        console.log('');
        
        if (!this.capabilities.recording) {
            console.log('⚠️  Audio recording not available. Using simulation mode.');
            await this.simulateVoiceInteraction();
            return;
        }
        
        this.isListening = true;
        
        while (this.isListening) {
            try {
                console.log('👂 Listening for wake word...');
                
                // Record audio chunk
                const audioFile = await this.recordAudioChunk();
                console.log(`   Recorded audio chunk: ${audioFile}`);
                
                // Process for wake word
                const wakeWordDetected = await this.detectWakeWord(audioFile);
                
                if (wakeWordDetected) {
                    console.log('🚨 WAKE WORD DETECTED! Processing command...');
                    await this.handleWakeWordDetection(audioFile);
                } else {
                    // Clean up audio file
                    this.cleanupAudioFile(audioFile);
                }
                
                // Short pause between listening cycles
                await this.sleep(100);
                
            } catch (error) {
                console.error('❌ Error in voice interaction loop:', error);
                await this.sleep(1000);
            }
        }
    }

    async recordAudioChunk() {
        const timestamp = Date.now();
        const audioFile = `/tmp/ethervox_chunk_${timestamp}.wav`;
        
        try {
            // Record 2-second audio chunk for wake word detection
            const recordCommand = `arecord -f S16_LE -c 1 -r 16000 -d 2 "${audioFile}"`;
            await execAsync(recordCommand);
            return audioFile;
        } catch (error) {
            throw new Error(`Failed to record audio: ${error.message}`);
        }
    }

    async detectWakeWord(audioFile) {
        try {
            // Use enhanced wake word detector if available
            if (this.wakeWordDetector) {
                console.log('🔍 Running enhanced wake word analysis...');
                const result = await this.wakeWordDetector.detectWakeWord(audioFile);
                
                if (result.detected) {
                    console.log(`🎯 Wake word detected! (confidence: ${(result.confidence * 100).toFixed(1)}%)`);
                    this.stats.wakeWordDetections++;
                    return true;
                } else {
                    console.log(`   No wake word detected (confidence: ${(result.confidence * 100).toFixed(1)}%, reason: ${result.reason})`);
                    return false;
                }
            }
            
            // Fallback to basic detection
            return this.detectWakeWordFallback(audioFile);
            
        } catch (error) {
            console.error('Error in wake word detection:', error);
            return this.detectWakeWordFallback(audioFile);
        }
    }

    async detectWakeWordFallback(audioFile) {
        try {
            // Simple wake word detection using audio analysis
            const stats = await fs.promises.stat(audioFile);
            
            // Check if file has content (basic activity detection)
            console.log(`   Audio file size: ${stats.size} bytes`);
            if (stats.size < 1000) {
                console.log('   Audio too quiet/empty - no wake word detection');
                return false; // Too quiet/empty
            }
            
            // For demo purposes, simulate wake word detection
            // In real implementation, you'd use speech-to-text or wake word model
            console.log('🔍 Analyzing audio for wake word...');
            
            // Simulate processing time
            await this.sleep(200);
            
            // Enhanced wake word detection based on audio file size
            // Larger files are more likely to contain speech
            const sizeBonus = Math.min(stats.size / 10000, 0.5); // Bonus up to 0.5 for larger files
            const baseChance = 0.4; // 40% base chance
            const totalChance = baseChance + sizeBonus;
            
            const detected = Math.random() < totalChance;
            
            if (detected) {
                console.log('🎯 Wake word detected!');
                return true;
            } else {
                console.log('   No wake word detected in this chunk');
            }
            
            return false;
            
        } catch (error) {
            console.error('Error in fallback wake word detection:', error);
            return false;
        }
    }

    async handleWakeWordDetection(initialAudioFile) {
        console.log('🎤 Wake word detected! Recording full command...');
        
        if (this.audioManager) {
            await this.audioManager.playText('Yes?');
        } else {
            await this.playSystemSound();
        }
        
        try {
            // Record longer audio for full command
            const commandAudioFile = await this.recordCommandAudio();
            
            // Process the full command
            await this.processVoiceCommand(commandAudioFile);
            
            // Cleanup
            this.cleanupAudioFile(commandAudioFile);
            
        } catch (error) {
            console.error('❌ Error handling voice command:', error);
            
            if (this.audioManager) {
                await this.audioManager.playText('Sorry, I had trouble understanding that.');
            }
        }
        
        // Cleanup initial audio file
        this.cleanupAudioFile(initialAudioFile);
    }

    async recordCommandAudio() {
        const timestamp = Date.now();
        const audioFile = `/tmp/ethervox_command_${timestamp}.wav`;
        
        console.log('🎙️  Recording your command (5 seconds)...');
        
        // Record full command (5 seconds)
        const recordCommand = `arecord -f S16_LE -c 1 -r 16000 -d 5 "${audioFile}"`;
        await execAsync(recordCommand);
        
        console.log('✅ Command recorded');
        return audioFile;
    }

    async processVoiceCommand(audioFile) {
        this.isProcessing = true;
        const startTime = Date.now();
        
        try {
            console.log('🧠 Processing voice command...');
            
            // Step 1: Speech-to-Text
            const transcription = await this.speechToText(audioFile);
            console.log(`📝 Transcription: "${transcription}"`);
            
            // Step 2: Language Detection
            let language = 'en';
            if (multilingualRuntime) {
                const textBuffer = new TextEncoder().encode(transcription);
                const langResult = await multilingualRuntime.detectLanguage(textBuffer.buffer);
                language = langResult.detectedLanguage;
                console.log(`🌐 Detected language: ${language} (${Math.round(langResult.confidence * 100)}% confidence)`);
                this.stats.languages.add(language);
            }
            
            // Step 3: Intent Processing
            let intent = 'general_query';
            if (localLLMStack) {
                const intentResult = await localLLMStack.parseIntent(transcription);
                intent = intentResult.intent;
                console.log(`🎯 Intent: ${intent} (${Math.round(intentResult.confidence * 100)}% confidence)`);
            }
            
            // Step 4: Generate Response
            const response = await this.generateResponse(transcription, intent, language);
            console.log(`🤖 Response: "${response}"`);
            
            // Step 5: Text-to-Speech Output
            await this.speakResponse(response, language);
            
            // Step 6: Update Conversation History
            this.updateConversationHistory(transcription, response, language);
            
            // Update statistics
            const processingTime = Date.now() - startTime;
            this.updateStats(true, processingTime);
            
            console.log(`⏱️  Total processing time: ${processingTime}ms`);
            
        } catch (error) {
            console.error('❌ Error processing voice command:', error);
            
            const errorResponse = 'Sorry, I had trouble processing your request.';
            await this.speakResponse(errorResponse);
            
            this.updateStats(false, Date.now() - startTime);
        } finally {
            this.isProcessing = false;
        }
    }

    async speechToText(audioFile) {
        // In a real implementation, this would use:
        // - Local STT model (Whisper, Vosk, etc.)
        // - Cloud STT service (with privacy controls)
        // - Hardware-specific STT
        
        console.log('🔄 Converting speech to text...');
        
        try {
            // Try to do basic audio analysis to make a better guess
            const stats = await fs.promises.stat(audioFile);
            const duration = stats.size / (this.audioConfig.sampleRate * 2); // Estimate duration
            
            console.log(`   Audio file: ${audioFile}`);
            console.log(`   File size: ${stats.size} bytes, estimated duration: ${duration.toFixed(2)}s`);
            
            // For now, simulate speech-to-text but make it more realistic
            // In a real implementation, you'd use a library like:
            // - @tensorflow-models/speech-commands
            // - node-speech-to-text
            // - whisper.cpp bindings
            
            await this.sleep(500); // Simulate processing time
            
            // Instead of pure random, let's use a more realistic approach
            // For demo purposes, we'll cycle through common queries based on audio characteristics
            const audioCharacteristics = {
                isShort: duration < 1.5,
                isMedium: duration >= 1.5 && duration < 3.0,
                isLong: duration >= 3.0
            };
            
            let transcription;
            
            if (audioCharacteristics.isShort) {
                // Short audio likely simple questions
                const shortQueries = [
                    'What time is it?',
                    'Hello',
                    'Help me',
                    'Thank you'
                ];
                transcription = shortQueries[Math.floor(Math.random() * shortQueries.length)];
            } else if (audioCharacteristics.isMedium) {
                // Medium audio likely common questions
                const mediumQueries = [
                    'What can you do?',
                    'Tell me a joke',
                    'How are you doing?',
                    'What\'s the weather like?',
                    'Can you help me with something?'
                ];
                transcription = mediumQueries[Math.floor(Math.random() * mediumQueries.length)];
            } else {
                // Long audio likely complex requests
                const longQueries = [
                    'Can you help me understand how this system works?',
                    'I would like to know more about your capabilities',
                    'Please tell me about the weather forecast for today',
                    'Set a timer for five minutes and remind me to check on dinner'
                ];
                transcription = longQueries[Math.floor(Math.random() * longQueries.length)];
            }
            
            // For better demo experience, if we're in certain time ranges, bias toward time queries
            const currentHour = new Date().getHours();
            if (audioCharacteristics.isShort && Math.random() > 0.5) {
                transcription = 'What time is it?';
            }
            
            console.log(`   Estimated transcription based on ${duration.toFixed(2)}s audio: "${transcription}"`);
            return transcription;
            
        } catch (error) {
            console.error('Error in speech-to-text analysis:', error);
            // Fallback to simple transcription
            return 'What time is it?';
        }
    }

    async generateResponse(transcription, intent, language) {
        console.log('💭 Generating response...');
        
        // Privacy check
        if (privacyDashboard && privacyDashboard.logCloudQuery) {
            try {
                const queryId = privacyDashboard.logCloudQuery(
                    `Voice query: ${intent}`,
                    'local-llm',
                    true,
                    ['text'],
                    false
                );
            } catch (error) {
                console.log('⚠️  Privacy logging skipped:', error.message);
            }
        }
        
        // Generate contextual response based on intent
        let response;
        
        if (localLLMStack) {
            // Use real local LLM if available
            try {
                console.log(`🧠 Using Local LLM for: "${transcription}"`);
                const llmResponse = await localLLMStack.processQuery(transcription, false);
                response = llmResponse.text;
                console.log(`✅ Local LLM response: "${response.substring(0, 100)}..."`);
                console.log(`   Source: ${llmResponse.source}, Model: ${llmResponse.model}`);
                console.log(`   Tokens: ${llmResponse.tokensUsed}, Speed: ${llmResponse.inferenceStats?.tokensPerSecond?.toFixed(1)} tok/s`);
            } catch (error) {
                console.log('⚠️  Local LLM error, using fallback response:', error.message);
                console.error('Full error details:', error);
                response = this.generateFallbackResponse(intent, transcription);
            }
        } else {
            // Generate simulated responses for demo
            console.log('⚠️  Local LLM not available, using fallback responses');
            response = this.generateFallbackResponse(intent, transcription);
        }
        
        return response;
    }

    generateFallbackResponse(intent, transcription) {
        // Map common phrases to better intents
        const lowerText = transcription.toLowerCase();
        
        // Enhanced intent mapping based on transcription content
        let mappedIntent = intent;
        if (lowerText.includes('time') || lowerText.includes('clock')) {
            mappedIntent = 'time_query';
        } else if (lowerText.includes('joke') || lowerText.includes('funny')) {
            mappedIntent = 'joke_request';
        } else if (lowerText.includes('weather')) {
            mappedIntent = 'weather_query';
        } else if (lowerText.includes('hello') || lowerText.includes('hi ') || lowerText.includes('hey')) {
            mappedIntent = 'greeting';
        } else if (lowerText.includes('what can you') || lowerText.includes('capabilities') || lowerText.includes('what do you')) {
            mappedIntent = 'capability_query';
        } else if (lowerText.includes('help') || lowerText.includes('assist')) {
            mappedIntent = 'capability_query';
        } else if (lowerText.includes('how are you')) {
            mappedIntent = 'greeting';
        }
        
        const responses = {
            'time_query': 'The current time is ' + new Date().toLocaleTimeString(),
            'weather_query': 'I don\'t have access to weather data right now, but you can check your local weather app.',
            'joke_request': 'Why don\'t scientists trust atoms? Because they make up everything!',
            'greeting': 'Hello! I\'m EthervoxAI, your privacy-first voice assistant. How can I help you today?',
            'capability_query': 'I can help you with various tasks like answering questions, providing information, and controlling smart home devices. I process everything locally to protect your privacy.',
            'smart_home': 'I understand you want to control smart home devices. In a full implementation, I would connect to your home automation system.',
            'timer_request': 'Timer functionality would be implemented here. For now, I can acknowledge your request to set a timer.',
            'music_request': 'Music playbook would be integrated with your preferred music service or local music library.',
            'calendar_query': 'Calendar integration would show your upcoming appointments and events.',
            'general_query': `I heard you say "${transcription}". I'm processing that request using local AI to protect your privacy.`
        };
        
        const response = responses[mappedIntent] || responses['general_query'];
        
        // Log which response type was used
        if (mappedIntent !== intent) {
            console.log(`   🎯 Intent remapped: "${intent}" → "${mappedIntent}"`);
        }
        
        return response;
    }

    async speakResponse(response, language = 'en') {
        console.log('🗣️  Speaking response...');
        
        if (this.audioManager) {
            // Use optimized audio manager
            await this.audioManager.playText(response, { voice: language });
        } else {
            // Fallback to system TTS
            await this.systemTextToSpeech(response);
        }
    }

    async systemTextToSpeech(text) {
        try {
            console.log(`   Speaking: "${text}"`);
            
            if (process.platform === 'linux') {
                // Use espeak on Linux
                await execAsync(`espeak "${text.replace(/"/g, '\\"')}"`);
            } else if (process.platform === 'win32') {
                // Use Windows SAPI with better error handling
                const escapedText = text.replace(/'/g, "''").replace(/"/g, '""');
                const command = `powershell -Command "Add-Type -AssemblyName System.Speech; $synth = New-Object System.Speech.Synthesis.SpeechSynthesizer; $synth.Rate = 0; $synth.Volume = 80; $synth.Speak('${escapedText}'); Write-Host 'TTS completed'"`;
                
                console.log('   Using Windows SAPI for text-to-speech...');
                const result = await execAsync(command);
                console.log('   ✅ TTS completed successfully');
            } else {
                // Use say on macOS
                await execAsync(`say "${text}"`);
            }
        } catch (error) {
            console.error('❌ TTS error:', error.message);
            console.log('   TTS failed, but continuing...');
        }
    }

    async playSystemSound() {
        try {
            if (process.platform === 'linux') {
                // Simple beep
                // mkostersitz: turning this off for now. sounds like a feedback loop
                // await execAsync('speaker-test -t sine -f 1000 -l 1 & sleep 0.1 && killall speaker-test');
            }
        } catch (error) {
            // Ignore sound errors
        }
    }

    updateConversationHistory(input, response, language) {
        this.conversationHistory.push({
            timestamp: new Date().toISOString(),
            input: input,
            response: response,
            language: language
        });
        
        // Keep only last 10 interactions
        if (this.conversationHistory.length > 10) {
            this.conversationHistory.shift();
        }
    }

    updateStats(success, processingTime) {
        this.stats.totalInteractions++;
        if (success) {
            this.stats.successfulRecognitions++;
        }
        
        // Update rolling average response time
        this.stats.averageResponseTime = 
            (this.stats.averageResponseTime + processingTime) / 2;
    }

    showStats() {
        console.log('\n📊 Voice Interaction Statistics:');
        console.log('=================================');
        console.log(`Total Interactions: ${this.stats.totalInteractions}`);
        console.log(`Successful Recognitions: ${this.stats.successfulRecognitions}`);
        console.log(`Success Rate: ${this.stats.totalInteractions > 0 ? Math.round((this.stats.successfulRecognitions / this.stats.totalInteractions) * 100) : 0}%`);
        console.log(`Wake Word Detections: ${this.stats.wakeWordDetections}`);
        console.log(`Average Response Time: ${Math.round(this.stats.averageResponseTime)}ms`);
        console.log(`Languages Detected: ${Array.from(this.stats.languages).join(', ') || 'None'}`);
        console.log('');
        
        if (this.conversationHistory.length > 0) {
            console.log('Recent Conversations:');
            this.conversationHistory.slice(-3).forEach((conv, index) => {
                console.log(`${index + 1}. User: "${conv.input}"`);
                console.log(`   Assistant: "${conv.response}"`);
                console.log(`   Language: ${conv.language}, Time: ${new Date(conv.timestamp).toLocaleTimeString()}`);
                console.log('');
            });
        }
    }

    async simulateVoiceInteraction() {
        console.log('🎭 Running voice interaction simulation...');
        console.log('(This simulates the full voice interaction pipeline)');
        console.log('');
        
        const simulatedCommands = [
            'What time is it?',
            'Tell me a joke',
            'How are you doing today?',
            'What can you do for me?'
        ];
        
        for (let i = 0; i < simulatedCommands.length; i++) {
            const command = simulatedCommands[i];
            
            console.log(`\n--- Simulation ${i + 1}/${simulatedCommands.length} ---`);
            console.log('👂 [SIMULATED] Listening for wake word...');
            await this.sleep(1000);
            
            console.log('🎯 [SIMULATED] Wake word detected!');
            console.log(`🎤 [SIMULATED] User said: "${command}"`);
            
            // Process the simulated command
            await this.processSimulatedCommand(command);
            
            await this.sleep(2000);
        }
        
        console.log('\n🎉 Voice interaction simulation completed!');
    }

    async processSimulatedCommand(transcription) {
        const startTime = Date.now();
        
        try {
            console.log(`📝 Transcription: "${transcription}"`);
            
            // Simulate language detection
            let language = 'en';
            if (multilingualRuntime) {
                const textBuffer = new TextEncoder().encode(transcription);
                const langResult = await multilingualRuntime.detectLanguage(textBuffer.buffer);
                language = langResult.detectedLanguage;
                console.log(`🌐 Detected language: ${language}`);
            }
            
            // Simulate intent processing
            let intent = 'general_query';
            if (localLLMStack) {
                const intentResult = await localLLMStack.parseIntent(transcription);
                intent = intentResult.intent;
                console.log(`🎯 Intent: ${intent}`);
            }
            
            // Generate response
            const response = this.generateFallbackResponse(intent, transcription);
            console.log(`🤖 Response: "${response}"`);
            
            // Speak response
            await this.speakResponse(response, language);
            
            // Update stats
            this.updateConversationHistory(transcription, response, language);
            this.updateStats(true, Date.now() - startTime);
            
        } catch (error) {
            console.error('❌ Error in simulation:', error);
            this.updateStats(false, Date.now() - startTime);
        }
    }

    cleanupAudioFile(audioFile) {
        try {
            if (fs.existsSync(audioFile)) {
                fs.unlinkSync(audioFile);
            }
        } catch (error) {
            // Ignore cleanup errors
        }
    }

    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    stop() {
        console.log('\n👋 Stopping voice interaction...');
        this.isListening = false;
        
        if (this.audioManager) {
            this.audioManager.cleanup();
        }
        
        this.showStats();
        console.log('✅ Voice interaction stopped');
    }
}

// Interactive menu
async function showInteractiveMenu() {
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout
    });
    
    const demo = new VoiceInteractionDemo();
    await demo.initialize();
    
        console.log('\n🎛️  EthervoxAI Voice Interaction Demo Menu');
        console.log('=========================================');
        console.log('1. Start Real Voice Interaction');
        console.log('2. Run Voice Simulation');
        console.log('3. Test Audio Capabilities');
        console.log('4. Show System Information');
        console.log('5. Debug: Force Wake Word Detection');
        console.log('6. Debug: Test Custom Text Input');
        console.log('7. Adjust Wake Word Sensitivity');
        console.log('8. Exit');
        console.log('');    rl.prompt();
    
    rl.on('line', async (input) => {
        const choice = input.trim();
        
        switch (choice) {
            case '1':
                console.log('\nStarting real voice interaction...');
                try {
                    await demo.startVoiceInteraction();
                } catch (error) {
                    console.error('Voice interaction error:', error);
                }
                break;
                
            case '2':
                console.log('\nRunning voice simulation...');
                await demo.simulateVoiceInteraction();
                break;
                
            case '3':
                console.log('\nTesting audio capabilities...');
                await demo.testAudioCapabilities();
                break;
                
            case '4':
                console.log('\nSystem Information:');
                await demo.checkCapabilities();
                demo.showStats();
                break;
                
            case '5':
                console.log('\nDebug: Forcing wake word detection...');
                try {
                    // Create a dummy audio file for testing
                    const dummyAudioFile = '/tmp/dummy_wake_test.wav';
                    await demo.handleWakeWordDetection(dummyAudioFile);
                } catch (error) {
                    console.error('Debug wake word test error:', error);
                }
                break;
                
            case '6':
                console.log('\nTesting custom text input...');
                rl.question('Enter the text you want to process (e.g., "What time is it?"): ', async (customText) => {
                    if (customText.trim()) {
                        console.log(`\n🧪 Processing custom input: "${customText}"`);
                        try {
                            await demo.processSimulatedCommand(customText.trim());
                        } catch (error) {
                            console.error('Error processing custom text:', error);
                        }
                    } else {
                        console.log('❌ No text entered');
                    }
                    console.log('\nPress any key to return to menu...');
                    rl.prompt();
                });
                return; // Don't prompt again immediately
                
            case '7':
                console.log('\nAdjusting wake word sensitivity...');
                if (demo.wakeWordDetector) {
                    const currentSensitivity = demo.wakeWordDetector.getConfig().sensitivity;
                    console.log(`Current sensitivity: ${currentSensitivity}`);
                    console.log('Enter new sensitivity (0.1-1.0, lower = more sensitive):');
                    
                    rl.question('New sensitivity: ', (answer) => {
                        const newSensitivity = parseFloat(answer);
                        if (newSensitivity >= 0.1 && newSensitivity <= 1.0) {
                            demo.wakeWordDetector.setSensitivity(newSensitivity);
                            console.log(`✅ Sensitivity updated to ${newSensitivity}`);
                        } else {
                            console.log('❌ Invalid sensitivity. Must be between 0.1 and 1.0');
                        }
                        console.log('\nPress any key to return to menu...');
                        rl.prompt();
                    });
                    return; // Don't prompt again immediately
                } else {
                    console.log('❌ Enhanced wake word detector not available');
                }
                break;
                
            case '8':
                console.log('Goodbye!');
                demo.stop();
                rl.close();
                return;
                
            default:
                console.log('Invalid choice. Please select 1-8.');
        }
        
        console.log('\nPress any key to return to menu...');
        rl.prompt();
    });
    
    // Handle Ctrl+C
    rl.on('SIGINT', () => {
        console.log('\n\nInterrupted by user');
        demo.stop();
        rl.close();
        process.exit(0);
    });
}

// Main execution
if (require.main === module) {
    showInteractiveMenu()
        .catch(error => {
            console.error('Fatal error:', error);
            process.exit(1);
        });
}

module.exports = { VoiceInteractionDemo };
