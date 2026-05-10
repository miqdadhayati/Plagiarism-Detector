@echo off
setlocal
cd /d "%~dp0"

echo === VP-Tree Plagiarism Detector Build/Test/Run (No Pause) ===

call build_qt6_app.bat --no-pause
if errorlevel 1 exit /b 1

call build_tests.bat
if errorlevel 1 exit /b 1

if not exist build_terminal\VPTreePlagiarismDetector_tests.exe (
  echo ERROR: test executable not found.
  exit /b 1
)

echo === Running tests ===
pushd build_terminal
VPTreePlagiarismDetector_tests.exe
popd
if errorlevel 1 exit /b 1

call run_qt6_app.bat
if errorlevel 1 exit /b 1

echo === Done ===
