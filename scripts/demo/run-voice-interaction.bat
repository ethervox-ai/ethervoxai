@echo off
REM 🎤 EthervoxAI Complete Voice Interaction Demo Launcher
REM Combines real audio recording, AI inference, and TTS output

echo.
echo =================================================
echo 🎤 EthervoxAI Voice Interaction Demo
echo =================================================
echo.
echo This demo provides:
echo   🎙️  Real microphone input recording
echo   🧠 Real AI model inference
echo   🗣️  Audio output with TTS
echo   👂 Voice activity detection
echo   🎯 Wake word detection
echo   🔄 Full conversation loop
echo.

REM Check if Node.js is available
node --version >nul 2>&1
if errorlevel 1 (
    echo ❌ Node.js is not installed or not in PATH
    echo    Please install Node.js from https://nodejs.org/
    pause
    exit /b 1
)

REM Check if the demo file exists
if not exist "%~dp0voice-interaction-demo.js" (
    echo ❌ Voice interaction demo file not found
    echo    Expected location: %~dp0voice-interaction-demo.js
    pause
    exit /b 1
)

REM Check if EthervoxAI is built
if not exist "%~dp0..\..\dist" (
    echo ⚠️  EthervoxAI modules not built. Building now...
    pushd "%~dp0..\.."
    call npm run build
    if errorlevel 1 (
        echo ❌ Build failed
        popd
        pause
        exit /b 1
    )
    popd
    echo ✅ Build completed
)

REM Start the voice interaction demo
echo 🚀 Starting voice interaction demo...
echo.
node "%~dp0voice-interaction-demo.js"

REM Check exit code
if errorlevel 1 (
    echo.
    echo ❌ Demo encountered an error
) else (
    echo.
    echo ✅ Demo completed successfully
)

echo.
pause
