@echo off
setlocal
cd /d "%~dp0"

if not exist build_terminal mkdir build_terminal

g++ -std=c++17 -O2 -Wall -Wextra -Isrc src\test_engine.cpp src\ngram.cpp src\vptree.cpp src\engine.cpp -o build_terminal\VPTreePlagiarismDetector_tests.exe
if errorlevel 1 exit /b 1

echo Built: build_terminal\VPTreePlagiarismDetector_tests.exe
