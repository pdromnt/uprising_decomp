# Manual testing

Test the changed behavior and nearby failure paths; avoid repeating unrelated
checks for every fix. Keep a stable executable name/path for firewall rules.
Use isolated saves/settings for destructive tests and compare performance under
matching mission, camera and activity.

- Display changes: 4:3 borders, pointer alignment, Apply/Keep/Cancel, timeout,
  focus/minimize/restore and preference persistence after restart.
- Rendering changes: compare FSR Off/Upscale/Native AA and Edge AA at the same
  output size; inspect 3D edges, HUD/text, sky/fog and citadel monitor masks.
- Loading/cache changes: compare first and repeated process launches, loading
  page to first complete world frame, reload and mission switching. For a cold
  check, move `CONFIG/vulkan-pipelines.bin` aside with the game closed. Check
  macOS on hardware; a successful build does not establish startup timing.
- Gameplay/ownership changes: movement, firing, busy combat, citadel/factory
  activity, save/reload, mission exit and repeated mission transitions.
- Menu/input/audio changes: click/drag/release, focus loss, bindings, movie timing,
  volume/track controls and return from gameplay.
- Network changes: discovery/join, chat/ready/start, host departure, reconnect and
  repeated missions. Use separate machines and more players when available.

Record the build/architecture, mission, exact action, reproduction steps and a
screenshot or log when useful. Keep unresolved reports in the backlog or issue
tracker; do not turn this checklist into a pass/fail history.
