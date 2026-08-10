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

REM SHIFT in the parse loop below also shifts %0, so capture the script dir first.
set "SCRIPT_DIR=%~dp0"

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

where node >nul 2>nul
if errorlevel 1 (
    echo ERROR: Node.js is required to generate the package manifest.
    exit /b 1
)

REM ─── Extract version info ──────────────────────────────────────────────────

REM powershell.exe -Command appends trailing arguments to the command text instead of
REM binding them to $args, so every helper below reads its path from MCP_PKG_PATH.
REM Keep these commands free of \" escapes: cmd treats that quote as a real closing
REM quote, which would expose the parentheses below to the surrounding block parser.
set "UE_VER=unknown"
set "UE_VERSION_FILE=%ENGINE_DIR%\Engine\Build\Build.version"
if not exist "%UE_VERSION_FILE%" goto ue_version_done
set "MCP_PKG_PATH=%UE_VERSION_FILE%"
REM FOR /F delimits the command with single quotes, so this line must contain none.
for /f "delims=" %%V in ('powershell -NoProfile -Command "$v = ConvertFrom-Json (Get-Content -LiteralPath $env:MCP_PKG_PATH -Raw); Write-Output ([string]$v.MajorVersion + [char]46 + [string]$v.MinorVersion)"') do set "UE_VER=%%V"
:ue_version_done

set "PLUGIN_VER=0.0.0"
set "MCP_PKG_PATH=%PLUGIN_FILE%"
for /f "delims=" %%V in ('powershell -NoProfile -Command "$d = ConvertFrom-Json (Get-Content -LiteralPath $env:MCP_PKG_PATH -Raw); Write-Output $d.VersionName"') do set "PLUGIN_VER=%%V"

set "ZIP_NAME=McpAutomationBridge-v%PLUGIN_VER%-UE%UE_VER%-Win64.zip"
set "ZIP_PATH=%OUTPUT_DIR%\%ZIP_NAME%"
set "MANIFEST_PATH=%ZIP_PATH:.zip=.manifest.json%"
REM BuildPlugin nests HostProject\Plugins\<plugin>\Intermediate\Build\Win64\x64\... under the
REM staging dir, and UBT refuses to produce any path over 260 characters. That chain plus the
REM longest source file name already spends 222 characters, so the staging path must stay short.
REM Point MCP_PACKAGE_STAGING_ROOT at a shallow root (e.g. X:\t) when %TEMP% is itself too deep.
if not defined MCP_PACKAGE_STAGING_ROOT set "MCP_PACKAGE_STAGING_ROOT=%TEMP%"
set "STAGING_DIR=%MCP_PACKAGE_STAGING_ROOT%\mcpab-%RANDOM%"
set "PACKAGE_DIR=%STAGING_DIR%\McpAutomationBridge"
set "SOURCE_PLUGIN_DIR=%STAGING_DIR%\source\McpAutomationBridge"
set "SOURCE_PLUGIN_FILE=%SOURCE_PLUGIN_DIR%\McpAutomationBridge.uplugin"

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
xcopy "%REPO_ROOT%\plugins\McpAutomationBridge" "%SOURCE_PLUGIN_DIR%\" /E /I /Q >nul
if errorlevel 1 (
    echo ERROR: Failed to stage plugin source.
    if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
    exit /b 1
)

for /f "tokens=1,2 delims=." %%A in ("%UE_VER%") do (
    set "UE_MAJOR=%%A"
    set "UE_MINOR=%%B"
)
set "MCP_PKG_PATH=%SOURCE_PLUGIN_FILE%"
if "!UE_MAJOR!"=="5" if !UE_MINOR! GEQ 2 (
    powershell -NoProfile -Command "try { $ErrorActionPreference='Stop'; $path=$env:MCP_PKG_PATH; $data=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json; $dependency=$data.Plugins | Where-Object { $_.Name -eq 'PCG' }; if ($null -ne $dependency) { $dependency.PSObject.Properties.Remove('Optional') }; $data | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $path } catch { Write-Error $_; exit 1 }"
    if errorlevel 1 (
        echo ERROR: Failed to prepare the versioned plugin descriptor.
        if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
        exit /b 1
    )
)

REM Without this every staged (writable) source is ejected from the unity blobs.
REM UBT reads UnrealBuildTool_<Category>__<Field>, scoping it to this build only.
set "UnrealBuildTool_BuildConfiguration__bUseAdaptiveUnityBuild=false"

call "%RUN_UAT%" BuildPlugin -Plugin="%SOURCE_PLUGIN_FILE%" -Package="%PACKAGE_DIR%" -TargetPlatforms=Win64 -Rocket %EXTRA_ARGS%
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
set "MCP_PKG_PATH=%OUTPUT_UPLUGIN%"
if exist "%OUTPUT_UPLUGIN%" (
    echo Setting Installed=true in output .uplugin...
    powershell -NoProfile -Command "try { $ErrorActionPreference='Stop'; $f=$env:MCP_PKG_PATH; $d=Get-Content -LiteralPath $f -Raw | ConvertFrom-Json; $d | Add-Member -Force -NotePropertyName Installed -NotePropertyValue $true; $d | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $f } catch { Write-Error $_; exit 1 }"
    if errorlevel 1 (
        echo ERROR: Failed to set Installed=true in .uplugin
        if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
        exit /b 1
    )
)

REM ─── Zip ───────────────────────────────────────────────────────────────────

echo Creating archive: %ZIP_NAME%
if exist "%ZIP_PATH%" del "%ZIP_PATH%"
if exist "%MANIFEST_PATH%" del "%MANIFEST_PATH%"
if exist "%OUTPUT_PLUGIN_DIR%\Intermediate" rmdir /s /q "%OUTPUT_PLUGIN_DIR%\Intermediate"
set "MCP_PKG_PATH=%OUTPUT_PLUGIN_DIR%"
set "MCP_PKG_ZIP=%ZIP_PATH%"
powershell -NoProfile -Command "try { $ErrorActionPreference='Stop'; $pluginDir=$env:MCP_PKG_PATH; $zipPath=$env:MCP_PKG_ZIP; Get-ChildItem -LiteralPath $pluginDir -Recurse -Directory -Filter '*.dSYM' | Sort-Object FullName -Descending | Remove-Item -Recurse -Force; Get-ChildItem -LiteralPath $pluginDir -Recurse -File | Where-Object { $_.Extension -in '.pdb', '.debug', '.sym' } | Remove-Item -Force; Push-Location (Split-Path -Parent $pluginDir); Compress-Archive -LiteralPath 'McpAutomationBridge' -DestinationPath $zipPath -Force; Pop-Location } catch { Write-Error $_; exit 1 }"
if errorlevel 1 (
    echo ERROR: Failed to create zip archive.
    if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
    exit /b 1
)
powershell -NoProfile -Command "try { $ErrorActionPreference='Stop'; Add-Type -AssemblyName System.IO.Compression.FileSystem; $archive=[System.IO.Compression.ZipFile]::OpenRead($env:MCP_PKG_ZIP); try { $forbidden=@($archive.Entries | Where-Object { $normalized=$_.FullName.Replace('\','/').ToLowerInvariant(); [System.IO.Path]::GetExtension($_.FullName).ToLowerInvariant() -in '.pdb', '.debug', '.sym' -or $normalized.Contains('.dsym/') -or $normalized.EndsWith('.dsym') }); if ($forbidden.Count -gt 0) { throw ('Distribution archive contains debug symbols: ' + (($forbidden | ForEach-Object FullName) -join ', ')) } } finally { $archive.Dispose() } } catch { Write-Error $_; exit 1 }"
if errorlevel 1 (
    echo ERROR: Archive verification failed.
    if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
    exit /b 1
)

REM Node crypto keeps SHA-256 generation identical on every supported host.
node "%SCRIPT_DIR%lib\package-manifest.mjs" "%MANIFEST_PATH%" "McpAutomationBridge" "%PLUGIN_VER%" "UE%UE_VER%-Win64" "%ENGINE_DIR%" "%ZIP_PATH%"
if errorlevel 1 (
    echo ERROR: Failed to generate package manifest.
    if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
    exit /b 1
)

if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"

echo.
echo ============================================
echo   Done!
echo   Archive: %ZIP_PATH%
echo   Manifest: %MANIFEST_PATH%
echo ============================================
echo.
echo To install: unzip into YourProject\Plugins\

endlocal
