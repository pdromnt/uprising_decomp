# Manual acceptance

Use one windowed session and a stable executable name/path for firewall rules.
Back up saves and `CONFIG/settings.ini` before destructive experiments.

- Start without flags; check icon/title, music and sound. Change a setting and
  binding, exit and restart. Confirm one CONFIG file restores them.
- Check menu pointer alignment, confinement in gameplay and release on pause or
  focus loss. Inspect campaign/list/debrief text and wrapping after the font
  backend change; layout metrics are retained, while glyph ink may differ.
- Play natural training and campaign progression, including mission completion,
  debriefing, save/reload and repeated mission transitions.
- Exercise movement, aiming and all available weapons, infantry, pickups, debris,
  citadel arrival/power-up, factories and turrets. Include Terra and Erudara.
- Check tank/citadel/O views, terrain seams, sky/horizon, HUD/map layering and text.
- In busy combat, switch views, pause, minimize/restore and exit the mission. Watch
  for hangs, persistent allocations and FPS degradation across mission changes.
- Exercise weapon/building lists and multiplayer routes with consistent names.

Record concrete failures with mission, action and screenshot/log where useful.
Compare performance under matching mission, camera and activity. A smoke pass
does not replace a full run. Known accepted fixes need no exhaustive repeat until
a related change or new report justifies it.
