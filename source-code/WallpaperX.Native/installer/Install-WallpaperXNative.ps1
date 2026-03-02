param(
    [string]$PayloadDir = "$PSScriptRoot\payload",
    [ValidateSet("auto","x64","x86")]
    [string]$InstallArch = "auto",
    [string]$InstallDir = "",
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

if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    if ($InstallArch -eq "x86") {
        $base = ${env:ProgramFiles(x86)}
    } elseif ($InstallArch -eq "x64") {
        $base = $env:ProgramW6432
    } else {
        $base = if ($env:ProgramW6432) { $env:ProgramW6432 } else { ${env:ProgramFiles(x86)} }
        if (-not $base) { $base = $env:ProgramFiles }
    }
    $InstallDir = Join-Path $base "WallpaperXNative"
}

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item -Path (Join-Path $PayloadDir "*") -Destination $InstallDir -Recurse -Force

$localDataRoot = Join-Path $env:LOCALAPPDATA "WallpaperXNative"
New-Item -ItemType Directory -Force -Path (Join-Path $localDataRoot "Config") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $localDataRoot "Videos4K") | Out-Null

$editorExePath = Join-Path $InstallDir "WallpaperXEditor.exe"
$startupExePath = Join-Path $InstallDir "WallpaperXStartup.exe"
if (-not (Test-Path -LiteralPath $editorExePath)) {
    throw "Missing WallpaperXEditor.exe in $InstallDir"
}
if (-not (Test-Path -LiteralPath $startupExePath)) {
    throw "Missing WallpaperXStartup.exe in $InstallDir"
}

$shortcutPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "WallpaperX Editor.lnk"
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $editorExePath
$shortcut.WorkingDirectory = $InstallDir
$shortcut.Description = "WallpaperX 4K MP4 Editor"
$shortcut.Save()

$extensions = @(".mp4", ".mkv", ".webm", ".mov", ".avi", ".m4v")
foreach ($ext in $extensions) {
    $base = "HKCU:\Software\Classes\SystemFileAssociations\$ext\shell\WallpaperXImport"
    New-Item -Path $base -Force | Out-Null
    Set-ItemProperty -Path $base -Name "(default)" -Value "Edit as 4K Live Wallpaper" -Type String
    Set-ItemProperty -Path $base -Name "Icon" -Value $editorExePath -Type String
    $cmd = Join-Path $base "command"
    New-Item -Path $cmd -Force | Out-Null
    Set-ItemProperty -Path $cmd -Name "(default)" -Value "`"$editorExePath`" --import `"%1`"" -Type String
}

$runKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
if ($EnableAutoStart) {
    Set-ItemProperty -Path $runKey -Name "WallpaperXNative" -Value "`"$startupExePath`"" -Type String
} else {
    Remove-ItemProperty -Path $runKey -Name "WallpaperXNative" -ErrorAction SilentlyContinue
}

Write-Host "Installed to: $InstallDir"
Write-Host "Data folders:"
Write-Host "  Config: $(Join-Path $localDataRoot 'Config')"
Write-Host "  Videos: $(Join-Path $localDataRoot 'Videos4K')"
Write-Host "Desktop shortcut created: $shortcutPath"
