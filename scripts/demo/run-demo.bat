@echo off
REM Simple Node.js launcher that ensures proper environment setup

REM Initialize Node.js environment
if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
) else (
    echo WARNING: Node.js environment script not found
)

REM Run the demo launcher
node launch-demo.js
