@echo off
if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
)

echo Installing remaining audio packages...
npm install play-sound node-powershell

echo Testing again...
node test-audio-alternatives.js
