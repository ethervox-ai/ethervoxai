@echo off
echo ==========================================
echo  Installing Audio Output Alternatives
echo ==========================================

if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
)

echo Installing cross-platform audio alternatives...
echo.

echo 1. Installing node-wav-player (pure JavaScript)...
npm install node-wav-player
echo.

echo 2. Installing play-sound (cross-platform)...
npm install play-sound
echo.

echo 3. Installing node-powershell (Windows native TTS)...
npm install node-powershell
echo.

echo Testing installations...
node test-audio-alternatives.js

pause
