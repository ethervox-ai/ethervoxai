/**
 * Interactive Audio Input/Output Test Console
 * 
 * Command-line interface for testing EthervoxAI audio capabilities
 */

import * as readline from 'readline';
import { AudioInputOutputTester } from './AudioInputOutputTester';

class AudioTestConsole {
  private tester: AudioInputOutputTester;
  private rl: readline.Interface;

  constructor() {
    this.tester = new AudioInputOutputTester();
    this.rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout,
      prompt: 'AudioTest> '
    });

    this.setupEventHandlers();
  }

  private setupEventHandlers(): void {
    this.tester.on('recordingStarted', (recording) => {
      console.log(`🎤 Recording started: ${recording.id}`);
    });

    this.tester.on('recordingStopped', (recording) => {
      console.log(`🛑 Recording stopped: ${recording.id} (${recording.duration}s)`);
    });

    this.tester.on('playbackCompleted', (recording) => {
      console.log(`🔊 Playback completed: ${recording.id}`);
    });

    this.tester.on('recordingError', (error) => {
      console.error(`❌ Recording error: ${error.message}`);
    });
  }

  async start(): Promise<void> {
    console.log('🎵 EthervoxAI Audio Input/Output Test Console');
    console.log('===========================================');
    console.log();

    // Initialize
    await this.tester.discoverAudioDevices();
    
    this.showWelcome();
    this.rl.prompt();

    this.rl.on('line', async (input) => {
      await this.handleCommand(input.trim());
      this.rl.prompt();
    });

    this.rl.on('close', () => {
      console.log('👋 Goodbye!');
      process.exit(0);
    });
  }

  private showWelcome(): void {
    console.log('Available commands:');
    console.log('  devices      - List available audio devices');
    console.log('  modules      - List available audio modules');
    console.log('  config       - Show/update configuration');
    console.log('  record       - Start recording');
    console.log('  stop         - Stop current recording');
    console.log('  play <id>    - Play back a recording');
    console.log('  recordings   - List all recordings');
    console.log('  tts <text>   - Test text-to-speech');
    console.log('  status       - Show current status');
    console.log('  help         - Show this help');
    console.log('  quit         - Exit application');
    console.log();
  }

  private async handleCommand(input: string): Promise<void> {
    const [command, ...args] = input.split(' ');

    try {
      switch (command.toLowerCase()) {
        case 'devices':
          await this.showDevices();
          break;

        case 'modules':
          this.showModules();
          break;

        case 'config':
          await this.handleConfig(args);
          break;

        case 'record':
          await this.startRecording();
          break;

        case 'stop':
          await this.stopRecording();
          break;

        case 'play':
          await this.playRecording(args[0]);
          break;

        case 'recordings':
          this.showRecordings();
          break;

        case 'tts':
          await this.testTTS(args.join(' '));
          break;

        case 'status':
          this.showStatus();
          break;

        case 'help':
          this.showWelcome();
          break;

        case 'quit':
        case 'exit':
          this.rl.close();
          break;

        default:
          if (command) {
            console.log(`❓ Unknown command: ${command}. Type 'help' for available commands.`);
          }
          break;
      }
    } catch (error: any) {
      console.error(`❌ Error: ${error.message}`);
    }
  }

  private async showDevices(): Promise<void> {
    console.log('🎤 Available Audio Devices:');
    console.log('===========================');
    
    const devices = await this.tester.discoverAudioDevices();
    
    const inputDevices = devices.filter(d => d.type === 'input');
    const outputDevices = devices.filter(d => d.type === 'output');

    if (inputDevices.length > 0) {
      console.log('\n📥 Input Devices:');
      inputDevices.forEach((device, index) => {
        const defaultMarker = device.isDefault ? ' (default)' : '';
        console.log(`  ${index + 1}. ${device.name}${defaultMarker}`);
        console.log(`     ID: ${device.id}, Channels: ${device.channels}, Sample Rate: ${device.sampleRate}Hz`);
      });
    }

    if (outputDevices.length > 0) {
      console.log('\n📤 Output Devices:');
      outputDevices.forEach((device, index) => {
        const defaultMarker = device.isDefault ? ' (default)' : '';
        console.log(`  ${index + 1}. ${device.name}${defaultMarker}`);
        console.log(`     ID: ${device.id}, Channels: ${device.channels}, Sample Rate: ${device.sampleRate}Hz`);
      });
    }
    console.log();
  }

  private showModules(): void {
    console.log('🔧 Available Audio Modules:');
    console.log('===========================');
    
    const modules = this.tester.getAvailableModules();
    
    modules.forEach((module, index) => {
      const inputSupport = module.inputSupported ? '✅' : '❌';
      const outputSupport = module.outputSupported ? '✅' : '❌';
      
      console.log(`  ${index + 1}. ${module.name}`);
      console.log(`     ${module.description}`);
      console.log(`     Input: ${inputSupport}, Output: ${outputSupport}`);
      console.log(`     ID: ${module.id}`);
      console.log();
    });
  }

  private async handleConfig(args: string[]): Promise<void> {
    if (args.length === 0) {
      // Show current config
      console.log('⚙️ Current Configuration:');
      console.log('========================');
      const config = this.tester.getConfig();
      Object.entries(config).forEach(([key, value]) => {
        console.log(`  ${key}: ${value}`);
      });
      console.log();
      console.log('Usage: config <property> <value>');
      console.log('Properties: inputDevice, outputDevice, inputChannels, outputChannels,');
      console.log('           sampleRate, bitDepth, recordingDuration, selectedModule');
      return;
    }

    if (args.length !== 2) {
      console.log('❌ Usage: config <property> <value>');
      return;
    }

    const [property, value] = args;
    const config = this.tester.getConfig();

    // Validate and convert value
    let newValue: any = value;
    if (['inputChannels', 'outputChannels', 'sampleRate', 'bitDepth', 'recordingDuration'].includes(property)) {
      newValue = parseInt(value);
      if (isNaN(newValue)) {
        console.log(`❌ ${property} must be a number`);
        return;
      }
    }

    if (!(property in config)) {
      console.log(`❌ Unknown property: ${property}`);
      return;
    }

    this.tester.updateConfig({ [property]: newValue });
    console.log(`✅ ${property} set to: ${newValue}`);
  }

  private async startRecording(): Promise<void> {
    try {
      await this.tester.startRecording();
      console.log('🎤 Recording started. Type "stop" to end recording.');
    } catch (error: any) {
      console.error(`❌ Failed to start recording: ${error.message}`);
    }
  }

  private async stopRecording(): Promise<void> {
    try {
      const recording = await this.tester.stopRecording();
      if (recording) {
        console.log(`✅ Recording stopped and saved: ${recording.id}`);
      } else {
        console.log('⚠️ No active recording to stop');
      }
    } catch (error: any) {
      console.error(`❌ Failed to stop recording: ${error.message}`);
    }
  }

  private async playRecording(recordingId: string): Promise<void> {
    if (!recordingId) {
      console.log('❌ Usage: play <recording-id>');
      console.log('Use "recordings" command to see available recordings');
      return;
    }

    try {
      await this.tester.playbackRecording(recordingId);
      console.log(`✅ Playback completed: ${recordingId}`);
    } catch (error: any) {
      console.error(`❌ Playback failed: ${error.message}`);
    }
  }

  private showRecordings(): void {
    console.log('📼 Recording History:');
    console.log('====================');
    
    const recordings = this.tester.getRecordings();
    
    if (recordings.length === 0) {
      console.log('  No recordings available');
      console.log('  Use "record" command to create a recording');
      return;
    }

    recordings.forEach((recording, index) => {
      console.log(`  ${index + 1}. ${recording.id}`);
      console.log(`     Created: ${recording.startTime.toLocaleString()}`);
      console.log(`     Duration: ${recording.duration}s`);
      console.log(`     Input: ${recording.inputDevice} (${recording.inputModule})`);
      console.log(`     Format: ${recording.format.channels}ch, ${recording.format.sampleRate}Hz, ${recording.format.bitDepth}bit`);
      console.log(`     Size: ${recording.audioData.length} bytes`);
      console.log();
    });
  }

  private async testTTS(text: string): Promise<void> {
    if (!text) {
      console.log('❌ Usage: tts <text to speak>');
      return;
    }

    console.log(`🗣️ Testing TTS: "${text}"`);
    
    try {
      await this.tester.testTextToSpeech(text);
      console.log('✅ TTS test completed');
    } catch (error: any) {
      console.error(`❌ TTS test failed: ${error.message}`);
    }
  }

  private showStatus(): void {
    console.log('📊 System Status:');
    console.log('=================');
    
    const status = this.tester.getStatus();
    
    console.log(`Recording: ${status.isRecording ? '🎤 Active' : '⏹️ Stopped'}`);
    if (status.currentRecording) {
      console.log(`Current Recording: ${status.currentRecording.id}`);
    }
    console.log(`Total Recordings: ${status.totalRecordings}`);
    console.log(`Available Devices: ${status.availableDevices}`);
    console.log(`Available Modules: ${status.availableModules}`);
    console.log();
    
    console.log('Current Configuration:');
    Object.entries(status.config).forEach(([key, value]) => {
      console.log(`  ${key}: ${value}`);
    });
    console.log();
    
    console.log('Audio Manager Status:');
    console.log(`  Platform: ${status.audioManagerStatus.platform} ${status.audioManagerStatus.architecture}`);
    console.log(`  Current Output: ${status.audioManagerStatus.currentOutput}`);
    console.log(`  Available Outputs: ${status.audioManagerStatus.availableOutputs.length}`);
    status.audioManagerStatus.availableOutputs.forEach((output: any) => {
      console.log(`    • ${output.description} (${output.type})`);
    });
  }
}

// Main entry point
async function main() {
  try {
    const console = new AudioTestConsole();
    await console.start();
  } catch (error: any) {
    console.error('❌ Failed to start audio test console:', error.message);
    process.exit(1);
  }
}

// Handle uncaught exceptions
process.on('uncaughtException', (error) => {
  console.error('❌ Uncaught exception:', error.message);
  process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
  console.error('❌ Unhandled promise rejection:', reason);
  process.exit(1);
});

// Run if this file is executed directly
if (require.main === module) {
  main();
}

export { AudioTestConsole };
export default AudioTestConsole;
