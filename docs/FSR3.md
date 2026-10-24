# Vulkan renderer and FSR3

Vulkan is the sole renderer. Supported targets are Windows x64/ARM64, macOS
ARM64 and Linux x64/ARM64. FSR3.1 is optional; there is no frame generation.
The renderer is native C; AMD's upscaler host is C++ behind a small C interface.
The `FSR3` branch is under acceptance; stable `main` remains unchanged.

## Graphics controls

Advanced Graphics offers4:3 window sizes and desktop fullscreen with black borders.
Vulkan FSR modes are **OFF / UPSCALE / NATIVE AA**, with sharpening0-100%.
The scene source remains640x480, or the actual O-monitor crop. UPSCALE reconstructs
to the drawable's fitted4:3 canvas (minimum640x480). NATIVE AA reconstructs at the
source size and scales that result into the fitted output. These are fixed-source
modes, not AMD Quality/Balanced/Performance presets. HUD is drawn afterward.
Menus/artwork use a separate spatial filter: fractional scales interpolate pixel
boundaries, while native/integer sizes retain exact pixels. HUD text uses padded
glyph atlases with filtered coverage; it never enters temporal history. Atlas
uploads share the draw submission. Sky quads explicitly select world sampling
instead of inheriting the HUD filter.

FSR provides temporal AA. With it off, **EDGE AA** filters diagonal world edges
without filtering menus, HUD or pre-scene artwork. This is a spatial filter, not hardware MSAA. Settings are `display.fsr3`
(0/1/2), `display.fsr3_sharpness` and `display.vulkan_aa`. Obsolete FSR1/MSAA
preferences are removed on load. Apply/Keep/Revert and persistence
cover all options. Changing SDK input/output dimensions recreates its context.

Common world/UI pipelines and the initial FSR3 resources are prepared before
the level loading page closes, without advancing gameplay. Uncommon draw states
and later camera/output changes can still require first-use work.
`CONFIG/vulkan-pipelines.bin` in the user-data directory caches world, UI and
FSR3 pipelines across launches. It is optional, capped at64MiB, checked against
the driver/device/UUID and a payload checksum, and replaced atomically only when
its contents change. Deleting it causes a cold start; it is not a game asset.

## Renderer contract

- Native projected triangles/points/lines preserve legacy texture, chroma/alpha,
  fog, depth, palette/NCC and draw-order semantics. Texture owners and bounded
  content-checked32x32 HUD atlases retain immutable submitted versions. Atlas
  filtering repeats/clamps inside each original tile, without neighbor bleeding.
- Owned color/depth/motion/scene attachments separate world rendering from UI.
  Depth is reversed-infinite `10/z`; motion is unjittered previous-minus-current
  pixel displacement. Terrain/model allocation identities and clipped-edge
  provenance retain correspondence; missing/ambiguous identities reject history.
- Base skies, clouds, horizons and monitor skies have independent texture-space
  motion at infinite depth. Cloud flow blends the tracked base/layer translations
  by actual opacity; remaining motion disagreement raises reactivity. Unknown
  geometry and untracked effects still reject history. Invisible blended
  no-write effects preserve underlying history.
  Reprojected raw rejection coverage protects vacated sprite/effect pixels for
  one following frame; it expires rather than feeding the combined mask back.
  Only the reactive input includes that expanded previous coverage. The
  transparency/composition input borrows the existing raw current-coverage image,
  so recently cleared sharp edges can resume accumulation. No extra allocation
  or full-image blur is introduced. Effect removal and separate-mask GPU checks
  pass; remaining subtle sharp-edge shimmer still needs manual confirmation.
- Trusted samples moving less than1/32source pixel retain8x the SDK's base
  accumulation weight. Disocclusion, reactive coverage, changing shading and new
  samples bypass this tuning; upstream rejection/rectification remain intact.
  A derived accumulation header preserves the checksum-pinned SDK downloads.
  This uses the existing history, without another image or spatial blur pass.
- O-view crops actual color/depth/motion and uses its own FOV. MRT coverage and
  retained artwork contribution preserve irregular monitor frames and holes.
  Screenshots and legacy pixel consumers retain their original dimensions.
- Camera/mode/projection changes, reload, output/resource changes, same-camera
  jumps over45degrees/2048world units and frame/time gaps reset history before
  submission. Smaller scripted discontinuities still need broader coverage.
- Polygon vertices are converted once per polygon; triangles retain order,
  culling and per-triangle motion rejection. Scratch belongs to the renderer,
  avoiding large per-call stack probes. Adjacent batches reuse compatible draw
  state only inside one command buffer. Interior GPU submissions are queued without
  a per-submit fence; presentation retires them together. Host reads/writes and
  resource destruction drain the queue before reuse. The pending list is bounded
  to64 commands; this is intraframe overlap, not a multiframe render ring.

The selected SDK profile is LDR/FP32, native subgroup width, low-resolution
motion and reversed-infinite depth. Other permutations fail explicitly rather
than falling back. Device requirements include Vulkan1.2 graphics/compute/present,
independent blending and the required R32F/RGBA32F attachment/storage formats.

## Build and run

Vulkan and the SDK are built by default. Keep development output under
`source/build/vulkan-game/bin`, away from the root release executables. Requires
C11/C++17 and glslang/glslangValidator (`UPRISING_GLSLANG` overrides the compiler).
All builds start in Vulkan without
flags or environment settings. Open **Options > Graphics > Advanced** and select
**FSR 3.1 (3D): UPSCALE** or **NATIVE AA**, then **Apply > Keep**. The selection
persists between launches. FSR starts off on a fresh configuration; devices
lacking upscaler features show **GPU UNSUPPORTED** while retaining native Vulkan.
`UPRISING_VULKAN_FSR3=0/1` can force startup off/upscale for diagnostics.
Use existing assets via `UPRISING_ASSET_DIRECTORY` and isolate tests through
`UPRISING_USER_DIRECTORY`; no asset copies are needed.

`-DUPRISING_VULKAN_PROBE_ONLY=ON` builds `bin/uprising_vulkan_probe` separately.
Its hidden window captures no input and plays no audio. `--validation` requires
Khronos validation layers and fails if absent. Probes dispatch the actual AMD SDK. Sources are checksum-pinned to
SDKv1.1.4 (`c6efa6bf7f2027b3ec94f28578bb5965eabb9e55`), Vulkan headersv1.3.296 and
SPIRV-Reflect SDK1.3.296. Derived backend fixes retain original downloads in cache.

## macOS runtime

Apple Silicon/macOS26+ uses KosmicKrisp and the Vulkan Loader, not MoltenVK.
Build with `UPRISING_VULKAN_SDK`/`VULKAN_SDK` pointing at SDK1.4.363.0's `macOS`
directory. `source/scripts/macos-vulkan-sdk.sh` installs that pinned SDK for CI
without system-wide installation. Runtime install bundles loader, ICD, transitive
non-system dylibs and matching upstream licenses. Library IDs are fixed before
ad-hoc signing. The DMG contains the app and original README.TXT outside it.

The app derives paths from SDL's real bundle base (normally Contents/Resources),
sets its own loader/ICD before loading Vulkan, and needs no SDK environment.
It does not unconditionally request Apple's portability-enumeration extension,
which KosmicKrisp does not support. The CI bundle-loader check temporarily runs
the existing probe under the app's executable identity, without SDK/driver shell
settings, before restoring/signing the game. This checks library loading.
User testing on physical Apple Silicon confirmed gameplay and FSR working;
the first loading-screen-to-game transition was slower and remains unprofiled.

Standalone developer probes can use:

```sh
export VULKAN_SDK="$HOME/VulkanSDK/1.4.363.0/macOS"
export DYLD_LIBRARY_PATH="$VULKAN_SDK/lib"
export VK_DRIVER_FILES="$VULKAN_SDK/share/vulkan/icd.d/libkosmickrisp_icd.json"
./uprising_vulkan_probe
```

## Current evidence and remaining work

Local Windows x64 core/config/audio and Vulkan GPU checks pass. The native
Training1 movement/fire/save/reload/shutdown route passes1740frames with exact
restored position/ammo. Erudara's120-frame route checks camera cuts, output resize,
monitor crops, native AA, sharpening and context recreation. Advanced-modal
clicks/Apply/Keep/Revert/persistence pass and its filtered capture was inspected.
The binary imports neither OpenGL nor a CPU/x87 execution runtime.

Matched quiet Training1/FSR960x720 samples, excluding the first5seconds, measured
133.81FPS before renderer cleanup and135.73FPS after. This is not crowded-battle
performance evidence. Timing samples are guard-terminated; clean shutdown is
checked separately. CI builds five64-bit targets in one cache per job, executes
Linux Vulkan validation and packages the macOS loader/ICD with its app.

User testing confirms broad sky jitter and soldier occlusion are improved/fixed,
with residual shimmer on sharp edges. Zero sharpening and Native AA did not
remove that shimmer. The stronger GPU fixture uses the actual Halton sequence
at native/1.5x/2x sizes and checks individual pixel pulses as well as edge position.
At1.5x, trusted stationary accumulation reduces the fixture's mean pulse from
4.82 to1.19/255 and its worst pixel from38 to11/255. Movement, effect removal,
disocclusion and an abrupt unreset colour change pass. Manual testing reports
improvement and no instability in the preceding candidate. The latest cleanup
and pixel-centre reprojection correction still need ordinary play confirmation.

macOS and Linux are user-confirmed working; CM4 performance is poor and deferred.
Remaining coverage includes Windows ARM64, additional GPUs/controllers, crowded
views, moving edges/animated textures and longer campaign/multiplayer sessions.
The unexpected Novus combat exit remains unreproduced; the manual debugger
launcher retains failure stacks. Transition fixtures deliberately reset history
and switch modes; do not use them as steady visual-quality comparisons.

References: [AMD integration](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-interpolation/),
[pinned SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/tree/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55),
[Vulkan rasterization](https://docs.vulkan.org/spec/latest/chapters/primsrast.html),
[KosmicKrisp](https://docs.mesa3d.org/drivers/kosmickrisp.html),
[LunarG macOS integration](https://vulkan.lunarg.com/doc/view/latest/mac/getting_started.html).
