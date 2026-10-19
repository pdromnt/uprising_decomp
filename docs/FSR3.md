# Vulkan / FSR3 experiment

Branch: `FSR3`, based on `c620bc4b`. `main` remains the release/bug-fix line.
The first playable target is Vulkan with FSR 3.1 temporal upscaling. Frame
generation is out of scope; the existing real frame rate is sufficient.
The game remains C. Keep any later AMD C++ host code behind a small C interface.

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
3. **Temporal inputs.** Carry previous camera/object transforms alongside current
   submissions. Current vertices are already projected and contain no motion
   history. Add consistent depth, jitter and motion vectors for terrain, actors,
   sprites and debris. Isolate HUD/cockpit/minimap. Reset history on camera cuts,
   mission reload, output-size changes and discontinuities; handle transparency
   and animated skies with reactive/composition masks.
4. **Temporal upscaling.** Integrate the full source-backed upscaler, native AA
   and quality controls; validate moving edges, disocclusion, transparencies and
   all tank/O/citadel views. Composite HUD/text at output resolution. Preserve
   4:3 presentation. Compare real frame time and image quality with FSR disabled.

## Dependency proof

Configure with `-DUPRISING_VULKAN_PROBE_ONLY=ON`, then build normally. Requires a
native C11 toolchain and `glslang` or `glslangValidator` (override with
`-DUPRISING_GLSLANG=/path/to/compiler`). Uses checksum-pinned Vulkan headers and
12 upstream AMD shader/header files, not an entire SDK download. Generated files
stay in the build cache. No assets, settings, saves or playable EXEs are modified.

Run `bin/uprising_vulkan_probe` (`.exe` on Windows). It uses a hidden window,
creates a real swapchain and dispatches the unmodified SDK v1.1.4 FSR3 reactive
shader in FP32. It checks all 64 readback pixels against the expected 0.4330127.
`--show` briefly shows the result; `--validation` requires installed Khronos
validation layers and fails rather than silently skipping them. Run as a normal
console program. It captures no input and plays no sound.

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

The existing indexed menu surface, fonts and widgets feed a persistent RGBA
texture/upload buffer, native triangle pipeline and aspect-preserving presenter.
Menu readback/PNG capture is supported. Advanced FSR/MSAA controls remain disabled
in this WIP backend. The level renderer now submits terrain, actors, projected
effects, HUD/text, layered skies/fog and masked remote-monitor skies to Vulkan.
The established material/texture owner is shared with the comparator; selecting
Vulkan never creates or re-enters an OpenGL context. Native text ink uses buffered
rectangles; immutable HUD tile versions use a bounded content-checked atlas.

Local Windows x64: the existing Graphics drawer regression passed three cycles.
Campaign GPU readback matched all 307,200 indexed/palette pixels exactly and
closed normally. Runs were windowed, muted and isolated; stable EXEs were untouched.
The existing Training1 1,740-frame movement/fire/weapon/save/reload/shutdown check
passed with exact saved position/ammo restored. Erudara's 320-frame O-view check
passed remote sky/mask, camera rotation, return to tank, pause and window restore.
These establish exercised routes, not complete visual equivalence or performance
acceptance. Matched 15-second stationary Training1 samples without GDB now measure
about 149 FPS Vulkan versus 136 FPS stable OpenGL on RTX4060Ti (640x480,
windowed/muted/isolated), up from ~53 FPS Vulkan. Profiling found texture comparisons
reading mapped upload memory and full-frame scratch scans for individual strings.
Texture comparisons now use an owned CPU cache; glyph writes report their actual
pixel span and retire only that scratch span. Texture uploads remain unchanged.
The lower HUD capture is pixel-identical before/after; animated scene/sky pixels
still differ. Sparse-text checks pass on x86/x64, including clipping, multiple
strings, empty writes and rejected out-of-bounds writes. Gameplay/reload and O-view
checks passed again; O-view now waits for real camera-sector coverage before
returning, retaining every acceptance assertion. This removes the measured
stationary regression, not the need for broader view/scene performance comparisons.
Retain the OpenGL release. The bounded timed samples
were deliberately terminated by the launcher; clean shutdown was checked above.
Six-platform application CI passed with the level extension. CI for these latest
performance changes and validation-layer coverage of level rendering remain pending, as do native Apple
GPU execution, all supported views/material comparisons and Vulkan MSAA.

Windows/Linux use the installed Vulkan driver/loader. macOS explicitly requires
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
- One reactive-mask pass does **not** establish complete FSR upscaling, temporal
  quality, frame generation, playable Vulkan rendering or a performance gain.
- The SDK v1.1.4 upscaler Vulkan backend already contains non-Windows paths;
  compile its full dependency closure before claiming Linux/macOS compatibility.
  Its separate frame-generation presenter directly uses Win32 threads/events/QPC;
  omitting that component avoids its platform port for upscale-only integration.
- Need full upscaler shader variants/context build, driver feature/format checks,
  KosmicKrisp native execution, and hardware coverage beyond the initial GPU.

Primary references: [AMD integration](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-interpolation/),
[pinned source](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/tree/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55),
[KosmicKrisp](https://docs.mesa3d.org/drivers/kosmickrisp.html),
[LunarG macOS SDK](https://vulkan.lunarg.com/doc/view/latest/mac/getting_started.html).
