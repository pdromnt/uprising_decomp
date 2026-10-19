# Manual testing

Test the changed behavior and nearby failure paths; avoid repeating unrelated
checks for every fix. Keep a stable executable name/path for firewall rules.
Use isolated saves/settings for destructive tests and compare performance under
matching mission, camera and activity.

- Display changes: 4:3 borders, pointer alignment, Apply/Keep/Cancel, timeout,
  focus/minimize/restore and preference persistence after restart.
- Rendering changes: compare FSR on/off at the same output size. Change MSAA and
  load a mission; inspect 3D edges, HUD/text, sky/fog and citadel monitor masks.
- Gameplay/ownership changes: movement, firing, busy combat, citadel/factory
  activity, save/reload, mission exit and repeated mission transitions.
- Menu/input/audio changes: click/drag/release, focus loss, bindings, movie timing,
  volume/track controls and return from gameplay.
- Network changes: discovery/join, chat/ready/start, host departure, reconnect and
  repeated missions. Use separate machines and more players when available.

Record the build/architecture, mission, exact action, reproduction steps and a
screenshot or log when useful. Keep unresolved reports in the backlog or issue
tracker; do not turn this checklist into a pass/fail history.
