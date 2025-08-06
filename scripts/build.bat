@echo off

REM Auto-switch from PowerShell to CMD for better compatibility
REM Check for PowerShell-specific automatic variables
if defined __PSLockDownPolicy (
    echo Detected PowerShell execution environment - switching to CMD for better compatibility...
    cmd.exe /c "%~f0" %*
    exit /b %errorlevel%
)

echo Building EthervoxAI...

REM Initialize Node.js environment
if exist "C:\Program Files\nodejs\nodevars.bat" (
    call "C:\Program Files\nodejs\nodevars.bat"
    echo Node.js environment initialized.
) else (
    echo ERROR: Node.js environment not found at C:\Program Files\nodejs\nodevars.bat
    echo Please ensure Node.js is properly installed
    pause
    exit /b 1
)

REM Run the build
npm run build

if %errorlevel% neq 0 (
    echo ERROR: Build failed
    pause
    exit /b %errorlevel%
)

echo Build completed successfully!
pause
