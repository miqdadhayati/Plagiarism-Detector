@echo off
setlocal
cd /d "%~dp0"

if not exist build_terminal\VPTreePlagiarismDetector.exe (
  echo ERROR: app executable not found. Run build_qt6_app.bat first.
  exit /b 1
)

set "PATH=%CD%\build_terminal;%PATH%"
start "" "%CD%\build_terminal\VPTreePlagiarismDetector.exe"
