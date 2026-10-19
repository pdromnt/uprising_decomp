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

SDL handles the application entry, executable base path, dialogs, window bounds,
icons, directory enumeration and file replacement/removal. Registry/OS-version
and CD-drive probes are retired; movies use packaged `VIDEO/` assets. Icon pixels
are embedded from the existing ICO; the Windows executable resource is retained.
Settings flush to disk before same-directory replacement. Small conditional CRT
adapters retain working-directory setup and durable FILE flushing; a Windows
hidden/system-attribute query preserves the original save/level-list exclusions.
The HWND property accessor remains only for existing Windows debugger fixtures.

CMake lists all shipping translation units explicitly. CPU/generated instruction
execution and oracle sources are absent from the build.
Unused guest-stack/CRT scaffolding and teardown dispatch metadata are retired;
the remaining runtime helpers serve actual native callers.

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
Input no longer publishes unused synthetic device identities.
Windows discovery opts into GameInput/WGI where SDL was built with those drivers;
the current MinGW build uses HIDAPI/XInput. A narrow build-time SDL patch
allows Xbox Series BLE (`045e:0b13`) Bluetooth endpoints through its Xbox HIDAPI
filter; wired proxy handling stays intact. Physical Bluetooth acceptance is pending.

`platform/sound_platform.c` mixes owned PCM clips with independent voice cursors
through SDL3. Duplicate voices share immutable clips. `portable_music.c` streams
OGG with stb_vorbis and reports completion to the existing playlist on the main
thread. App-local music/effects gains replace system volume manipulation.
DirectSound, MCI/AUX/WAVE audio calls and the native SDL message hook are removed.
Menus read and set the native music/mixer gains directly; obsolete discovery,
system-volume snapshots and restore wrappers are removed.

The legacy I-FORCE SDK/input/effect route is removed; SDL joystick and haptic
ownership is independent.

`platform/portable_network.c` owns ENet sessions, SDL worker/mutex, bounded copied
receive/outgoing queues and UDP discovery. Game packets and roster/clock consumers
retain their byte layouts and IDs. A versioned envelope carries session/peer IDs;
normal traffic goes through the elected host, while mesh links support migration.
Admission policy follows the session to its successor. No COM/DirectPlay objects
or callbacks remain. The TCP/IP menu row retains its stable selection ID; obsolete
serial/modem/IPX providers are not advertised. LAN discovery uses UDP 2303 and the
initial host uses UDP 2302; migrated hosts/other peers use their bound UDP ports.
`UPRISING_RECOMP_NETWORK_ADDRESS` can direct discovery to a hostname/IPv4 address.
The optional `UPRISING_NETWORK_LOBBY=host:name` launch intent hosts a native session;
the old external DirectPlay lobby ABI is retired. Neither is required for menu play.
Next: stabilization and manual acceptance. Other platforms remain unvalidated.

`platform/portable_display.c` owns the advanced graphics modal and display options
in the shared CONFIG file. It previews mode changes with a monotonic rollback
deadline; rejected modes never become persisted preferences. Window output and
mouse mapping use the same aspect-preserved viewport. Front-end graphics controls
retain their existing native consumers. The modal uses native-size beveled metal
trim, a recessed terminal interior and existing fonts. Separate arrow hit areas
cycle values backward/forward; confirmation has a draining rollback bar.
SDL owns desktop fullscreen transitions; the display mode stays intact. Window
sizes are 4:3 only. Fullscreen fits the original aspect inside the desktop canvas
with black borders and reports its automatic output size in the modal.
Isolated automation rejects fullscreen and ignores saved display modes.

`portable_fsr.inc` wraps AMD's unmodified FSR1 EASU/RCAS GLSL in an optional OpenGL
4.3 compatibility context. CMake embeds the shader headers; no runtime shader
files or compiler DLLs are shipped. The original 640x480 render/depth/readback
buffers remain unchanged; FSR scales the complete gameplay frame (including HUD)
at presentation. Front-end menus use the same filter, with their top-down texture
orientation handled in the final pass. Programs and the reusable intermediate
target belong to the display owner and retire before context destruction.
The non-FSR path uses TOU's integer-prescale coordinate filter with linear
sampling, preserving pixel interiors at integer scales without uneven nearest
pixel widths at fractional scales. Its shader uses GLSL 1.20 independently of
FSR availability; shader failure retains linear fractional/nearest integer scaling.

The display owner persists `display.msaa_samples` and bounds selectable values
to device support. On level opening, the GL owner optionally allocates two
multisample color targets and a shared multisample depth/stencil target. The
world submission scope enables sample coverage, excluding the final 2D overlay
queue; UI and CPU glyph writes retain center-sample coverage. Color resolves into
the original single-sample textures for presentation/readback, preserving their
640x480 dimensions. Level teardown releases all multisample targets. AA changes
apply at the next level opening, independently of FSR's all-screen scaling.
