"""
🎵 Audio Manager - MicroPython Implementation

Handles I2S audio input/output for microcontrollers, specifically optimized
for Raspberry Pi Pico with external audio codecs.

Key Features:
- I2S audio interface for high-quality audio
- Circular buffer management for real-time processing
- Board-specific pin configurations
- Memory-optimized audio processing
- Power management for battery operation

Supported Audio Codecs:
- PCM5102A: I2S DAC for audio output
- INMP441: I2S MEMS microphone for input
- MAX98357A: I2S amplifier for speakers
- WM8960: Full-duplex codec (future support)
"""

import gc
import time
import machine
from array import array
from collections import deque
import micropython

# Try to import I2S if available (MicroPython 1.19+)
try:
    from machine import I2S
    I2S_AVAILABLE = True
except ImportError:
    I2S_AVAILABLE = False
    print("⚠️ I2S not available, using PWM fallback")

class AudioManager:
    """
    Audio Manager for microcontroller-based audio processing
    
    Optimized for:
    - Real-time audio processing with minimal latency
    - Low memory footprint with circular buffers
    - Power-efficient operation
    - Board-specific hardware configurations
    """
    
    def __init__(self, board_type='pico', sample_rate=16000, buffer_size=1024, debug=False):
        """
        Initialize Audio Manager
        
        Args:
            board_type (str): Target board type ('pico', 'pico_w', 'esp32')
            sample_rate (int): Audio sample rate in Hz
            buffer_size (int): Buffer size in samples
            debug (bool): Enable debug output
        """
        self.board_type = board_type
        self.sample_rate = sample_rate
        self.buffer_size = buffer_size
        self.debug = debug
        
        # Audio format settings
        self.bits_per_sample = 16
        self.channels = 1  # Mono for MCU efficiency
        self.bytes_per_sample = self.bits_per_sample // 8
        
        # I2S instances
        self.i2s_input = None
        self.i2s_output = None
        
        # Audio buffers (circular buffers for real-time processing)
        self.input_buffer = deque((), buffer_size)
        self.output_buffer = deque((), buffer_size)
        
        # Pin configurations based on board type
        self.pin_config = self._get_pin_config()
        
        # Processing state
        self.capturing = False
        self.playing = False
        self.last_audio_time = 0
        
        # Initialize hardware
        self._init_audio_hardware()
        
        if self.debug:
            print(f"🎵 AudioManager initialized for {board_type}")
            print(f"   Sample rate: {sample_rate}Hz")
            print(f"   Buffer size: {buffer_size} samples")
            print(f"   Memory usage: ~{self._estimate_memory_usage()}KB")
    
    def _get_pin_config(self):
        """Get pin configuration for the target board"""
        if self.board_type in ['pico', 'pico_w']:
            return {
                # I2S Output (DAC) pins
                'output_sck': 16,    # Serial Clock (BCK)
                'output_ws': 17,     # Word Select (LRCK)
                'output_sd': 18,     # Serial Data (DIN)
                
                # I2S Input (ADC) pins  
                'input_sck': 19,     # Serial Clock
                'input_ws': 20,      # Word Select
                'input_sd': 21,      # Serial Data
                
                # Control pins
                'enable_pin': 22,    # Enable/shutdown pin
                'mute_pin': None     # Mute control (optional)
            }
        elif self.board_type == 'esp32':
            return {
                'output_sck': 26,
                'output_ws': 25,
                'output_sd': 22,
                'input_sck': 32,
                'input_ws': 33,
                'input_sd': 34,
                'enable_pin': 21,
                'mute_pin': None
            }
        else:
            raise ValueError(f"Unsupported board type: {self.board_type}")
    
    def _init_audio_hardware(self):
        """Initialize I2S audio hardware"""
        try:
            if not I2S_AVAILABLE:
                self._init_pwm_fallback()
                return
                
            # Initialize I2S output (DAC)
            if self.pin_config['output_sck'] is not None:
                self.i2s_output = I2S(
                    0,  # I2S peripheral 0
                    sck=machine.Pin(self.pin_config['output_sck']),
                    ws=machine.Pin(self.pin_config['output_ws']),
                    sd=machine.Pin(self.pin_config['output_sd']),
                    mode=I2S.TX,
                    bits=self.bits_per_sample,
                    format=I2S.MONO if self.channels == 1 else I2S.STEREO,
                    rate=self.sample_rate,
                    ibuf=self.buffer_size * 2  # Double buffer for smooth playback
                )
                
                if self.debug:
                    print("✅ I2S output initialized")
            
            # Initialize I2S input (ADC)
            if self.pin_config['input_sck'] is not None:
                self.i2s_input = I2S(
                    1,  # I2S peripheral 1
                    sck=machine.Pin(self.pin_config['input_sck']),
                    ws=machine.Pin(self.pin_config['input_ws']),
                    sd=machine.Pin(self.pin_config['input_sd']),
                    mode=I2S.RX,
                    bits=self.bits_per_sample,
                    format=I2S.MONO if self.channels == 1 else I2S.STEREO,
                    rate=self.sample_rate,
                    ibuf=self.buffer_size * 2  # Double buffer for smooth capture
                )
                
                if self.debug:
                    print("✅ I2S input initialized")
            
            # Initialize control pins
            if self.pin_config['enable_pin'] is not None:
                self.enable_pin = machine.Pin(self.pin_config['enable_pin'], machine.Pin.OUT)
                self.enable_pin.on()  # Enable audio codec
                
        except Exception as e:
            print(f"⚠️ I2S initialization failed: {e}")
            self._init_pwm_fallback()
    
    def _init_pwm_fallback(self):
        """Initialize PWM-based audio fallback"""
        try:
            # Simple PWM audio output for basic functionality
            output_pin = self.pin_config.get('output_sd', 18)
            self.pwm_output = machine.PWM(machine.Pin(output_pin))
            self.pwm_output.freq(self.sample_rate)
            self.pwm_output.duty_u16(32768)  # 50% duty cycle (silence)
            
            if self.debug:
                print("✅ PWM audio fallback initialized")
                
        except Exception as e:
            print(f"❌ PWM fallback initialization failed: {e}")
    
    def start_capture(self):
        """Start audio capture"""
        if not self.i2s_input and not hasattr(self, 'adc_input'):
            if self.debug:
                print("⚠️ No audio input available")
            return False
            
        self.capturing = True
        self.last_audio_time = time.ticks_ms()
        
        if self.debug:
            print("🎤 Audio capture started")
            
        return True
    
    def stop_capture(self):
        """Stop audio capture"""
        self.capturing = False
        
        if self.debug:
            print("⏹️ Audio capture stopped")
    
    def read_audio(self):
        """Read audio data from input buffer"""
        if not self.capturing:
            return None
            
        try:
            if self.i2s_input:
                return self._read_i2s_audio()
            else:
                return self._read_adc_audio()
                
        except Exception as e:
            if self.debug:
                print(f"⚠️ Audio read error: {e}")
            return None
    
    def _read_i2s_audio(self):
        """Read audio from I2S interface"""
        try:
            # Create buffer for audio data
            audio_buffer = bytearray(self.buffer_size * self.bytes_per_sample)
            
            # Read from I2S (non-blocking)
            bytes_read = self.i2s_input.readinto(audio_buffer)
            
            if bytes_read > 0:
                # Convert to signed 16-bit samples
                samples = array('h')  # signed short array
                
                for i in range(0, bytes_read, 2):
                    if i + 1 < len(audio_buffer):
                        # Little-endian 16-bit conversion
                        sample = (audio_buffer[i + 1] << 8) | audio_buffer[i]
                        if sample > 32767:
                            sample -= 65536  # Convert to signed
                        samples.append(sample)
                
                return samples
            
        except OSError:
            # No data available (non-blocking)
            pass
            
        return None
    
    def _read_adc_audio(self):
        """Read audio from ADC fallback"""
        # Simple ADC reading for basic audio input
        # This is a fallback when I2S is not available
        try:
            if hasattr(self, 'adc_input'):
                # Read ADC value and convert to audio sample
                adc_value = self.adc_input.read_u16()
                
                # Convert 16-bit ADC to signed audio sample
                sample = adc_value - 32768
                
                # Return array with single sample
                return array('h', [sample])
                
        except Exception as e:
            if self.debug:
                print(f"⚠️ ADC read error: {e}")
                
        return None
    
    def play_audio(self, audio_data):
        """Play audio data through output"""
        if not audio_data:
            return False
            
        try:
            if self.i2s_output:
                return self._play_i2s_audio(audio_data)
            else:
                return self._play_pwm_audio(audio_data)
                
        except Exception as e:
            if self.debug:
                print(f"⚠️ Audio playback error: {e}")
            return False
    
    def _play_i2s_audio(self, audio_data):
        """Play audio through I2S interface"""
        try:
            # Convert audio samples to bytes
            audio_bytes = bytearray()
            
            for sample in audio_data:
                # Ensure sample is in valid range
                sample = max(-32768, min(32767, int(sample)))
                
                # Convert to unsigned for transmission
                if sample < 0:
                    sample += 65536
                    
                # Little-endian 16-bit encoding
                audio_bytes.append(sample & 0xFF)
                audio_bytes.append((sample >> 8) & 0xFF)
            
            # Write to I2S output
            bytes_written = self.i2s_output.write(audio_bytes)
            
            return bytes_written > 0
            
        except Exception as e:
            if self.debug:
                print(f"⚠️ I2S playback error: {e}")
            return False
    
    def _play_pwm_audio(self, audio_data):
        """Play audio through PWM fallback"""
        try:
            if not hasattr(self, 'pwm_output'):
                return False
                
            # Simple PWM audio output
            for sample in audio_data:
                # Convert signed sample to PWM duty cycle
                duty = int((sample + 32768) * 65535 // 65536)
                duty = max(0, min(65535, duty))
                
                self.pwm_output.duty_u16(duty)
                
                # Small delay for audio timing
                time.sleep_us(1000000 // self.sample_rate)
                
            return True
            
        except Exception as e:
            if self.debug:
                print(f"⚠️ PWM playback error: {e}")
            return False
    
    def play_response(self, text):
        """Play text response (simple beep for now)"""
        if self.debug:
            print(f"🔊 Playing response: {text}")
            
        # Generate simple tone pattern for response
        # In a full implementation, this would be TTS
        tone_samples = self._generate_tone(800, 0.2)  # 800Hz for 200ms
        return self.play_audio(tone_samples)
    
    def _generate_tone(self, frequency, duration):
        """Generate tone samples for testing/feedback"""
        import math
        
        num_samples = int(self.sample_rate * duration)
        samples = array('h')
        
        for i in range(num_samples):
            t = i / self.sample_rate
            amplitude = 8000  # Lower amplitude for safety
            sample = int(amplitude * math.sin(2 * math.pi * frequency * t))
            samples.append(sample)
            
        return samples
    
    def get_audio_stats(self):
        """Get audio processing statistics"""
        return {
            'sample_rate': self.sample_rate,
            'buffer_size': self.buffer_size,
            'channels': self.channels,
            'bits_per_sample': self.bits_per_sample,
            'capturing': self.capturing,
            'playing': self.playing,
            'i2s_available': I2S_AVAILABLE,
            'input_available': self.i2s_input is not None,
            'output_available': self.i2s_output is not None or hasattr(self, 'pwm_output'),
            'last_audio_time': self.last_audio_time,
            'memory_usage_kb': self._estimate_memory_usage()
        }
    
    def _estimate_memory_usage(self):
        """Estimate memory usage in KB"""
        buffer_memory = (self.buffer_size * self.bytes_per_sample * 2)  # Input + output
        overhead = 2048  # Estimated overhead
        return (buffer_memory + overhead) // 1024
    
    def enter_low_power(self):
        """Enter low power mode"""
        self.stop_capture()
        
        # Disable audio codec if possible
        if hasattr(self, 'enable_pin'):
            self.enable_pin.off()
            
        if self.debug:
            print("💤 Audio manager entered low power mode")
    
    def exit_low_power(self):
        """Exit low power mode"""
        # Re-enable audio codec
        if hasattr(self, 'enable_pin'):
            self.enable_pin.on()
            time.sleep_ms(10)  # Allow codec to stabilize
            
        if self.debug:
            print("🔋 Audio manager exited low power mode")
    
    def test_audio_loopback(self):
        """Test audio loopback (input to output)"""
        if not self.i2s_input or not self.i2s_output:
            print("⚠️ Audio loopback requires both input and output")
            return False
            
        print("🔄 Starting audio loopback test...")
        print("   Speak into microphone, you should hear it back")
        print("   Press Ctrl+C to stop")
        
        self.start_capture()
        
        try:
            while True:
                audio_data = self.read_audio()
                if audio_data:
                    self.play_audio(audio_data)
                time.sleep_ms(1)
                
        except KeyboardInterrupt:
            print("\n⏹️ Audio loopback test stopped")
            self.stop_capture()
            return True
    
    def cleanup(self):
        """Clean up audio resources"""
        self.stop_capture()
        
        # Close I2S interfaces
        if self.i2s_input:
            try:
                self.i2s_input.deinit()
            except:
                pass
                
        if self.i2s_output:
            try:
                self.i2s_output.deinit()
            except:
                pass
                
        # Clean up PWM
        if hasattr(self, 'pwm_output'):
            try:
                self.pwm_output.deinit()
            except:
                pass
                
        # Clear buffers
        self.input_buffer.clear()
        self.output_buffer.clear()
        
        # Force garbage collection
        gc.collect()
        
        if self.debug:
            print("🧹 Audio manager cleanup completed")
