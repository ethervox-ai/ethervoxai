@echo off
setlocal EnableDelayedExpansion

REM Auto-switch from PowerShell to CMD for better compatibility
REM Most reliable method: check for PowerShell-specific automatic variables
if defined __PSLockDownPolicy (
    echo Detected PowerShell execution environment - switching to CMD for better compatibility...
    cmd.exe /c "%~f0" %*
    exit /b %errorlevel%
)

REM Backup check: test if batch file commands work as expected in current shell
ver >nul 2>&1
if %errorlevel% neq 0 (
    echo Detected incompatible shell environment - switching to CMD...
    cmd.exe /c "%~f0" %*
    exit /b %errorlevel%
)

echo ========================================
echo  EthervoxAI UI Demo - Master Launcher
echo ========================================
echo.

REM Check if Node.js environment exists
if not exist "C:\Program Files\nodejs\nodevars.bat" (
    echo ERROR: Node.js environment not found!
    echo Please ensure Node.js is properly installed at C:\Program Files\nodejs\
    echo.
    echo Download Node.js from: https://nodejs.org/
    pause
    exit /b 1
)

echo Step 1: Loading Node.js environment...
call "C:\Program Files\nodejs\nodevars.bat"

if "%NODE_VERSION%" == "" (
    echo WARNING: Node.js environment may not have loaded properly
) else (
    echo ✅ Node.js environment loaded (version: %NODE_VERSION%)
)

echo.
echo Step 2: Checking Node.js and npm availability...
node --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: node command not found after loading environment
    pause
    exit /b 1
)

npm --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: npm command not found after loading environment
    pause
    exit /b 1
)

echo ✅ Node.js and npm are available

echo.
echo Step 3: Installing required dependencies...
cd /d "%~dp0..\..\..\..\..\"
echo Installing express and cors...
npm install express cors --no-progress --silent

if %errorlevel% neq 0 (
    echo ERROR: Failed to install dependencies
    pause
    exit /b 1
)

echo ✅ Dependencies installed successfully

echo.
echo Step 4: Starting UI Demo Server...
cd /d "%~dp0"
node launch-ui-demo.js

if %errorlevel% neq 0 (
    echo.
    echo ERROR: Failed to start UI Demo Server (Error code: %errorlevel%)
    pause
    exit /b %errorlevel%
)

echo.
echo UI Demo session completed.
pause
