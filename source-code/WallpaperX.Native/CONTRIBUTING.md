# Contributing to WallpaperX Native

Thanks for contributing.

## Development Setup

1. Install Visual Studio C++ workload and CMake.
2. Open terminal in project root (`WallpaperX.Native`).
3. Build:

```powershell
.\build_msvc.cmd
```

## Branching and Commits

- Create a feature branch from `main`.
- Keep commits focused and small.
- Use clear commit messages:
  - `feat: add ...`
  - `fix: resolve ...`
  - `build: update ...`

## Code Guidelines

- C++20, Win32 native APIs.
- Keep code ASCII unless required.
- Prefer simple, explicit logic over abstractions.
- Add comments only when code intent is non-obvious.
- Do not commit generated build output (`build-msvc`, `.obj`, `.pdb`, installer staging folders).

## Testing Checklist

Before opening a PR:

1. Build Editor, Startup, Installer successfully.
2. Run installer and verify:
   - install path selection works
   - startup checkbox behavior works
   - progress/status UI works
3. Verify wallpaper playback under desktop icons.
4. Verify closing Editor keeps wallpaper running in tray mode.
5. Verify duplicate video import does not create extra copies.

## Pull Request Content

PR description should include:

- Summary of user-visible changes
- Technical changes
- Manual test results
- Any known limitations
