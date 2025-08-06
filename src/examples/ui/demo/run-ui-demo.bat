@echo off

REM Auto-switch from PowerShell to CMD for better compatibility
REM Check for PowerShell-specific automatic variables
if defined __PSLockDownPolicy (
    echo Detected PowerShell execution environment - switching to CMD for better compatibility...
    cmd.exe /c "%~f0" %*
    exit /b %errorlevel%
)

echo ========================================
echo  EthervoxAI UI Demo - Node.js Wrapper
echo ========================================
echo.

REM Check if Node.js environment exists
if not exist "C:\Program Files\nodejs\nodevars.bat" (
    echo ERROR: Node.js environment not found!
    echo Please ensure Node.js is properly installed at C:\Program Files\nodejs\
    pause
    exit /b 1
)

echo Loading Node.js environment and starting UI Demo...
echo.

REM Use cmd /c to execute with proper Node.js environment
cmd /c ""C:\Program Files\nodejs\nodevars.bat" && node "%~dp0launch-ui-demo.js""

REM Check the result
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Failed to start UI Demo (Error code: %errorlevel%)
    echo.
    echo Troubleshooting steps:
    echo 1. Ensure Node.js is properly installed
    echo 2. Check that npm packages are installed: npm install express cors
    echo 3. Verify the demo files are in the correct location
    pause
    exit /b %errorlevel%
)

echo.
echo UI Demo completed successfully.
pause
