param(
    [string]$BuildDir = "$PSScriptRoot\..\build-msvc",
    [string]$OutputExe = "$PSScriptRoot\WallpaperXNativeInstaller.exe"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "Build-InstallerExe.ps1") -BuildDir $BuildDir

$installerExe = Join-Path (Resolve-Path $BuildDir).Path "WallpaperXInstaller.exe"
if (-not (Test-Path -LiteralPath $installerExe)) {
    throw "Installer exe not found: $installerExe"
}

Copy-Item -LiteralPath $installerExe -Destination $OutputExe -Force
Write-Host "Single-file installer ready:"
Write-Host "  $OutputExe"
