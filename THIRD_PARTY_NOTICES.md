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

AMD FidelityFX Super Resolution 1 shader headers are bundled unmodified from
GPUOpen-Effects/FidelityFX-FSR revision `a21ffb8f6c13233ba336352bdff293894c706575`.
Copyright (c) 2021 Advanced Micro Devices, Inc. Used under the MIT license in
`source/third_party/fsr/LICENSE.txt`, included as `FSR-LICENSE.txt` in packages.
