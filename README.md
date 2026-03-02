# WallpaperX Native Workspace

Developer workspace for the native C++ WallpaperX project.

## Where To Start

> Open this folder first:
>
> `source-code/WallpaperX.Native`

That folder is the clean GitHub package you should push.

## Folder Guide

| Folder | Purpose |
|---|---|
| `source-code/WallpaperX.Native` | Clean source package (recommended repo root) |
| `WallpaperX.Native` | Local working/output area |

## Developer Build Commands

### Build all native targets (Editor, Startup, Uninstaller, Installer)

```powershell
cd source-code/WallpaperX.Native
.\build_msvc.cmd
```

### Build release installer (.exe)

```powershell
cd source-code/WallpaperX.Native
powershell -ExecutionPolicy Bypass -File .\installer\Build-SelfExtractInstaller.ps1 -BuildDir .\build-msvc
```

Output:

- `source-code/WallpaperX.Native/installer/WallpaperXNativeInstaller.exe`

### Build uninstaller target only

```powershell
cd source-code/WallpaperX.Native
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
cmake -S . -B build-msvc -G "NMake Makefiles"
cmake --build build-msvc --target WallpaperXUninstaller
```

## Toolchain (Recommended)

- Build system: CMake (project is CMake-native)
- Visual Studio: 2026 (v18) + `Desktop development with C++`
- C++ standard: C++20

Download links:

- Visual Studio: https://visualstudio.microsoft.com/downloads/
- CMake: https://cmake.org/download/
- MSYS2 (for MinGW): https://www.msys2.org/

For full MSVC and MinGW build guide, see:

- `source-code/WallpaperX.Native/README.md`

## Project Capabilities

- Desktop host pipeline: `Progman -> WorkerW -> SetParent`
- Background tray mode (closing editor does not stop wallpaper)
- Startup helper executable
- Native uninstaller executable (`WallpaperXUninstaller.exe`) that stops WallpaperX processes before removing app files
- Duplicate video import reuse
- Installer UI with:
  - install path picker
  - startup checkbox
  - progress/status
  - admin auto-elevation

## Docs

Inside `source-code/WallpaperX.Native`:

- `README.md`
- `CONTRIBUTING.md`
- `RELEASE.md`
- `SECURITY.md`
- `SUPPORT.md`
- `LICENSE`

## License

This project is GPL-3.0.

- Duoc dung.
- Duoc sua.
- Neu sua va phan phoi, phai public source tuong ung theo GPL-3.0.
