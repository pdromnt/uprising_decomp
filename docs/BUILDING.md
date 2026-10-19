# Building

The current build needs Windows, MinGW-w64 GCC/G++, windres, CMake 3.24+ and
Ninja. MSVC, Linux and macOS builds are not supported yet.

From the game root:

```powershell
./source/scripts/build.ps1 -Bits 32 -Tests
./source/scripts/build.ps1 -Bits 64 -Tests
```

Run local architectures sequentially: their dependency source caches are shared.
`source\scripts\build.bat` forwards the same arguments, preferring PowerShell 7.
Use `-Compiler` to choose GCC, `-OutputDirectory` for staging, and `-?` for help.
The script finds tools on PATH, with local TDM/Visual Studio fallbacks.

Outputs are `uprising.exe` (x86) and `uprising64.exe` (x64). Builds are incremental,
use at most four compiler processes and share assets without copying them.
Per-architecture caches live under `source/build/windows-32` and `windows-64`.
The script refuses to replace an original/unknown EXE, retains the previous
output, and checks the linked execution symbols and renderer identity. `-Tests`
runs the asset-free core/config/audio checks; it does not establish gameplay parity.

CMake fetches checksum-pinned SDL 3.4.16, FreeType 2.14.3 and ENet 1.3.18 once.
They link statically; bundled libsmacker, stb_vorbis and FSR shader sources need
no extra runtime DLLs. The Windows resource script is compiled during the build.
Do not commit generated resources or binaries.

Keep `uprising.cln`, `FONTS/`, `GRIDS/`, `VIDEO/` and `Music/` beside the EXE.
Include the complete supplied fonts and licenses. No original EXE or legacy
Glide/DirectDraw/Smacker DLL/VXD is required.

For diagnostics, `UPRISING_RECOMP_NATIVE_OBSERVE=1` enables a log replaced on each
launch; normal launches create no log. `UPRISING_USER_DIRECTORY` selects isolated
user data and enables diagnostic logging. Direct CMake builds use Ninja/GCC,
`-m32`/`-m64`, windres and `UPRISING_ASSET_DIRECTORY`; `BUILD_TESTING=ON` enables CTest.

## Releases

Tag the desired revision, then manually run **Build release** for that existing
tag. The workflow builds both architectures and attaches their runtime ZIPs.
Each package names its EXE `uprising.exe` and includes assets/component notices;
source, settings, saves and build caches are excluded. GitHub provides the tagged
source archives. Ordinary main/PR workflows upload the two EXEs separately.

`source/scripts/package-release.ps1` performs packaging. Preserve asset and
third-party terms in [Third-party notices](../THIRD_PARTY_NOTICES.md).
