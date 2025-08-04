@echo off
echo =====================================
echo  EthervoxAI Windows Desktop Demo
echo =====================================
echo.

REM Initialize Node.js environment using the full command approach
echo Initializing Node.js environment...
C:\Windows\System32\cmd.exe /c "call \"C:\Program Files\nodejs\nodevars.bat\" && cd /d \"%~dp0\" && npm --version > nul 2>&1"
if %errorlevel% neq 0 (
    echo ERROR: Node.js environment initialization failed
    echo Please ensure Node.js is properly installed from https://nodejs.org/
    pause
    exit /b 1
)

REM Set up the Node.js environment for this session
call "C:\Program Files\nodejs\nodevars.bat"

REM Verify Node.js is working
echo Checking Node.js installation...
node --version
if %errorlevel% neq 0 (
    echo ERROR: Node.js is not accessible after environment setup
    pause
    exit /b 1
)

echo Node.js environment ready!
echo.

REM Check if npm dependencies are installed
if not exist node_modules (
    echo Installing core dependencies...
    npm install
    if %errorlevel% neq 0 (
        echo ERROR: Failed to install core dependencies
        pause
        exit /b 1
    )
    
    echo Installing basic TTS support...
    npm install say
    if %errorlevel% neq 0 (
        echo WARNING: TTS installation failed, continuing with demo...
    )
)

REM Build the project
echo Building EthervoxAI...
npm run build
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

REM Check if audio dependencies are available
echo.
echo Checking audio dependencies...
npm list say > nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo NOTICE: TTS engine not found.
    echo Installing Text-to-Speech support...
    npm install say
    if %errorlevel% neq 0 (
        echo WARNING: Could not install TTS engine.
        echo Speech output will be simulated.
    ) else (
        echo TTS engine installed successfully.
    )
    echo.
) else (
    echo TTS engine available.
)

REM Check for optional audio libraries (skip if they fail)
echo.
echo Checking optional audio libraries...
npm list mic > nul 2>&1
if %errorlevel% neq 0 (
    echo NOTICE: Advanced audio libraries not found.
    echo Attempting to install optional audio libraries...
    echo.
    
    REM Try to install advanced audio libraries, but don't fail if they can't be installed
    npm install mic speaker wav > nul 2>&1
    if %errorlevel% neq 0 (
        echo WARNING: Could not install advanced audio libraries.
        echo This is likely due to missing C++ build tools.
        echo.
        echo The demo will run with basic audio simulation instead.
        echo.
        echo To enable advanced audio features:
        echo 1. Install Visual Studio Build Tools from:
        echo    https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
        echo 2. Select "C++ build tools" workload during installation
        echo 3. Run: npm install mic speaker wav
        echo.
        echo For now, continuing with basic demo...
    ) else (
        echo Advanced audio libraries installed successfully.
    )
    echo.
) else (
    echo Advanced audio libraries found.
)

echo.
echo Starting Windows Desktop Demo...
echo =====================================
echo Commands available during demo:
echo   start  - Start voice listening
echo   stop   - Stop voice listening  
echo   status - Show current status
echo   quit   - Exit application
echo =====================================
echo.

REM Start the demo
npm run demo:windows

echo.
echo Demo ended.
pause
