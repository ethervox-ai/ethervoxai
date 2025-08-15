"""
🔵🎵 Simple Pico W 2 Bluetooth Audio Example

A basic example showing how to use EthervoxAI with Bluetooth audio
on the Raspberry Pi Pico W 2.

This example:
1. Detects and connects to a Bluetooth speaker/headset
2. Listens for "Hey Ethervox" wake word via I2S microphone
3. Processes voice commands locally or via cloud
4. Responds via Bluetooth audio output

Hardware Required:
- Raspberry Pi Pico W 2 (RP2350)
- I2S microphone (INMP441) for voice input
- Bluetooth speaker or headset for audio output
"""

import asyncio
import time
from ethervoxai import EthervoxAI

async def simple_bluetooth_voice_assistant():
    """Simple Bluetooth voice assistant"""
    
    print("🔵 Pico W 2 Bluetooth Voice Assistant")
    print("=" * 40)
    
    # Initialize EthervoxAI for Pico W 2
    ai = EthervoxAI(board_type='pico_w2')
    
    try:
        # Initialize the voice assistant
        print("🤖 Initializing voice assistant...")
        await ai.initialize()
        
        # Check if Bluetooth is available
        board_info = ai.get_board_info()
        if not board_info.get('has_bluetooth'):
            print("❌ Bluetooth not available on this board")
            print("💡 This example requires Pico W 2 with Bluetooth support")
            return
        
        print(f"✅ Board: {board_info['board_name']}")
        print(f"🧠 Memory: {board_info['memory_free_kb']}KB free")
        print(f"🔵 Bluetooth: Available")
        
        # Scan for Bluetooth audio devices
        print("\n🔍 Scanning for Bluetooth audio devices...")
        board_config = ai.get_board_config()
        
        devices = board_config.scan_bluetooth_audio_devices(scan_duration_ms=5000)
        
        if devices['success'] and devices['devices']:
            # Use the first available device
            target_device = devices['devices'][0]
            print(f"🔵 Connecting to: {target_device['name']}")
            
            # Connect to the device
            connection = board_config.connect_bluetooth_audio(
                target_device['address'],
                profile='a2dp'  # Use A2DP for high-quality audio output
            )
            
            if connection['success']:
                print(f"✅ Connected! Audio quality: {connection['audio_quality']}")
                
                # Configure audio routing
                await ai.configure_audio({
                    'input_source': 'i2s_microphone',
                    'output_destination': 'bluetooth',
                    'bluetooth_device': target_device['address']
                })
                
                print("\n🎤 Voice assistant ready!")
                print("🗣️  Say 'Hey Ethervox' followed by a command")
                print("📱 Audio will play through your Bluetooth device")
                print("🛑 Press Ctrl+C to stop")
                
                # Main listening loop
                while True:
                    print("\n👂 Listening for wake word...")
                    
                    # Wait for wake word
                    wake_detected = await ai.wait_for_wake_word(timeout_ms=30000)
                    
                    if wake_detected:
                        print("🎯 Wake word detected!")
                        
                        # Play acknowledgment tone via Bluetooth
                        await ai.play_bluetooth_tone(frequency=800, duration_ms=200)
                        
                        print("🎤 Listening for command...")
                        
                        # Record voice command
                        command_audio = await ai.record_command(max_duration_ms=5000)
                        
                        if command_audio:
                            print("🔄 Processing command...")
                            
                            # Try local processing first
                            local_result = await ai.process_command_local(command_audio)
                            
                            if local_result and local_result['confidence'] > 0.8:
                                response = local_result['response']
                                print(f"🧠 Local: {response}")
                            else:
                                # Use cloud processing for complex commands
                                print("☁️  Using cloud processing...")
                                response = await ai.process_command_cloud(command_audio)
                                print(f"☁️  Cloud: {response}")
                            
                            # Convert response to speech and play via Bluetooth
                            if response:
                                await ai.speak_via_bluetooth(response)
                        else:
                            print("❌ No command recorded")
                            await ai.speak_via_bluetooth("Sorry, I didn't hear anything.")
                    
                    # Brief pause
                    await asyncio.sleep(0.1)
            
            else:
                print(f"❌ Failed to connect: {connection['error']}")
                print("💡 Make sure your device is in pairing mode")
        
        else:
            print("❌ No Bluetooth audio devices found")
            print("💡 Make sure your Bluetooth speaker/headset is:")
            print("   - Powered on")
            print("   - In pairing/discoverable mode")
            print("   - Within range (~10 meters)")
    
    except KeyboardInterrupt:
        print("\n🛑 Stopping voice assistant...")
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        # Cleanup
        await ai.stop()
        print("✅ Voice assistant stopped")

async def test_bluetooth_connection():
    """Test basic Bluetooth functionality"""
    
    print("🔵 Testing Bluetooth Connection")
    print("=" * 30)
    
    ai = EthervoxAI(board_type='pico_w2')
    await ai.initialize()
    
    board_config = ai.get_board_config()
    
    # Test scanning
    print("🔍 Testing Bluetooth scan...")
    scan_result = board_config.scan_bluetooth_audio_devices(3000)
    
    print(f"📊 Scan result: {scan_result['success']}")
    print(f"📱 Devices found: {scan_result.get('count', 0)}")
    
    for device in scan_result.get('devices', []):
        print(f"   • {device['name']} ({device['device_type']})")
        print(f"     Signal: {device['rssi']}dBm")
    
    # Test audio profiles
    print("\n📡 Supported Bluetooth audio profiles:")
    profiles = board_config.get_bluetooth_audio_profiles()
    
    for name, info in profiles.items():
        available = info.get('available', True)
        status = "✅" if available else "⏳"
        print(f"   {status} {info['name']} ({name.upper()})")
        print(f"      Latency: ~{info['latency_ms']}ms")
        print(f"      Use cases: {', '.join(info['use_cases'][:2])}")

def main():
    """Main entry point"""
    
    try:
        # Quick test first
        print("🧪 Running Bluetooth test...")
        asyncio.run(test_bluetooth_connection())
        
        print("\n" + "="*50)
        input("Press Enter to start voice assistant demo...")
        
        # Main demo
        asyncio.run(simple_bluetooth_voice_assistant())
        
    except KeyboardInterrupt:
        print("\n👋 Demo interrupted")
    except Exception as e:
        print(f"❌ Demo failed: {e}")

if __name__ == "__main__":
    main()
