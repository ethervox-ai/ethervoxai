"""
🔊 Audio Input/Output Test

Test and validate audio hardware setup for EthervoxAI.
This example helps debug audio issues and validate your hardware configuration.

Features:
- Audio input (microphone) testing
- Audio output (speaker/headphones) testing  
- I2S hardware validation
- Audio quality analysis
- Pin configuration verification
- Real-time audio monitoring

Hardware Requirements:
- I2S microphone (INMP441 or compatible)
- I2S DAC/amplifier (PCM5102A + MAX98357A or compatible)
- Proper wiring according to board configuration

Usage:
1. Connect audio hardware
2. Run this test
3. Follow the interactive prompts
4. Check for any hardware issues
"""

import time
import gc
import math
from ethervoxai.core.audio_manager import AudioManager
from ethervoxai.boards import get_board_config

class AudioIOTester:
    """Comprehensive audio input/output testing"""
    
    def __init__(self):
        """Initialize audio testing"""
        print("🔊 EthervoxAI Audio I/O Tester")
        print("=" * 40)
        
        # Get board configuration
        self.board_config = get_board_config()
        if not self.board_config:
            print("❌ Failed to detect board configuration")
            return
        
        print(f"📋 Board: {self.board_config.board_name}")
        print(f"🔧 MCU: {self.board_config.mcu_type}")
        
        # Initialize audio manager
        self.audio_manager = AudioManager(self.board_config)
        self.test_results = {}
        
    def run_all_tests(self):
        """Run complete audio test suite"""
        if not self.board_config:
            return False
        
        print("\n🚀 Starting audio hardware tests...")
        
        tests = [
            ("Pin Configuration", self.test_pin_configuration),
            ("Audio Manager Init", self.test_audio_manager_init),
            ("I2S Hardware", self.test_i2s_hardware),
            ("Microphone Input", self.test_microphone_input),
            ("Speaker Output", self.test_speaker_output),
            ("Audio Loopback", self.test_audio_loopback),
            ("Audio Quality", self.test_audio_quality)
        ]
        
        for test_name, test_func in tests:
            print(f"\n📝 Running: {test_name}")
            try:
                result = test_func()
                self.test_results[test_name] = result
                if result:
                    print(f"✅ {test_name}: PASSED")
                else:
                    print(f"❌ {test_name}: FAILED")
            except Exception as e:
                print(f"❌ {test_name}: ERROR - {e}")
                self.test_results[test_name] = False
        
        self.print_test_summary()
        return self.get_overall_result()
    
    def test_pin_configuration(self):
        """Test pin configuration and hardware setup"""
        try:
            print("🔌 Checking pin configuration...")
            
            # Display configured pins
            pins = self.board_config.pins
            print(f"   I2S Output: SCK={pins.i2s_output_sck}, WS={pins.i2s_output_ws}, SD={pins.i2s_output_sd}")
            print(f"   I2S Input:  SCK={pins.i2s_input_sck}, WS={pins.i2s_input_ws}, SD={pins.i2s_input_sd}")
            print(f"   Control:    Enable={pins.enable_pin}, LED={pins.led_pin}, Button={pins.button_pin}")
            
            # Try to create pin objects
            audio_pins = self.board_config.create_audio_pins()
            if audio_pins:
                print("   ✅ Pin objects created successfully")
                
                # Test LED control
                if 'led' in audio_pins:
                    print("   🔆 Testing LED...")
                    for _ in range(3):
                        audio_pins['led'].on()
                        time.sleep_ms(200)
                        audio_pins['led'].off()
                        time.sleep_ms(200)
                    print("   ✅ LED test completed")
                
                # Test button
                if 'button' in audio_pins:
                    button_state = audio_pins['button'].value()
                    print(f"   🔘 Button state: {'PRESSED' if button_state == 0 else 'RELEASED'}")
                
                return True
            else:
                print("   ❌ Failed to create pin objects")
                return False
                
        except Exception as e:
            print(f"   ❌ Pin configuration error: {e}")
            return False
    
    def test_audio_manager_init(self):
        """Test audio manager initialization"""
        try:
            print("🎛️ Testing audio manager initialization...")
            
            # Check if audio manager was created
            if not self.audio_manager:
                print("   ❌ Audio manager not created")
                return False
            
            # Test initialization
            success = self.audio_manager.initialize()
            if success:
                print("   ✅ Audio manager initialized successfully")
                
                # Check sample rate
                sample_rate = self.audio_manager.get_sample_rate()
                print(f"   📊 Sample rate: {sample_rate}Hz")
                
                # Check buffer size
                buffer_size = self.audio_manager.get_buffer_size()
                print(f"   🗃️ Buffer size: {buffer_size} samples")
                
                return True
            else:
                print("   ❌ Audio manager initialization failed")
                return False
                
        except Exception as e:
            print(f"   ❌ Audio manager error: {e}")
            return False
    
    def test_i2s_hardware(self):
        """Test I2S hardware interface"""
        try:
            print("🔧 Testing I2S hardware interface...")
            
            # Test I2S output initialization
            output_ok = self.audio_manager.setup_output()
            if output_ok:
                print("   ✅ I2S output setup successful")
            else:
                print("   ❌ I2S output setup failed")
            
            # Test I2S input initialization
            input_ok = self.audio_manager.setup_input()
            if input_ok:
                print("   ✅ I2S input setup successful")
            else:
                print("   ❌ I2S input setup failed")
            
            return output_ok and input_ok
            
        except Exception as e:
            print(f"   ❌ I2S hardware error: {e}")
            return False
    
    def test_microphone_input(self):
        """Test microphone input functionality"""
        try:
            print("🎤 Testing microphone input...")
            print("   🗣️ Please speak or make noise near the microphone...")
            
            # Record for a few seconds
            duration_ms = 3000
            samples_recorded = 0
            max_amplitude = 0
            start_time = time.ticks_ms()
            
            while time.ticks_diff(time.ticks_ms(), start_time) < duration_ms:
                # Read audio data
                audio_data = self.audio_manager.read_audio(512)
                if audio_data:
                    samples_recorded += len(audio_data)
                    
                    # Calculate amplitude
                    for sample in audio_data:
                        amplitude = abs(sample)
                        if amplitude > max_amplitude:
                            max_amplitude = amplitude
                
                time.sleep_ms(10)
            
            print(f"   📊 Samples recorded: {samples_recorded}")
            print(f"   📈 Max amplitude: {max_amplitude}")
            
            if samples_recorded > 0:
                print("   ✅ Microphone input working")
                if max_amplitude > 100:  # Arbitrary threshold
                    print("   🔊 Audio signal detected!")
                else:
                    print("   🔇 Very quiet or no signal (check microphone)")
                return True
            else:
                print("   ❌ No audio data received")
                return False
                
        except Exception as e:
            print(f"   ❌ Microphone test error: {e}")
            return False
    
    def test_speaker_output(self):
        """Test speaker/headphone output"""
        try:
            print("🔊 Testing speaker output...")
            print("   🎵 Playing test tones... (listen for sound)")
            
            # Generate and play test tones
            sample_rate = self.audio_manager.get_sample_rate()
            duration_ms = 2000
            frequencies = [440, 880, 1320]  # A4, A5, E6
            
            for freq in frequencies:
                print(f"   🎵 Playing {freq}Hz tone...")
                
                # Generate sine wave
                samples_per_tone = (sample_rate * duration_ms) // (1000 * len(frequencies))
                tone_data = []
                
                for i in range(samples_per_tone):
                    # Generate sine wave sample
                    t = i / sample_rate
                    amplitude = 0.3  # 30% volume to avoid damage
                    sample = int(amplitude * 32767 * math.sin(2 * math.pi * freq * t))
                    tone_data.append(sample)
                
                # Play the tone
                success = self.audio_manager.write_audio(tone_data)
                if not success:
                    print(f"   ❌ Failed to play {freq}Hz tone")
                    return False
                
                time.sleep_ms(100)  # Brief pause between tones
            
            print("   ✅ Speaker output test completed")
            print("   👂 Did you hear the test tones? (Manual verification required)")
            return True
            
        except Exception as e:
            print(f"   ❌ Speaker test error: {e}")
            return False
    
    def test_audio_loopback(self):
        """Test audio loopback (if supported)"""
        try:
            print("🔄 Testing audio loopback...")
            print("   🎤 Recording while playing (check for feedback)")
            
            # This test plays audio while recording to detect loopback
            # Useful for detecting wiring issues or interference
            
            # Generate a simple test pattern
            test_pattern = [1000, -1000, 2000, -2000] * 100
            
            # Start recording
            recorded_data = []
            
            # Play test pattern while recording
            self.audio_manager.write_audio(test_pattern)
            
            # Record for a short time
            for _ in range(10):
                audio_chunk = self.audio_manager.read_audio(256)
                if audio_chunk:
                    recorded_data.extend(audio_chunk)
                time.sleep_ms(50)
            
            if recorded_data:
                # Analyze for loopback
                max_recorded = max(abs(sample) for sample in recorded_data)
                print(f"   📊 Max recorded amplitude: {max_recorded}")
                
                if max_recorded > 5000:  # High threshold indicates possible loopback
                    print("   ⚠️  Possible audio loopback detected")
                    print("      Check wiring and speaker/microphone placement")
                else:
                    print("   ✅ No significant loopback detected")
                
                return True
            else:
                print("   ❌ No loopback data recorded")
                return False
                
        except Exception as e:
            print(f"   ❌ Loopback test error: {e}")
            return False
    
    def test_audio_quality(self):
        """Test audio quality metrics"""
        try:
            print("📊 Testing audio quality...")
            
            # Record ambient noise to check quality
            print("   🤫 Measuring background noise (stay quiet)...")
            
            noise_samples = []
            for _ in range(50):  # Record for ~0.5 seconds
                chunk = self.audio_manager.read_audio(128)
                if chunk:
                    noise_samples.extend(chunk)
                time.sleep_ms(10)
            
            if noise_samples:
                # Calculate noise statistics
                noise_avg = sum(abs(s) for s in noise_samples) / len(noise_samples)
                noise_max = max(abs(s) for s in noise_samples)
                
                print(f"   📉 Average noise level: {noise_avg:.1f}")
                print(f"   📈 Max noise level: {noise_max}")
                
                # Quality assessment
                if noise_avg < 100:
                    print("   ✅ Excellent audio quality (low noise)")
                elif noise_avg < 500:
                    print("   ⚠️  Good audio quality (moderate noise)")
                else:
                    print("   ❌ Poor audio quality (high noise)")
                    print("      Check connections and power supply")
                
                return noise_avg < 1000  # Arbitrary quality threshold
            else:
                print("   ❌ Could not measure audio quality")
                return False
                
        except Exception as e:
            print(f"   ❌ Audio quality test error: {e}")
            return False
    
    def print_test_summary(self):
        """Print summary of all test results"""
        print("\n" + "=" * 40)
        print("📋 AUDIO TEST SUMMARY")
        print("=" * 40)
        
        passed = 0
        total = len(self.test_results)
        
        for test_name, result in self.test_results.items():
            status = "✅ PASS" if result else "❌ FAIL"
            print(f"{test_name:<20} {status}")
            if result:
                passed += 1
        
        print("-" * 40)
        print(f"Tests passed: {passed}/{total}")
        
        if passed == total:
            print("🎉 All audio tests PASSED!")
            print("🔊 Your audio hardware is working correctly.")
        elif passed >= total * 0.7:
            print("⚠️  Most tests passed, but check failed items.")
        else:
            print("❌ Many tests failed. Check your wiring and configuration.")
        
        self.print_troubleshooting_tips()
    
    def print_troubleshooting_tips(self):
        """Print troubleshooting guidance"""
        print("\n🔧 TROUBLESHOOTING TIPS:")
        print("-" * 40)
        
        if not self.test_results.get("Pin Configuration", False):
            print("• Check microcontroller pin assignments")
            print("• Verify board type detection")
        
        if not self.test_results.get("I2S Hardware", False):
            print("• Check I2S wiring (SCK, WS, SD pins)")
            print("• Verify power supply to audio modules")
            print("• Check for loose connections")
        
        if not self.test_results.get("Microphone Input", False):
            print("• Check microphone wiring and power")
            print("• Verify INMP441 or compatible microphone")
            print("• Test with different microphone if available")
        
        if not self.test_results.get("Speaker Output", False):
            print("• Check speaker/headphone wiring")
            print("• Verify PCM5102A/MAX98357A connections")
            print("• Check volume levels and amplifier enable pin")
        
        print("• Consult the board-specific wiring diagrams")
        print("• Check power supply stability (3.3V/5V)")
        print("• Verify MicroPython I2S support on your board")
    
    def get_overall_result(self):
        """Get overall test result"""
        if not self.test_results:
            return False
        
        passed = sum(1 for result in self.test_results.values() if result)
        total = len(self.test_results)
        
        return passed >= total * 0.7  # 70% pass rate

# Interactive test runner
def run_interactive_test():
    """Run interactive audio test"""
    print("🎙️ EthervoxAI Audio Hardware Test")
    print("=" * 50)
    print("This test will validate your audio hardware setup.")
    print("Make sure your microphone and speaker are connected.")
    print()
    
    input_ready = input("Ready to start? (y/n): ").lower().strip()
    if input_ready != 'y':
        print("👋 Test cancelled")
        return
    
    # Run the tests
    tester = AudioIOTester()
    success = tester.run_all_tests()
    
    if success:
        print("\n🎉 Audio hardware validation SUCCESSFUL!")
        print("✅ Your setup is ready for EthervoxAI voice processing.")
    else:
        print("\n❌ Audio hardware validation FAILED!")
        print("🔧 Please check the troubleshooting tips above.")
    
    return success

# Main execution
if __name__ == "__main__":
    try:
        run_interactive_test()
    except KeyboardInterrupt:
        print("\n⏹️  Test interrupted by user")
    except Exception as e:
        print(f"\n❌ Test failed with error: {e}")
    finally:
        print("🧹 Cleaning up...")
        gc.collect()
        print("👋 Audio test finished")
