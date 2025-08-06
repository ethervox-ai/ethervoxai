@echo off

REM Auto-switch from PowerShell to CMD for better compatibility
REM Check for PowerShell-specific automatic variables
if defined __PSLockDownPolicy (
    echo Detected PowerShell execution environment - switching to CMD for better compatibility...
    cmd.exe /c "%~f0" %*
    exit /b %errorlevel%
)

echo ========================================
echo  EthervoxAI UI Demo
echo ========================================
echo.

REM Initialize Node.js environment
if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
    echo Node.js environment initialized.
    echo Node.js version: %NODE_VERSION%
    echo npm version: 
    npm --version
    echo.
) else (
    echo ERROR: Node.js environment not found at C:\Program Files\nodejs\nodevars.bat
    echo Please ensure Node.js is properly installed
    pause
    exit /b 1
)

echo Starting EthervoxAI UI Demo Server...
echo.

REM Run the demo launcher
node launch-ui-demo.js

if %errorlevel% neq 0 (
    echo.
    echo ERROR: Failed to start UI demo
    echo Make sure Node.js is installed and try again
    pause
    exit /b %errorlevel%
)

echo.
echo Demo server stopped.
pause
