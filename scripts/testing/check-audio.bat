@echo off
REM Initialize Node.js environment and verify audio libraries

if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
) else (
    echo WARNING: Node.js environment script not found
)

echo Checking currently installed audio libraries...
npm list mic speaker wav say

echo.
echo Running audio verification...
node verify-audio.js
