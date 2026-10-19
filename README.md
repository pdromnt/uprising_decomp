# Uprising: Join or Die — RE/Decompiled

A behavior-focused reconstruction of **Uprising: Join or Die**, the 1997 game
by Cyclone Studios. Game data is included alongside the source; original game
content and third-party components retain their separate ownership and terms.

The game runs ordinary C with native OpenGL rendering, without CPU/x87 emulation,
generated instruction execution, Glide or loading the original executable.
Recovered IDs and bounded legacy layouts remain where they preserve behavior;
this is still a decompilation, not a new engine.

Most of the game runs fine, but it is still under heavy testing and may have
crashes that have not yet been found.

## Running

Launch `uprising.exe` (x86) or `uprising64.exe` (x64); no flags are required.
Keep it beside `uprising.cln`, `FONTS/`, `GRIDS/`, `Music/` and `VIDEO/`, using the
complete fonts supplied by this project. Assets resolve beside the executable,
regardless of the launch directory. Windows builds are currently supported;
Linux and macOS ports are next.

The first launch is windowed and centered on the primary monitor. Settings,
keyboard/controller bindings and music preferences use `CONFIG/settings.ini`.
Legacy `FONTS/settings.cfg` and `FONTS/keymap.cfg` are imported on first use when
present. Saves and screenshots are separate user data.

Options → Graphics → **ADVANCED** provides:

- **Display mode:** windowed or desktop fullscreen. The game stays 4:3 with black
  borders on wide monitors; windowed resolutions are 4:3 only.
- **FSR1:** upscales gameplay, HUD and menus. Off uses a pixel-art scaling filter.
  FSR requires an OpenGL 4.3 compatibility context; the base renderer does not.
- **3D anti-aliasing:** Off or supported MSAA 2×/4×/8×, applied on the next mission
  load. HUD, text, menus and 2D overlay draws remain outside the AA pass.

**Apply → KEEP** saves the choice. Unconfirmed changes revert after 20 seconds.
Music and effects volume affect the game only. Movies continue while unfocused.
Bindings can be changed in Options; **Tab** switches the player camera and **K**
follows a selected unit.

Multiplayer's **TCP/IP** entry uses ENet UDP between reconstructed builds, up to
four players. Allow UDP 2302 for the initial host, 2303 for discovery, and peer
traffic through the firewall. Retail DirectPlay peers, external DirectPlay
lobbies, IPX, modem and serial-cable connections are unsupported.

## Building and contributing

Use MinGW-w64 GCC/G++, windres, CMake 3.24+ and Ninja on Windows:

```powershell
./source/scripts/build.ps1 -Bits 32 -Tests
```

`source\scripts\build.bat` is the batch shortcut. Use `-Bits 64` for x64.
Outputs go beside the assets; CI packages each architecture as `uprising.exe`.
See [Building](docs/BUILDING.md), [Codebase](docs/CODEBASE.md),
[Remaining work](docs/BACKLOG.md) and [Manual testing](docs/MANUAL_TESTING.md).

Report the mission, action, build/architecture and steps to reproduce a bug.
No project-wide license has been selected for the reconstructed code; see
[Third-party notices](THIRD_PARTY_NOTICES.md) for component and asset terms.
