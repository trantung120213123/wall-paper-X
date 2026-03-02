param(
    [string]$BuildDir = "$PSScriptRoot\..\build-msvc",
    [string]$VsDevCmd = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $VsDevCmd)) {
    throw "VsDevCmd not found: $VsDevCmd"
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildRoot = (Resolve-Path $BuildDir).Path

$cmd = Join-Path $projectRoot "build_installer.cmd"

@"
@echo off
call "$VsDevCmd" -arch=x64
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S "$projectRoot" -B "$buildRoot" -G "NMake Makefiles"
if errorlevel 1 exit /b 1
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "$buildRoot" --target WallpaperXEditor WallpaperXStartup WallpaperXUninstaller
if errorlevel 1 exit /b 1
"@ | Set-Content -Path $cmd -Encoding Ascii

cmd /c $cmd
if ($LASTEXITCODE -ne 0) {
    throw "Failed to build editor/startup."
}

& (Join-Path $PSScriptRoot "Generate-InstallerPayload.ps1") -BuildOutputDir $buildRoot

@"
@echo off
call "$VsDevCmd" -arch=x64
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "$buildRoot" --target WallpaperXInstaller
"@ | Set-Content -Path $cmd -Encoding Ascii

cmd /c $cmd
if ($LASTEXITCODE -ne 0) {
    throw "Failed to build installer target."
}

Remove-Item -LiteralPath $cmd -Force

$installerExe = Join-Path $buildRoot "WallpaperXInstaller.exe"
if (-not (Test-Path -LiteralPath $installerExe)) {
    throw "Installer exe not found: $installerExe"
}

Write-Host "Installer built:"
Write-Host "  $installerExe"
