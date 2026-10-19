# Remaining work

- Continue manual campaign/tutorial progression, mission cycling, crowded combat,
  mission exit, menus and multiplayer checks; fix concrete reports.
- Validate complete x64 gameplay/audio and later other architectures/platforms.
- Migrate platform services to SDL3, including replacement of the Windows CD/OGG
  music adapter and its separate configuration.
  Windows/events, menu surfaces/palette, fonts, splash and GL-context/timing
  ownership have migrated. Next: physical input/controllers, audio/music, then
  network transport and remaining Windows services. Validate the new font ink
  and interactive pointer behavior manually; retain existing rendering/gameplay.
- After the SDL3 migration, remove legacy code and dependencies that are no longer
  used.
- Once the SDL3 build is stable, add FSR upscaling.
- Select a license for project-authored code.
- Later cleanup: better module boundaries, descriptive fields and layout cleanup.
  Preserve behavior first.

Previously accepted gameplay fixes are not reopened without new evidence.
