@echo off
REM Wrapper for installing speaker for arm64

setlocal
cd /d "%~dp0"

if not exist "scripts\setup\install-speaker-arm64.bat" (
	echo ERROR: Target script not found: scripts\setup\install-speaker-arm64.bat
	exit /b 1
)

call scripts\setup\install-speaker-arm64.bat
exit /b %errorlevel%
