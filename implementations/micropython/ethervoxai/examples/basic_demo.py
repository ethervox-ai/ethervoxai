"""
🎙️ Basic Voice Assistant Demo

A simple demonstration of EthervoxAI on microcontrollers.
This example shows:
- Board auto-detection
- Audio I/O setup
- Wake word detection
- Basic voice commands
- LED feedback

Hardware Requirements:
- Supported microcontroller (Pico, Pico W, ESP32)
- I2S microphone (INMP441 recommended)
- I2S DAC/amp (PCM5102A + MAX98357A recommended)
- LED (usually built-in)
- Button (usually built-in)

Usage:
1. Connect audio hardware to configured pins
2. Flash this code to your microcontroller
3. Say "Hey Computer" to activate
4. Give voice commands like "turn on light" or "what time is it"
"""

import time
import gc
from ethervoxai import EthervoxAI

class BasicVoiceAssistant:
    """Simple voice assistant demonstration"""
    
    def __init__(self):
        """Initialize the voice assistant"""
        print("🎙️ Initializing EthervoxAI Basic Demo...")
        
        # Initialize EthervoxAI with auto-detection
        self.ethervox = EthervoxAI()
        
        # Check if initialization was successful
        if not self.ethervox:
            print("❌ Failed to initialize EthervoxAI")
            return
        
        # Set up voice commands
        self.commands = {
            'turn on light': self.turn_on_light,
            'turn off light': self.turn_off_light,
            'what time is it': self.tell_time,
            'how are you': self.status_check,
            'sleep mode': self.enter_sleep_mode,
            'system info': self.show_system_info
        }
        
        # State variables
        self.light_on = False
        self.is_running = True
        self.sleep_mode = False
        
        print("✅ Basic Voice Assistant initialized!")
        print("📢 Say 'Hey Computer' to activate voice commands")
        self._show_help()
    
    def _show_help(self):
        """Display available commands"""
        print("\n🎯 Available voice commands:")
        for command in self.commands.keys():
            print(f"   • {command}")
        print("   • help - show this help")
        print("   • stop - exit the demo\n")
    
    async def run(self):
        """Main assistant loop"""
        if not self.ethervox:
            print("❌ Cannot run: EthervoxAI not initialized")
            return
        
        print("🚀 Starting voice assistant...")
        print("🔘 Press the button or say 'Hey Computer' to activate")
        
        try:
            while self.is_running:
                if self.sleep_mode:
                    await self._sleep_mode_loop()
                else:
                    await self._active_mode_loop()
                
                # Small delay to prevent overwhelming the system
                await asyncio.sleep_ms(50)
                
        except KeyboardInterrupt:
            print("\n⏹️  Stopping voice assistant...")
        except Exception as e:
            print(f"❌ Error in main loop: {e}")
        finally:
            await self.cleanup()
    
    async def _active_mode_loop(self):
        """Main active processing loop"""
        try:
            # Check for wake word or button press
            if await self.ethervox.listen_for_wake_word(timeout_ms=100):
                print("👂 Wake word detected!")
                await self._process_voice_command()
            
            # Check button press (non-blocking)
            if self.ethervox.is_button_pressed():
                print("🔘 Button pressed!")
                await self._process_voice_command()
                
        except Exception as e:
            print(f"⚠️  Error in active loop: {e}")
    
    async def _sleep_mode_loop(self):
        """Sleep mode processing (reduced power)"""
        try:
            # In sleep mode, only respond to button or loud sounds
            if self.ethervox.is_button_pressed():
                print("🔘 Waking up from sleep mode...")
                self.sleep_mode = False
                self.ethervox.set_led(True)  # Indicate wake up
                await asyncio.sleep_ms(500)
                self.ethervox.set_led(False)
            else:
                # Longer sleep in sleep mode
                await asyncio.sleep_ms(1000)
                
        except Exception as e:
            print(f"⚠️  Error in sleep loop: {e}")
    
    async def _process_voice_command(self):
        """Process a voice command after wake word detection"""
        try:
            # Indicate listening with LED
            self.ethervox.set_led(True)
            
            print("🎤 Listening for command...")
            
            # Listen for command (with timeout)
            command_text = await self.ethervox.listen_for_command(timeout_ms=5000)
            
            if command_text:
                print(f"🗣️  Heard: '{command_text}'")
                await self._execute_command(command_text.lower())
            else:
                print("⏱️  No command heard, going back to listening...")
                await self._speak("Sorry, I didn't hear anything.")
            
        except Exception as e:
            print(f"❌ Error processing command: {e}")
        finally:
            # Turn off listening indicator
            self.ethervox.set_led(False)
    
    async def _execute_command(self, command_text):
        """Execute a recognized voice command"""
        try:
            # Check for exact matches first
            if command_text in self.commands:
                await self.commands[command_text]()
                return
            
            # Check for partial matches
            for cmd_key, cmd_func in self.commands.items():
                if cmd_key in command_text:
                    await cmd_func()
                    return
            
            # Handle special commands
            if 'help' in command_text:
                self._show_help()
                await self._speak("Commands shown on screen.")
            elif 'stop' in command_text or 'exit' in command_text:
                await self._speak("Goodbye!")
                self.is_running = False
            else:
                print(f"❓ Unknown command: {command_text}")
                await self._speak("Sorry, I don't understand that command.")
                
        except Exception as e:
            print(f"❌ Error executing command: {e}")
            await self._speak("Sorry, there was an error.")
    
    # Command implementations
    async def turn_on_light(self):
        """Turn on LED light"""
        self.light_on = True
        self.ethervox.set_led(True)
        print("💡 Light turned ON")
        await self._speak("Light is now on.")
    
    async def turn_off_light(self):
        """Turn off LED light"""
        self.light_on = False
        self.ethervox.set_led(False)
        print("🔘 Light turned OFF")
        await self._speak("Light is now off.")
    
    async def tell_time(self):
        """Tell the current time"""
        try:
            import time
            current_time = time.localtime()
            hour = current_time[3]
            minute = current_time[4]
            
            print(f"🕐 Current time: {hour:02d}:{minute:02d}")
            await self._speak(f"The time is {hour}:{minute:02d}")
        except Exception as e:
            print(f"❌ Error getting time: {e}")
            await self._speak("Sorry, I can't get the time right now.")
    
    async def status_check(self):
        """Report system status"""
        try:
            memory_free = gc.mem_free() // 1024
            board_info = self.ethervox.get_board_info()
            
            print(f"🤖 System Status:")
            print(f"   Memory free: {memory_free}KB")
            print(f"   Board: {board_info.get('board_name', 'Unknown')}")
            print(f"   Light: {'ON' if self.light_on else 'OFF'}")
            
            await self._speak(f"I'm doing well. Memory free: {memory_free} kilobytes.")
            
        except Exception as e:
            print(f"❌ Error checking status: {e}")
            await self._speak("I'm having trouble checking my status.")
    
    async def enter_sleep_mode(self):
        """Enter power saving sleep mode"""
        print("😴 Entering sleep mode...")
        self.sleep_mode = True
        self.ethervox.set_led(False)
        await self._speak("Going to sleep. Press the button to wake me up.")
    
    async def show_system_info(self):
        """Show detailed system information"""
        try:
            board_info = self.ethervox.get_board_info()
            
            print("🔧 System Information:")
            for key, value in board_info.items():
                print(f"   {key}: {value}")
            
            board_name = board_info.get('board_name', 'Unknown board')
            await self._speak(f"I'm running on a {board_name}.")
            
        except Exception as e:
            print(f"❌ Error getting system info: {e}")
            await self._speak("Sorry, I can't access system information.")
    
    async def _speak(self, text):
        """Convert text to speech (if TTS available)"""
        try:
            # Try to use TTS if available
            success = await self.ethervox.speak(text)
            if not success:
                # Fallback: just print and flash LED
                print(f"🔊 Would say: '{text}'")
                for _ in range(3):
                    self.ethervox.set_led(True)
                    await asyncio.sleep_ms(100)
                    self.ethervox.set_led(False)
                    await asyncio.sleep_ms(100)
        except Exception as e:
            print(f"⚠️  Speech error: {e}")
    
    async def cleanup(self):
        """Clean up resources"""
        try:
            if self.ethervox:
                await self.ethervox.cleanup()
            print("🧹 Cleanup completed")
        except Exception as e:
            print(f"⚠️  Cleanup error: {e}")

# Main execution
async def main():
    """Main demo function"""
    print("=" * 50)
    print("🎙️ EthervoxAI Basic Voice Assistant Demo")
    print("=" * 50)
    
    # Check for asyncio support
    try:
        import asyncio
    except ImportError:
        print("❌ This demo requires asyncio support")
        print("   Please use MicroPython with asyncio enabled")
        return
    
    # Create and run the assistant
    assistant = BasicVoiceAssistant()
    if assistant.ethervox:
        await assistant.run()
    else:
        print("❌ Failed to start voice assistant")

# Entry point
if __name__ == "__main__":
    # Handle both asyncio and non-asyncio environments
    try:
        import asyncio
        asyncio.run(main())
    except ImportError:
        print("❌ This demo requires asyncio support")
    except Exception as e:
        print(f"❌ Demo failed: {e}")
    finally:
        print("👋 Demo finished")
