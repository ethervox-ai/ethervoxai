@echo off
REM Wrapper to run audio manager test

setlocal
cd /d "%~dp0"

if not exist "scripts\testing\test-audio-mgr.bat" (
	echo ERROR: Target script not found: scripts\testing\test-audio-mgr.bat
	exit /b 1
)

call scripts\testing\test-audio-mgr.bat
exit /b %errorlevel%
