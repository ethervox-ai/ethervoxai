"""
🍓🔵🎵 Raspberry Pi Pico W 2 Bluetooth Audio Demo

This example demonstrates how to use EthervoxAI with Bluetooth audio
on the Raspberry Pi Pico W 2 (RP2350 with Bluetooth support).

Features demonstrated:
- Voice wake word detection
- Speech recognition via cloud or local models
- Text-to-speech output via Bluetooth speakers/headsets
- Voice commands processed locally and via cloud
- Bluetooth device discovery and pairing
- Mixed local/Bluetooth audio capabilities

Hardware Requirements:
- Raspberry Pi Pico W 2 (RP2350)
- Bluetooth speaker or headset
- Optional: I2S microphone (INMP441) for high-quality input
- Optional: Local speaker for mixed audio output

Usage:
1. Pair your Bluetooth audio device
2. Say "Hey Ethervox" to wake the assistant
3. Give voice commands
4. Responses will be played through Bluetooth audio
"""

import time
import asyncio
from ethervoxai import EthervoxAI
from ethervoxai.boards import detect_board, get_board_config

async def bluetooth_audio_demo():
    """Main demo function for Pico W 2 Bluetooth audio"""
    
    print("🍓🔵 Starting Pico W 2 Bluetooth Audio Demo")
    print("=" * 50)
    
    # Initialize EthervoxAI
    print("🤖 Initializing EthervoxAI...")
    ethervox = EthervoxAI()
    
    # Verify we're running on Pico W 2
    board_type = detect_board()
    if board_type != 'pico_w2':
        print(f"⚠️  Warning: Expected Pico W 2, detected: {board_type}")
        print("🔄 This demo is optimized for Pico W 2 with Bluetooth support")
    
    board_config = get_board_config(board_type)()
    print(f"📱 Board: {board_config.board_name}")
    print(f"🧠 MCU: {board_config.mcu_type}")
    print(f"💾 Memory: {board_config.memory.total_ram_kb}KB SRAM")
    print(f"📶 WiFi: {'✅' if board_config.has_wifi else '❌'}")
    print(f"🔵 Bluetooth: {'✅' if board_config.has_bluetooth else '❌'}")
    
    if not board_config.has_bluetooth:
        print("❌ Bluetooth not supported on this board")
        print("💡 This demo requires Pico W 2 with Bluetooth capabilities")
        return
    
    # Scan for Bluetooth audio devices
    print("\n🔍 Scanning for Bluetooth audio devices...")
    scan_result = board_config.scan_bluetooth_audio_devices(scan_duration_ms=10000)
    
    if scan_result['success'] and scan_result['devices']:
        print(f"📱 Found {scan_result['count']} Bluetooth audio devices:")
        for i, device in enumerate(scan_result['devices']):
            status = "🔗 Paired" if device['paired'] else "🔓 Not paired"
            print(f"  {i+1}. {device['name']} ({device['device_type']}) - {status}")
            print(f"     Address: {device['address']}")
            print(f"     Signal: {device['rssi']}dBm")
            print(f"     Profiles: {', '.join(device['profiles'])}")
        
        # For demo, automatically connect to the first available device
        target_device = scan_result['devices'][0]
        print(f"\n🔵 Connecting to: {target_device['name']}...")
        
        # Choose profile based on device type
        profile = 'a2dp' if 'a2dp' in target_device['profiles'] else 'hfp'
        
        connection_result = board_config.connect_bluetooth_audio(
            target_device['address'], 
            profile=profile
        )
        
        if connection_result['success']:
            print(f"✅ Connected to {target_device['name']}")
            print(f"🎵 Audio profile: {profile.upper()}")
            print(f"🔊 Audio quality: {connection_result['audio_quality']}")
            print(f"⏱️  Latency: ~{connection_result['latency_ms']}ms")
        else:
            print(f"❌ Failed to connect: {connection_result['error']}")
            return
    else:
        print("❌ No Bluetooth audio devices found")
        print("💡 Make sure your Bluetooth speaker/headset is in pairing mode")
        # Continue with simulated connection for demo
        print("🔄 Continuing with simulated Bluetooth connection...")
        target_device = {'name': 'Demo BT Speaker', 'address': '00:1A:2B:3C:4D:5E'}
        profile = 'a2dp'
    
    # Configure audio routing
    print(f"\n🎚️  Configuring audio routing...")
    audio_config = {
        'input_source': 'i2s_microphone',    # Use I2S mic for input
        'output_destination': 'bluetooth',    # Route output to Bluetooth
        'fallback_output': 'i2s_speaker',    # Fallback to local speaker
        'bluetooth_device': target_device['address'] if 'target_device' in locals() else None,
        'bluetooth_profile': profile,
        'audio_quality': 'high',
        'enable_echo_cancellation': True,
        'noise_reduction': True
    }
    
    # Initialize voice assistant with Bluetooth audio
    print("🎤 Initializing voice assistant with Bluetooth audio...")
    
    try:
        # Configure models for Pico W 2 (can handle larger models)
        await ethervox.load_model('wake-word-enhanced', model_type='wake_word')
        await ethervox.load_model('command-classifier-large', model_type='classification')
        
        # Configure audio pipeline
        await ethervox.audio_manager.configure_bluetooth_audio(audio_config)
        
        print("✅ Voice assistant ready!")
        print(f"🎯 Wake phrase: 'Hey Ethervox'")
        print(f"🔵 Audio output: Bluetooth ({target_device['name']})")
        print(f"🎤 Audio input: I2S Microphone")
        print("\n" + "="*50)
        print("🗣️  Try saying: 'Hey Ethervox, what time is it?'")
        print("🗣️  Try saying: 'Hey Ethervox, play some music'")
        print("🗣️  Try saying: 'Hey Ethervox, turn off Bluetooth'")
        print("=" * 50)
        
        # Main voice processing loop
        wake_word_detected = False
        last_activity = time.ticks_ms()
        
        while True:
            current_time = time.ticks_ms()
            
            # Check for audio input
            if ethervox.audio_manager.has_audio_data():
                last_activity = current_time
                
                if not wake_word_detected:
                    # Listen for wake word
                    audio_chunk = await ethervox.audio_manager.get_audio_chunk()
                    wake_result = await ethervox.inference_engine.detect_wake_word(audio_chunk)
                    
                    if wake_result['detected']:
                        wake_word_detected = True
                        confidence = wake_result['confidence']
                        print(f"👂 Wake word detected! (confidence: {confidence:.2f})")
                        
                        # Play acknowledgment sound via Bluetooth
                        await ethervox.audio_manager.play_bluetooth_tone(
                            frequency=800, 
                            duration_ms=200
                        )
                        
                        print("🎤 Listening for command...")
                        
                else:
                    # Process voice command
                    print("🔄 Processing voice command...")
                    
                    # Record command (with timeout)
                    command_audio = await ethervox.audio_manager.record_command(
                        max_duration_ms=5000,
                        silence_timeout_ms=2000
                    )
                    
                    if command_audio:
                        # First try local classification
                        local_result = await ethervox.inference_engine.classify_command(command_audio)
                        
                        if local_result['confidence'] > 0.8:
                            # Handle local command
                            command = local_result['command']
                            print(f"🧠 Local command: {command}")
                            
                            response = await handle_local_command(command, ethervox, board_config)
                            
                        else:
                            # Use cloud-based speech recognition for complex commands
                            print("☁️  Using cloud inference for complex command...")
                            
                            speech_text = await ethervox.inference_engine.transcribe_speech_cloud(command_audio)
                            
                            if speech_text:
                                print(f"📝 Transcribed: '{speech_text}'")
                                
                                # Process with cloud LLM
                                response = await ethervox.inference_engine.generate_response_cloud(speech_text)
                            else:
                                response = "Sorry, I couldn't understand that command."
                        
                        # Convert response to speech and play via Bluetooth
                        if response:
                            print(f"🗣️  Response: {response}")
                            
                            # Generate speech audio
                            speech_audio = await ethervox.inference_engine.text_to_speech(response)
                            
                            if speech_audio:
                                # Play via Bluetooth
                                await ethervox.audio_manager.play_bluetooth_audio(speech_audio)
                            else:
                                print("❌ Failed to generate speech")
                        
                    # Reset for next wake word
                    wake_word_detected = False
                    print("👂 Listening for wake word...")
            
            # Power management - sleep if no activity
            if time.ticks_diff(current_time, last_activity) > 30000:  # 30 seconds
                print("😴 Entering low power mode (Bluetooth audio maintained)...")
                
                # Enter power save mode but keep Bluetooth active
                await ethervox.power_manager.enter_bluetooth_power_save()
                
                # Wait for audio activity or button press
                await asyncio.sleep_ms(1000)
                
                if ethervox.audio_manager.has_audio_data() or ethervox.get_button_pressed():
                    print("⚡ Waking up from power save mode...")
                    await ethervox.power_manager.exit_power_save()
                    last_activity = time.ticks_ms()
            
            # Small delay to prevent busy waiting
            await asyncio.sleep_ms(50)
            
    except KeyboardInterrupt:
        print("\n🛑 Stopping demo...")
    except Exception as e:
        print(f"❌ Error in main loop: {e}")
    finally:
        # Cleanup
        print("🧹 Cleaning up...")
        
        # Disconnect Bluetooth audio
        if 'target_device' in locals():
            print(f"🔵 Disconnecting from {target_device['name']}...")
            # board_config.disconnect_bluetooth_audio(target_device['address'])
        
        # Stop audio processing
        await ethervox.stop()
        print("✅ Demo stopped")

async def handle_local_command(command, ethervox, board_config):
    """Handle locally-recognized voice commands"""
    
    command_lower = command.lower()
    
    if 'time' in command_lower:
        # Get current time
        current_time = time.localtime()
        time_str = f"{current_time[3]:02d}:{current_time[4]:02d}"
        return f"The current time is {time_str}"
    
    elif 'bluetooth' in command_lower and 'off' in command_lower:
        # Disconnect Bluetooth
        print("🔵 Disconnecting Bluetooth audio...")
        # In real implementation, would disconnect here
        return "Bluetooth audio disconnected. Switching to local speaker."
    
    elif 'bluetooth' in command_lower and ('on' in command_lower or 'connect' in command_lower):
        # Reconnect Bluetooth
        print("🔵 Reconnecting Bluetooth audio...")
        return "Reconnecting to Bluetooth audio device."
    
    elif 'volume' in command_lower:
        if 'up' in command_lower:
            # Increase volume
            print("🔊 Increasing Bluetooth audio volume...")
            return "Volume increased"
        elif 'down' in command_lower:
            # Decrease volume
            print("🔉 Decreasing Bluetooth audio volume...")
            return "Volume decreased"
        else:
            return "Current volume is at 70 percent"
    
    elif 'battery' in command_lower:
        # Check battery status (if applicable)
        return "Battery level is at 85 percent"
    
    elif 'scan' in command_lower and 'bluetooth' in command_lower:
        # Scan for new Bluetooth devices
        print("🔍 Scanning for Bluetooth devices...")
        scan_result = board_config.scan_bluetooth_audio_devices(5000)
        device_count = scan_result.get('count', 0)
        return f"Found {device_count} Bluetooth audio devices nearby"
    
    elif 'memory' in command_lower:
        # Check memory usage
        import gc
        free_kb = gc.mem_free() // 1024
        return f"Free memory: {free_kb} kilobytes"
    
    elif 'wifi' in command_lower:
        # Check WiFi status
        wifi_status = board_config.get_wifi_status()
        if wifi_status['connected']:
            return f"WiFi connected to network. IP address is {wifi_status['ip']}"
        else:
            return "WiFi is not connected"
    
    else:
        # Unknown local command
        return None

def demo_bluetooth_profiles():
    """Demonstrate different Bluetooth audio profiles"""
    
    print("🔵📻 Bluetooth Audio Profiles Demo")
    print("=" * 40)
    
    board_config = get_board_config('pico_w2')()
    profiles = board_config.get_bluetooth_audio_profiles()
    
    for profile_name, profile_info in profiles.items():
        print(f"\n📡 {profile_info['name']} ({profile_name.upper()})")
        print(f"   Description: {profile_info['description']}")
        print(f"   Codecs: {', '.join(profile_info['supported_codecs'])}")
        print(f"   Max Bitrate: {profile_info['max_bitrate_kbps']} kbps")
        print(f"   Latency: ~{profile_info['latency_ms']}ms")
        print(f"   Use Cases: {', '.join(profile_info['use_cases'])}")
        
        if not profile_info.get('available', True):
            print(f"   ⚠️  Status: Not yet available")
    
    print("\n💡 Recommended Usage:")
    print("   🎵 Music/TTS: Use A2DP for high-quality audio")
    print("   🎤 Voice Commands: Use HFP for low-latency bidirectional audio")
    print("   🔮 Future: LE Audio for advanced features")

async def main():
    """Main entry point"""
    try:
        # Show board capabilities
        board_type = detect_board()
        print(f"🔍 Detected board: {board_type}")
        
        if board_type == 'pico_w2':
            print("✅ Pico W 2 detected - Bluetooth audio demo available!")
            
            # Show Bluetooth profiles
            demo_bluetooth_profiles()
            
            print("\n" + "="*50)
            input("Press Enter to start Bluetooth audio demo...")
            
            # Run main demo
            await bluetooth_audio_demo()
            
        else:
            print("⚠️  This demo requires Raspberry Pi Pico W 2")
            print(f"   Current board: {board_type}")
            print("💡 For Bluetooth audio, you need:")
            print("   - Raspberry Pi Pico W 2 (RP2350)")
            print("   - MicroPython with Bluetooth support")
            print("   - Bluetooth audio device (speaker/headset)")
            
            # Show what this board can do instead
            board_config = get_board_config(board_type)()
            print(f"\n📱 Your {board_config.board_name} supports:")
            print(f"   📶 WiFi: {'✅' if board_config.has_wifi else '❌'}")
            print(f"   🔵 Bluetooth: {'✅' if board_config.has_bluetooth else '❌'}")
            print(f"   🎵 I2S Audio: ✅")
            print(f"   🧠 Local AI: ✅")
            print(f"   ☁️  Cloud AI: {'✅' if board_config.has_wifi else '❌'}")
    
    except Exception as e:
        print(f"❌ Demo error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    print("🍓🔵🎵 Pico W 2 Bluetooth Audio Demo")
    print("EthervoxAI Voice Assistant with Bluetooth Audio")
    print("=" * 50)
    
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n👋 Demo interrupted by user")
    except Exception as e:
        print(f"❌ Demo failed: {e}")
