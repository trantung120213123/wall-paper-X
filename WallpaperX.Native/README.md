# WallpaperX Native (C++)

Win32 C++ dynamic wallpaper app with real desktop embedding:
- `Progman -> WorkerW -> SetParent`
- Runs under desktop icons (not fullscreen app, not taskbar window)
- Imports videos into managed local storage (`%LOCALAPPDATA%\WallpaperXNative\Videos4K`)
- Config in `%LOCALAPPDATA%\WallpaperXNative\Config\settings.ini`
- Auto-start support
- Desktop shortcut + right-click import menu for `.mp4/.mkv/.webm/.mov/.avi/.m4v`

## Build

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Offline installer package

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\Build-OfflinePackage.ps1 -BuildOutputDir .\build\Release
```

Then run as Administrator:

```powershell
cd .\installer\package
powershell -ExecutionPolicy Bypass -File .\Install-WallpaperXNative.ps1 -EnableAutoStart
```

Uninstall:

```powershell
powershell -ExecutionPolicy Bypass -File .\Uninstall-WallpaperXNative.ps1 -RemoveUserData
```
