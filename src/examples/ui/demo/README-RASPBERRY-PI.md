# EthervoxAI on Raspberry Pi 🍓

EthervoxAI UI Demo is fully optimized for Raspberry Pi, providing privacy-first AI demonstrations on ARM-based single-board computers.

## 🎯 Supported Raspberry Pi Models

- **Raspberry Pi 4** (4GB/8GB) - Full performance
- **Raspberry Pi 3B+/3B** - Optimized performance  
- **Raspberry Pi Zero 2 W** - Lightweight mode
- **Raspberry Pi Zero/1** - Minimal configuration

## 🚀 Quick Start

### 1. **One-Command Setup** (Recommended)
```bash
cd src/examples/ui/demo
./run-ui-demo.sh  
```

This script will:
- ✅ Detect your Raspberry Pi model
- ✅ Check and install Node.js if needed
- ✅ Apply memory optimizations automatically
- ✅ Install dependencies with ARM-specific settings
- ✅ Start the demo server with Pi optimizations

### 2. **Manual Setup**

#### Install Node.js (if not already installed):
```bash
# For Raspberry Pi OS
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt-get install -y nodejs

# Or using package manager
sudo apt update
sudo apt install nodejs npm
```

#### Install Dependencies:
```bash
npm install express cors
```

#### Start Demo:
```bash
./run-ui-demo.sh
# or
node launch-ui-demo.js
```

## 🔧 Raspberry Pi Optimizations

### **Automatic Detection**
- **Model Identification**: Reads `/proc/device-tree/model` to identify exact Pi model
- **Memory Analysis**: Detects available RAM and applies appropriate limits
- **Architecture Detection**: Handles ARM64 vs ARM32 differences
- **Performance Tuning**: Optimizes Node.js settings based on hardware

### **Memory Optimization**
```bash
# Automatically applied for systems with <1GB RAM
export NODE_OPTIONS="--max-old-space-size=512"

# Garbage collection optimization
# Runs every 30 seconds on low-memory systems
```

### **Lightweight Mode** (Pi Zero/1)
- Minimal memory footprint
- Reduced feature set for better performance
- Aggressive garbage collection
- Optimized network timeouts

## 📊 Performance Expectations

| Model | RAM | Expected Performance | Notes |
|-------|-----|---------------------|-------|
| Pi 4 (8GB) | 8GB | Excellent | Full feature set |
| Pi 4 (4GB) | 4GB | Very Good | Full feature set |
| Pi 3B+ | 1GB | Good | Memory optimizations applied |
| Pi Zero 2 W | 512MB | Fair | Lightweight mode |
| Pi Zero/1 | 512MB | Basic | Minimal configuration |

## 🌐 Network Access

Once started, access the demo at:
- **Local**: http://localhost:3000
- **Network**: http://[pi-ip-address]:3000

To find your Pi's IP address:
```bash
hostname -I
```

## 🛠️ Troubleshooting

### **Memory Issues**
```bash
# Check available memory
free -m

# If low on memory, try:
sudo systemctl stop unnecessary-service
sudo apt autoremove
```

### **Node.js Issues**
```bash
# Verify installation
node --version
npm --version

# If Node.js is old, upgrade:
sudo npm install -g n
sudo n stable
```

### **Permission Issues**
```bash
# If npm install fails:
sudo npm install --unsafe-perm

# Or fix npm permissions:
mkdir ~/.npm-global
npm config set prefix '~/.npm-global'
echo 'export PATH=~/.npm-global/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

### **Port Already in Use**
```bash
# Check what's using port 3000
sudo netstat -tlnp | grep 3000

# Use different port
PORT=3001 ./run-ui-demo.sh
```

## 🔒 Security Considerations

### **Firewall Setup**
```bash
# Allow access from local network only
sudo ufw allow from 192.168.0.0/16 to any port 3000

# Or allow specific IP
sudo ufw allow from 192.168.1.100 to any port 3000
```

### **HTTPS (Optional)**
For production use, consider setting up HTTPS with Let's Encrypt or a reverse proxy like nginx.

## 📈 Performance Monitoring

The demo includes Raspberry Pi specific monitoring:
- Real-time memory usage
- CPU temperature (if available)
- System load averages
- Network statistics

Access at: http://localhost:3000/api/demo-data

## 🎮 GPIO Integration (Future)

EthervoxAI is designed to integrate with Raspberry Pi GPIO for:
- LED status indicators
- Physical buttons for voice activation
- Sensor integration
- Hardware-based privacy switches

## 🚀 Production Deployment

### **Systemd Service**
```bash
# Create service file
sudo nano /etc/systemd/system/ethervoxai-demo.service

[Unit]
Description=EthervoxAI UI Demo
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/ethervoxai/src/examples/ui/demo
ExecStart=/usr/bin/node server/demo-server.js
Restart=on-failure
RestartSec=10
Environment=NODE_ENV=production
Environment=PORT=3000

[Install]
WantedBy=multi-user.target

# Enable and start
sudo systemctl enable ethervoxai-demo
sudo systemctl start ethervoxai-demo
```

### **Auto-start on Boot**
```bash
# Add to crontab
crontab -e

# Add line:
@reboot cd /home/pi/ethervoxai/src/examples/ui/demo && ./launch-ui-demo-master.sh
```

## 🌟 Advanced Features

### **Headless Operation**
Perfect for headless Pi setups - access via web browser from any device on the network.

### **Mobile Optimization**
The mobile demo interface is touch-optimized and works great on Pi touchscreen displays.

### **Edge AI Ready**
Designed to work with local AI models and edge computing frameworks on Raspberry Pi.

---

**Need Help?** Check the main [LAUNCHER-GUIDE.md](./LAUNCHER-GUIDE.md) for cross-platform launcher options and troubleshooting.
