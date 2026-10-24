# Uprising: Join or Die — RE/Decompiled

<img width="400" src="https://github.com/user-attachments/assets/5aab263b-75ba-4a55-867a-71572ff7030f" /> <img width="400" src="https://github.com/user-attachments/assets/e8512391-3f07-4b7e-b8c6-00d9ea17c4de" />

A behavior-focused reconstruction of **Uprising: Join or Die**, the 1997 game
by Cyclone Studios. Game data is included alongside the source; original game
content and third-party components retain their separate ownership and terms.

This `FSR3` branch provides Vulkan game packages with optional FSR 3.1 upscaling
on 64-bit systems. Download the **vulkan-game-...** CI artifact for your platform;
it launches normally, without flags. See [Vulkan / FSR3](docs/FSR3.md) for build
instructions and current validation limits. Stable `main` remains unchanged.

The game runs ordinary C with native rendering, without CPU/x87 emulation,
generated instruction execution, Glide or loading the original executable.
Recovered IDs and bounded legacy layouts remain where they preserve behavior;
this is still a decompilation, not a new engine.

Most of the game runs fine, but it is still under heavy testing and may have
crashes that have not yet been found.

The project has no official Discord server or subreddit. Please use the repository's Issues and Pull Requests for collaboration.

## Running

Launch `uprising.exe` on Windows, `uprising` on Linux or `Uprising.app` on macOS;
no flags are required. Local Windows x64 builds use `uprising64.exe`.
Keep it beside `uprising.cln`, `FONTS/`, `GRIDS/`, `Music/` and `VIDEO/`, using the
complete fonts supplied by this project. Assets resolve beside the executable,
regardless of the launch directory. The macOS app contains its own assets.

Target platforms: Windows x86/x64/ARM64, macOS ARM64, and Linux x64/ARM64.
CI builds native runtime packages for each target; see [Building](docs/BUILDING.md).
macOS users may need to change some game keybindings in Options to avoid
conflicts with macOS system shortcuts.

The first launch is windowed and centered on the primary monitor. Settings,
keyboard/controller bindings and music preferences use `CONFIG/settings.ini`.
On macOS, user data lives in `~/uprising/`, including
`~/uprising/CONFIG/settings.ini`, so replacing the app preserves it.
Legacy `FONTS/settings.cfg` and `FONTS/keymap.cfg` are imported on first use when
present. Saves and screenshots are separate user data. Screenshots are saved as
PNG files in `scrshts/` under the user-data directory.

Options → Graphics → **ADVANCED** provides:

- **Display mode:** windowed or desktop fullscreen. The game stays 4:3 with black
  borders on wide monitors; windowed resolutions are 4:3 only.
- **FSR 3.1 (3D):** Off, Upscale or Native AA, with adjustable sharpening.
  Available on supported 64-bit Vulkan devices; 32-bit builds use native Vulkan.
  FSR processes the world separately from HUD and menus.
  Menus and HUD use automatic spatial filtering at fractional display scales;
  integer scaling keeps the original pixel edges crisp.
- **3D anti-aliasing:** Edge AA when FSR is off; temporal AA when FSR is enabled.
  HUD, text, menus and 2D artwork remain outside the AA pass.

OpenGL comparator builds retain FSR1 and supported MSAA 2×/4×/8× controls.

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
