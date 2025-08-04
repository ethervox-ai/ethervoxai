@echo off
if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
)
node verify-audio.js
pause
