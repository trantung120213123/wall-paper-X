param(
    [string]$BuildOutputDir = "$PSScriptRoot\..\build\Release",
    [string]$PackageDir = "$PSScriptRoot\package"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $BuildOutputDir)) {
    throw "Build output not found: $BuildOutputDir"
}

if (Test-Path -LiteralPath $PackageDir) {
    Remove-Item -LiteralPath $PackageDir -Recurse -Force
}

$payloadDir = Join-Path $PackageDir "payload"
New-Item -ItemType Directory -Force -Path $payloadDir | Out-Null

$required = @(
    "WallpaperXNative.exe"
)

foreach ($name in $required) {
    $candidate = Join-Path $BuildOutputDir $name
    if (-not (Test-Path -LiteralPath $candidate)) {
        throw "Missing required file: $candidate"
    }
}

Copy-Item -Path (Join-Path $BuildOutputDir "*") -Destination $payloadDir -Recurse -Force
Copy-Item -Path (Join-Path $PSScriptRoot "Install-WallpaperXNative.ps1") -Destination $PackageDir -Force
Copy-Item -Path (Join-Path $PSScriptRoot "Uninstall-WallpaperXNative.ps1") -Destination $PackageDir -Force

Write-Host "Offline package created:"
Write-Host "  $PackageDir"
Write-Host "Run as admin:"
Write-Host "  powershell -ExecutionPolicy Bypass -File .\Install-WallpaperXNative.ps1 -EnableAutoStart"
