# GitHub Release Template

Use this template when publishing a new release.

---

## Title

`WallpaperX Native vX.Y.Z`

## Tag

`vX.Y.Z`

## Summary

Short summary of this release (1-3 lines).

## Highlights

### Added

- ...

### Improved

- ...

### Fixed

- ...

## Installer

### Included assets

- `WallpaperXNativeInstaller.exe` (single-file installer)
- Optional debug artifacts (if needed)

### Install behavior

- Installs runtime files into `Program Files\WallpaperXNative`
- Creates `%LOCALAPPDATA%\WallpaperXNative\Config`
- Creates `%LOCALAPPDATA%\WallpaperXNative\Videos4K`
- Creates Desktop shortcut: `WallpaperX Editor.lnk`
- Registers right-click import menu for video files
- Optional startup registration (`WallpaperXStartup.exe`)

## Upgrade Notes

- Existing installs: [compatible / migration needed]
- Breaking changes:
  - ...

## Checks Before Publish

- [ ] Build succeeds on clean machine
- [ ] Installer launches and auto-elevates with UAC
- [ ] Install path selection works
- [ ] Startup checkbox works
- [ ] Editor + Startup executables run correctly after install
- [ ] Wallpaper remains active when Editor window closes
- [ ] Duplicate video import reuse works

## Known Issues

- ...

## SHA256 (Optional)

- `WallpaperXNativeInstaller.exe`: `...`

---

### Example Release Body

```
WallpaperX Native v1.0.0 is now available.

Highlights:
- Official single-file installer with setup UI
- Real desktop WorkerW wallpaper host
- Startup helper and tray-based background mode
- Duplicate video import protection

Download `WallpaperXNativeInstaller.exe` and run as Administrator.
```
