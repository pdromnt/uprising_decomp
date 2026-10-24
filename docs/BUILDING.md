# Building

Targets: Windows x64/ARM64, Linux x64/ARM64 and macOS ARM64. All builds use
Vulkan; FSR3 is optional at runtime. A 64-bit compiler is required. CI builds,
checks and packages all five targets; successful builds do not prove full gameplay.

## Windows

Use MinGW-w64 GCC/G++, windres, CMake 3.24+, Ninja and glslang on PATH:

```powershell
./source/scripts/build.ps1 -Tests
```

`source/scripts/build.bat` forwards arguments. The default local output is
`uprising64.exe` beside the existing assets. `-OutputDirectory` selects staging;
`-ExecutableName uprising.exe` selects the package basename. The script preserves
the previous executable and checks native renderer identity/execution symbols.

Windows ARM64 uses Visual Studio with the ARM64 C++ tools, Windows SDK and glslang:

```powershell
cmake -S source -B source/build/native -A ARM64 -DBUILD_TESTING=ON -DUPRISING_EXECUTABLE_NAME=uprising
cmake --build source/build/native --config Release --parallel 2
ctest --test-dir source/build/native -C Release --output-on-failure
```

## Linux and macOS

Use C11/C++17 compilers, CMake 3.24+, Ninja and glslang. Linux needs Vulkan drivers
and SDL's X11/Wayland/audio/input development dependencies; CI lists the packages.
OpenGL development libraries are not required.

Apple Silicon requires macOS26+ and KosmicKrisp. The pinned SDK setup is:

```sh
bash source/scripts/macos-vulkan-sdk.sh "$PWD/source/build/vulkan-sdk"
export VULKAN_SDK="$PWD/source/build/vulkan-sdk/macOS"
```

Build using the same commands on Linux and macOS:

```sh
cmake -S source -B source/build/native -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build source/build/native --parallel 2
ctest --test-dir source/build/native --output-on-failure
```

Linux produces `uprising`; macOS produces `Uprising.app`. Local builds share the
existing assets. Populate a self-contained runtime only when packaging:

```sh
cmake --install source/build/native --config Release --prefix source/build/stage --component Runtime
```

The macOS bundle includes its Vulkan loader/ICD and dependencies. Assets live in
`Contents/Resources`; saves/settings live in `~/uprising/`, surviving app updates.

## Checks and dependencies

`BUILD_TESTING=ON` builds core/config/audio checks plus `bin/uprising_vulkan_probe`
in the same cache, sharing SDL and SDK objects with the game. Run the probe directly;
`--validation` requires Khronos layers. Linux CI uses Mesa under Xvfb. A focused
probe-only configuration remains available through `UPRISING_VULKAN_PROBE_ONLY=ON`.

CMake fetches checksum-pinned SDL, FreeType, ENet, Vulkan headers, AMD SDK and
SPIRV-Reflect. SDL/FreeType/ENet link statically; libsmacker and stb_vorbis are
bundled source. No original executable, legacy support DLL or OpenGL runtime is
required. See [Graphics](FSR3.md) for GPU requirements and shader tuning.
FreeType downloads have alternate mirrors with the same pinned archive checksum;
this library rasterizes the supplied TTF fonts, rather than replacing them.

Normal launches are quiet about diagnostics. The Windows manual debugger launcher
is `source/scripts/debug-game.bat`; it uses normal menus, windowed controls and
isolated user data. Its overwritten, bounded logs live in `runtime-logs/debug-game`.
