# Manual acceptance

Use one windowed session and a stable executable name/path for firewall rules.
Back up saves and `CONFIG/settings.ini` before destructive experiments.

- Start without flags; check icon/title, music and sound. Change a setting and
  binding, exit and restart. Confirm one CONFIG file restores them.
- Font/pointer visuals were accepted before the SDL input change. Recheck
  aiming, weapon/building lists, bindings, pause/focus release and resume with
  the new input backend. If available, check joystick axes, hats, buttons,
  reconnect and periodic force feedback; virtual-device checks cannot prove
  physical hardware feel or haptic output.
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
