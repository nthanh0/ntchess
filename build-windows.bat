@echo off
:: ============================================================================
:: build-windows.bat  –  one-shot Windows build helper for ntchess
:: ============================================================================
::
:: Usage:
::   build-windows.bat [debug|release]
::
:: Requirements:
::   • Qt 6 installed via the Qt Online Installer (https://www.qt.io/download)
::   • cmake on PATH  –  either from Qt Creator's bundled cmake or system cmake
::   • This script auto-detects Qt under C:\Qt\; alternatively, set QT_ROOT_DIR
::     before running, e.g.:
::       set QT_ROOT_DIR=C:\Qt\6.10.0\mingw_64
::       build-windows.bat release
::
:: After a successful build, the executable and all bundled Qt DLLs are in:
::   build\windows-<config>\appntchess.exe
:: ============================================================================

setlocal enabledelayedexpansion

:: ── 1. Parse optional build type argument ────────────────────────────────────
set BUILD_TYPE=debug
if /i "%~1"=="release" set BUILD_TYPE=release
if /i "%~1"=="debug"   set BUILD_TYPE=debug

set PRESET=windows-mingw-%BUILD_TYPE%

:: ── 2. Auto-detect QT_ROOT_DIR if not already set ────────────────────────────
if not defined QT_ROOT_DIR (
    echo [build-windows] QT_ROOT_DIR not set – searching C:\Qt for a MinGW kit...
    for /d %%V in (C:\Qt\6.*) do (
        for /d %%K in (%%V\mingw*_64) do (
            if not defined QT_ROOT_DIR (
                set QT_ROOT_DIR=%%K
            )
        )
    )
)

if not defined QT_ROOT_DIR (
    echo.
    echo  ERROR: Could not find a Qt 6 MinGW kit under C:\Qt.
    echo  Please set QT_ROOT_DIR to your Qt MinGW kit directory, for example:
    echo    set QT_ROOT_DIR=C:\Qt\6.10.0\mingw_64
    echo    build-windows.bat
    echo.
    exit /b 1
)

echo [build-windows] Using Qt root: %QT_ROOT_DIR%

:: ── 3. Add cmake and MinGW to PATH ───────────────────────────────────────────
:: Qt Creator ships cmake under <Qt>/Tools/CMake_64/bin
set "_CMAKE_HINT=%QT_ROOT_DIR%\..\..\Tools\CMake_64\bin"
if exist "%_CMAKE_HINT%\cmake.exe" set "PATH=%_CMAKE_HINT%;%PATH%"

:: MinGW
set "_MINGW_BIN=%QT_ROOT_DIR%\..\..\Tools\mingw1310_64\bin"
if exist "%_MINGW_BIN%\gcc.exe" set "PATH=%_MINGW_BIN%;%PATH%"

:: Qt bin (needed so cmake can find Qt6Config.cmake and so windeployqt runs)
set "PATH=%QT_ROOT_DIR%\bin;%PATH%"

:: Verify cmake is available
where cmake >nul 2>&1
if errorlevel 1 (
    echo.
    echo  ERROR: cmake not found on PATH.
    echo  Install cmake from https://cmake.org/download/ or via the Qt Online Installer.
    echo.
    exit /b 1
)

:: ── 4. Configure ─────────────────────────────────────────────────────────────
echo.
echo [build-windows] Configuring with preset: %PRESET%
cmake --preset %PRESET%
if errorlevel 1 (
    echo.
    echo  ERROR: CMake configuration failed.
    exit /b 1
)

:: ── 5. Build ──────────────────────────────────────────────────────────────────
echo.
echo [build-windows] Building...
cmake --build --preset %PRESET%
if errorlevel 1 (
    echo.
    echo  ERROR: Build failed.
    exit /b 1
)

echo.
echo  Build complete.
echo  Executable: build\%PRESET%\appntchess.exe
echo.
endlocal
