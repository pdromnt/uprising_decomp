# Remaining work

- Continue manual campaign/tutorial progression, mission cycling, crowded combat,
  mission exit, menus and multiplayer checks; fix concrete reports.
- Validate complete x64 gameplay/audio and later other architectures/platforms.
- Finish the remaining SDL3/platform-service migration.
  Windows/events, menu surfaces/palette, fonts, splash, GL-context/timing and
  physical input and audio/music ownership have migrated. Font/pointer visuals
  and SDL keyboard/mouse were manually accepted. Controller/haptic hardware is
  pending, including Xbox Series Bluetooth detection with the TOU-derived SDL
  endpoint fix. Check new audio levels, voices, movie sync, music controls and reload
  cleanup manually. Legacy I-FORCE support is removed; SDL joystick/haptics remain.
  Next: network transport and Windows services.
- After the SDL3 migration, remove legacy code and dependencies that are no longer
  used.
- Once the SDL3 build is stable, add FSR upscaling.
- Select a license for project-authored code.
- Later cleanup: better module boundaries, descriptive fields and layout cleanup.
  Preserve behavior first.

Previously accepted gameplay fixes are not reopened without new evidence.
