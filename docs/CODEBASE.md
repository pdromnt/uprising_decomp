# Codebase

Project notes live in `docs/`; the root contains the README, CI and game data.
C sources and build scripts live in `source/`; builds emit EXEs beside the assets.

```text
source/
  core/          Native algorithms, record owners and source-defined initial data
  game/          Application, menus, level simulation, weapons and presentation
  platform/      SDL window/events/OpenGL, remaining Windows services
  resources/     Application icon, version information and Windows resource script
  third_party/   libsmacker and third-party licenses
  cmake/         Shipping source manifest and pinned SDL dependency
  scripts/       Incremental build and release packaging
  tests/         Asset-free native checks
docs/            Build and manual-validation instructions
.github/         Separate x86/x64 CI packages
```

Paths below are relative to `source/`.
`platform/portable_main.c` enters the real application. `core/game_initial_data.c`
constructs owned initial records from C declarations; it never loads an EXE.
`game/application_entry.c` selects application routes. Platform services cross
explicit C interfaces; changing those backends does not require CPU fallback.

Legacy addresses are stable IDs or bounded record offsets. They are not native
pointers. Keep pointer widths, explicit little-endian access, ownership and
original numeric behavior intact. Native address-keyed callbacks take real
arguments and returns. Do not introduce instruction execution or guest calling
conventions to fill a missing function.

`core/game_config.c` is the portable INI codec. `game/menu_settings.c`,
`menu_bindings.c` and `menu_music.c` persist their existing state through it;
`platform/portable_platform.c` owns loading and atomic file replacement. Neutral
`field_*` option names preserve unrenamed values. Configuration is a persistence
snapshot, not another live simulation copy. The campaign catalog is compiled C
content in `game/campaign_catalog_data.inc`.

CMake lists all shipping translation units explicitly. CPU/generated instruction
execution and oracle sources are absent from the build.

The OpenGL device in `platform/portable_gl1.inc` is called directly, without a
Glide SDK function table or fallback loader. `core/render_texture_cache.c` uses
texture identities instead of simulated VRAM addresses and eviction spans.
Texture owners release their device resources explicitly. Original pixel formats,
depth arithmetic and bounded stream-cache records remain for visual parity.

SDL3 owns the windows, indexed menu surfaces/palette, OpenGL context, swap and
shared monotonic clock. Menus upload their front pixels through the same context
borrowed by gameplay; level teardown restores its saved GL state. There is no
DirectDraw loader or GDI text/surface path. `platform/portable_font.c` uses
FreeType with the two original fonts and Liberation Sans in place of Arial;
verified advances/line metrics retain existing layout. Glyph rasterization may
differ. `core/ui_geometry.c` owns bounded keyed/mirrored indexed copies.
Missing mission sky images use the TempSky BMP layers, including fog setup;
surface initialization also uses the BMP and no longer reads `tempsky.tga`.

`platform/portable_input_devices.c` owns SDL keyboard/mouse queues, saved scan-ID
translation, fractional relative motion, joystick sampling and periodic haptics.
Saved bindings and joystick ranges remain stable. Focus loss publishes releases;
the game loop consumes input and advances simulation in the same iteration.
The former DirectInput service and Windows keyboard/focus hooks are removed.

`platform/sound_platform.c` mixes owned PCM clips with independent voice cursors
through SDL3. Duplicate voices share immutable clips. `portable_music.c` streams
OGG with stb_vorbis and reports completion to the existing playlist on the main
thread. App-local music/effects gains replace system volume manipulation.
DirectSound, MCI/AUX/WAVE audio calls and the native SDL message hook are removed.

The legacy I-FORCE SDK/input/effect route is removed; SDL joystick and haptic
ownership is independent. DirectPlay still uses Windows services.
Next: network transport and remaining Windows services.
