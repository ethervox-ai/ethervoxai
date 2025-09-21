"""
🎙️ EthervoxAI Voice Processing Module

Handles speech-to-text, text-to-speech, wake word detection, and voice activity detection
for the EthervoxAI MVP implementation.
"""

import asyncio
import logging
import threading
import time
from typing import Optional, Callable, Dict, Any, List
from dataclasses import dataclass
from enum import Enum
import queue

try:
    import sounddevice as sd
    import soundfile as sf
    import numpy as np
    from scipy import signal
    import speech_recognition as sr
    import pyttsx3
    import webrtcvad
    import whisper
    from transformers import pipeline
    AUDIO_DEPENDENCIES_AVAILABLE = True
except ImportError as e:
    AUDIO_DEPENDENCIES_AVAILABLE = False
    logging.warning(f"Audio dependencies not available: {e}")

logger = logging.getLogger(__name__)

class VoiceProcessingState(Enum):
    """Voice processing system states"""
    IDLE = "idle"
    LISTENING_FOR_WAKE_WORD = "listening_for_wake_word"
    RECORDING_COMMAND = "recording_command"
    PROCESSING_SPEECH = "processing_speech"
    SPEAKING = "speaking"
    ERROR = "error"

@dataclass
class AudioConfig:
    """Audio configuration settings"""
    sample_rate: int = 16000
    channels: int = 1
    chunk_size: int = 1024
    device_id: Optional[int] = None
    input_device_id: Optional[int] = None
    output_device_id: Optional[int] = None
    
@dataclass
class VoiceCommand:
    """Represents a processed voice command"""
    text: str
    confidence: float
    language: str
    timestamp: float
    raw_audio: Optional[np.ndarray] = None

class WakeWordDetector:
    """Wake word detection using various backends"""
    
    def __init__(self, wake_words: List[str] = None):
        self.wake_words = wake_words or ["hey ethervox", "ethervox"]
        self.is_listening = False
        self.detection_callback: Optional[Callable] = None
        
    async def start_detection(self, callback: Callable):
        """Start wake word detection"""
        self.detection_callback = callback
        self.is_listening = True
        logger.info(f"Wake word detection started for: {self.wake_words}")
        
        # Simulate wake word detection for demo
        # In real implementation, this would use pvporcupine or openwakeword
        asyncio.create_task(self._simulate_wake_word_detection())
    
    async def stop_detection(self):
        """Stop wake word detection"""
        self.is_listening = False
        logger.info("Wake word detection stopped")
    
    async def _simulate_wake_word_detection(self):
        """Simulate wake word detection for demo purposes"""
        while self.is_listening:
            await asyncio.sleep(0.1)
            # In real implementation, process audio chunks here

class VoiceActivityDetector:
    """Voice activity detection to determine when user is speaking"""
    
    def __init__(self, sample_rate: int = 16000):
        if AUDIO_DEPENDENCIES_AVAILABLE:
            self.vad = webrtcvad.Vad()
            self.vad.set_aggressiveness(2)  # 0-3, higher = more aggressive
        self.sample_rate = sample_rate
        self.frame_duration = 30  # ms
        self.frame_size = int(sample_rate * self.frame_duration / 1000)
        
    def is_speech(self, audio_chunk: np.ndarray) -> bool:
        """Determine if audio chunk contains speech"""
        if not AUDIO_DEPENDENCIES_AVAILABLE:
            # Fallback: simple energy-based detection
            energy = np.sum(audio_chunk ** 2)
            return energy > 0.01
            
        # Convert to bytes for webrtcvad
        audio_int16 = (audio_chunk * 32767).astype(np.int16)
        audio_bytes = audio_int16.tobytes()
        
        try:
            return self.vad.is_speech(audio_bytes, self.sample_rate)
        except Exception as e:
            logger.warning(f"VAD error: {e}")
            return False

class SpeechToText:
    """Speech-to-text processing with multiple backends"""
    
    def __init__(self, model_name: str = "base"):
        self.model_name = model_name
        self.whisper_model = None
        self.recognizer = None
        
        if AUDIO_DEPENDENCIES_AVAILABLE:
            try:
                # Initialize Whisper for offline STT
                self.whisper_model = whisper.load_model(model_name)
                logger.info(f"Whisper model '{model_name}' loaded successfully")
            except Exception as e:
                logger.warning(f"Could not load Whisper model: {e}")
            
            # Initialize speech_recognition as fallback
            self.recognizer = sr.Recognizer()
    
    async def transcribe_audio(self, audio_data: np.ndarray, language: str = None) -> VoiceCommand:
        """Transcribe audio to text"""
        if self.whisper_model is not None:
            return await self._transcribe_with_whisper(audio_data, language)
        elif self.recognizer is not None:
            return await self._transcribe_with_sr(audio_data, language)
        else:
            return VoiceCommand("", 0.0, "en", time.time())
    
    async def _transcribe_with_whisper(self, audio_data: np.ndarray, language: str = None) -> VoiceCommand:
        """Transcribe using Whisper model"""
        try:
            # Whisper expects float32 audio normalized to [-1, 1]
            audio_float32 = audio_data.astype(np.float32)
            if np.max(np.abs(audio_float32)) > 1.0:
                audio_float32 = audio_float32 / np.max(np.abs(audio_float32))
            
            result = self.whisper_model.transcribe(
                audio_float32,
                language=language,
                task="transcribe"
            )
            
            return VoiceCommand(
                text=result["text"].strip(),
                confidence=1.0,  # Whisper doesn't provide confidence scores
                language=result.get("language", "en"),
                timestamp=time.time(),
                raw_audio=audio_data
            )
        except Exception as e:
            logger.error(f"Whisper transcription error: {e}")
            return VoiceCommand("", 0.0, "en", time.time())
    
    async def _transcribe_with_sr(self, audio_data: np.ndarray, language: str = None) -> VoiceCommand:
        """Transcribe using speech_recognition library"""
        try:
            # Convert numpy array to AudioData
            audio_data_sr = sr.AudioData(
                audio_data.tobytes(),
                sample_rate=16000,
                sample_width=audio_data.dtype.itemsize
            )
            
            # Try Google Speech Recognition (requires internet)
            text = self.recognizer.recognize_google(audio_data_sr, language=language)
            
            return VoiceCommand(
                text=text,
                confidence=1.0,  # Google STT doesn't provide confidence
                language=language or "en",
                timestamp=time.time(),
                raw_audio=audio_data
            )
        except sr.UnknownValueError:
            logger.warning("Could not understand audio")
            return VoiceCommand("", 0.0, "en", time.time())
        except sr.RequestError as e:
            logger.error(f"STT request error: {e}")
            return VoiceCommand("", 0.0, "en", time.time())

class TextToSpeech:
    """Text-to-speech with multiple backends"""
    
    def __init__(self):
        self.tts_engine = None
        self.is_speaking = False
        
        if AUDIO_DEPENDENCIES_AVAILABLE:
            try:
                self.tts_engine = pyttsx3.init()
                # Configure voice properties
                voices = self.tts_engine.getProperty('voices')
                if voices:
                    # Try to set a female voice if available
                    for voice in voices:
                        if 'female' in voice.name.lower() or 'zira' in voice.name.lower():
                            self.tts_engine.setProperty('voice', voice.id)
                            break
                
                self.tts_engine.setProperty('rate', 150)  # Speed of speech
                self.tts_engine.setProperty('volume', 0.8)  # Volume level
                logger.info("TTS engine initialized successfully")
            except Exception as e:
                logger.warning(f"Could not initialize TTS engine: {e}")
    
    async def speak(self, text: str, language: str = "en") -> bool:
        """Convert text to speech and play it"""
        if not self.tts_engine or not text.strip():
            return False
        
        try:
            self.is_speaking = True
            logger.info(f"Speaking: {text}")
            
            # Run TTS in a separate thread to avoid blocking
            def _speak():
                self.tts_engine.say(text)
                self.tts_engine.runAndWait()
                self.is_speaking = False
            
            thread = threading.Thread(target=_speak)
            thread.start()
            
            # Wait for speaking to complete
            while self.is_speaking:
                await asyncio.sleep(0.1)
            
            return True
        except Exception as e:
            logger.error(f"TTS error: {e}")
            self.is_speaking = False
            return False
    
    def stop_speaking(self):
        """Stop current speech"""
        if self.tts_engine:
            try:
                self.tts_engine.stop()
                self.is_speaking = False
            except Exception as e:
                logger.error(f"Error stopping TTS: {e}")

class AudioDeviceManager:
    """Manages audio input/output devices"""
    
    def __init__(self):
        self.input_devices = []
        self.output_devices = []
        self.default_input = None
        self.default_output = None
        self._refresh_devices()
    
    def _refresh_devices(self):
        """Refresh list of available audio devices"""
        if not AUDIO_DEPENDENCIES_AVAILABLE:
            return
        
        try:
            devices = sd.query_devices()
            self.input_devices = []
            self.output_devices = []
            
            for i, device in enumerate(devices):
                device_info = {
                    'id': i,
                    'name': device['name'],
                    'channels': device['max_input_channels'] if device['max_input_channels'] > 0 else device['max_output_channels'],
                    'sample_rate': device['default_samplerate']
                }
                
                if device['max_input_channels'] > 0:
                    self.input_devices.append(device_info)
                    if 'default' in device['name'].lower():
                        self.default_input = device_info
                
                if device['max_output_channels'] > 0:
                    self.output_devices.append(device_info)
                    if 'default' in device['name'].lower():
                        self.default_output = device_info
            
            logger.info(f"Found {len(self.input_devices)} input devices and {len(self.output_devices)} output devices")
        except Exception as e:
            logger.error(f"Error querying audio devices: {e}")
    
    def get_input_devices(self) -> List[Dict[str, Any]]:
        """Get list of available input devices"""
        return self.input_devices
    
    def get_output_devices(self) -> List[Dict[str, Any]]:
        """Get list of available output devices"""
        return self.output_devices

class VoiceProcessor:
    """Main voice processing coordinator"""
    
    def __init__(self, config: AudioConfig = None):
        self.config = config or AudioConfig()
        self.state = VoiceProcessingState.IDLE
        self.wake_word_detector = WakeWordDetector()
        self.vad = VoiceActivityDetector(self.config.sample_rate)
        self.stt = SpeechToText()
        self.tts = TextToSpeech()
        self.device_manager = AudioDeviceManager()
        
        # Audio recording
        self.audio_queue = queue.Queue()
        self.recording_thread = None
        self.is_recording = False
        
        # Callbacks
        self.wake_word_callback: Optional[Callable] = None
        self.command_callback: Optional[Callable[[VoiceCommand], None]] = None
        
        logger.info("VoiceProcessor initialized")
    
    async def start(self, wake_word_callback: Callable = None, command_callback: Callable = None):
        """Start the voice processing system"""
        self.wake_word_callback = wake_word_callback
        self.command_callback = command_callback
        
        if not AUDIO_DEPENDENCIES_AVAILABLE:
            logger.warning("Audio dependencies not available - running in simulation mode")
            self.state = VoiceProcessingState.LISTENING_FOR_WAKE_WORD
            return
        
        # Start wake word detection
        await self.wake_word_detector.start_detection(self._on_wake_word_detected)
        self.state = VoiceProcessingState.LISTENING_FOR_WAKE_WORD
        logger.info("Voice processing started - listening for wake words")
    
    async def stop(self):
        """Stop the voice processing system"""
        await self.wake_word_detector.stop_detection()
        self._stop_recording()
        self.tts.stop_speaking()
        self.state = VoiceProcessingState.IDLE
        logger.info("Voice processing stopped")
    
    async def _on_wake_word_detected(self):
        """Handle wake word detection"""
        if self.state != VoiceProcessingState.LISTENING_FOR_WAKE_WORD:
            return
        
        logger.info("Wake word detected - starting command recording")
        if self.wake_word_callback:
            await self.wake_word_callback()
        
        await self._start_command_recording()
    
    async def _start_command_recording(self):
        """Start recording user command after wake word"""
        self.state = VoiceProcessingState.RECORDING_COMMAND
        self._start_recording()
        
        # Record for a few seconds or until silence
        await asyncio.sleep(3.0)  # Max recording time
        audio_data = self._stop_recording()
        
        if audio_data is not None and len(audio_data) > 0:
            await self._process_command(audio_data)
        
        # Return to listening for wake words
        self.state = VoiceProcessingState.LISTENING_FOR_WAKE_WORD
    
    async def _process_command(self, audio_data: np.ndarray):
        """Process recorded command"""
        self.state = VoiceProcessingState.PROCESSING_SPEECH
        logger.info("Processing voice command...")
        
        try:
            command = await self.stt.transcribe_audio(audio_data)
            logger.info(f"Transcribed command: '{command.text}' (confidence: {command.confidence})")
            
            if self.command_callback and command.text.strip():
                await self.command_callback(command)
        except Exception as e:
            logger.error(f"Error processing command: {e}")
    
    def _start_recording(self):
        """Start audio recording in background thread"""
        if not AUDIO_DEPENDENCIES_AVAILABLE:
            return
        
        self.is_recording = True
        self.audio_queue = queue.Queue()
        
        def recording_thread():
            try:
                with sd.InputStream(
                    device=self.config.input_device_id,
                    channels=self.config.channels,
                    samplerate=self.config.sample_rate,
                    dtype=np.float32,
                    blocksize=self.config.chunk_size,
                    callback=self._audio_callback
                ):
                    while self.is_recording:
                        time.sleep(0.1)
            except Exception as e:
                logger.error(f"Recording error: {e}")
        
        self.recording_thread = threading.Thread(target=recording_thread)
        self.recording_thread.start()
    
    def _audio_callback(self, indata, frames, time_info, status):
        """Audio input callback"""
        if status:
            logger.warning(f"Audio callback status: {status}")
        
        if self.is_recording:
            self.audio_queue.put(indata.copy())
    
    def _stop_recording(self) -> Optional[np.ndarray]:
        """Stop recording and return collected audio"""
        self.is_recording = False
        
        if self.recording_thread:
            self.recording_thread.join(timeout=1.0)
        
        # Collect all audio chunks
        audio_chunks = []
        while not self.audio_queue.empty():
            try:
                chunk = self.audio_queue.get_nowait()
                audio_chunks.append(chunk)
            except queue.Empty:
                break
        
        if audio_chunks:
            audio_data = np.concatenate(audio_chunks, axis=0).flatten()
            logger.info(f"Recorded {len(audio_data)} audio samples")
            return audio_data
        
        return None
    
    async def speak_response(self, text: str, language: str = "en"):
        """Speak a response to the user"""
        if not text.strip():
            return
        
        previous_state = self.state
        self.state = VoiceProcessingState.SPEAKING
        
        success = await self.tts.speak(text, language)
        
        if success:
            logger.info(f"Successfully spoke response: '{text}'")
        else:
            logger.warning(f"Failed to speak response: '{text}'")
        
        self.state = previous_state
    
    def get_state(self) -> VoiceProcessingState:
        """Get current processing state"""
        return self.state
    
    def get_audio_devices(self) -> Dict[str, List[Dict[str, Any]]]:
        """Get available audio devices"""
        return {
            'input_devices': self.device_manager.get_input_devices(),
            'output_devices': self.device_manager.get_output_devices()
        }

# Convenience functions for easy use
async def create_voice_processor(config: AudioConfig = None) -> VoiceProcessor:
    """Create and return a configured voice processor"""
    return VoiceProcessor(config)
