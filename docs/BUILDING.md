# Building

Use Windows, MinGW-w64 GCC (with the desired multilib target), windres, CMake
3.24+ and Ninja. `source/scripts/build.ps1` finds tools on PATH, or GCC at the existing
local TDM installation and CMake/Ninja from Visual Studio. Set `-Compiler` to
override GCC. MSVC and non-Windows backends are not currently supported.

The batch shortcut is `source\scripts\build.bat`; it forwards the same arguments
to `build.ps1`, preferring PowerShell 7 when installed. Pass `-?` to view help.
CI calls the PowerShell script directly.

```powershell
./source/scripts/build.ps1 -Bits 32 -Tests
./source/scripts/build.ps1 -Bits 64 -OutputDirectory ./source/build/stage64 -Tests
```

Local builds use uprising.exe (x86) and uprising64.exe (x64). CI passes `-ExecutableName uprising.exe` for each separate architecture package.

The build is incremental, uses at most four compiler processes, and does not copy
assets. Per-architecture caches are in `source/build/windows-32` and `windows-64`.
The `.rc` is compiled to `uprising.res` and a matching COFF object before linking.
Do not commit generated resources or binaries.

For direct CMake use, configure a Ninja build with GCC, `-m32`/`-m64`, windres and
`UPRISING_ASSET_DIRECTORY` set to the output directory. `BUILD_TESTING=ON` enables
CTest. The build script also checks the linked symbols and native OpenGL identity.

Required assets beside the output are `uprising.cln`, `FONTS/`, `GRIDS/`, `VIDEO/`
and `Music/`. No original EXE, Glide, DirectDraw proxy, Smacker DLL or VXD is a
shipping dependency. Music currently retains a separate Windows CD/OGG adapter;
see the README. Do not package retail assets without permission.

`source/scripts/package-release.ps1` packages the EXE, game data, notices and x86
music adapter. It excludes source code, build output, user settings and saves.
Each architecture package contains `uprising.exe`.

For a release, push the desired tag, then open Actions → Build release → Run
workflow and enter that existing tag. Both architecture checks must pass before
the workflow creates/updates the GitHub release and attaches its ZIP files.
The tag identifies the corresponding source through GitHub's source archives.
Ordinary main/PR builds check both architectures and upload just their EXEs.
