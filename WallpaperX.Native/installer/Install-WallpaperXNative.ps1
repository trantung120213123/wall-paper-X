param(
    [string]$PayloadDir = "$PSScriptRoot\payload",
    [string]$InstallDir = "$env:ProgramFiles\WallpaperXNative",
    [switch]$EnableAutoStart
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
}

if (-not (Test-Admin)) {
    throw "Please run this installer as Administrator."
}

if (-not (Test-Path -LiteralPath $PayloadDir)) {
    throw "Payload folder not found: $PayloadDir"
}

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item -Path (Join-Path $PayloadDir "*") -Destination $InstallDir -Recurse -Force

$localDataRoot = Join-Path $env:LOCALAPPDATA "WallpaperXNative"
New-Item -ItemType Directory -Force -Path (Join-Path $localDataRoot "Config") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $localDataRoot "Videos4K") | Out-Null

$exePath = Join-Path $InstallDir "WallpaperXNative.exe"
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Missing WallpaperXNative.exe in $InstallDir"
}

$shortcutPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "WallpaperX Editor.lnk"
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $exePath
$shortcut.WorkingDirectory = $InstallDir
$shortcut.Description = "WallpaperX MP4 Editor"
$shortcut.Save()

$extensions = @(".mp4", ".mkv", ".webm", ".mov", ".avi", ".m4v")
foreach ($ext in $extensions) {
    $base = "HKCU:\Software\Classes\SystemFileAssociations\$ext\shell\WallpaperXImport"
    New-Item -Path $base -Force | Out-Null
    Set-ItemProperty -Path $base -Name "(default)" -Value "Set as WallpaperX wallpaper" -Type String
    Set-ItemProperty -Path $base -Name "Icon" -Value $exePath -Type String
    $cmd = Join-Path $base "command"
    New-Item -Path $cmd -Force | Out-Null
    Set-ItemProperty -Path $cmd -Name "(default)" -Value "`"$exePath`" --import `"%1`"" -Type String
}

$runKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
if ($EnableAutoStart) {
    Set-ItemProperty -Path $runKey -Name "WallpaperXNative" -Value "`"$exePath`" --background" -Type String
} else {
    Remove-ItemProperty -Path $runKey -Name "WallpaperXNative" -ErrorAction SilentlyContinue
}

Write-Host "Installed to: $InstallDir"
Write-Host "Data folders:"
Write-Host "  Config: $(Join-Path $localDataRoot 'Config')"
Write-Host "  Videos: $(Join-Path $localDataRoot 'Videos4K')"
Write-Host "Desktop shortcut created: $shortcutPath"
