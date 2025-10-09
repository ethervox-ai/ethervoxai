# Cross-Compilation Guide for Raspberry Pi

## Prerequisites

Install ARM cross-compilation toolchain:
```bash
sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

## Build for Raspberry Pi

```bash
# Configure and build
make build-rpi

# Verify ARM binary
file build-rpi/ethervoxai
```

## Deploy to Raspberry Pi

```bash
# Set Raspberry Pi connection details (optional)
export RPI_USER=pi
export RPI_HOST=raspberrypi.local
export RPI_DIR=/home/pi/ethervoxai

# Deploy
./scripts/deploy-rpi.sh
```

## Manual Deployment

```bash
# Copy to Raspberry Pi
scp build-rpi/ethervoxai pi@raspberrypi.local:/home/pi/

# SSH and run
ssh pi@raspberrypi.local
./ethervoxai
```

## Troubleshooting

### Missing Libraries on Pi
If you get library errors on the Pi, install dependencies:
```bash
sudo apt install libasound2 libcurl4
```

### Different Pi Model
For Raspberry Pi 4 or newer, you can optimize further:
- Edit `cmake/toolchain-rpi.cmake`
- Change `-march=armv7-a` to `-march=armv8-a` for 64-bit Pi OS