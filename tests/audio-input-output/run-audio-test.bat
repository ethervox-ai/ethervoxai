@echo off
echo ========================================
echo  EthervoxAI Audio Input/Output Tester
echo ========================================
echo.

REM Initialize Node.js environment
if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
    echo Node.js environment initialized.
) else (
    echo ERROR: Node.js environment not found at C:\Program Files\nodejs\nodevars.bat
    echo Please ensure Node.js is properly installed
    pause
    exit /b 1
)

REM Build the project first
echo Building EthervoxAI test modules...
npm run build
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo Starting Audio Input/Output Test Console...
echo ========================================
echo This interactive tool allows you to:
echo   • Test audio input/output devices
echo   • Record audio with different modules
echo   • Play back recordings
echo   • Test text-to-speech
echo   • Configure audio settings
echo ========================================
echo.

REM Start the audio test console
node tests/audio-input-output/launch-audio-test.js

echo.
echo Audio test ended.
pause
