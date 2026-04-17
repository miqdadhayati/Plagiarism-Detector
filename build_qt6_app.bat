@echo off
setlocal
cd /d "%~dp0"

if not exist build_terminal mkdir build_terminal

where qmake6 >nul 2>&1
if errorlevel 1 (
  echo ERROR: qmake6 not found in PATH.
  exit /b 1
)

where pkg-config >nul 2>&1
if errorlevel 1 (
  echo ERROR: pkg-config not found in PATH.
  exit /b 1
)

for /f "usebackq delims=" %%I in (`qmake6 -query QT_INSTALL_PREFIX`) do set "QT_PREFIX=%%I"
set "QT_PREFIX=%QT_PREFIX:/=\%"

set "MOC=%QT_PREFIX%\share\qt6\bin\moc.exe"
if not exist "%MOC%" set "MOC=%QT_PREFIX%\qt6-static\share\qt6\bin\moc.exe"

if not exist "%MOC%" (
  echo ERROR: Qt6 moc not found at "%MOC%".
  exit /b 1
)

set "WINDEPLOYQT=%QT_PREFIX%\bin\windeployqt6.exe"
if not exist "%WINDEPLOYQT%" (
  echo ERROR: windeployqt6 not found at "%WINDEPLOYQT%".
  exit /b 1
)

"%MOC%" src\mainwindow.h -o build_terminal\moc_mainwindow.cpp
if errorlevel 1 exit /b 1

for /f "usebackq delims=" %%I in (`pkg-config --cflags Qt6Widgets`) do set "QT_CFLAGS=%%I"
if errorlevel 1 (
  echo ERROR: Could not read Qt6Widgets cflags via pkg-config.
  exit /b 1
)

for /f "usebackq delims=" %%I in (`pkg-config --libs Qt6Widgets`) do set "QT_LIBS=%%I"
if errorlevel 1 (
  echo ERROR: Could not read Qt6Widgets libs via pkg-config.
  exit /b 1
)

g++ -std=c++17 -O2 -Wall -Wextra -Isrc %QT_CFLAGS% src\main.cpp src\ngram.cpp src\vptree.cpp src\engine.cpp src\mainwindow.cpp build_terminal\moc_mainwindow.cpp -o build_terminal\VPTreePlagiarismDetector.exe %QT_LIBS%
if errorlevel 1 exit /b 1

"%WINDEPLOYQT%" --release --compiler-runtime --no-translations --dir build_terminal build_terminal\VPTreePlagiarismDetector.exe
if errorlevel 1 exit /b 1

set "QT_BIN=%QT_PREFIX%\bin"
for %%D in (
  libfreetype-6.dll
  libharfbuzz-0.dll
  libpng16-16.dll
  zlib1.dll
  libbz2-1.dll
  libbrotlicommon.dll
  libbrotlidec.dll
  libglib-2.0-0.dll
  libgraphite2.dll
  libiconv-2.dll
  libintl-8.dll
  libpcre2-8-0.dll
  libffi-8.dll
  libgmodule-2.0-0.dll
  libstdc++-6.dll
  libgcc_s_seh-1.dll
  libwinpthread-1.dll
  libmd4c.dll
  libdouble-conversion.dll
) do (
  if exist "%QT_BIN%\%%D" copy /Y "%QT_BIN%\%%D" "build_terminal\%%D" >nul
)

echo Built: build_terminal\VPTreePlagiarismDetector.exe
echo Deployed Qt runtime to build_terminal
