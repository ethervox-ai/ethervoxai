@echo off
echo =====================================
echo  EthervoxAI Audio Setup (Advanced)
echo =====================================
echo.
echo This script helps install advanced audio features.
echo The main demo works without these dependencies.
echo.

REM Check Node.js version
echo Checking Node.js version...
node --version
echo.

REM Check if Visual Studio Build Tools are available
echo Checking for build tools...
where cl > nul 2>&1
if %errorlevel% equ 0 (
    echo ✅ Visual Studio Build Tools found
    set BUILD_TOOLS_AVAILABLE=1
) else (
    echo ❌ Visual Studio Build Tools not found
    set BUILD_TOOLS_AVAILABLE=0
)

REM Check for Python
where python > nul 2>&1
if %errorlevel% equ 0 (
    echo ✅ Python found
    set PYTHON_AVAILABLE=1
) else (
    echo ❌ Python not found
    set PYTHON_AVAILABLE=0
)

echo.
echo =====================================
echo  Installation Options
echo =====================================
echo.
echo 1. Basic Demo (TTS only) - Recommended
echo 2. Advanced Audio (requires build tools)
echo 3. Install Build Tools First
echo 4. Skip and run basic demo
echo.
set /p choice="Choose option (1-4): "

if "%choice%"=="1" goto BASIC_INSTALL
if "%choice%"=="2" goto ADVANCED_INSTALL
if "%choice%"=="3" goto BUILD_TOOLS_HELP
if "%choice%"=="4" goto SKIP_INSTALL
goto INVALID_CHOICE

:BASIC_INSTALL
echo.
echo Installing basic TTS support...
npm install say
if %errorlevel% neq 0 (
    echo ❌ TTS installation failed
    echo The demo will run in text-only mode
) else (
    echo ✅ TTS installed successfully
)
goto RUN_DEMO

:ADVANCED_INSTALL
if "%BUILD_TOOLS_AVAILABLE%"=="0" (
    echo.
    echo ❌ Advanced audio requires Visual Studio Build Tools
    echo Please choose option 3 to install build tools first
    pause
    goto START
)

echo.
echo Installing advanced audio libraries...
echo This may take several minutes...
npm install mic speaker wav
if %errorlevel% neq 0 (
    echo ❌ Advanced audio installation failed
    echo Falling back to basic TTS...
    npm install say
) else (
    echo ✅ Advanced audio installed successfully
)
goto RUN_DEMO

:BUILD_TOOLS_HELP
echo.
echo =====================================
echo  Modern Build Tools Installation
echo =====================================
echo.
echo The old 'windows-build-tools' package is deprecated and broken.
echo Here are modern alternatives:
echo.
echo Option A: Visual Studio Community (Recommended)
echo   1. Download from: https://visualstudio.microsoft.com/downloads/
echo   2. Install with "Desktop development with C++" workload
echo   3. This includes all necessary build tools
echo.
echo Option B: Visual Studio Build Tools (Minimal)
echo   1. Download from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
echo   2. Install "C++ build tools" workload
echo   3. Much smaller download than full Visual Studio
echo.
echo Option C: Use Chocolatey (Package Manager)
echo   1. Install Chocolatey: https://chocolatey.org/install
echo   2. Run: choco install visualstudio2022buildtools
echo   3. Run: choco install visualstudio2022-workload-vctools
echo.
echo Option D: Skip Advanced Audio
echo   The demo works great with just TTS (say package)
echo   Advanced audio is optional for most use cases
echo.
echo After installing build tools, restart this script and choose option 2.
echo.
pause
goto START

:SKIP_INSTALL
echo.
echo Skipping audio installation...
goto RUN_DEMO

:INVALID_CHOICE
echo Invalid choice. Please select 1-4.
pause
goto START

:RUN_DEMO
echo.
echo =====================================
echo  Starting EthervoxAI Demo
echo =====================================
echo.

REM Build the project
echo Building EthervoxAI...
npm run build
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo Starting Windows Desktop Demo...
echo.
echo Commands available during demo:
echo   start  - Start voice listening
echo   stop   - Stop voice listening  
echo   status - Show current status
echo   quit   - Exit application
echo.

npm run demo:windows

echo.
echo Demo ended.
pause
exit /b 0

:START
goto BASIC_INSTALL
