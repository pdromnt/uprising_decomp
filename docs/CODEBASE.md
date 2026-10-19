# Codebase

The root holds game data, README/notices and CI. Shipping code and build scripts
live in `source/`; build outputs go beside the assets.

```text
source/
  core/          Algorithms, record/storage owners and source-defined initial data
  game/          Application, menus, simulation, weapons and presentation
  platform/      SDL services, OpenGL renderer and small host adapters
  resources/     Icon, version information and Windows resource script
  third_party/   Bundled codecs/shaders and component licenses
  cmake/         Explicit source manifest and pinned dependencies
  scripts/       Incremental build and release packaging
  tests/         Asset-free native checks
docs/            Build, architecture and testing guidance
.github/         Build and release workflows
```

Paths below are relative to `source/`.

## Entry, state and storage

`platform/portable_main.c` enters `game/application_entry.c`.
`core/game_initial_data.c` constructs owned initial records from C declarations;
`game/campaign_catalog_data.inc` supplies campaign data. The source manifest in
`cmake/sources.cmake` excludes CPU/generated execution and oracle sources.

Legacy addresses are stable IDs or bounded offsets, never truncated native
pointers. Preserve explicit byte access, ownership and numeric behavior. Native
address-keyed callbacks take real arguments/returns, with no guest calling ABI.

`core/game_config.c` encodes the shared INI. Menu settings/bindings/music and the
display owner persist through `platform/portable_platform.c`, which owns loading,
durable flushing and atomic replacement. Configuration is a persistence snapshot,
not a second live simulation. Neutral `field_*` keys retain unrenamed options.
macOS keeps user data in `~/uprising/`; bundled assets stay in the app's Resources.
`core/host_file.c` adapts retail path separators and case-insensitive reads to
case-sensitive filesystems without copying or renaming the assets.

## Platform owners

| Owner | Responsibility |
| --- | --- |
| `portable_window.c` | SDL window/context, indexed menu surfaces, palette, pointer mapping and event delivery |
| `portable_display.c` | Advanced graphics modal, display preferences, 4:3 viewport and timed mode rollback |
| `portable_gl1.inc` | Fixed-function rendering, texture/FBO ownership, MSAA and CPU pixel access |
| `portable_gl1_sky.inc` | Layered BMP skies/horizon, fog sampling and citadel monitor masks |
| `portable_fsr.inc` | Embedded FSR1 EASU/RCAS and fractional pixel-art presentation filtering |
| `portable_font.c` | FreeType rasterization using the supplied fonts and preserved layout metrics |
| `portable_input_devices.c` | SDL input queues, scan-ID bindings, joystick samples and periodic haptics |
| `sound_platform.c` | SDL PCM mixer, shared immutable clips and independent voice cursors |
| `portable_music.c` / `portable_movie.c` | Streamed OGG playlist / libsmacker video and owned PCM playback |
| `portable_network.c` | ENet sessions, worker/queues, discovery and host routing/migration |

Gameplay borrows the window's GL context; teardown restores menu state. The
640x480 render/readback buffers remain independent of output resolution. FSR
scales every screen; its optional shaders require GL 4.3 compatibility. The
non-FSR pixel-art shader uses GLSL 1.20. Desktop fullscreen preserves the display
mode and fits the same 4:3 viewport used for pointer mapping.

MSAA targets belong to the level GL owner. Only world geometry enables sample
coverage; the final overlay queue and UI/glyph writes retain center-sample
coverage. Resolve multisample color before presentation or **any** CPU read,
including sky/fog and monitor-mask sampling. AA changes apply on level opening.

SDL owns startup/path/dialog services. Small host adapters cover cwd, durable
FILE flushing and Windows hidden/system attributes; the HWND accessor serves
Windows debugger fixtures. Movies resolve from `VIDEO/`; missing sky layers use
TempSky BMPs. Pinned SDL receives an Xbox Series BLE endpoint-filter patch.

ENet uses UDP 2303 discovery and 2302 for the initial host; other peers/migrated
hosts use their bound ports. `UPRISING_RECOMP_NETWORK_ADDRESS` directs discovery;
`UPRISING_NETWORK_LOBBY=host:name` optionally hosts a native session at launch.
Neither variable is needed for normal menu play.
