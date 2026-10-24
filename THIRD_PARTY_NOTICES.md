# Third-party material

SDL 3.4.16 is fetched from its official release and linked statically under the
zlib license. Its license is preserved in `source/third_party/SDL3-LICENSE.txt`
and included in binary packages.

The pinned SDL source receives a marked Xbox Series BLE compatibility patch,
adapted from TOU: Bluetooth VID/PID `045e:0b13` endpoints use SDL's existing Xbox
HIDAPI parser. Wired XGIP proxy endpoints retain their original filtering.
SDL's upstream license applies to the modified driver.

ENet 1.3.18 (upstream tag `v1.3.18`, commit
`2662c0de09e36f2a2030ccc2c528a3e4c9e8138a`) is fetched from lsalzman/enet and
linked statically under the MIT license. Its license is preserved in
`source/third_party/enet-LICENSE.txt` and included in binary packages.

Portions of this software are copyright © 2026 The FreeType Project
(https://freetype.org/). All rights reserved. FreeType 2.14.3 is linked under
the FreeType License, reproduced in `source/third_party/FreeType-FTL.txt` and
binary packages.

`FONTS/LiberationSans-Regular.ttf` is the unmodified Liberation Fonts 2.1.5
Arial-compatible font. Its SIL Open Font License is in
`FONTS/LICENSE-Liberation.txt`. The original Neuropol and Lauck Linear fonts
retain their separate ownership.

`source/third_party/libsmacker/` retains its upstream README, UPSTREAM and COPYING
(GNU Lesser General Public License 2.1). Preserve those files and comply with
its terms when distributing linked binaries, including the corresponding source
and a way to relink with a modified library where required.

`source/resources/uprising.ico` was extracted from the original Uprising executable for
application identification. It is original game artwork, not project-authored
art. Its presence does not grant redistribution permission.

stb_vorbis 1.22 by Sean Barrett is bundled unmodified from nothings/stb commit
`2c980bb59875b0d32144a71867fbdebb2f77cd20`. It is used under the MIT option of
its dual MIT/public-domain license. See `source/third_party/stb-LICENSE.txt`,
also included in binary packages.

Original game data and historical TXT documentation retain their separate
ownership and licenses.

The Vulkan renderer and developer probe use AMD FidelityFX SDK
v1.1.4 host/backend/shader sources at revision `c6efa6bf7f2027b3ec94f28578bb5965eabb9e55`,
downloaded into the build cache with per-file SHA256 checks. Their MIT license is
in `source/third_party/FSR3-LICENSE.txt`. Vulkan headers v1.3.296 come from Khronos,
under their upstream Apache-2.0/MIT terms. Game and developer probe artifacts include these notices. Derived build files
adapt memory selection/alignment and trusted stationary accumulation; downloaded
SDK files remain checksum-pinned. The SDK shares RCAS helpers with its earlier FSR
code; the game has no FSR1 renderer or upscaling mode.

The upscaler uses Khronos SPIRV-Reflect, tag
`vulkan-sdk-1.3.296.0`, for metadata from its compiled shaders. Copyright
2017-2022 Google Inc.; Apache-2.0, in `source/third_party/SPIRV-Reflect-LICENSE.txt`.
Its bundled SPIR-V header retains the Khronos MIT notice. Files are downloaded
with per-file SHA256 checks and stay in the build cache.

macOS game bundles include the Khronos Vulkan Loader and
Mesa KosmicKrisp from LunarG SDK1.4.363.0, plus their non-system dynamic-library
dependencies. SDK license files are retained under `Vulkan-Licenses` in the app's
Resources directory. Vulkan header, AMD SDK and SPIRV-Reflect notices are included
alongside the other runtime notices.
Matching loader licenses are from tag `vulkan-sdk-1.4.363`; Mesa's license set and
licensing guide are from SDK tag `vulkan-sdk-1.4.363.0`, revision
`269a955c898df283fc36f4827643769068cf3d29` in `aitor/mesa` on freedesktop.org.
Copies live in `source/third_party/vulkan-loader-licenses` and
`source/third_party/kosmickrisp-licenses`.
