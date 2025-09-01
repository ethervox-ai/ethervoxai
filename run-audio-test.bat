@echo off
REM Wrapper to run Audio Input/Output Tester

setlocal
cd /d "%~dp0"

if not exist "tests\audio-input-output\run-audio-test.bat" (
	echo ERROR: Target script not found: tests\audio-input-output\run-audio-test.bat
	exit /b 1
)

call tests\audio-input-output\run-audio-test.bat
exit /b %errorlevel%
