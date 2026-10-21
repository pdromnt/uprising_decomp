# Vulkan / FSR3 experiment

Branch `FSR3`, based on stable main `c620bc4b` (0.2.0). Main and its releases
remain unchanged. Local root executables now contain this branch's
Vulkan candidate for manual testing. Vulkan rendering is native C; the optional AMD
FSR3.1 upscaler host is C++ behind a small C interface. No frame generation.
**32-bit builds are Vulkan-only by design; FSR3 is exclusive to64-bit builds.**

## Graphics controls

Advanced Graphics offers4:3 window sizes and desktop fullscreen with black borders.
Vulkan FSR modes are **OFF / UPSCALE / NATIVE AA**, with sharpening0-100%.
The scene source remains640x480, or the actual O-monitor crop. UPSCALE reconstructs
to the drawable's fitted4:3 canvas (minimum640x480). NATIVE AA reconstructs at the
source size and scales that result into the fitted output. These are fixed-source
modes, not AMD Quality/Balanced/Performance presets. HUD is drawn afterward.

FSR provides temporal AA. With it off, **EDGE AA** filters diagonal world edges
without filtering menus, HUD or pre-scene artwork. It supports32-bit Vulkan too.
This is a spatial filter, not hardware MSAA. Vulkan settings `display.fsr3`
(0/1/2), `display.fsr3_sharpness` and `display.vulkan_aa` are independent of
OpenGL's `display.fsr1` and `display.msaa_samples`. Apply/Keep/Revert and persistence
cover all options. Changing SDK input/output dimensions recreates its context.

## Renderer contract

- Native projected triangles/points/lines preserve legacy texture, chroma/alpha,
  fog, depth, palette/NCC and draw-order semantics. Texture owners and bounded
  content-checked32x32 HUD atlases retain immutable submitted versions. Atlas
  filtering repeats/clamps inside each original tile, without neighbor bleeding.
- Owned color/depth/motion/scene attachments separate world rendering from UI.
  Depth is reversed-infinite `10/z`; motion is unjittered previous-minus-current
  pixel displacement. Terrain/model allocation identities and clipped-edge
  provenance retain correspondence; missing/ambiguous identities reject history.
- Opaque base skies have texture-space motion at infinite depth. Independent
  layers, unknown geometry and surviving translucent coverage conservatively
  reject history. Invisible blended no-write effects preserve underlying history.
  Reprojected raw rejection coverage protects vacated sprite/effect pixels for
  one following frame; it expires rather than feeding the combined mask back.
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

Configure the game with `-DUPRISING_EXPERIMENTAL_VULKAN=ON`. Keep output under
`source/build/vulkan-game/bin`, away from the root release executables. Requires
C11/C++17 and glslang/glslangValidator (`UPRISING_GLSLANG` overrides the compiler).
Vulkan-enabled builds and all Vulkan CI game artifacts start in Vulkan without
flags or environment settings. Open **Options > Graphics > Advanced** and select
**FSR 3.1 (3D): UPSCALE** or **NATIVE AA**, then **Apply > Keep**. The selection
persists between launches. FSR starts off on a fresh configuration. 32-bit builds
show **64-BIT ONLY**; unsupported 64-bit GPUs show **GPU UNSUPPORTED**.
`UPRISING_RENDERER=opengl` retains the developer comparator. `UPRISING_VULKAN_FSR3=0/1` can
force startup off/upscale; subsequent UI changes still work.
Use existing assets via `UPRISING_ASSET_DIRECTORY` and isolate tests through
`UPRISING_USER_DIRECTORY`; no asset copies are needed.

`-DUPRISING_VULKAN_PROBE_ONLY=ON` builds `bin/uprising_vulkan_probe` separately.
Its hidden window captures no input and plays no audio. `--validation` requires
Khronos validation layers and fails if absent.32-bit probes check native Vulkan;
64-bit probes also dispatch the actual AMD SDK. Sources are checksum-pinned to
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
settings, before restoring/signing the game. This checks library loading, not
native Apple GPU rendering. Physical Apple GPU execution remains unverified.

Standalone developer probes can use:

```sh
export VULKAN_SDK="$HOME/VulkanSDK/1.4.363.0/macOS"
export DYLD_LIBRARY_PATH="$VULKAN_SDK/lib"
export VK_DRIVER_FILES="$VULKAN_SDK/share/vulkan/icd.d/libkosmickrisp_icd.json"
./uprising_vulkan_probe
```

## Current evidence and remaining work

Local Windows x86/x64 application builds and GPU probes pass. Numeric fixtures
check depth/motion/jitter, transparency masks, sky inputs, fractional HUD atlas
sampling, irregular monitor composition and world-only spatial AA. The64-bit
SDK moving fixture checks disocclusion/effect removal over10frames. Gameplay movement/fire/save/reload/shutdown passes1740frames on both x86 Vulkan
and x64 FSR with deferred submission; restored position/ammo match exactly.
Priva's actual particle-mask, claim/O-return/citadel-camera route passes1479frames.
The current120-frame Erudara route checks resize, camera resets, native AA,
sharpening, spatial tank/O AA, SDK retirement/recreation and clean shutdown.
Advanced-modal click routing/Apply/Revert/persistence passed; its capture was inspected.
These fixtures do not establish complete temporal image-quality acceptance.
Local Khronos layers are unavailable; Linux software-driver CI supplies validation.
Six-target CI39366845037 passed for d852ce6a, including deferred-queue checks,
Linux x64/ARM64 validation, the SDK-free macOS bundle-loader check and signed DMG.
The GPU probe exercises both immediate and deferred submission, including the
64-command bound, readback, host-buffer reuse and destruction.

Training3 first-attack archive, seed1792110400, FSR640x480 ->960x720, normal fire,
windowed/muted/isolated. Sequential25s samples exclude the first5s:

| View | Previous polygon path | Current path |
| --- | --- | --- |
| Citadel, pitch200 |69.31FPS |96.07FPS |
| O monitor |100.96FPS |110.38FPS |

No measured-interval resets; these samples improved about39%/9% respectively
after removing interior fence waits. The earlier CPU-only change showed no FPS
gain. The citadel capture
looks at terrain/walls, not a dense battle; severe crowded-view slowdown remains
unresolved. Observe-only preparation/scene/flush/reconstruction wall times can
overlap and are not exclusive GPU timings. With deferred submission, recording
scopes exclude GPU completion; compare full present intervals for performance.
Short profiles are guard-terminated;
clean shutdown is checked separately. A speculative arena-access change was
dropped after timings failed to justify it.

Reported infantry/target-marker trails reproduced a vacated-coverage gap in the
GPU fixture. The one-frame reprojection fix passes rejection/expiry and game
resize/view checks; visual confirmation in the reported combat scene is pending.

Remaining: real crowded-target performance, broad moving-edge/particle/layered-sky
quality and scripted-camera/alternate-monitor coverage, native Apple GPU execution,
and hardware coverage beyond the initial RTX4060Ti.32-bit FSR and hardware Vulkan
MSAA are not acceptance blockers. Keep the OpenGL release until Vulkan acceptance.

References: [AMD integration](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-interpolation/),
[pinned SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/tree/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55),
[Vulkan rasterization](https://docs.vulkan.org/spec/latest/chapters/primsrast.html),
[KosmicKrisp](https://docs.mesa3d.org/drivers/kosmickrisp.html),
[LunarG macOS integration](https://vulkan.lunarg.com/doc/view/latest/mac/getting_started.html).
