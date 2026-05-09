@echo off
setlocal
cd /d "%~dp0"

echo [1/3] Building app and tests...
call "%~dp0build_qt6_app.bat"
if errorlevel 1 exit /b 1

call "%~dp0build_tests.bat"
if errorlevel 1 exit /b 1

echo [2/3] Running tests...
"%~dp0build_terminal\VPTreePlagiarismDetector_tests.exe"
if errorlevel 1 (
  exit /b 1
)

if /I "%~1"=="--no-launch" (
  echo Build and tests complete. Launch skipped.
  exit /b 0
)

echo [3/3] Launching app...
call "%~dp0run_qt6_app.bat"
if errorlevel 1 exit /b 1

echo Build, tests, and app launch complete.
