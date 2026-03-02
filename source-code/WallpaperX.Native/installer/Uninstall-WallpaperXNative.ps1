param(
    [string]$InstallDir = "$env:ProgramFiles\WallpaperXNative",
    [switch]$RemoveUserData
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
}

if (-not (Test-Admin)) {
    throw "Please run this uninstaller as Administrator."
}

$shortcutPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "WallpaperX Editor.lnk"
if (Test-Path -LiteralPath $shortcutPath) {
    Remove-Item -LiteralPath $shortcutPath -Force
}

$extensions = @(".mp4", ".mkv", ".webm", ".mov", ".avi", ".m4v")
foreach ($ext in $extensions) {
    $base = "HKCU:\Software\Classes\SystemFileAssociations\$ext\shell\WallpaperXImport"
    if (Test-Path -LiteralPath $base) {
        Remove-Item -LiteralPath $base -Recurse -Force
    }
}

$runKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
Remove-ItemProperty -Path $runKey -Name "WallpaperXNative" -ErrorAction SilentlyContinue

if (Test-Path -LiteralPath $InstallDir) {
    Remove-Item -LiteralPath $InstallDir -Recurse -Force
}

if ($RemoveUserData) {
    $localDataRoot = Join-Path $env:LOCALAPPDATA "WallpaperXNative"
    if (Test-Path -LiteralPath $localDataRoot) {
        Remove-Item -LiteralPath $localDataRoot -Recurse -Force
    }
}

Write-Host "WallpaperX Native removed."
