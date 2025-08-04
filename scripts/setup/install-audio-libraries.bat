@echo off
echo ==========================================
echo  EthervoxAI Advanced Audio Setup
echo ==========================================
echo.

REM Initialize Node.js environment
if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
    echo Node.js environment initialized.
) else (
    echo WARNING: Node.js environment script not found
)

echo Checking for Visual Studio Build Tools...
echo.

REM Check for Visual Studio Build Tools
set "VS_FOUND=0"

REM Check for VS2022 Build Tools
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" (
    echo ✓ Visual Studio 2022 Build Tools found
    set "VS_FOUND=1"
)

REM Check for VS2022 Community/Professional
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    echo ✓ Visual Studio 2022 Community found
    set "VS_FOUND=1"
)

if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    echo ✓ Visual Studio 2022 Professional found
    set "VS_FOUND=1"
)

REM Check for VS2019 Build Tools
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe" (
    echo ✓ Visual Studio 2019 Build Tools found
    set "VS_FOUND=1"
)

if "%VS_FOUND%"=="0" (
    echo ❌ No Visual Studio Build Tools found
    echo.
    echo Please install Visual Studio Build Tools 2022:
    echo 1. Download from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
    echo 2. Run the installer
    echo 3. Select "C++ build tools" workload
    echo 4. Include "Windows 10/11 SDK" and "CMake tools"
    echo.
    pause
    exit /b 1
)

echo.
echo Checking Node.js and npm versions...
node --version
npm --version

echo.
echo Removing existing audio library installations...
npm uninstall mic speaker wav 2>nul

echo.
echo Installing audio libraries with build tools...
echo This may take several minutes...
echo.

REM Try to install the audio libraries one by one for better error reporting
echo Installing 'mic' library...
npm install mic --verbose
if %errorlevel% neq 0 (
    echo ❌ Failed to install 'mic' library
    set "MIC_FAILED=1"
) else (
    echo ✓ 'mic' library installed successfully
)

echo.
echo Installing 'speaker' library...
npm install speaker --verbose
if %errorlevel% neq 0 (
    echo ❌ Failed to install 'speaker' library
    set "SPEAKER_FAILED=1"
) else (
    echo ✓ 'speaker' library installed successfully
)

echo.
echo Installing 'wav' library...
npm install wav --verbose
if %errorlevel% neq 0 (
    echo ❌ Failed to install 'wav' library
    set "WAV_FAILED=1"
) else (
    echo ✓ 'wav' library installed successfully
)

echo.
echo ==========================================
echo Installation Summary:
echo ==========================================

if not defined MIC_FAILED (
    echo ✓ mic: Installed
) else (
    echo ❌ mic: Failed
)

if not defined SPEAKER_FAILED (
    echo ✓ speaker: Installed
) else (
    echo ❌ speaker: Failed
)

if not defined WAV_FAILED (
    echo ✓ wav: Installed
) else (
    echo ❌ wav: Failed
)

echo.
if not defined MIC_FAILED if not defined SPEAKER_FAILED if not defined WAV_FAILED (
    echo 🎉 All audio libraries installed successfully!
    echo The demo will now use real audio input/output.
) else (
    echo ⚠️ Some audio libraries failed to install.
    echo The demo will run with partial or simulated audio.
    echo.
    echo Common solutions:
    echo 1. Restart your command prompt as Administrator
    echo 2. Ensure Windows SDK is installed with Visual Studio Build Tools
    echo 3. Try: npm install --global --production windows-build-tools
    echo 4. Check Windows Defender isn't blocking the installation
)

echo.
echo Rebuilding EthervoxAI...
npm run build

echo.
echo Ready to test! Run: start-windows-demo.bat
echo.
pause
