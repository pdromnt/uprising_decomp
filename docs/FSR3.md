# Vulkan / FSR3 experiment

Branch: `FSR3`, based on `c620bc4b`. `main` remains the release/bug-fix line.
The first playable target is Vulkan with FSR 3.1 temporal upscaling. Frame
generation is out of scope; the existing real frame rate is sufficient.
The game remains C. AMD's C++ host code is isolated behind a small C interface.

## Stages and acceptance

1. **Dependency proof.** Native SDL/Vulkan device, queues, explicit
   resource ownership/barriers, real AMD shader dispatch/readback and presentation.
   Probe every build architecture. Run with validation on actual drivers where
   available; Linux software-driver CI is functional evidence, not hardware FPS.
2. **Playable Vulkan rendering (current).** Retain OpenGL for comparisons. Replace immediate
   drawing with buffered submissions and shaders preserving palette/NCC textures,
   chroma/alpha, legacy W-depth, fog, layered skies, menus, videos and HUD order.
   Keep pixel-readback/glyph/mask consumers working before optimizing transfers.
   Accept menu -> level -> movement/fire -> reload -> shutdown and matched captures.
3. **Temporal inputs (in progress).** World reciprocal depth and previous rendered
   positions now produce vectors for terrain and stable owned-model vertices,
   including camera/object motion and owned clipping-edge provenance.
   Reversed-infinite perspective depth and opt-in scene jitter are connected.
   Scene-only color and jittered layered skies feed opt-in normal-view upscaling;
   HUD is rendered afterward at output resolution. Conservative GPU reactive/
   composition masks and cropped remote-monitor composition are connected.
   Complete particle/sky/discontinuity history and variable output sizes.
   Isolate HUD/cockpit/minimap. Reset history on camera cuts,
   mission reload, output-size changes and discontinuities; handle transparency
   and animated skies with reactive/composition masks.
4. **Temporal upscaling.** Integrate the full source-backed upscaler, native AA
   and quality controls; validate moving edges, disocclusion, transparencies and
   all tank/O/citadel views. Composite HUD/text at output resolution. Preserve
   4:3 presentation. Compare real frame time and image quality with FSR disabled.

## Dependency proof

Configure with `-DUPRISING_VULKAN_PROBE_ONLY=ON`, then build normally. Requires a
native C11/C++17 toolchain and `glslang` or `glslangValidator` (override with
`-DUPRISING_GLSLANG=/path/to/compiler`). Uses checksum-pinned Vulkan headers and
the selected AMD upscaler host/backend/shader closure and SPIRV-Reflect, not an entire SDK download. Generated files
stay in the build cache. No assets, settings, saves or playable EXEs are modified.

Run `bin/uprising_vulkan_probe` (`.exe` on Windows). It uses a hidden window,
creates a real swapchain and dispatches the unmodified SDK v1.1.4 FSR3 reactive
shader in FP32. It checks all 64 readback pixels against the expected 0.4330127.
`--show` briefly shows the result; `--validation` requires installed Khronos
validation layers and fails rather than silently skipping them. Run as a normal
console program. It captures no input and plays no sound.

On 64-bit targets the probe also builds the actual upstream context/pipelines and
dispatches three 64x64 ->128x128 frames, checking finite output, known flat color,
sharpening, reset and context recreation. The profile is LDR/FP32, native subgroup
width, reversed-infinite depth and low-resolution unjittered motion. Compiled
SPIR-V supplies binding metadata through SPIRV-Reflect; unsupported permutations
fail explicitly. The adapter resolves promoted memory-query entry points through
SDL's loader and exposes only enabled/promoted functionality to the SDK. The
frame-generation presenter callback is absent. Required storage-image features
are checked. C++ is used only inside this optional SDK integration.
The adapter also bounds descriptor reservations, supports unified device memory,
and aligns every SDK scratch region to its declared 32-byte requirement. Unix
uses bounded CRT helpers and a larger opaque context for native four-byte
`wchar_t`; the SDK's size assertion is preserved. Original downloads stay intact;
the build derives a backend file with the memory/layout corrections.

The upstream external-resource API stores Vulkan image handles in `void*`.
The FSR host is therefore disabled on x86 rather than truncating 64-bit handles;
the x86 Vulkan renderer/reactive probe remain supported. A handle-safe SDK adapter
is still needed there. Local RTX4060Ti dispatch passed. CI39350817763 passed all
six application targets, including upscaler execution with Khronos validation on
Linux x64/ARM64. macOS/Windows ARM64 have compile evidence, not native GPU evidence.
No local validation-layer pass is claimed. The newer game integration below
still needs output-size selection and temporal-quality acceptance.

The graphics foundation now buffers projected triangles, snapshots each batch's
texture/depth/blend state, and renders into owned RGBA/depth targets. The probe
checks occlusion, projective UV division, transparent cutouts, blending and HUD
ordering over three frames, then presents that rendered image through a reusable
aspect-preserving swapchain, including a forced rebuild. Command buffers and
texture descriptors are released after use. This parity path is synchronous;
frame overlap and complete rendering
parity remain ahead. Samplers use the selected base LOD, matching the comparator's
non-mipmapped minification. Original mip bytes remain owned by the shared cache.

## Experimental game integration

`-DUPRISING_EXPERIMENTAL_VULKAN=ON` builds the optional game integration. Set
`UPRISING_RENDERER=vulkan` to select it; otherwise the OpenGL comparator remains
available. Build to `source/build/vulkan-game/bin`, not the root release output.
Set `UPRISING_ASSET_DIRECTORY` to the existing game directory and
`UPRISING_USER_DIRECTORY` to an isolated test directory. No asset copies are needed.

On 64-bit builds, `UPRISING_VULKAN_FSR3=1` dispatches the actual SDK on centered
game cameras: 640x480 scene/depth/motion ->1280x960, then native HUD draws at that
output resolution. Jitter uses the SDK's 32-phase sequence; vertical FOV comes
from the real projection. Camera/projection changes, reload and long frame gaps
reset history. O-view crops color, depth and motion to its actual camera rectangle,
uses that crop's vertical FOV, and composites the 2x result into the full output.
Pixel displacements survive the origin translation unchanged. Native MRT coverage
and the retained pre-scene artwork contribution keep irregular glass/frame art
outside temporal history. Later UI draws remain separate. The SDK/crop owner is
rebuilt when view dimensions change; returning to tank resets history.
The output remains 4:3; existing screenshots/pixel consumers get GPU-downsampled
legacy dimensions. A renderer-owned compute pass extracts an R8 rejection mask
from actual motion validity and surviving translucent coverage, feeding both SDK
mask inputs. Stable opaque pixels retain history even when blending is enabled;
depth-writing effects with effective alpha below one reject it. Unknown geometry
and skies conservatively reject history until their motion is implemented.
The mask is allocated once and stays on the GPU; HUD never enters it.
This remains experimental, with fixed 2x output and incomplete moving-sky/particle
history. No complete image-quality or speedup claim.

Local RTX4060Ti checks exercise real gameplay/reload and normal/O-view transitions.
A settled-frame visual comparison found black world silhouettes: the previous
output owner incorrectly received the next frame's depth-mask preparation. Swap
now restores the low-resolution back owner immediately. Corrected captures show
textured terrain/trees; a focused fixed-seed regression checks that handoff and
terrain samples. The GPU probe checks output conversion, HUD composition outside
scene history, and legacy-size readbacks. These do not establish all-view parity.
Mask checks compare every GPU texel for stable/unknown motion, surviving versus
occluded translucency, opaque blending, depth-writing effects and HUD exclusion.
Live Training1 mask readback contains both trusted and rejected coverage; its
reload/shutdown and Priva's citadel power-up/O-return/camera route passed. The
settled output was visually inspected. Broad effects/temporal-quality comparisons
and matched FPS evidence remain pending.
The monitor fixture verifies exact color/depth/motion crops at a nonzero origin,
31x23 dimensions, irregular holes, translucent artwork composition and later HUD.
Live Erudara uses a225x142 crop at403/317 ->450x284; rotation, tank return,
pause/minimize/restore and shutdown with the cropped owner active passed. Its
output and the normal-view settled capture were visually inspected. Alternate
monitor layouts and broad temporal-quality comparisons still need coverage.

The existing indexed menu surface, fonts and widgets feed a persistent RGBA
texture/upload buffer, native triangle pipeline and aspect-preserving presenter.
Menu readback/PNG capture is supported. Advanced FSR/MSAA controls remain disabled
in this WIP backend. The level renderer now submits terrain, actors, projected
effects, HUD/text, layered skies/fog and masked remote-monitor skies to Vulkan.
The established material/texture owner is shared with the comparator; selecting
Vulkan never creates or re-enters an OpenGL context. Native text ink uses buffered
rectangles; immutable HUD tile versions use a bounded content-checked atlas.
Points and lines now use native primitive pipelines rather than expanded/cullable
triangles. The probe checks mixed primitive batches, pixel coverage and occlusion;
complete cross-driver line raster equivalence remains unproved.

An owned R32F target records reversed-infinite perspective depth `10/z`, derived
from unquantized `q=numerator/z`, under existing hardware depth/alpha tests. Terrain
and model near clipping both use z=10; finite visibility limits remain unchanged. Explicit
tank/citadel and O-view world scopes exclude artwork and final overlay queues;
skies and draws without depth writes preserve this input. Its independent reset
also works on resumed double-buffer targets without erasing color or legacy depth.
The live check compares all 1,228,800 depth bytes before/after HUD draws, including
world geometry underneath the cockpit. The GPU probe checks interpolation,
occlusion, alpha cutouts, preserved depth through UI/resume and removal of stale
depth on reset. The historical `reciprocal_depth` API name now returns normalized near/z.

An RGBA32F target now receives previous-minus-current motion in pixel units,
correspondence validity and conservative history rejection. Terrain identities follow world-lattice
coordinates (including animated water); model identities include the transformed
allocation generation and vertex offset. A bounded renderer-owned cache retains
previous rendered positions, rejecting gaps, missing IDs and conflicting samples.
Previous projection is interpolated projectively, retaining changes in depth.
Camera identity, mode and projection changes clear history; level teardown frees it.
The live HUD check also preserves every motion byte, and the movement check reads
nonzero GPU vectors while driving. Cache/generation checks pass x86/x64; GPU tests
cover translation, projective previous depth, UI preservation, reset and invalidity.
Near/screen clippers now retain edge endpoints and their rounded fraction in
arena-owned per-frame provenance. Motion resolves that graph with separate
camera-space and screen-space interpolation rules, not scratch-address history.
Missing/behind-camera endpoints and bounded-recursion failures remain invalid.
Core checks exercise the real near/screen clippers, different depth interpolation,
cycles, missing parents and frame retirement on x86/x64. The live readback now
requires valid clipped vectors as well as exact HUD preservation.

Depth-tested translucent world draws without depth writes now raise rejection
coverage with independent MAX blending. They preserve solid motion/depth/validity;
occlusion, alpha rejection and screen artwork retain their existing ordering.
GPU fixtures verify visible/occluded translucency and later UI. Training1's sampled
moving frame had no reactive batches, so it does not establish live particle-mask
coverage. Unknown point particles reject history rather than inventing vectors.
Priva's actual claim/power-up/O-return/citadel-camera/shutdown route also passed,
including normalized native point RGB. Its fixture now uses native pixel views;
the comparator's point check remains, with a Vulkan submission check added.
Particle quality, same-owner camera discontinuities, composition masks and final
post-upscale UI composition still need coverage. No temporal upscaler is enabled yet.

Per-batch projection snapshots apply subpixel jitter to world geometry and skies;
motion subtracts the current raster jitter and history keeps original positions.
`UPRISING_VULKAN_JITTER=1` enables an eight-phase Halton[2,3] sequence at the
current 1:1 render size. Default is off until temporal reconstruction can resolve
it. Final post-upscale composition, variable-resolution phase counts
and FSR dispatch metadata still need integration. GPU checks cover depth scaling,
clamping, alternating jitter, actual shifted coverage, unchanged HUD and vectors;
x86/x64 core checks cover sequence bounds and wrapping. Live jitter-enabled
Training1 readback preserved every depth/motion byte across HUD rendering.
Jitter-enabled Training1 movement/fire/save/reload/shutdown and Erudara
O-view/mask/rotation/return checks passed. Full temporal image quality is unproved.

A separate owned RGBA8 scene target now receives world geometry and layered
normal/remote skies, with the same depth, alpha, blending and monitor mask tests
as final color. Artwork/text never write it, including artwork drawn before the
O-view monitor. Scene color clears with temporal inputs, survives flush/resume,
and remains byte-identical through later HUD draws. The O-view scope now begins
before masked sky submission, so it uses the active camera's jitter. Sky color
is captured without inventing solid sky depth/motion; sky history quality still
needs work. Four color attachments fit Vulkan's baseline attachment limit.
GPU fixtures check masked sky jitter, blend/occlusion, UI exclusion and resumed
reset. Live Training1 checks all 1,228,800 scene bytes before/after HUD; Erudara
readback verifies nonempty scene color with no artwork outside the monitor.
The scene-color candidate also passed jitter-enabled Training1 movement/fire/
save/reload/shutdown and Priva claim/power-up/O-return/citadel-camera/shutdown.
This is the scene input for future FSR, not completed upscaling or a separate
output-resolution HUD compositor.

Local Windows x64: the existing Graphics drawer regression passed three cycles.
Campaign GPU readback matched all 307,200 indexed/palette pixels exactly and
closed normally. Runs were windowed, muted and isolated; stable EXEs were untouched.
The existing Training1 1,740-frame movement/fire/weapon/save/reload/shutdown check
passed with exact saved position/ammo restored. Erudara's 320-frame O-view check
passed remote sky/mask, camera rotation, return to tank, pause and window restore.
These establish exercised routes, not complete visual equivalence or performance
acceptance. Matched 15-second stationary Training1 samples without GDB now measure
about 152 FPS Vulkan versus 141 FPS stable OpenGL with depth and motion capture on
RTX4060Ti (640x480, windowed/muted/isolated). CPU texture comparison caches and
sparse glyph spans removed the earlier ~53 FPS regression; uploads remain unchanged.
The lower HUD capture is pixel-identical before/after; animated scene/sky pixels
still differ. Sparse-text checks pass on x86/x64, including clipping, multiple
strings, empty writes and rejected out-of-bounds writes. Gameplay/reload and O-view
checks passed again; O-view now waits for real camera-sector coverage before
returning, retaining every acceptance assertion. This removes the measured
stationary regression, not the need for broader view/scene performance comparisons.
Retain the OpenGL release. The bounded timed samples
were deliberately terminated by the launcher; clean shutdown was checked above.
Six-platform application/upscaler-host CI passed, including Linux x64/ARM64 probe
validation. Validation-layer coverage of level rendering remains pending, as do native Apple
GPU execution, all supported views/material comparisons and Vulkan MSAA.

Windows/Linux use the installed Vulkan driver/loader. The new separate world
targets explicitly require independent blending and R32F/RGBA32F color/sampled images;
unsupported devices fail with a diagnostic. macOS explicitly requires
KosmicKrisp, Apple Silicon and macOS 26+, with a developer Vulkan SDK for now:

```sh
export VULKAN_SDK="$HOME/VulkanSDK/1.4.363.0/macOS"
export DYLD_LIBRARY_PATH="$VULKAN_SDK/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
export VK_DRIVER_FILES="$VULKAN_SDK/share/vulkan/icd.d/libkosmickrisp_icd.json"
./uprising_vulkan_probe
```

Bundling the loader/driver and their dependencies into our app is still pending;
end users must not require an SDK in the eventual playable build. Current probe
artifacts are developer executables, not game releases. Main's macOS app remains
unchanged. Unsupported hardware keeps the existing game until Vulkan parity.

## Evidence and open dependencies

- Local Windows x64/RTX4060Ti: reactive-mask numeric check and swapchain present
  passed. Khronos validation layers were unavailable locally, so no validation
  pass is claimed. Initial six-target CI passed, including validation-enabled
  Linux x64/ARM64 software-driver execution. The buffered graphics extension also
  passed all six CI targets and both Linux validation runs. macOS/Windows ARM64 have compile evidence,
  not native GPU execution evidence.
- The upstream host now creates its context and dispatches the chosen FP32
  profile locally. This does not establish in-game temporal quality, frame
  generation, full shader-variant coverage or a performance gain.
- The SDK v1.1.4 upscaler Vulkan backend already contains non-Windows paths;
  the selected profile now builds across the 64-bit targets and runs with Linux
  software-driver validation. Native Apple GPU execution is still pending.
  Its separate frame-generation presenter directly uses Win32 threads/events/QPC;
  omitting that component avoids its platform port for upscale-only integration.
- Need sky/particle history, variable output sizes and
  temporal-quality/performance acceptance, remaining shader variants, 32-bit safe handles,
  KosmicKrisp native execution and hardware coverage beyond the initial GPU.

Primary references: [AMD integration](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-interpolation/),
[pinned source](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/tree/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55),
[Vulkan rasterization](https://docs.vulkan.org/spec/latest/chapters/primsrast.html),
[KosmicKrisp](https://docs.mesa3d.org/drivers/kosmickrisp.html),
[LunarG macOS SDK](https://vulkan.lunarg.com/doc/view/latest/mac/getting_started.html).
