@echo off
REM Wrapper to run audio capability checks

setlocal
cd /d "%~dp0"

if not exist "scripts\testing\check-audio.bat" (
	echo ERROR: Target script not found: scripts\testing\check-audio.bat
	exit /b 1
)

call scripts\testing\check-audio.bat
exit /b %errorlevel%
