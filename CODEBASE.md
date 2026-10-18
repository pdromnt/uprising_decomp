# Codebase

The repository root contains documentation, CI and game data. C sources and build
scripts live in `source/`; builds emit EXEs beside the assets.

```text
source/
  core/          Native algorithms, record owners and source-defined initial data
  game/          Application, menus, level simulation, weapons and presentation
  platform/      Windows services, input, audio, OpenGL and networking
  resources/     Application icon, version information and Windows resource script
  third_party/   libsmacker and its license/provenance
  cmake/         Explicit shipping source manifest
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
