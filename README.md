# Uprising: Join or Die — RE/Decompiled

A behavior-focused reconstruction of **Uprising: Join or Die**, the 1997 game
developed by Cyclone Studios. This project does not claim ownership of the
original game, artwork, music or other assets. Game data is included alongside the source.

The shipping target runs ordinary C with native OpenGL rendering. It does not
execute generated x86 instructions, use a CPU/x87 interpreter, load Glide, or read
the original executable. Recovered addresses and bounded legacy records remain
where they preserve behavior; this is still a decompilation, not a new engine.

Most of the game runs fine, but it is still under heavy testing and may have
crashes that have not yet been found.

## Running

Place the reconstructed `uprising.exe` (x86) or `uprising64.exe` (x64) beside `uprising.cln`, `FONTS/`, `GRIDS/`,
`Music/` and `VIDEO/`. Preserve your original executable elsewhere first.
Use the complete `FONTS/` from this project/release, including Liberation Sans.
Double-click your chosen executable; no command-line flags are required. The game starts
windowed with keyboard, mouse and audio enabled. Its title is
**Uprising: Join or Die - RE/Decompiled**.

Settings, keyboard/controller mappings and music preferences share
`CONFIG/settings.ini`. The first run imports old `FONTS/settings.cfg` and
`FONTS/keymap.cfg` if present. Later runs use the
new file. Saves and screenshots remain separate user data.

SDL3 plays effects/movie audio and streams the OGG tracks in `Music/` on both
architectures. The old music DLLs and `winmm.ini` are no longer required.
Music/effects volume and track preferences use `CONFIG/settings.ini`; volume
controls affect the game, leaving desktop volume alone.

## Building

From the repository root, run `source\scripts\build.bat` or:

```powershell
./source/scripts/build.ps1 -Bits 32
```

Requires MinGW-w64 GCC, windres, CMake 3.24+ and Ninja. Local output names are `uprising.exe` and `uprising64.exe`. CI packages each architecture separately as `uprising.exe`. The default output directory is
the repository root, beside the assets; intermediate files stay in `source/build/`.
The script refuses to overwrite an original or unknown executable. Use
`-OutputDirectory path` for staging, `-Bits 64` for an x64 build, and `-Tests` for
the asset-free checks. See [docs/BUILDING.md](docs/BUILDING.md).

## Status and contributing

Windows x86 gameplay has received iterative manual testing and original-code
comparisons. Windows x64 has targeted gameplay/build/core-check coverage; full x64 gameplay,
natural campaign/tutorial progression and broader multiplayer acceptance remain
manual validation. SDL3 platform migration is underway; other platforms remain
deferred. A successful link or smoke test does not establish whole-game parity.

See [CODEBASE.md](CODEBASE.md), [BACKLOG.md](BACKLOG.md), and
[docs/MANUAL_TESTING.md](docs/MANUAL_TESTING.md). Report the mission, exact action,
architecture and reproducible steps when filing a bug. Keep unrelated cleanup
separate from behavior fixes.

No project-wide license has been selected for the reconstructed code. Original
and third-party material retains its separate ownership and terms; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
