@echo off
REM Wrapper to launch advanced audio setup

setlocal
cd /d "%~dp0"

if not exist "scripts\setup\setup-audio-advanced.bat" (
	echo ERROR: Target script not found: scripts\setup\setup-audio-advanced.bat
	exit /b 1
)

call scripts\setup\setup-audio-advanced.bat
exit /b %errorlevel%
