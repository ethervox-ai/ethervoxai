# EthervoxAI UI Demo

Experience the EthervoxAI dashboard interfaces through interactive web and mobile demos running on your local machine.

## 🎯 Overview

This demo provides a complete preview of the EthervoxAI user interfaces without requiring the full AI stack installation. You can explore:

- **Web Dashboard**: Full desktop interface with all features
- **Mobile Interface**: Touch-optimized mobile experience  
- **Live Data**: Real-time simulation of AI processing and privacy controls
- **Interactive Features**: Voice queries, language switching, model management

## 🚀 Quick Start

### Method 1: NPM Script (Recommended)
```bash
# From the project root
npm run demo:ui
```

### Method 2: Master Launcher (Windows - Handles Everything)
```bash
# Navigate to the demo directory
cd src/examples/ui/demo

# Run the master launcher (automatically handles Node.js environment and dependencies)
.\launch-ui-demo-master.bat
```

### Method 3: Enhanced Launcher (Windows)
```bash
# Navigate to the demo directory  
cd src/examples/ui/demo

# Use the enhanced launcher with proper Node.js environment loading
.\run-ui-demo.bat
```

### Method 4: Manual Node.js Environment Setup
```bash
# Load Node.js environment first, then run
cmd /c ""C:\Program Files\nodejs\nodevars.bat" && node launch-ui-demo.js"
```

### Method 5: Direct Launch (If Node.js is already in PATH)
```bash
# Navigate to the demo directory
cd src/examples/ui/demo

# Launch the demo
node launch-ui-demo.js
```

## 🌐 Access the Demo

Once started, the demo will be available at:

- **Demo Home**: http://localhost:3000
- **Web Dashboard**: http://localhost:3000/web-demo
- **Mobile Interface**: http://localhost:3000/mobile-demo

## 📱 Demo Features

### Web Dashboard
- **📊 Overview Tab**: System statistics and voice query interface
- **🌐 Languages Tab**: Available language profiles and confidence levels
- **🧠 AI Models Tab**: Local AI models with status and capabilities
- **🔐 Privacy Tab**: Privacy settings and controls

### Mobile Interface  
- **🏠 Home**: Quick stats and voice assistant
- **🌐 Languages**: Mobile-optimized language selection
- **🧠 Models**: AI model management on mobile
- **🔐 Privacy**: Touch-friendly privacy controls

### Interactive Elements
- **Voice Input Simulation**: Tap the microphone for simulated voice recognition
- **Text Queries**: Type questions to see AI responses
- **Privacy Toggles**: Interactive privacy setting controls
- **Real-time Updates**: Live status indicators and processing feedback

## 🛠️ Technical Details

### Dependencies
The demo automatically installs required packages:
- `express` - Web server
- `cors` - Cross-origin resource sharing

### Architecture
```
demo/
├── server/
│   └── demo-server.js     # Express server with API endpoints
├── web/
│   └── index.html         # Web dashboard demo
├── mobile/
│   └── index.html         # Mobile interface demo
├── launch-ui-demo.js      # Demo launcher script
├── start-ui-demo.bat      # Windows batch launcher
└── README.md              # This file
```

### API Endpoints
The demo server provides these endpoints:
- `GET /` - Demo selection page
- `GET /web-demo` - Web dashboard
- `GET /mobile-demo` - Mobile interface
- `GET /api/languages` - Language profiles data
- `GET /api/models` - AI models data
- `GET /api/privacy` - Privacy settings data
- `POST /api/query` - Process text queries

## 🎨 Customization

### Styling
- Web demo uses modern CSS with gradients and animations
- Mobile demo simulates native app appearance
- Responsive design adapts to different screen sizes

### Data
The demo uses simulated data that matches the actual EthervoxAI interfaces:
- 3 language profiles (English, Spanish, Mandarin)
- 2 loaded AI models (Mistral Lite, TinyLlama)
- Privacy settings with realistic options

### Adding Features
You can extend the demo by:
1. Adding new API endpoints in `demo-server.js`
2. Creating new UI components in the HTML files
3. Implementing additional interactive features

## 🔧 Troubleshooting

### Common Issues

**Port already in use:**
```bash
# Check what's using port 3000
netstat -ano | findstr :3000

# Kill the process or change port
set PORT=3001 && node launch-ui-demo.js
```

**Dependencies not found:**
```bash
# Install manually from project root
npm install express cors
```

**Node.js not found:**
- Ensure Node.js is installed and in your PATH
- Try running `node --version` to verify installation

### Development Mode
For development with auto-reload:
```bash
# Install nodemon globally
npm install -g nodemon

# Run with auto-reload
nodemon server/demo-server.js
```

## 📊 Demo Data

The demo simulates realistic data:

### Language Profiles
- English (US): 95% confidence, active
- Spanish (Latin America): 88% confidence, active  
- Mandarin (Simplified): 82% confidence, loading

### AI Models
- Mistral Lite: 4.1GB, loaded, conversation/QA capabilities
- TinyLlama: 1.2GB, loaded, simple QA capabilities
- CodeLlama 7B: 6.8GB, available, code generation

### Privacy Settings
- Mode: Balanced
- Cloud Fallback: Disabled
- Data Retention: 7 days
- Audit Logging: Enabled
- Encryption: Enabled

## 🤝 Integration

This demo showcases the interfaces that will integrate with:
- Core EthervoxAI modules (`multilingualRuntime`, `localLLMStack`, `privacyDashboard`)
- Voice processing pipeline
- Local AI model management
- Privacy-first data handling

## 📝 Notes

- This is a **demo only** - AI responses are simulated
- Voice input shows UI behavior but doesn't process actual speech
- All data is generated for demonstration purposes
- Real EthervoxAI integration requires the full module installation

## 🎯 Next Steps

After exploring the demo:
1. Review the actual dashboard components in `../dashboard/`
2. Install React dependencies for full functionality
3. Integrate with real EthervoxAI modules
4. Customize the interfaces for your use case

---

**🚀 Enjoy exploring the EthervoxAI user interfaces!**
