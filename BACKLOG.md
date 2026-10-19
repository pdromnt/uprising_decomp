# Remaining work

- Continue manual campaign/tutorial progression, mission cycling, crowded combat,
  mission exit, menus and multiplayer checks; fix concrete reports.
- Validate complete x64 gameplay/audio and later other architectures/platforms.
- Migrate platform services to SDL3, including replacement of the Windows CD/OGG
  music adapter and its separate configuration.
  Windows/events, menu surfaces/palette, fonts, splash, GL-context/timing and
  physical input ownership have migrated. Font/pointer visuals were manually
  accepted before the input migration. Check the new SDL relative input,
  bindings, focus release and real joystick/haptic hardware manually. Next:
  audio/music, optional legacy I-FORCE, network transport and Windows services.
- After the SDL3 migration, remove legacy code and dependencies that are no longer
  used.
- Once the SDL3 build is stable, add FSR upscaling.
- Select a license for project-authored code.
- Later cleanup: better module boundaries, descriptive fields and layout cleanup.
  Preserve behavior first.

Previously accepted gameplay fixes are not reopened without new evidence.
