@echo off
REM 🧪 AI Integration Test Launcher
REM Automatically handles Node.js environment and builds project

echo 🧪 EthervoxAI AI Integration Test
echo ====================================

REM Check if we're in PowerShell
if defined __PSLockDownPolicy (
    echo 🔄 PowerShell detected - switching to CMD for Node.js compatibility
    cmd /c "%~f0"
    goto :EOF
)

REM Load Node.js environment
call "C:\Program Files\nodejs\nodevars.bat" >nul 2>&1
if %errorlevel% neq 0 (
    echo ❌ Failed to load Node.js environment
    exit /b 1
)

echo ⚡ Node.js environment loaded
echo 🔨 Building project...

REM Build the project first
call npm run build
if %errorlevel% neq 0 (
    echo ❌ Build failed
    exit /b 1
)

echo ✅ Build completed
echo 🧪 Running AI integration test...

REM Run the AI integration test
node src/test-ai-integration.js
if %errorlevel% neq 0 (
    echo ❌ AI integration test failed
    exit /b 1
)

echo ✅ AI integration test completed successfully
pause
