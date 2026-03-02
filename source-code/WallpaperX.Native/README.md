# WallpaperX Native

Native C++ live wallpaper app for Windows with a release-ready installer pipeline.

## Features

- Real desktop embedding (`Progman -> WorkerW -> SetParent`)
- Dynamic wallpaper playback under desktop icons
- Editor closes to background (tray) without stopping wallpaper
- Video library in `%LOCALAPPDATA%\WallpaperXNative\Videos4K`
- Duplicate import protection (same video is reused, not copied again)
- Startup helper executable for Windows logon
- Single-file installer build flow
- Installer UI with:
  - Install path picker
  - Run-at-startup checkbox
  - Progress bar + status text
  - Admin auto-elevation
- Custom `.ico` icon on all executables

## Project Layout

```text
WallpaperX.Native/
  CMakeLists.txt
  icons/
    app.ico
  resources/
    app_icon.rc
  src/
    main.cpp
    startup.cpp
    installer.cpp
    generated_payload.h
  installer/
    Build-InstallerExe.ps1
    Build-SelfExtractInstaller.ps1
    Generate-InstallerPayload.ps1
    Build-OfflinePackage.ps1
    Install-WallpaperXNative.ps1
    Uninstall-WallpaperXNative.ps1
```

## Build Requirements

- Windows 10/11
- Visual Studio with C++ workload (MSVC)
- CMake (Visual Studio bundled CMake works)
- PowerShell

## Build (MSVC)

```powershell
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
cmake -S . -B build-msvc -G "NMake Makefiles"
cmake --build build-msvc
```

Outputs:

- `build-msvc\WallpaperXEditor.exe`
- `build-msvc\WallpaperXStartup.exe`
- `build-msvc\WallpaperXInstaller.exe`

## Build Official Single-File Installer

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\Build-SelfExtractInstaller.ps1 -BuildDir .\build-msvc
```

Release output:

- `installer\WallpaperXNativeInstaller.exe`

## Installer Behavior

- Installs runtime files to `Program Files\WallpaperXNative`
- Creates user data folders in `LocalAppData\WallpaperXNative`
- Adds desktop shortcut (`WallpaperX Editor.lnk`)
- Registers right-click import menu for video formats
- Optional startup registration (`WallpaperXStartup.exe`)

## Notes for Contributors

- `src/generated_payload.h` is generated during release packaging.
- Keep it as placeholder in source control.
- Do not commit `build-msvc/` or installer staging artifacts.
