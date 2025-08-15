"""
🍓🎙️ Raspberry Pi Pico Voice Assistant

A complete voice assistant implementation optimized specifically for the
Raspberry Pi Pico (RP2040) microcontroller.

This example demonstrates:
- Memory-optimized voice processing for 264KB SRAM
- I2S audio with external codecs
- Aggressive power management
- Local-only inference (privacy-first)
- Real-time voice activity detection
- Simple voice commands with LED feedback

Hardware Setup for Raspberry Pi Pico:
- INMP441 I2S microphone on pins 19,20,21 (SCK,WS,SD)
- PCM5102A I2S DAC on pins 16,17,18 (SCK,WS,SD)
- MAX98357A I2S amplifier (optional, for better audio)
- Built-in LED on pin 25
- Built-in button on pin 14

Wiring Diagram:
┌─────────────────┐    ┌─────────────────┐
│   Raspberry Pi  │    │    INMP441      │
│      Pico       │    │   Microphone    │
├─────────────────┤    ├─────────────────┤
│ GP19 (I2S_SCK)  ├────┤ SCK             │
│ GP20 (I2S_WS)   ├────┤ WS              │
│ GP21 (I2S_SD)   ├────┤ SD              │
│ 3V3             ├────┤ VDD             │
│ GND             ├────┤ GND             │
└─────────────────┘    └─────────────────┘

┌─────────────────┐    ┌─────────────────┐
│   Raspberry Pi  │    │    PCM5102A     │
│      Pico       │    │      DAC        │
├─────────────────┤    ├─────────────────┤
│ GP16 (I2S_SCK)  ├────┤ BCK             │
│ GP17 (I2S_WS)   ├────┤ LRCK            │
│ GP18 (I2S_SD)   ├────┤ DIN             │
│ 3V3             ├────┤ VCC             │
│ GND             ├────┤ GND             │
└─────────────────┘    └─────────────────┘

Power Consumption (estimated):
- Active listening: ~50mA @ 3.3V
- Sleep mode: ~20mA @ 3.3V
- Deep sleep: ~5mA @ 3.3V
"""

import gc
import time
import machine
from ethervoxai import EthervoxAI

class PicoVoiceAssistant:
    """Raspberry Pi Pico optimized voice assistant"""
    
    def __init__(self):
        """Initialize Pico voice assistant"""
        print("🍓🎙️ Raspberry Pi Pico Voice Assistant")
        print("=" * 45)
        
        # Memory optimization - collect garbage before initialization
        gc.collect()
        initial_memory = gc.mem_free()
        print(f"💾 Available memory: {initial_memory // 1024}KB")
        
        # Initialize EthervoxAI specifically for Pico
        self.ethervox = EthervoxAI(board_type='pico')
        
        if not self.ethervox:
            print("❌ Failed to initialize EthervoxAI for Pico")
            return
        
        # Verify we're running on Pico
        board_info = self.ethervox.get_board_info()
        if 'pico' not in board_info.get('board_name', '').lower():
            print("⚠️  Warning: Not detected as Raspberry Pi Pico")
        
        # Pico-specific configuration
        self.setup_pico_specific_config()
        
        # Voice commands optimized for memory constraints
        self.commands = {
            'light on': self.light_on,
            'light off': self.light_off,
            'status': self.system_status,
            'memory': self.memory_info,
            'sleep': self.enter_sleep,
            'help': self.show_help
        }
        
        # State management
        self.is_running = True
        self.sleep_mode = False
        self.light_state = False
        self.last_activity = time.ticks_ms()
        
        # Memory monitoring
        self.peak_memory_usage = initial_memory
        self.min_free_memory = initial_memory
        
        print(f"✅ Pico Voice Assistant initialized!")
        print(f"🧠 Model cache: {len(self.ethervox.get_loaded_models())} models loaded")
        self.show_help()
    
    def setup_pico_specific_config(self):
        """Configure Pico-specific optimizations"""
        try:
            # Set conservative CPU frequency for power saving
            machine.freq(125_000_000)  # 125MHz instead of 133MHz
            print(f"⚡ CPU frequency: {machine.freq() // 1_000_000}MHz")
            
            # Configure aggressive garbage collection
            gc.threshold(800)  # Trigger GC more frequently
            
            # Memory optimization settings
            self.max_audio_buffer_size = 512  # Small buffer for memory efficiency
            self.gc_interval_ms = 5000  # Garbage collect every 5 seconds
            self.memory_warning_threshold = 50 * 1024  # 50KB warning threshold
            
            print("🔧 Pico optimizations applied")
            
        except Exception as e:
            print(f"⚠️  Optimization setup error: {e}")
    
    def show_help(self):
        """Show available commands"""
        print("\n🎯 Voice Commands (say clearly):")
        print("   • 'light on' - Turn on LED")
        print("   • 'light off' - Turn off LED") 
        print("   • 'status' - System status")
        print("   • 'memory' - Memory information")
        print("   • 'sleep' - Enter power save mode")
        print("   • 'help' - Show this help")
        print("\n🎤 Say 'Hey Computer' to activate listening")
        print("🔘 Or press the button to activate")
        print()
    
    async def run(self):
        """Main assistant event loop"""
        if not self.ethervox:
            return
        
        print("🚀 Starting Pico voice assistant...")
        
        # Background maintenance timer
        last_gc_time = time.ticks_ms()
        last_memory_check = time.ticks_ms()
        
        try:
            while self.is_running:
                current_time = time.ticks_ms()
                
                # Memory management
                if time.ticks_diff(current_time, last_memory_check) > 1000:
                    self.monitor_memory()
                    last_memory_check = current_time
                
                # Periodic garbage collection
                if time.ticks_diff(current_time, last_gc_time) > self.gc_interval_ms:
                    self.perform_maintenance()
                    last_gc_time = current_time
                
                # Main processing
                if self.sleep_mode:
                    await self.sleep_mode_processing()
                else:
                    await self.active_mode_processing()
                
                # Small delay to prevent CPU overload
                await asyncio.sleep_ms(20)
                
        except KeyboardInterrupt:
            print("\n⏹️  Shutting down...")
        except Exception as e:
            print(f"❌ Runtime error: {e}")
        finally:
            await self.cleanup()
    
    async def active_mode_processing(self):
        """Process in active listening mode"""
        try:
            # Check for button press (immediate response)
            if self.ethervox.is_button_pressed():
                print("🔘 Button activated!")
                await self.process_voice_command()
                return
            
            # Listen for wake word (non-blocking)
            wake_detected = await self.ethervox.listen_for_wake_word(timeout_ms=50)
            if wake_detected:
                print("👂 Wake word detected!")
                await self.process_voice_command()
                return
            
            # Auto-sleep after inactivity (power saving)
            if time.ticks_diff(time.ticks_ms(), self.last_activity) > 300_000:  # 5 minutes
                print("😴 Auto-entering sleep mode after inactivity")
                await self.enter_sleep()
            
        except Exception as e:
            print(f"⚠️  Active mode error: {e}")
    
    async def sleep_mode_processing(self):
        """Process in power-saving sleep mode"""
        try:
            # Only respond to button in sleep mode (saves power)
            if self.ethervox.is_button_pressed():
                print("🌅 Waking up from sleep mode...")
                self.sleep_mode = False
                self.flash_led(3, 100)  # Visual feedback
                self.last_activity = time.ticks_ms()
            else:
                # Deep sleep for power saving
                await asyncio.sleep_ms(1000)
            
        except Exception as e:
            print(f"⚠️  Sleep mode error: {e}")
    
    async def process_voice_command(self):
        """Process voice command with memory optimization"""
        try:
            # Update activity timestamp
            self.last_activity = time.ticks_ms()
            
            # Indicate listening
            self.set_led(True)
            
            # Check memory before processing
            free_memory = gc.mem_free()
            if free_memory < self.memory_warning_threshold:
                print(f"⚠️  Low memory: {free_memory // 1024}KB")
                gc.collect()  # Emergency cleanup
            
            print("🎤 Listening for command...")
            
            # Listen for command with timeout
            command = await self.ethervox.listen_for_command(timeout_ms=4000)
            
            if command:
                print(f"🗣️  Command: '{command}'")
                await self.execute_command(command.lower().strip())
            else:
                print("⏱️  No command heard")
                self.flash_led(2, 150)  # Indicate timeout
            
        except Exception as e:
            print(f"❌ Command processing error: {e}")
        finally:
            self.set_led(False)
    
    async def execute_command(self, command):
        """Execute recognized voice command"""
        try:
            # Direct command matching (memory efficient)
            if command in self.commands:
                await self.commands[command]()
                return
            
            # Fuzzy matching for robustness
            for cmd_key, cmd_func in self.commands.items():
                if cmd_key in command or self.commands_similar(command, cmd_key):
                    await cmd_func()
                    return
            
            # Unknown command
            print(f"❓ Unknown command: '{command}'")
            self.flash_led(5, 100)  # Error indication
            
        except Exception as e:
            print(f"❌ Command execution error: {e}")
    
    def commands_similar(self, heard, expected):
        """Simple similarity check for command matching"""
        # Basic similarity - check if main words match
        heard_words = heard.split()
        expected_words = expected.split()
        
        matches = sum(1 for word in expected_words if word in heard_words)
        return matches >= len(expected_words) * 0.7  # 70% similarity
    
    # Command implementations (memory optimized)
    async def light_on(self):
        """Turn on LED light"""
        self.light_state = True
        self.set_led(True)
        print("💡 Light ON")
        await self.speak_feedback("Light on")
    
    async def light_off(self):
        """Turn off LED light"""
        self.light_state = False
        self.set_led(False)
        print("🔘 Light OFF")
        await self.speak_feedback("Light off")
    
    async def system_status(self):
        """Report system status"""
        try:
            memory_kb = gc.mem_free() // 1024
            cpu_freq_mhz = machine.freq() // 1_000_000
            uptime_s = time.ticks_ms() // 1000
            
            print("🤖 Pico System Status:")
            print(f"   💾 Memory: {memory_kb}KB free")
            print(f"   ⚡ CPU: {cpu_freq_mhz}MHz")
            print(f"   ⏱️  Uptime: {uptime_s}s")
            print(f"   💡 Light: {'ON' if self.light_state else 'OFF'}")
            
            await self.speak_feedback(f"Memory {memory_kb} kilobytes. Light is {'on' if self.light_state else 'off'}.")
            
        except Exception as e:
            print(f"❌ Status error: {e}")
    
    async def memory_info(self):
        """Detailed memory information"""
        try:
            current_free = gc.mem_free()
            current_free_kb = current_free // 1024
            
            print("💾 Memory Information:")
            print(f"   Free: {current_free_kb}KB ({current_free} bytes)")
            print(f"   Peak usage: {self.peak_memory_usage // 1024}KB")
            print(f"   Minimum free: {self.min_free_memory // 1024}KB")
            
            # Memory health assessment
            if current_free_kb > 100:
                health = "Excellent"
            elif current_free_kb > 50:
                health = "Good"
            elif current_free_kb > 20:
                health = "Low"
            else:
                health = "Critical"
            
            print(f"   Health: {health}")
            
            await self.speak_feedback(f"Memory health is {health.lower()}. {current_free_kb} kilobytes free.")
            
        except Exception as e:
            print(f"❌ Memory info error: {e}")
    
    async def enter_sleep(self):
        """Enter power-saving sleep mode"""
        print("😴 Entering sleep mode...")
        self.sleep_mode = True
        self.set_led(False)
        
        # Cleanup before sleep
        gc.collect()
        
        await self.speak_feedback("Going to sleep. Press button to wake.")
    
    async def speak_feedback(self, text):
        """Provide audio/visual feedback"""
        try:
            # Try audio feedback first
            if hasattr(self.ethervox, 'speak'):
                success = await self.ethervox.speak(text)
                if success:
                    return
            
            # Fallback to LED pattern feedback
            print(f"🔊 {text}")
            self.flash_led(len(text.split()), 200)  # Flash per word
            
        except Exception as e:
            print(f"⚠️  Feedback error: {e}")
    
    # Utility methods
    def set_led(self, state):
        """Control built-in LED"""
        try:
            if self.ethervox and hasattr(self.ethervox, 'set_led'):
                self.ethervox.set_led(state)
        except Exception as e:
            print(f"⚠️  LED error: {e}")
    
    def flash_led(self, count, delay_ms):
        """Flash LED pattern"""
        try:
            for _ in range(count):
                self.set_led(True)
                time.sleep_ms(delay_ms)
                self.set_led(False)
                time.sleep_ms(delay_ms)
        except Exception as e:
            print(f"⚠️  LED flash error: {e}")
    
    def monitor_memory(self):
        """Monitor memory usage for optimization"""
        try:
            current_free = gc.mem_free()
            
            # Track memory statistics
            if current_free < self.min_free_memory:
                self.min_free_memory = current_free
            
            # Memory warnings
            if current_free < self.memory_warning_threshold:
                print(f"⚠️  Memory low: {current_free // 1024}KB")
                
                # Emergency garbage collection
                if current_free < 20 * 1024:  # 20KB critical threshold
                    print("🚨 Critical memory! Force garbage collection")
                    gc.collect()
            
        except Exception as e:
            print(f"⚠️  Memory monitoring error: {e}")
    
    def perform_maintenance(self):
        """Perform periodic maintenance"""
        try:
            # Garbage collection
            before_gc = gc.mem_free()
            gc.collect()
            after_gc = gc.mem_free()
            
            freed = after_gc - before_gc
            if freed > 1024:  # Only report significant cleanup
                print(f"🧹 GC freed {freed // 1024}KB")
            
        except Exception as e:
            print(f"⚠️  Maintenance error: {e}")
    
    async def cleanup(self):
        """Clean shutdown"""
        try:
            print("🧹 Cleaning up...")
            
            if self.ethervox:
                await self.ethervox.cleanup()
            
            # Final memory report
            final_memory = gc.mem_free()
            print(f"💾 Final memory: {final_memory // 1024}KB free")
            print(f"📊 Min memory during run: {self.min_free_memory // 1024}KB")
            
        except Exception as e:
            print(f"⚠️  Cleanup error: {e}")

# Main demo function
async def main():
    """Main Pico voice assistant demo"""
    print("🍓 Starting Raspberry Pi Pico Voice Assistant")
    print("=" * 50)
    
    # Check if we're on the right platform
    try:
        import sys
        if 'rp2' not in sys.platform:
            print("⚠️  Warning: Not running on RP2040 platform")
            print(f"   Detected platform: {sys.platform}")
    except:
        pass
    
    # Memory check
    initial_memory = gc.mem_free()
    print(f"💾 Starting with {initial_memory // 1024}KB free memory")
    
    if initial_memory < 100 * 1024:  # 100KB minimum
        print("❌ Insufficient memory to run voice assistant")
        return
    
    # Create and run the assistant
    assistant = PicoVoiceAssistant()
    if assistant.ethervox:
        await assistant.run()
    else:
        print("❌ Failed to initialize voice assistant")

# Entry point
if __name__ == "__main__":
    try:
        import asyncio
        asyncio.run(main())
    except ImportError:
        print("❌ This demo requires asyncio support")
        print("   Use MicroPython firmware with asyncio enabled")
    except KeyboardInterrupt:
        print("\n👋 Demo stopped by user")
    except Exception as e:
        print(f"❌ Demo error: {e}")
    finally:
        # Final cleanup
        gc.collect()
        print(f"💾 Final memory: {gc.mem_free() // 1024}KB")
        print("🍓 Pico Voice Assistant demo complete")
