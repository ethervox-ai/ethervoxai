# EthervoxAI ESP32 - Quick Start Guide

Get your EthervoxAI voice assistant running on ESP32 in under 30 minutes.

---

# # Build Status

✅ **Build Status:** SUCCESS  
📦 **Binary Size:** 227,136 bytes (0x37f80)  
💾 **Flash Usage:** 22% of 1MB app partition  
🆓 **Available Space:** 820,224 bytes (78%) for OTA updates

---

# # Prerequisites

## # Required Software
- **ESP-IDF v5.1+** - Already installed at `/root/esp/esp-idf`
- **Python 3.8+** - For ESP-IDF tools
- **Git** - For version control
- **Make** - Build automation

**Verify Installation:**

```bash
# Check ESP-IDF version
idf.py --version
# Should show: ESP-IDF v5.1 or higher

# Check Python version
python3 --version
# Should show: Python 3.8.0 or higher
```

## # Required Hardware
- **ESP32 Development Board**
  - ESP32-DevKitC V4 (recommended)
  - NodeMCU-32S
  - DOIT ESP32 DevKit V1
- **INMP441 I2S Digital Microphone** module
- **USB Cable** (Micro-USB or USB-C depending on board)
- **Breadboard and jumper wires** (for prototyping)

**Optional but Recommended:**
- Multimeter (for voltage verification)
- Logic analyzer (for I2S debugging)
- External 5V power supply (if USB power is insufficient)

See [hardware.md](hardware.md) for complete specifications and BOM.

---

# # Step 1: Hardware Setup

## # 1.1 Wiring the INMP441 Microphone

Connect the INMP441 to your ESP32 **exactly** as shown:

| INMP441 Pin | ESP32 GPIO | Function | Wire Color (suggested) |
|-------------|------------|----------|------------------------|
| **VDD**     | **3.3V**   | Power    | Red                    |
| **GND**     | **GND**    | Ground   | Black                  |
| **SD**      | **GPIO 33**| Data In  | Yellow                 |
| **WS**      | **GPIO 25**| Word Select | Green               |
| **SCK**     | **GPIO 26**| Bit Clock | Blue                  |
| **L/R**     | **GND**    | Left Channel | Black               |

⚠️ **IMPORTANT:** These GPIO pins are defined in `src/audio/platform_esp32.c`:

```c
#define I2S_BCK_IO          26    // Bit Clock
#define I2S_WS_IO           25    // Word Select
#define I2S_DATA_IN_IO      33    // Serial Data In
```

## # 1.2 Connection Diagram

```
ESP32-DevKitC          INMP441 Module
┌──────────┐          ┌────────────┐
│          │          │            │
│  3.3V    ├──────────┤ VDD        │
│  GND     ├─────┬────┤ GND        │
│          │     │    │ L/R        ├──┐
│  GPIO 33 ├─────┼────┤ SD         │  │
│  GPIO 25 ├─────┼────┤ WS         │  │
│  GPIO 26 ├─────┼────┤ SCK        │  │
│          │     │    └────────────┘  │
└──────────┘     │                    │
                 └────────────────────┘
                      (L/R to GND)
```

## # 1.3 Power Verification

Before proceeding, verify power supply:

```bash
# Connect USB cable to ESP32
# Measure voltage between 3.3V and GND pins
# Expected: 3.28V - 3.32V
```

✅ **Checklist:**
- [ ] 3.3V rail measures 3.28-3.32V
- [ ] All GND connections are secure
- [ ] No short circuits between VDD and GND
- [ ] INMP441 L/R pin connected to GND (left channel)
- [ ] GPIO 33, 25, 26 not connected to anything else

---

# # Step 2: Clone and Setup Project

## # 2.1 Navigate to Project Directory

```bash
cd /root/ethervoxai
```

## # 2.2 Verify Project Structure

```bash
ls -la
# Should show:
# esp32-project/
# include/
# src/
# Makefile
# README.md
```

## # 2.3 Set ESP-IDF Environment (if not already set)

```bash
# Add to ~/.bashrc for permanent setup
source /root/esp/esp-idf/export.sh

# Verify
echo $IDF_PATH
# Should show: /root/esp/esp-idf
```

---

# # Step 3: Build the Firmware

## # 3.1 Clean Previous Builds (Optional)

```bash
# Only needed if you've built before
make clean-esp32
```

## # 3.2 Build for ESP32

```bash
make build-esp32
```

**Expected Output:**

```
Executing action: all (aliases: build)
...
[890/890] Generating binary image from built executable
esptool.py v4.10.0
Creating esp32 image...
Merged 2 ELF sections
Successfully created esp32 image.
Generated /root/ethervoxai/esp32-project/build/ethervoxai.bin

ethervoxai.bin binary size 0x37f80 bytes.
Smallest app partition is 0x100000 bytes.
0xc8080 bytes (78%) free.
```

✅ **Build Success Indicators:**
- Binary size: ~227 KB (0x37f80 bytes)
- 78% partition space free
- No errors or warnings
- `ethervoxai.bin` created in `esp32-project/build/`

**Build Times:**
- First build: ~3-5 minutes
- Incremental builds: ~30 seconds

## # 3.3 Troubleshooting Build Errors

| Error | Solution |
|-------|----------|
| `ESP-IDF not found` | Run `source /root/esp/esp-idf/export.sh` |
| `Python not found` | Install Python 3.8+: `apt install python3` |
| `Permission denied` | Run `chmod +x Makefile` |
| `ninja failed` | Clean and rebuild: `make clean-esp32 && make build-esp32` |

---

# # Step 4: Flash the Firmware

## # 4.1 Find Your ESP32 Serial Port

**Linux:**

```bash
ls /dev/ttyUSB*
# Usually shows: /dev/ttyUSB0
```

**macOS:**

```bash
ls /dev/cu.usbserial*
# Usually shows: /dev/cu.usbserial-XXXX
```

**Windows:**

```powershell
# Check Device Manager → Ports (COM & LPT)
# Usually shows: COM3, COM4, etc.
```

## # 4.2 Set Permissions (Linux/macOS only)

```bash
# Add user to dialout group (one-time setup)
sudo usermod -a -G dialout $USER

# Or set permissions directly
sudo chmod 666 /dev/ttyUSB0
```

## # 4.3 Flash to ESP32

**Using Makefile (Recommended):**

```bash
# Auto-detect port
make flash-esp32

# Or specify port explicitly
make flash-esp32 PORT=/dev/ttyUSB0
```

**Manual Flashing:**

```bash
cd esp32-project
idf.py -p /dev/ttyUSB0 flash
```

**Expected Output:**

```
esptool.py v4.10.0
Serial port /dev/ttyUSB0
Connecting....
Chip is ESP32-D0WDQ6 (revision v1.0)
Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, Coding Scheme None
Crystal is 40MHz

Wrote 227968 bytes (131088 compressed) at 0x00010000 in 2.9 seconds
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
```

✅ **Flash Success Indicators:**
- "Hash of data verified" message
- "Hard resetting via RTS pin"
- No timeout errors
- ESP32 restarts automatically

**Flash Time:** 10-15 seconds @ 460800 baud

## # 4.4 Troubleshooting Flash Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `Failed to connect` | Boot mode wrong | Hold GPIO0 button, press Reset |
| `Serial port not found` | Wrong port | Check `ls /dev/ttyUSB*` |
| `Permission denied` | No port access | Run `sudo chmod 666 /dev/ttyUSB0` |
| `Timeout waiting for packet` | Bad cable/connection | Try different USB cable |
| `Hash mismatch` | Corrupted flash | Lower baud rate: `idf.py -b 115200 flash` |

**Manual Boot Mode:**
1. Hold down **GPIO0 (BOOT)** button
2. Press and release **EN (RESET)** button
3. Release **GPIO0** button
4. Run flash command again

---

# # Step 5: Monitor Serial Output

## # 5.1 Start Serial Monitor

```bash
# Using Makefile
make monitor-esp32

# Or manually
idf.py -p /dev/ttyUSB0 monitor
```

## # 5.2 Expected Boot Output

```
ESP-ROM:esp32-20200220
Build:Feb 20 2020
rst:0x1 (POWERON),boot:0x13 (SPI_FAST_FLASH_BOOT)
...

I (298) cpu_start: Starting scheduler on APP CPU.
I (308) ESP32_AUDIO: Audio runtime initialized for ESP32
I (308) main: EthervoxAI starting on ESP32...
I (318) PLATFORM: Initializing EthervoxAI Platform
I (328) ESP32_HAL: ESP32 HAL registered
I (338) main: Platform: ESP32
I (338) ESP32_AUDIO: ESP32 audio platform driver registered
I (348) main: EthervoxAI initialized successfully
```

✅ **Boot Success Indicators:**
- No error messages (lines starting with `E`)
- "ESP32 HAL registered" message
- "EthervoxAI initialized successfully"
- System enters main loop

## # 5.3 Monitor Commands

| Key | Action |
|-----|--------|
| `Ctrl + ]` | Exit monitor |
| `Ctrl + T` | Menu (reset, toggle logging, etc.) |
| `Ctrl + T` → `Ctrl + R` | Reset ESP32 |
| `Ctrl + T` → `Ctrl + H` | Toggle hex output |

## # 5.4 Troubleshooting Boot Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Brownout detector | Weak power supply | Use powered USB hub or external 5V supply |
| Guru Meditation Error | Stack overflow | Check for infinite loops in code |
| Watchdog timer reset | Task stuck | Verify audio initialization |
| No output | Wrong baud rate | Set monitor to 115200 baud |
| Garbage characters | Baud mismatch | Press Reset button on ESP32 |

---

# # Step 6: Test Audio Capture (Optional)

## # 6.1 Enable Audio Debug Logging

Edit `esp32-project/main/main.c` to add audio test:

```c
// After ethervox_audio_init()
if (ethervox_audio_start_capture(&audio) == 0) {
    printf("Audio capture started\n");

    // Test read
    ethervox_audio_buffer_t buffer;
    if (ethervox_audio_read(&audio, &buffer) == 0) {
        printf("Audio data: %zu bytes @ %llu us\n",
               buffer.size, buffer.timestamp_us);
    }
}
```

## # 6.2 Rebuild and Flash

```bash
make build-esp32
make flash-esp32
make monitor-esp32
```

## # 6.3 Expected Audio Output

```
I (500) ESP32_AUDIO: Initializing I2S for audio capture...
I (520) ESP32_AUDIO: I2S audio capture started successfully
Audio capture started
Audio data: 1024 bytes @ 523456 us
```

✅ **Audio Working Indicators:**
- I2S channel created successfully
- Bytes read > 0
- Timestamp incrementing
- No I2S DMA errors

---

# # Step 7: Configure WiFi (Optional)

## # 7.1 Enter Configuration Menu

```bash
cd esp32-project
idf.py menuconfig
```

## # 7.2 Navigate to WiFi Settings
1. Arrow keys → `Component config`
2. Enter → `Wi-Fi`
3. Configure:
   - **WiFi SSID:** Your network name
   - **WiFi Password:** Your network password
   - **Maximum retry:** 5

## # 7.3 Save and Exit
- Press `S` to save
- Press `Q` to quit
- Rebuild: `make build-esp32`

## # 7.4 Verify WiFi Connection

```bash
make flash-esp32 monitor-esp32
```

Expected output:

```
I (1234) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1
I (2345) wifi:state: init -> auth (b0)
I (2355) wifi:state: auth -> assoc (0)
I (2365) wifi:state: assoc -> run (10)
I (2385) wifi:connected with YOUR_SSID, channel 6
I (3456) wifi:got ip:192.168.1.100
```

---

# # Step 8: Next Steps

## # Immediate Actions
✅ Hardware is working  
✅ Firmware is flashed  
✅ Serial monitor shows boot  

## # What You Can Do Now

**1. Test Voice Input**
- Speak near microphone
- Watch serial output for audio data
- Verify timestamps are updating

**2. Configure Cloud API**
- Set up speech recognition service (Google/Azure/AWS)
- Add API credentials via `menuconfig`
- Test voice-to-text conversion

**3. Add Features**
- Wake word detection
- Custom commands
- LED status indicators
- OLED display (I2C)

**4. Production Deployment**
- Design custom PCB (see `hardware/` folder)
- 3D print enclosure
- Set up OTA updates
- Configure cloud backend

## # Documentation References

| Document | Purpose |
|----------|---------|
| [hardware.md](hardware.md) | Complete hardware specs, BOM, schematics |
| [API_INTEGRATION.md](API_INTEGRATION.md) | Cloud API setup guides |
| [PRODUCTION_DEPLOYMENT.md](PRODUCTION_DEPLOYMENT.md) | Manufacturing & deployment |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Common issues & solutions |

---

# # Common Issues & Solutions

## # Issue: ESP32 won't boot after flashing
**Solution:**

```bash
# Erase flash completely
esptool.py --port /dev/ttyUSB0 erase_flash

# Re-flash firmware
make flash-esp32
```

## # Issue: No audio data
**Solution:**
- Verify I2S pin connections (GPIO 25, 26, 33)
- Check 3.3V power to INMP441
- Verify L/R pin is connected to GND
- Test with different microphone module

## # Issue: WiFi won't connect
**Solution:**
- Verify SSID and password in `menuconfig`
- Check 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Move closer to router
- Check antenna connection on ESP32

## # Issue: Brownout detector triggered
**Solution:**
- Use powered USB hub (not laptop USB)
- Add 47µF capacitor on 3.3V rail
- Reduce WiFi transmit power in `menuconfig`
- Use external 5V 1A power supply

---

# # Quick Reference Commands

```bash
# Project setup
cd /root/ethervoxai
source /root/esp/esp-idf/export.sh

# Build firmware
make build-esp32

# Flash firmware
make flash-esp32 PORT=/dev/ttyUSB0

# Monitor output
make monitor-esp32

# All-in-one (flash + monitor)
make flash-esp32 monitor-esp32

# Clean build
make clean-esp32

# Configuration menu
cd esp32-project && idf.py menuconfig

# Erase flash
esptool.py --port /dev/ttyUSB0 erase_flash
```

---

# # Getting Help

**GitHub Issues:**
- Report bugs: https://github.com/yourusername/ethervoxai/issues
- Feature requests: Use "enhancement" label

**Documentation:**
- Read `docs/` folder for detailed guides
- Check `examples/` for sample code

**Community:**
- ESP32 Forums: https://esp32.com
- ESP-IDF Docs: https://docs.espressif.com/projects/esp-idf/

---

# # Success Checklist

Before moving to production, verify:

- [ ] ESP32 boots without errors
- [ ] Serial output shows "EthervoxAI initialized successfully"
- [ ] I2S audio capture working (bytes > 0)
- [ ] WiFi connects to network
- [ ] OTA partition accessible (78% free space)
- [ ] Power supply stable (3.28-3.32V on 3.3V rail)
- [ ] Microphone captures audio (test with speech)
- [ ] No brownout detector resets
- [ ] Firmware size < 500KB (currently 227KB)
- [ ] All GPIO pins correctly assigned

---

**Document Version:** 1.0  
**Last Updated:** 2025-10-09  
**Firmware Version:** 0.1.0 (Build 0x37f80)  
**Tested On:** ESP32-DevKitC V4, ESP-IDF v5.1

**Total Setup Time:** ~30 minutes (including hardware assembly)

🎉 **Congratulations! Your EthervoxAI is ready for development!**
