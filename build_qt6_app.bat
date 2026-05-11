@echo off
setlocal
cd /d "%~dp0"

echo === VP-Tree Plagiarism Detector Build Script ===

:: Set Qt/MinGW paths (MSYS2 installation)
set "QT_DIR=C:\msys64\mingw64"
set "QT_INC=%QT_DIR%\include\qt6"
set "GCC=%QT_DIR%\bin\g++.exe"

:: 1. NUKE THE OLD FOLDER (This prevents corrupted DLL crashes)
if exist build_terminal (
    echo [1/4] Cleaning old conflicting files...
    rmdir /s /q build_terminal
)
mkdir build_terminal

:: 2. Run MOC
echo [2/4] Running MOC on mainwindow.h...
"%QT_DIR%\share\qt6\bin\moc.exe" src\mainwindow.h -o build_terminal\moc_mainwindow.cpp
if errorlevel 1 (
    echo ERROR: MOC failed.
    pause
    exit /b 1
)

:: 3. Compile
echo [3/4] Compiling C++ files...
"%GCC%" -std=c++17 -O2 -Wall -Wextra -Isrc -I"%QT_INC%" -I"%QT_INC%\QtWidgets" -I"%QT_INC%\QtGui" -I"%QT_INC%\QtCore" src\main.cpp src\ngram.cpp src\vptree.cpp src\engine.cpp src\mainwindow.cpp build_terminal\moc_mainwindow.cpp -o build_terminal\VPTreePlagiarismDetector.exe -L"%QT_DIR%\lib" -lQt6Widgets -lQt6Gui -lQt6Core
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