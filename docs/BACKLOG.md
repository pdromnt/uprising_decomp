# Remaining work

- `FSR3` branch: Vulkan renderer and FSR 3.1 temporal upscaling; frame generation
  deferred;32-bit builds are Vulkan-only. Stages, current proof and platform gaps: [experiment](FSR3.md).
  Physical Mac gameplay/FSR is user-confirmed. Next approved cleanup: remove x86,
  OpenGL and FSR1; first confirm the current UI/occlusion fixes and investigate
  the reported terrain/sky jitter and unexpected Novus combat exit.

- Collect manual gameplay results on Windows ARM64 and Linux x64; extend the
  confirmed macOS ARM64 and CM5/Linux ARM64 runs to longer sessions and reloads.
- Fix concrete gameplay, menu, mission-transition and performance reports from
  continued play. Use the focused checklist in [Manual testing](MANUAL_TESTING.md).
- Broaden coverage on older GPUs and physical controllers/haptics, and exercise
  multiplayer across separate machines when hardware/players are available.
- Select a license for project-authored code.
- Later: improve module boundaries, field names and legacy layouts where useful.

Feature history and completed work belong in Git, not this backlog.
