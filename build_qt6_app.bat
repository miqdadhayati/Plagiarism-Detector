@echo off
setlocal
cd /d "%~dp0"

echo === VP-Tree Plagiarism Detector Build Script ===

:: Set your exact Qt path
set "QT_DIR=C:\Qt\6.11.0\mingw_64"

:: 1. NUKE THE OLD FOLDER (This prevents corrupted DLL crashes)
if exist build_terminal (
    echo [1/4] Cleaning old conflicting files...
    rmdir /s /q build_terminal
)
mkdir build_terminal

:: 2. Run MOC
echo [2/4] Running MOC on mainwindow.h...
"%QT_DIR%\bin\moc.exe" src\mainwindow.h -o build_terminal\moc_mainwindow.cpp
if errorlevel 1 (
    echo ERROR: MOC failed.
    pause
    exit /b 1
)

:: 3. Compile
echo [3/4] Compiling C++ files...
"C:\Qt\Tools\mingw1310_64\bin\g++.exe" -std=c++17 -O2 -Wall -Wextra -Isrc -I"%QT_DIR%\include" -I"%QT_DIR%\include\QtWidgets" -I"%QT_DIR%\include\QtGui" -I"%QT_DIR%\include\QtCore" src\main.cpp src\ngram.cpp src\vptree.cpp src\engine.cpp src\mainwindow.cpp build_terminal\moc_mainwindow.cpp -o build_terminal\VPTreePlagiarismDetector.exe -L"%QT_DIR%\lib" -lQt6Widgets -lQt6Gui -lQt6Core
if errorlevel 1 (
    echo ERROR: Compilation failed.
    pause
    exit /b 1
)

:: 4. Deploy graphical DLLs
echo [4/4] Deploying Qt libraries...
"%QT_DIR%\bin\windeployqt.exe" --release --no-translations build_terminal\VPTreePlagiarismDetector.exe >nul 2>&1

:: 5. Manually copy the sneaky missing MinGW DLLs!
echo Fetching missing system DLLs...
copy /Y "%QT_DIR%\bin\libstdc++-6.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libgcc_s_seh-1.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libwinpthread-1.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libbz2-1.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libicu*.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libzstd.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libbrotli*.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libdouble-conversion.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libmd4c.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libpcre*.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libfreetype-6.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libharfbuzz-0.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\libpng16-16.dll" build_terminal\ >nul 2>&1
copy /Y "%QT_DIR%\bin\zlib1.dll" build_terminal\ >nul 2>&1

echo === Build Successful! ===
pause