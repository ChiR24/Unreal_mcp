@echo off
setlocal enabledelayedexpansion
REM
REM Package McpAutomationBridge plugin as pre-built binaries.
REM Output can be distributed to Blueprint-only projects (no compilation needed).
REM
REM Usage:
REM   scripts\package-plugin.bat C:\UE\UE_5.6
REM   scripts\package-plugin.bat C:\UE\UE_5.6 C:\output
REM   scripts\package-plugin.bat C:\UE\UE_5.6 C:\output -NoDefaultPlugins
REM

REM ─── Arguments ─────────────────────────────────────────────────────────────

set "ENGINE_DIR=%~1"
set "OUTPUT_DIR="
set "EXTRA_ARGS="

if "%ENGINE_DIR%"=="" (
    echo Usage: %~nx0 ^<UnrealEngineDir^> [OutputDir] [extra RunUAT args...]
    exit /b 1
)

REM Parse remaining args: if starts with -, it's an extra arg; otherwise it's output dir
shift
:parse_args
if "%~1"=="" goto done_args
set "_ARG=%~1"
if "!_ARG:~0,1!"=="-" (
    set "EXTRA_ARGS=!EXTRA_ARGS! %~1"
) else (
    if "!OUTPUT_DIR!"=="" (
        set "OUTPUT_DIR=%~1"
    ) else (
        echo ERROR: Unexpected extra output directory argument: %~1
        exit /b 1
    )
)
shift
goto parse_args
:done_args

if "!OUTPUT_DIR!"=="" set "OUTPUT_DIR=%cd%\build"
if not exist "!OUTPUT_DIR!" mkdir "!OUTPUT_DIR!"
for %%I in ("!OUTPUT_DIR!") do set "OUTPUT_DIR=%%~fI"

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%.."
set "PLUGIN_FILE=%REPO_ROOT%\plugins\McpAutomationBridge\McpAutomationBridge.uplugin"

if not exist "%PLUGIN_FILE%" (
    echo ERROR: Plugin file not found: %PLUGIN_FILE%
    exit /b 1
)

set "RUN_UAT=%ENGINE_DIR%\Engine\Build\BatchFiles\RunUAT.bat"
if not exist "%RUN_UAT%" (
    echo ERROR: RunUAT not found: %RUN_UAT%
    echo Make sure the first argument points to your UE installation root.
    exit /b 1
)

REM ─── Extract version info ──────────────────────────────────────────────────

set "UE_VER=unknown"
set "UE_VERSION_FILE=%ENGINE_DIR%\Engine\Build\Build.version"
if exist "%UE_VERSION_FILE%" (
    for /f "delims=" %%V in ('powershell -NoProfile -Command "$v = Get-Content -LiteralPath $args[0] -Raw | ConvertFrom-Json; Write-Output \"$($v.MajorVersion).$($v.MinorVersion)\"" "%UE_VERSION_FILE%"') do set "UE_VER=%%V"
)

set "PLUGIN_VER=0.0.0"
for /f "delims=" %%V in ('powershell -NoProfile -Command "$d = Get-Content -LiteralPath $args[0] -Raw | ConvertFrom-Json; Write-Output $d.VersionName" "%PLUGIN_FILE%"') do set "PLUGIN_VER=%%V"

set "ZIP_NAME=McpAutomationBridge-v%PLUGIN_VER%-UE%UE_VER%-Win64.zip"
set "ZIP_PATH=%OUTPUT_DIR%\%ZIP_NAME%"
set "STAGING_DIR=%TEMP%\McpAutomationBridge-package-%RANDOM%%RANDOM%"
set "PACKAGE_DIR=%STAGING_DIR%\McpAutomationBridge"

if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
mkdir "%STAGING_DIR%"
if errorlevel 1 (
    echo ERROR: Failed to create staging directory: %STAGING_DIR%
    exit /b 1
)

echo ============================================
echo   Package McpAutomationBridge Plugin
echo ============================================
echo   Plugin version : %PLUGIN_VER%
echo   UE version     : %UE_VER%
echo   Platform       : Win64
echo   Engine         : %ENGINE_DIR%
echo   Output         : %ZIP_PATH%
echo ============================================
echo.

REM ─── Build ─────────────────────────────────────────────────────────────────

echo Building plugin...
call "%RUN_UAT%" BuildPlugin -Plugin="%PLUGIN_FILE%" -Package="%PACKAGE_DIR%" -TargetPlatforms=Win64 -Rocket %EXTRA_ARGS%
if errorlevel 1 (
    echo ERROR: Build failed.
    if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
    exit /b 1
)

echo.
echo Build complete.

REM ─── Post-process: set Installed=true ──────────────────────────────────────

set "OUTPUT_PLUGIN_DIR="
if exist "%PACKAGE_DIR%\McpAutomationBridge.uplugin" set "OUTPUT_PLUGIN_DIR=%PACKAGE_DIR%"
if not defined OUTPUT_PLUGIN_DIR if exist "%PACKAGE_DIR%\HostProject\Plugins\McpAutomationBridge\McpAutomationBridge.uplugin" set "OUTPUT_PLUGIN_DIR=%PACKAGE_DIR%\HostProject\Plugins\McpAutomationBridge"

if not defined OUTPUT_PLUGIN_DIR (
    echo ERROR: Packaged plugin output not found under: %PACKAGE_DIR%
    if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
    exit /b 1
)

set "OUTPUT_UPLUGIN=%OUTPUT_PLUGIN_DIR%\McpAutomationBridge.uplugin"
if exist "%OUTPUT_UPLUGIN%" (
    echo Setting Installed=true in output .uplugin...
    powershell -NoProfile -Command "try { $ErrorActionPreference='Stop'; $f=$args[0]; $d=Get-Content -LiteralPath $f -Raw | ConvertFrom-Json; $d | Add-Member -Force -NotePropertyName Installed -NotePropertyValue $true; $d | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $f } catch { Write-Error $_; exit 1 }" "%OUTPUT_UPLUGIN%"
    if errorlevel 1 (
        echo ERROR: Failed to set Installed=true in .uplugin
        if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
        exit /b 1
    )
)

REM ─── Zip ───────────────────────────────────────────────────────────────────

echo Creating archive: %ZIP_NAME%
if exist "%ZIP_PATH%" del "%ZIP_PATH%"
if exist "%OUTPUT_PLUGIN_DIR%\Intermediate" rmdir /s /q "%OUTPUT_PLUGIN_DIR%\Intermediate"
powershell -NoProfile -Command "try { $ErrorActionPreference='Stop'; $pluginDir=$args[0]; $zipPath=$args[1]; Get-ChildItem -LiteralPath $pluginDir -Recurse -Filter '*.pdb' | Remove-Item -Force; Push-Location (Split-Path -Parent $pluginDir); Compress-Archive -LiteralPath 'McpAutomationBridge' -DestinationPath $zipPath -Force; Pop-Location } catch { Write-Error $_; exit 1 }" "%OUTPUT_PLUGIN_DIR%" "%ZIP_PATH%"
if errorlevel 1 (
    echo ERROR: Failed to create zip archive.
    if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
    exit /b 1
)

if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"

echo.
echo ============================================
echo   Done!
echo   Archive: %ZIP_PATH%
echo ============================================
echo.
echo To install: unzip into YourProject\Plugins\

endlocal
