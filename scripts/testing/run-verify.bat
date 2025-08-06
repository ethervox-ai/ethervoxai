@echo off

REM Auto-switch from PowerShell to CMD for better compatibility
REM Check for PowerShell-specific automatic variables
if defined __PSLockDownPolicy (
    echo Detected PowerShell execution environment - switching to CMD for better compatibility...
    cmd.exe /c "%~f0" %*
    exit /b %errorlevel%
)

if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
)
node verify-audio.js
pause
