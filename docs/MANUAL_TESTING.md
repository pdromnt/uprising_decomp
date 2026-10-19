# Manual acceptance

Audio is user-confirmed working. Controller and multiplayer are accepted for now
on local test evidence; physical hardware and separate-machine checks below are
deferred follow-up, not blockers or claims of verified hardware/remote behavior.

Use one windowed session and a stable executable name/path for firewall rules.
Back up saves and `CONFIG/settings.ini` before destructive experiments.

- Start without flags; check icon/title, music and sound. Change a setting and
  binding, exit and restart. Confirm one CONFIG file restores them.
- In Options → Graphics → ADVANCED, change output resolution and fullscreen mode.
  Check pointer alignment, KEEP, CANCEL, timeout and minimizing before confirmation;
  exit to confirm desktop restoration, then restart to check saved preferences.
  Compare FSR1 on/off during gameplay at the same output size; include HUD text,
  menus, view changes and repeated missions. FSR is inactive when no upscale is
  needed. Fullscreen and older-GPU fallback remain manual checks.
- When audio changes or a new report warrants it, listen for effect/voice pitch and looping,
  movie synchronization, music track changes/shuffle and separate music/effects
  sliders. Check both builds; mute, change missions and exit during busy audio.
  Check thumb clicks, dragging outside the window and changing focus during a
  drag; menus should continue responding afterward.
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
  Use current reconstructed builds on separate machines; check LAN discovery,
  joining, four players, chat/ready/start, host departure, closed-session admission
  and repeated connections. TCP/IP now selects ENet UDP; old DirectPlay peers
  and serial/modem/IPX providers are unsupported.

Record concrete failures with mission, action and screenshot/log where useful.
Compare performance under matching mission, camera and activity. A smoke pass
does not replace a full run. Known accepted fixes need no exhaustive repeat until
a related change or new report justifies it.
