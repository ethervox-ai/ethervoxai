@echo off
REM Wrapper to build the project via npm scripts

setlocal
cd /d "%~dp0"

if exist "C:\Program Files\nodejs\nodevars.bat" (
	call "C:\Program Files\nodejs\nodevars.bat"
)

echo Building EthervoxAI...
npm run build
exit /b %errorlevel%
