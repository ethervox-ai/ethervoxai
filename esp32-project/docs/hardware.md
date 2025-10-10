# EthervoxAI - ESP32 Hardware Requirements

## Build Summary
✅ **Build Status:** SUCCESS  
📦 **Binary Size:** 227,136 bytes (0x37f80)  
💾 **Flash Usage:** 22% of 1MB app partition  
🆓 **Available Space:** 820,224 bytes (78%) for OTA updates

---

## Minimum Hardware Configuration

### 1. ESP32 Module

| Specification | Requirement | Recommended |
|--------------|-------------|-------------|
| **Module** | ESP32-WROOM-32 | ESP32-WROOM-32D (4MB) |
| **Flash** | 4MB minimum | 4MB or 8MB |
| **PSRAM** | Optional | 4MB (for future AI models) |
| **CPU** | Dual-core Xtensa LX6 @ 240MHz | Same |
| **RAM** | 520KB SRAM (built-in) | Same |
| **WiFi** | 802.11 b/g/n | Same |
| **Bluetooth** | BLE 4.2 | Same |

**Recommended Development Boards:**
- ESP32-DevKitC V4
- NodeMCU-32S
- DOIT ESP32 DevKit V1

---

### 2. I2S Digital Microphone

**Primary Choice: INMP441**

| Pin | ESP32 GPIO | Function | Description |
|-----|------------|----------|-------------|
| SCK | **GPIO 26** | I2S Bit Clock (BCK) | Clock signal for data sync |
| WS  | **GPIO 25** | I2S Word Select (LRCK) | Left/Right channel select |
| SD  | **GPIO 33** | I2S Serial Data (DIN) | Audio data input |
| VDD | 3.3V | Power | 1.8V-3.3V compatible |
| GND | GND | Ground | Common ground |
| L/R | GND or VDD | Channel Select | GND=Left, VDD=Right |

**Specifications:**
- **Sensitivity:** -26 dBFS
- **SNR:** 61 dB
- **Dynamic Range:** 61 dB
- **Sample Rate:** 16 kHz (configured in firmware)
- **Bit Depth:** 16-bit
- **Power Consumption:** <1 mA

**Alternative Microphones:**
- SPH0645 (I2S, Adafruit)
- ICS-43434 (I2S, InvenSense)
- MAX9814 (Analog, requires ADC - not recommended)

---

### 3. Power Supply

#### Voltage Regulator
- **Model:** AMS1117-3.3 or HT7333
- **Input:** 5V DC
- **Output:** 3.3V @ 800mA minimum
- **Dropout:** <1.2V

#### Power Distribution
```
5V Input (USB/Battery)
    │
    ├──► AMS1117-3.3 ──► ESP32 VIN (3.3V)
    │                        │
    │                        ├──► INMP441 VDD
    │                        └──► Peripheral 3.3V
    │
    └──► Common GND
```

#### Decoupling Capacitors
| Location | Value | Type | Purpose |
|----------|-------|------|---------|
| Input (5V) | 10µF | Electrolytic | Smooth input voltage |
| ESP32 VIN | 10µF | Tantalum | Bulk decoupling |
| ESP32 VIN | 100nF | Ceramic | High-freq noise |
| Microphone VDD | 10µF | Ceramic | Audio stability |
| Microphone VDD | 100nF | Ceramic | High-freq decoupling |

**Power Consumption:**
- **Active (WiFi ON):** 160-260 mA
- **Active (WiFi OFF):** 80-120 mA  
- **Light Sleep:** 0.8-2 mA
- **Deep Sleep:** 10-150 µA

---

### 4. Programming Interface

#### USB-to-Serial Bridge (Development)
- **Chips:** CH340G, CP2102, or FTDI FT232RL
- **Baud Rate:** 115200 (debug), 921600 (flashing)
- **Connections:**
  ```
  USB Bridge     ESP32
  ─────────      ──────
  TX      ─────► RXD (GPIO 3)
  RX      ◄───── TXD (GPIO 1)
  DTR     ─────► EN (auto-reset)
  RTS     ─────► GPIO 0 (auto-boot)
  GND     ─────► GND
  ```

#### Boot Mode Control
- **Normal Boot:** GPIO0 = HIGH (pulled up)
- **Flash Boot:** GPIO0 = LOW (button pressed during reset)

**Recommended Circuit:**
```
        10kΩ
EN ─────/\/\/\─────┬──── 3.3V
                   │
                  ╱ 
         Reset   │  Tactile Switch
         Button  │
                  ╲
                   │
                  GND

        10kΩ
GPIO0 ──/\/\/\─────┬──── 3.3V
                   │
                  ╱
         Boot    │  Tactile Switch
         Button  │
                  ╲
                   │
                  GND
```

---

## Complete Pin Assignment

### Active Pins (Currently Used)

| GPIO | Function | Direction | Module | Notes |
|------|----------|-----------|--------|-------|
| 0 | Boot Button | Input | System | Pull-up required |
| 1 | UART TX | Output | Debug | USB serial |
| 2 | LED | Output | Status | Built-in LED (optional) |
| 3 | UART RX | Input | Debug | USB serial |
| 25 | I2S WS | Output | Audio | INMP441 word select |
| 26 | I2S BCK | Output | Audio | INMP441 bit clock |
| 33 | I2S SD | Input | Audio | INMP441 data in |

### Reserved Pins (Future Use)

| GPIO | Function | Direction | Purpose | Notes |
|------|----------|-----------|---------|-------|
| 21 | I2C SDA | I/O | Sensors/Display | Pull-up required |
| 22 | I2C SCL | Output | Sensors/Display | Pull-up required |
| 18 | SPI CLK | Output | Flash/Sensors | Reserved |
| 19 | SPI MISO | Input | Flash/Sensors | Reserved |
| 23 | SPI MOSI | Output | Flash/Sensors | Reserved |
| 5 | SPI CS | Output | Flash/Sensors | Reserved |

### Strapping Pins (Avoid if possible)
| GPIO | Boot Behavior | Constraint |
|------|---------------|------------|
| 0 | Must be HIGH on boot | Pull-up required |
| 2 | Must be LOW on boot | Don't connect pull-up |
| 5 | Must be HIGH on boot | Reserved for SPI CS |
| 12 | Sets flash voltage | Must be LOW for 3.3V flash |
| 15 | Must be HIGH on boot | Use with caution |

### Input-Only Pins (Cannot be outputs)
- GPIO 34, 35, 36, 39: ADC only, no pull-up/pull-down

---

## Schematic Diagram

```
                         ESP32-WROOM-32
                    ┌─────────────────────┐
                    │                     │
    5V ──┬──────────┤ VIN             3V3 ├──┬── 3.3V
         │          │                     │  │
        ┴ 10µF      │                 GND ├──┴── GND
    Electrolytic    │                     │
                    │                     │
    ┌───────────────┤ EN (Reset)          │
    │               │                     │
    │  10kΩ         │ GPIO 0 (Boot)       │
    └──/\/\/\── 3.3V│                     │
         │          │                     │
        ╱           │ GPIO 1 (TX) ────────┼──► USB Serial TX
       │ Reset      │ GPIO 3 (RX) ◄───────┼──── USB Serial RX
    Button          │ GPIO 2 (LED) ───────┼──► Status LED (opt)
       │            │                     │
      GND           │                     │
                    │ GPIO 25 (I2S_WS) ───┼──► INMP441 WS
    ┌───────────────┤ GPIO 26 (I2S_BCK)───┼──► INMP441 SCK
    │               │ GPIO 33 (I2S_SD) ◄──┼──── INMP441 SD
    │  10kΩ         │                     │
    └──/\/\/\── 3.3V│                     │
         │          │ GPIO 21 (SDA) ──────┼──── I2C SDA (future)
        ╱           │ GPIO 22 (SCL) ──────┼──── I2C SCL (future)
       │ Boot       │                     │
    Button          │ GPIO 18 (SPI_CLK)───┼──── SPI Clock (future)
       │            │ GPIO 19 (SPI_MISO)──┼──── SPI MISO (future)
      GND           │ GPIO 23 (SPI_MOSI)──┼──── SPI MOSI (future)
                    │ GPIO 5  (SPI_CS)────┼──── SPI CS (future)
                    │                     │
                    └─────────────────────┘

              INMP441 Microphone Module
                    ┌─────────────┐
                    │             │
         ESP32  ────┤ SCK     VDD ├──── 3.3V ──┬── 10µF
         GPIO 26    │             │            │
                    │ WS      GND ├──── GND ───┴── 100nF
         ESP32  ────┤             │
         GPIO 25    │ SD      L/R ├──── GND (Left channel)
                    │             │
         ESP32  ────┤             │
         GPIO 33    └─────────────┘
```

---

## Bill of Materials (BOM)

### Core Components

| Qty | Component | Part Number | Description | Unit Price | Total |
|-----|-----------|-------------|-------------|------------|-------|
| 1 | ESP32 Module | ESP32-WROOM-32 | 4MB flash, WiFi+BT | $3.50 | $3.50 |
| 1 | I2S Microphone | INMP441 | Digital MEMS mic | $2.50 | $2.50 |
| 1 | Voltage Regulator | AMS1117-3.3 | LDO 3.3V 1A | $0.20 | $0.20 |
| 1 | USB-Serial | CH340G | USB to UART | $0.50 | $0.50 |

### Passive Components

| Qty | Component | Value | Package | Unit Price | Total |
|-----|-----------|-------|---------|------------|-------|
| 3 | Capacitor | 10µF | 0805/Through-hole | $0.10 | $0.30 |
| 3 | Capacitor | 100nF | 0603/0805 | $0.05 | $0.15 |
| 2 | Resistor | 10kΩ | 0603/0805 | $0.02 | $0.04 |
| 2 | Tactile Switch | 6x6mm | Through-hole | $0.10 | $0.20 |
| 1 | LED (optional) | Red 3mm | Through-hole | $0.05 | $0.05 |
| 1 | Resistor (LED) | 220Ω | 0603/0805 | $0.02 | $0.02 |

### Connectors & Misc

| Qty | Component | Description | Unit Price | Total |
|-----|-----------|-------------|------------|-------|
| 1 | Micro USB | Power/Programming | $0.30 | $0.30 |
| 1 | Header Pins | 2.54mm pitch | $0.10 | $0.10 |
| 1 | PCB | Custom or universal | $2.00 | $2.00 |

**Total Estimated Cost: ~$10-12 USD**

---

## PCB Layout Recommendations

### Layer Stack (2-layer PCB)
```
Top Layer:    Signal + Components
Bottom Layer: Ground plane + Power
```

### Design Rules
- **Trace Width:** 
  - Power (3.3V): 0.4mm minimum (20mil)
  - Signal: 0.2mm minimum (8mil)
  - Ground: As wide as possible
  
- **Clearance:** 0.2mm minimum (8mil)

- **Via Size:**
  - Drill: 0.3mm
  - Pad: 0.6mm

### Critical Layout Guidelines

1. **Power Distribution:**
   - Star ground topology from regulator
   - Wide 3.3V traces to ESP32 and microphone
   - Ground plane on bottom layer

2. **ESP32 Placement:**
   - Keep antenna area clear (15mm radius)
   - Place away from noisy signals
   - Good thermal contact to ground plane

3. **Microphone Placement:**
   - Away from switching noise sources
   - Sound port accessible (top or edge of PCB)
   - Short traces to ESP32

4. **Decoupling Strategy:**
   - Capacitors as close as possible to IC pins
   - 100nF ceramics <5mm from power pins
   - 10µF bulk caps within 10mm

5. **I2S Trace Routing:**
   - Keep BCK, WS, SD traces same length (±2mm)
   - Route parallel, maintain spacing
   - Avoid crossing under noisy signals
   - Length: <100mm total

---

## Flash Partition Table

Current configuration supports OTA updates:

```csv
# Name,     Type, SubType,  Offset,   Size,      Flags
nvs,        data, nvs,      0x9000,   0x6000,    
phy_init,   data, phy,      0xf000,   0x1000,    
factory,    app,  factory,  0x10000,  0x100000,  
ota_0,      app,  ota_0,    0x110000, 0x100000,  
ota_1,      app,  ota_1,    0x210000, 0x100000,  
storage,    data, spiffs,   0x310000, 0xF0000,   
```

**Partition Usage:**
- **NVS:** 24KB - WiFi credentials, config
- **PHY Init:** 4KB - RF calibration
- **Factory:** 1MB - Initial firmware (227KB used, 78% free)
- **OTA 0/1:** 1MB each - Update slots
- **SPIFFS:** 960KB - Audio samples, models, data

**Total Flash:** 4MB (3,932KB usable)

---

## Assembly Instructions

### Step 1: Power Supply
1. Solder AMS1117-3.3 regulator to PCB
2. Add 10µF capacitor on input (5V)
3. Add 10µF + 100nF on output (3.3V)
4. Test output voltage (should be 3.28-3.32V)

### Step 2: ESP32 Module
1. Apply solder paste to pads (if using SMD)
2. Place ESP32-WROOM-32 carefully aligned
3. Reflow or hot air solder
4. Add decoupling caps near VIN pin
5. Solder boot/reset buttons and pull-ups

### Step 3: Microphone
1. Solder INMP441 module to PCB
2. Add 10µF + 100nF caps near VDD pin
3. Connect I2S signals (BCK, WS, SD)
4. Set L/R pin (GND for left channel)

### Step 4: USB Interface
1. Solder CH340G or USB connector
2. Connect TX/RX to ESP32
3. Add auto-reset circuit (DTR/RTS)

### Step 5: Testing
1. Power on, check 3.3V rail
2. Connect USB, verify enumeration
3. Flash test firmware
4. Verify serial output at 115200 baud

---

## Testing & Validation

### Power-On Self Test
```cpp
// Add to main.c for hardware validation
void hardware_self_test() {
    // 1. Check voltage
    ESP_LOGI("TEST", "VDD: %.2fV", get_vdd33() / 1000.0);
    
    // 2. Test I2S microphone
    ESP_LOGI("TEST", "Initializing I2S...");
    if (ethervox_audio_init(&audio, &config) == 0) {
        ESP_LOGI("TEST", "✓ I2S OK");
    } else {
        ESP_LOGE("TEST", "✗ I2S FAIL");
    }
    
    // 3. Test WiFi
    ESP_LOGI("TEST", "Testing WiFi...");
    wifi_init_sta();
    ESP_LOGI("TEST", "✓ WiFi initialized");
    
    // 4. Test flash
    ESP_LOGI("TEST", "Flash size: %d MB", 
             spi_flash_get_chip_size() / (1024*1024));
}
```

### Audio Capture Test
```bash
# Monitor audio data
make monitor-esp32

# Expected output:
# I (1234) ESP32_AUDIO: Audio runtime initialized for ESP32
# I (1240) ESP32_AUDIO: Initializing I2S for audio capture...
# I (1250) ESP32_AUDIO: I2S audio capture started successfully
```

### Signal Quality Check
- **BCK Frequency:** 512 kHz (16kHz × 32bits/sample)
- **WS Frequency:** 16 kHz
- **SD Data:** Valid I2S data stream
- **Power Ripple:** <50mV on 3.3V rail

---

## Troubleshooting Guide

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| ESP32 won't boot | Power supply too weak | Use 800mA+ regulator |
| Brownout detector | Voltage drop during WiFi | Add more bulk capacitance (47µF) |
| No I2S data | Wrong pin connections | Verify BCK=26, WS=25, SD=33 |
| Noisy audio | Poor grounding | Add ground plane, shorter traces |
| Can't flash | Wrong boot mode | Hold GPIO0 LOW during reset |
| WiFi won't connect | Antenna interference | Keep area clear, external antenna |
| Flash verification fails | Corrupted upload | Lower baud rate to 460800 |
| Random crashes | ESD/EMI | Add ferrite bead on power input |

---

## Compliance & Certifications

### FCC/CE Requirements (for production)
- **WiFi:** Certified ESP32 modules (pre-certified)
- **EMI:** Follow Espressif design guidelines
- **ESD:** Add protection diodes on exposed pins
- **Power:** Use certified power supplies

### Safety Considerations
- **Voltage:** All signals ≤3.3V
- **Current:** Fuse on 5V input (500mA max)
- **Thermal:** ESP32 max temp 85°C
- **Enclosure:** Non-conductive, flame-retardant

---

## Next Steps

✅ **You've successfully built the firmware!**

**Hardware Setup:**
1. Order components from BOM
2. Assemble on breadboard or PCB
3. Flash firmware: `make flash-esp32`
4. Test audio capture: `make monitor-esp32`

**Software Configuration:**
1. Configure WiFi credentials via `idf.py menuconfig`
2. Set up cloud API endpoints (see API_INTEGRATION.md)
3. Test wake word detection
4. Calibrate microphone sensitivity

**Production Deployment:**
- See `docs/PRODUCTION_DEPLOYMENT.md`
- PCB design files: `hardware/gerber/`
- 3D enclosure: `hardware/enclosure/`

---

**Document Version:** 1.0  
**Last Updated:** 2025-10-09  
**Firmware Version:** 0.1.0 (Build 0x37f80)