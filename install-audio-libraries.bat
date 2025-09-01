@echo off
REM Wrapper for installing audio libraries

setlocal
cd /d "%~dp0"

if not exist "scripts\setup\install-audio-libraries.bat" (
	echo ERROR: Target script not found: scripts\setup\install-audio-libraries.bat
	exit /b 1
)

call scripts\setup\install-audio-libraries.bat
exit /b %errorlevel%
