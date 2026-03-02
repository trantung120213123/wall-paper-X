param(
    [string]$BuildOutputDir = "$PSScriptRoot\..\build-msvc",
    [string]$PackageDir = "$PSScriptRoot\package"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $BuildOutputDir)) {
    throw "Build output not found: $BuildOutputDir"
}

if (Test-Path -LiteralPath $PackageDir) {
    cmd /c rmdir /s /q "$PackageDir" | Out-Null
}

$payloadDir = Join-Path $PackageDir "payload"
New-Item -ItemType Directory -Force -Path $payloadDir | Out-Null

$required = @(
    "WallpaperXEditor.exe",
    "WallpaperXStartup.exe",
    "WallpaperXUninstaller.exe"
)

foreach ($name in $required) {
    $candidate = Join-Path $BuildOutputDir $name
    if (-not (Test-Path -LiteralPath $candidate)) {
        throw "Missing required file: $candidate"
    }
}

$runtimeExtensions = @(".exe", ".dll", ".json", ".dat", ".pak", ".bin", ".cfg", ".ini")
Get-ChildItem -Path $BuildOutputDir -File | ForEach-Object {
    $ext = $_.Extension.ToLowerInvariant()
    if ($ext -eq ".exe") {
        if ($required -contains $_.Name) {
            Copy-Item -Path $_.FullName -Destination $payloadDir -Force
        }
        return
    }
    if ($runtimeExtensions -contains $ext) {
        Copy-Item -Path $_.FullName -Destination $payloadDir -Force
    }
}

$optionalDirs = @("libvlc", "plugins", "assets", "resources")
foreach ($dir in $optionalDirs) {
    $src = Join-Path $BuildOutputDir $dir
    if (Test-Path -LiteralPath $src) {
        Copy-Item -Path $src -Destination $payloadDir -Recurse -Force
    }
}
Copy-Item -Path (Join-Path $PSScriptRoot "Install-WallpaperXNative.ps1") -Destination $PackageDir -Force
Copy-Item -Path (Join-Path $PSScriptRoot "Uninstall-WallpaperXNative.ps1") -Destination $PackageDir -Force

Write-Host "Offline package created:"
Write-Host "  $PackageDir"
Write-Host "Run as admin:"
Write-Host "  powershell -ExecutionPolicy Bypass -File .\Install-WallpaperXNative.ps1 -EnableAutoStart"
