@echo off
echo ========================================
echo  EthervoxAI UI Demo - Enhanced Launcher
echo ========================================
echo.

REM Check if Node.js environment exists
if not exist "C:\Program Files\nodejs\nodevars.bat" (
    echo ERROR: Node.js environment not found!
    echo Please ensure Node.js is properly installed at C:\Program Files\nodejs\
    echo.
    echo You can download Node.js from: https://nodejs.org/
    pause
    exit /b 1
)

echo Initializing Node.js environment and starting UI Demo...
echo.

REM Use cmd /k to load Node.js environment, then run the demo
cmd /k "C:\Program Files\nodejs\nodevars.bat" && echo Node.js environment loaded && node launch-ui-demo.js

REM If we get here, something went wrong
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Failed to start UI Demo
    echo Error code: %errorlevel%
    pause
    exit /b %errorlevel%
)

echo.
echo UI Demo session ended.
pause
