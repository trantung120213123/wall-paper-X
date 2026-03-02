# WallpaperX Native

Developer-oriented C++ live wallpaper project for Windows, with a native installer pipeline and GitHub-ready repository structure.

## Audience

This repository is for developers. It documents source builds and release packaging workflows.

## Quick Start

```powershell
cd source-code/WallpaperX.Native
.\build_msvc.cmd
```

`build_msvc.cmd` builds all native targets:

- `WallpaperXEditor.exe`
- `WallpaperXStartup.exe`
- `WallpaperXUninstaller.exe`
- `WallpaperXInstaller.exe`

## Build release installer

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\Build-SelfExtractInstaller.ps1 -BuildDir .\build-msvc
```

Output:

- `installer\WallpaperXNativeInstaller.exe`

## What This Project Includes

| Component | Purpose |
|---|---|
| `WallpaperXEditor.exe` | Main controller + wallpaper runtime |
| `WallpaperXStartup.exe` | Startup helper for Windows logon |
| `WallpaperXUninstaller.exe` | Native uninstaller (kills running WallpaperX processes, removes app files) |
| `WallpaperXInstaller.exe` | Installer executable |
| `WallpaperXNativeInstaller.exe` | Single-file release installer |

### Runtime behavior

- Tray/background mode (editor close does not stop wallpaper)
- Duplicate video import detection (reuses saved file)
- Installer UI:
  - custom install path
  - startup checkbox
  - progress/status
  - admin auto-elevation

## Repository Structure

```text
WallpaperX.Native/
  .github/
    ISSUE_TEMPLATE/
    workflows/build.yml
  icons/
    app.ico
  resources/
    app_icon.rc
  src/
    main.cpp
    startup.cpp
    installer.cpp
    uninstaller.cpp
    generated_payload.h
  installer/
    Build-InstallerExe.ps1
    Build-SelfExtractInstaller.ps1
    Generate-InstallerPayload.ps1
    Build-OfflinePackage.ps1
    Install-WallpaperXNative.ps1
    Uninstall-WallpaperXNative.ps1
  build_msvc.cmd
  CMakeLists.txt
  LICENSE
  CONTRIBUTING.md
  SECURITY.md
  SUPPORT.md
  RELEASE.md
```

## Build Requirements

- Windows 10/11
- CMake 3.20+
- C++ standard: C++20 (`CMAKE_CXX_STANDARD 20`)
- PowerShell

## Toolchain and Downloads

### Recommended (MSVC, official)

- Visual Studio 2026 (v18) Community or newer with `Desktop development with C++`
- CMake 3.20+ (or Visual Studio bundled CMake)

Download links:

- Visual Studio: https://visualstudio.microsoft.com/downloads/
- CMake: https://cmake.org/download/

Note:

- Current helper scripts (`build_msvc.cmd`, installer build scripts) are tuned for Visual Studio 2026 path (`...Visual Studio\\18\\...`).
- If you use VS2022, update paths from `18` to `17` in the `.cmd` / `.ps1` scripts.

## Build Commands (Manual)

```powershell
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
cmake -S . -B build-msvc -G "NMake Makefiles"
cmake --build build-msvc
```

## Build a Single Target (Uninstaller)

```powershell
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
cmake -S . -B build-msvc -G "NMake Makefiles"
cmake --build build-msvc --target WallpaperXUninstaller
```

## MinGW Build (MSYS2)

This project can be built with `mingw-w64` using CMake, but release installer scripts are MSVC-oriented.

### 1) Install MSYS2 + MinGW toolchain

Download:

- MSYS2: https://www.msys2.org/

Inside `MSYS2 UCRT64` terminal:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

### 2) Configure + build

```bash
cd /d/object\ code\ c++/chess/source-code/WallpaperX.Native
cmake -S . -B build-mingw -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw
```

### 3) Build only uninstaller target

```bash
cmake --build build-mingw --target WallpaperXUninstaller
```

## GitHub Release Workflow

1. Build and test locally.
2. Follow checklist in `RELEASE.md`.
3. Create tag `vX.Y.Z`.
4. Publish GitHub Release and upload installer.

## Contribution

- Read `CONTRIBUTING.md` before opening PRs.
- Use issue templates for bug reports and feature requests.
- Keep commits focused and avoid generated artifacts.

## License

Licensed under GNU General Public License v3.0 (GPL-3.0).

- You can use and modify this project.
- If you distribute a modified version, you must provide the corresponding source code under GPL-3.0.
- See `LICENSE` for full terms.
