#!/bin/bash

# EthervoxAI Raspberry Pi 5 Audio Setup Script
# Installs and configures Text-to-Speech and audio dependencies

set -e

echo "🍓 EthervoxAI Raspberry Pi 5 Audio Setup"
echo "========================================"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running on Raspberry Pi
check_raspberry_pi() {
    log_info "Checking if running on Raspberry Pi..."
    
    if [[ -f /proc/device-tree/model ]]; then
        MODEL=$(cat /proc/device-tree/model)
        if [[ $MODEL == *"Raspberry Pi"* ]]; then
            log_success "Detected: $MODEL"
            return 0
        fi
    fi
    
    log_warning "Not running on Raspberry Pi, but continuing anyway..."
    return 0
}

# Update system packages
update_system() {
    log_info "Updating system packages..."
    sudo apt update && sudo apt upgrade -y
    log_success "System packages updated"
}

# Install audio and TTS dependencies
install_audio_deps() {
    log_info "Installing audio and TTS dependencies..."
    
    # Core audio packages
    sudo apt install -y \
        alsa-utils \
        pulseaudio \
        pulseaudio-utils \
        pulseaudio-module-bluetooth \
        pavucontrol
    
    log_success "Core audio packages installed"
    
    # Text-to-Speech engines
    log_info "Installing Text-to-Speech engines..."
    
    sudo apt install -y \
        espeak \
        espeak-ng \
        espeak-data \
        libttspico-utils \
        festival \
        festvox-kallpc16k \
        festvox-en1
    
    log_success "TTS engines installed"
}

# Configure audio system
configure_audio() {
    log_info "Configuring audio system..."
    
    # Enable audio for current user
    sudo usermod -a -G audio $USER
    
    # Start PulseAudio if not running
    if ! pgrep -x "pulseaudio" > /dev/null; then
        log_info "Starting PulseAudio..."
        pulseaudio --start --log-target=syslog
    fi
    
    # Configure ALSA for better compatibility
    if [[ ! -f ~/.asoundrc ]]; then
        log_info "Creating ALSA configuration..."
        cat > ~/.asoundrc << 'EOF'
pcm.!default {
    type pulse
    fallback "sysdefault"
    hint {
        show on
        description "Default ALSA Output (currently PulseAudio Sound Server)"
    }
}

ctl.!default {
    type pulse
    fallback "sysdefault"
}
EOF
        log_success "ALSA configuration created"
    fi
}

# Test TTS engines
test_tts_engines() {
    log_info "Testing TTS engines..."
    
    # Test eSpeak
    log_info "Testing eSpeak..."
    if command -v espeak &> /dev/null; then
        echo "Testing eSpeak TTS engine" | espeak -s 150 -a 80 2>/dev/null && \
        log_success "eSpeak working" || log_warning "eSpeak may have issues"
    else
        log_error "eSpeak not found"
    fi
    
    # Test Pico2Wave
    log_info "Testing Pico2Wave..."
    if command -v pico2wave &> /dev/null; then
        TEMP_FILE="/tmp/pico_test.wav"
        pico2wave -w "$TEMP_FILE" "Testing Pico2Wave TTS engine" 2>/dev/null && \
        aplay "$TEMP_FILE" 2>/dev/null && \
        rm -f "$TEMP_FILE" && \
        log_success "Pico2Wave working" || log_warning "Pico2Wave may have issues"
    else
        log_error "Pico2Wave not found"
    fi
    
    # Test Festival
    log_info "Testing Festival..."
    if command -v festival &> /dev/null; then
        echo "Testing Festival TTS engine" | festival --tts 2>/dev/null && \
        log_success "Festival working" || log_warning "Festival may have issues"
    else
        log_error "Festival not found"
    fi
}

# Detect and configure Bluetooth audio
configure_bluetooth() {
    log_info "Configuring Bluetooth audio..."
    
    # Install Bluetooth audio support
    sudo apt install -y \
        bluez \
        bluez-tools \
        pulseaudio-module-bluetooth
    
    # Enable Bluetooth service
    sudo systemctl enable bluetooth
    sudo systemctl start bluetooth
    
    # Add user to bluetooth group
    sudo usermod -a -G bluetooth $USER
    
    # Configure PulseAudio for Bluetooth
    if [[ ! -f ~/.config/pulse/default.pa ]]; then
        mkdir -p ~/.config/pulse
        cp /etc/pulse/default.pa ~/.config/pulse/default.pa
    fi
    
    # Ensure Bluetooth modules are loaded
    if ! grep -q "load-module module-bluetooth-discover" ~/.config/pulse/default.pa; then
        echo "load-module module-bluetooth-discover" >> ~/.config/pulse/default.pa
        log_success "Bluetooth audio modules configured"
    fi
    
    # Restart PulseAudio to load Bluetooth modules
    pulseaudio -k && pulseaudio --start
    
    log_success "Bluetooth audio configuration complete"
}

# Show available audio devices
show_audio_devices() {
    log_info "Available audio devices:"
    
    echo ""
    echo "ALSA Devices:"
    aplay -l 2>/dev/null || log_warning "No ALSA devices found"
    
    echo ""
    echo "PulseAudio Sinks:"
    pactl list sinks short 2>/dev/null || log_warning "PulseAudio not running"
    
    echo ""
    echo "Connected Bluetooth devices:"
    bluetoothctl devices 2>/dev/null || log_warning "Bluetooth not available"
}

# Create test script
create_test_script() {
    log_info "Creating audio test script..."
    
    cat > ~/test_ethervoxai_audio.sh << 'EOF'
#!/bin/bash

# EthervoxAI Audio Test Script for Raspberry Pi 5

echo "🍓 Testing EthervoxAI Audio on Raspberry Pi 5"
echo "=============================================="

# Test eSpeak
echo ""
echo "Testing eSpeak:"
if command -v espeak &> /dev/null; then
    echo "Hello from EthervoxAI using eSpeak!" | espeak -s 150 -a 80
    echo "✅ eSpeak test complete"
else
    echo "❌ eSpeak not available"
fi

# Test Pico2Wave
echo ""
echo "Testing Pico2Wave:"
if command -v pico2wave &> /dev/null; then
    TEMP_FILE="/tmp/pico_ethervox_test.wav"
    pico2wave -w "$TEMP_FILE" "Hello from EthervoxAI using Pico2Wave!"
    aplay "$TEMP_FILE" 2>/dev/null
    rm -f "$TEMP_FILE"
    echo "✅ Pico2Wave test complete"
else
    echo "❌ Pico2Wave not available"
fi

# Test Festival
echo ""
echo "Testing Festival:"
if command -v festival &> /dev/null; then
    echo "Hello from EthervoxAI using Festival!" | festival --tts
    echo "✅ Festival test complete"
else
    echo "❌ Festival not available"
fi

# Show audio devices
echo ""
echo "Available audio devices:"
echo "------------------------"
pactl list sinks short

echo ""
echo "🎉 Audio test complete!"
EOF

    chmod +x ~/test_ethervoxai_audio.sh
    log_success "Test script created: ~/test_ethervoxai_audio.sh"
}

# Main installation process
main() {
    echo ""
    log_info "Starting EthervoxAI Raspberry Pi 5 audio setup..."
    
    check_raspberry_pi
    update_system
    install_audio_deps
    configure_audio
    configure_bluetooth
    test_tts_engines
    show_audio_devices
    create_test_script
    
    echo ""
    log_success "🎉 EthervoxAI Raspberry Pi 5 audio setup complete!"
    echo ""
    echo "📋 Next steps:"
    echo "1. Reboot your Raspberry Pi to ensure all changes take effect"
    echo "2. Pair your Bluetooth speaker using: bluetoothctl"
    echo "3. Run the test script: ~/test_ethervoxai_audio.sh"
    echo "4. Configure EthervoxAI to use RaspberryPiAudioManager"
    echo ""
    echo "🔵 To pair Bluetooth speaker:"
    echo "   sudo bluetoothctl"
    echo "   scan on"
    echo "   pair [DEVICE_MAC]"
    echo "   connect [DEVICE_MAC]"
    echo "   trust [DEVICE_MAC]"
    echo ""
    echo "🎵 To test Bluetooth audio:"
    echo "   speaker-test -t wav -c 2"
    echo ""
    
    log_warning "Please reboot your Raspberry Pi for all changes to take effect!"
}

# Run main function
main "$@"
