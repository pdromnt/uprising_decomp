# Building

Targets: Windows x86/x64/ARM64, Linux x64/ARM64 and macOS ARM64. Native CI builds
and checks each target; a successful build does not establish gameplay parity.

## Windows

For the existing MinGW-w64 GCC/G++, windres, CMake 3.24+ and Ninja toolchain:

```powershell
./source/scripts/build.ps1 -Bits 32 -Tests
./source/scripts/build.ps1 -Bits 64 -Tests
```

Run local architectures sequentially; dependency sources are shared. The batch
shortcut forwards these arguments. Outputs are `uprising.exe` and `uprising64.exe`
beside the assets. The script retains the previous EXE and checks native renderer
identity and execution symbols. `-OutputDirectory` selects staging.

Windows ARM64 uses Visual Studio 2022 with the ARM64 C++ tools and Windows SDK:

```powershell
cmake -S source -B source/build/native -A ARM64 -DBUILD_TESTING=ON -DUPRISING_EXECUTABLE_NAME=uprising
cmake --build source/build/native --config Release --parallel 2
ctest --test-dir source/build/native -C Release --output-on-failure
```

## Linux and macOS

Use a native C11 compiler, CMake 3.24+ and Ninja. Linux also needs OpenGL development
headers and SDL platform dependencies (X11/Wayland, ALSA/PulseAudio and udev);
the CI workflow lists the packages. macOS needs Xcode command line tools and Ninja.

```sh
cmake -S source -B source/build/native -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build source/build/native --parallel 2
ctest --test-dir source/build/native --output-on-failure
```

Linux produces `uprising`; macOS produces `Uprising.app`. Ordinary local builds
do not copy the asset tree. To populate a self-contained runtime, install once:

```sh
cmake --install source/build/native --config Release --prefix source/build/stage --component Runtime
```

On macOS, SDL resolves bundled assets in `Contents/Resources`; user data goes to
`~/uprising/`. The base renderer uses legacy OpenGL 2.1. FSR requires OpenGL 4.3
compatibility and is unavailable on macOS; the default pixel-art scaling filter
remains available. Other features depend on the driver's exposed capabilities.

## Dependencies and diagnostics

CMake fetches checksum-pinned SDL 3.4.16, FreeType 2.14.3 and ENet 1.3.18. These
link statically; bundled libsmacker, stb_vorbis and FSR sources need no extra game
DLLs. System libraries/frameworks remain platform dependencies. Assets retain
original casing; the file adapter handles legacy separators and case-insensitive
reads on case-sensitive filesystems. No original EXE or legacy support DLL is used.

`BUILD_TESTING=ON` runs the existing core/config/audio checks. Normal launches
create no diagnostic log. `UPRISING_RECOMP_NATIVE_OBSERVE=1` enables a replaced log;
`UPRISING_USER_DIRECTORY` selects isolated user data and also enables logging.

## Windows debugger launcher

Double-click `source/scripts/debug-game.bat` for manual testing through the normal
menus. It defaults to the root `uprising64.exe`, uses GDB from PATH or
`C:/TDM-GCC-64/bin`, and requires GDB's Python support. Local builds with debug
symbols provide the most useful stacks. The game starts with sound in a 960x720 window
with physical controls enabled; FSR preferences and bindings come from your normal
settings. Use `debug-game.bat -Muted` to disable sound.

Logs are overwritten under `runtime-logs/debug-game/`; `debugger.log` captures
errors/stacks and is capped at 8 MiB. The separate game log and diagnostic saves
live under `runtime-logs/debug-game/user/`. Your normal saves/settings stay intact.
Copy failure logs before another run. Close any existing game first. No direct
`.lfl` argument is supplied, so mission endings retain their normal debrief flow.
`debug-game.ps1 -Check` verifies the setup without launching the game.

## CI and releases

Main/PR builds produce six runtime packages. Windows packages use `uprising.exe`
regardless of architecture; Linux uses `uprising`. macOS ships an ad-hoc-signed app
in a DMG, with the original `README.TXT` also outside the app. It is not notarized.
Windows archives are ZIP; Linux archives are tar.gz. Source, user configuration,
saves and private-lab content are excluded. Only installation's `Runtime` component
is packaged, so dependency development files stay out.

Tag the desired revision, then manually run **Build release** for that existing
tag. The release workflow resolves the tag's commit, reuses the same six-target
build and publishes only after every target succeeds. `package-release.ps1`
checks executable identity, native architecture and required runtime content.
Keep asset/component terms from [Third-party notices](../THIRD_PARTY_NOTICES.md).
