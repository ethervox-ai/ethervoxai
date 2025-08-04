@echo off
echo ==========================================
echo  Installing Speaker Package for ARM64
echo ==========================================

if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
)

echo System: ARM64 Windows with Node.js %NODE_VERSION%
echo.

echo Attempting to install speaker package...
echo This may require native compilation...
echo.

REM Try different installation approaches for ARM64
echo Method 1: Standard installation
npm install speaker --verbose

if %errorlevel% neq 0 (
    echo.
    echo Method 1 failed, trying with rebuild...
    npm install speaker --build-from-source
)

if %errorlevel% neq 0 (
    echo.
    echo Method 2 failed, trying alternative package...
    npm install node-speaker
)

if %errorlevel% neq 0 (
    echo.
    echo All methods failed. The speaker package may not be compatible with ARM64.
    echo.
    echo Alternative solutions:
    echo 1. Use Windows Subsystem for Linux WSL2 with x64 emulation
    echo 2. Use a different audio output library
    echo 3. Run on x64 hardware
    echo.
    echo The demo will continue to work with simulated audio output.
) else (
    echo.
    echo ✓ Speaker package installed successfully!
)

echo.
echo Testing installation...
node -e "try { require('speaker'); console.log('✓ Speaker module loads correctly'); } catch(e) { console.log('❌ Speaker module failed:', e.message); }"

echo.
pause
