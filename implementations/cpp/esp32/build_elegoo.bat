@echo off
REM ELEGOO EL-SM-012 Build and Test Script
REM Optimized build configuration for ELEGOO EL-SM-012 ESP32 module

echo ====================================================
echo  ELEGOO EL-SM-012 ESP32 Module - Build & Test
echo ====================================================

set ELEGOO_ENV=elegoo_el_sm_012

echo.
echo 🔧 Building for ELEGOO EL-SM-012 module...
echo Environment: %ELEGOO_ENV%

REM Clean previous build
echo.
echo 🧹 Cleaning previous build...
pio run -e %ELEGOO_ENV% -t clean

REM Check configuration
echo.
echo ⚙️  Checking PlatformIO configuration...
pio project config -e %ELEGOO_ENV%

REM Build firmware
echo.
echo 🔨 Building firmware...
pio run -e %ELEGOO_ENV%

if %ERRORLEVEL% neq 0 (
    echo.
    echo ❌ Build failed for ELEGOO EL-SM-012!
    echo Check the error messages above.
    pause
    exit /b 1
)

echo.
echo ✅ Build successful!

REM Run tests if requested
set /p run_tests="🧪 Run ELEGOO-specific tests? (y/n): "
if /i "%run_tests%"=="y" (
    echo.
    echo 🧪 Running ELEGOO EL-SM-012 specific tests...
    pio test -e %ELEGOO_ENV% -f test_elegoo_el_sm_012
    
    if %ERRORLEVEL% neq 0 (
        echo.
        echo ❌ Tests failed!
        pause
        exit /b 1
    )
    
    echo.
    echo ✅ All tests passed!
)

REM Upload firmware if requested
set /p upload_firmware="📡 Upload firmware to ELEGOO EL-SM-012? (y/n): "
if /i "%upload_firmware%"=="y" (
    echo.
    echo 📡 Uploading firmware to ELEGOO EL-SM-012...
    echo Make sure your module is connected to the correct COM port.
    pause
    
    pio run -e %ELEGOO_ENV% -t upload
    
    if %ERRORLEVEL% neq 0 (
        echo.
        echo ❌ Upload failed!
        echo Check the connection and COM port settings.
        pause
        exit /b 1
    )
    
    echo.
    echo ✅ Upload successful!
    
    REM Open serial monitor
    set /p open_monitor="📟 Open serial monitor? (y/n): "
    if /i "%open_monitor%"=="y" (
        echo.
        echo 📟 Opening serial monitor...
        echo Press Ctrl+C to exit monitor.
        pio device monitor -e %ELEGOO_ENV%
    )
)

echo.
echo ====================================================
echo  ELEGOO EL-SM-012 Build Process Complete
echo ====================================================
echo.
echo 📁 Binary location: .pio\build\%ELEGOO_ENV%\firmware.bin
echo 🔧 Partition table: partitions_elegoo.csv
echo 📖 Configuration: [env:%ELEGOO_ENV%] in platformio.ini
echo.

pause
