@echo off
REM Wrapper for installing remaining dependencies

setlocal
cd /d "%~dp0"

if not exist "scripts\setup\install-remaining.bat" (
	echo ERROR: Target script not found: scripts\setup\install-remaining.bat
	exit /b 1
)

call scripts\setup\install-remaining.bat
exit /b %errorlevel%
