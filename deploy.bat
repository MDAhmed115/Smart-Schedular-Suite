@echo off
setlocal EnableDelayedExpansion
title Smart Scheduler Suite - Packager

REM ======================= SETTINGS =======================
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set "VERSION=1.0.0"
set "APP=SmartSchedulerSuite"

REM Leave empty to auto-detect from your existing build\CMakeCache.txt.
REM Fill in only if detection fails, e.g. C:\Qt\6.7.2\msvc2022_64
set "QT_ROOT="
REM ========================================================

set "RB=build-release"
set "OUT=dist\%APP%"

echo.
echo [1/7] Locating Qt...
if not defined QT_ROOT (
    set "QT6DIR="
    for /f "tokens=2 delims==" %%A in ('findstr /b /c:"Qt6_DIR:" build\CMakeCache.txt 2^>nul') do set "QT6DIR=%%A"
    if not defined QT6DIR (
        for /f "tokens=2 delims==" %%A in ('findstr /b /c:"Qt6Widgets_DIR:" build\CMakeCache.txt 2^>nul') do set "QT6DIR=%%A"
    )
    if defined QT6DIR (
        for %%I in ("!QT6DIR!\..\..\..") do set "QT_ROOT=%%~fI"
    )
)
if not exist "%QT_ROOT%\bin\windeployqt.exe" (
    echo [ERROR] Qt not found. Set QT_ROOT at the top, for example: C:\Qt\6.7.2\msvc2022_64
    pause
    exit /b 1
)
set "QT_BIN=%QT_ROOT%\bin"
echo       Qt root: %QT_ROOT%

echo.
echo [2/7] Setting up Visual Studio environment...
call "%VCVARS%" >nul
if errorlevel 1 (
    echo [ERROR] vcvars64.bat failed. Check the VCVARS path at the top of this file.
    pause
    exit /b 1
)

echo.
echo [3/7] Configuring RELEASE build in %RB%...
cmake -S . -B %RB% -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_ROOT%"
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    pause
    exit /b 1
)

echo.
echo [4/7] Building Release...
cmake --build %RB%
if errorlevel 1 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo.
echo [5/7] Locating executable...
set "EXE_PATH="
for %%F in ("%RB%\*.exe") do if not defined EXE_PATH set "EXE_PATH=%%F"
if not defined EXE_PATH (
    echo [ERROR] No exe found in %RB%.
    pause
    exit /b 1
)
echo       Found: %EXE_PATH%

echo.
echo [6/7] Collecting Qt DLLs, plugins and VC++ runtime...
if exist dist rmdir /s /q dist
mkdir "%OUT%"
copy /y "%EXE_PATH%" "%OUT%\%APP%.exe" >nul
"%QT_BIN%\windeployqt.exe" --release --no-translations "%OUT%\%APP%.exe"
if errorlevel 1 (
    echo [ERROR] windeployqt failed.
    pause
    exit /b 1
)

REM App-local VC++ runtime (Release versions) so it runs without Visual Studio
for %%D in (msvcp140.dll msvcp140_1.dll msvcp140_2.dll vcruntime140.dll vcruntime140_1.dll concrt140.dll) do (
    if exist "%SystemRoot%\System32\%%D" copy /y "%SystemRoot%\System32\%%D" "%OUT%\" >nul
)

if exist data xcopy /e /i /y data "%OUT%\data" >nul
if exist README.md copy /y README.md "%OUT%\" >nul
if exist LICENSE copy /y LICENSE "%OUT%\" >nul

echo.
echo [7/7] Creating ZIP...
powershell -NoProfile -Command "Compress-Archive -Path '%OUT%\*' -DestinationPath 'dist\%APP%-v%VERSION%-win64.zip' -Force"
if errorlevel 1 (
    echo [ERROR] ZIP creation failed.
    pause
    exit /b 1
)

echo.
echo ==================================================
echo  Done!
echo  Folder: %OUT%
echo  ZIP:    dist\%APP%-v%VERSION%-win64.zip
echo ==================================================
echo.
pause