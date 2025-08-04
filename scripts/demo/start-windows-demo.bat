@echo off
echo =====================================
echo  EthervoxAI Windows Desktop Demo
echo =====================================
echo.

REM Initialize Node.js environment
echo Initializing Node.js environment...
if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
    echo Node.js environment initialized.
) else (
    echo WARNING: Node.js environment script not found at standard location.
    echo Trying to continue with current PATH...
)
echo.

REM Check if Node.js is installed and accessible
node --version > nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Node.js is not installed or not accessible
    echo Please ensure Node.js is installed from https://nodejs.org/
    echo and that the PATH is properly configured.
    pause
    exit /b 1
) else (
    echo Node.js version: 
    node --version
)

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

REM Start the demo using the Node.js launcher with proper environment
call run-demo.bat

echo.
echo Demo ended.
pause
