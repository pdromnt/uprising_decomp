# Remaining work

- Continue manual campaign/tutorial progression, mission cycling, crowded combat,
  mission exit, menus and multiplayer checks; fix concrete reports.
- Validate complete x64 gameplay/audio; other platforms/architectures are deferred.
- Check physical controller/haptics, including Xbox Series Bluetooth detection.
- Listen for audio levels/voices/loops, movie sync, track controls and reload cleanup.
- Validate ENet on separate machines: discovery, four players, abrupt loss/reconnect,
  repeated missions and congested links. Mixed x86/x64 host migration has passed
  same-machine integration; this does not establish broader network acceptance.
- Remove unused legacy code/dependencies after the SDL3/platform migration.
  Small Windows file-attribute and CRT cwd/durability adapters remain deliberate.
- Once the SDL3 build is stable, add FSR upscaling.
- Select a license for project-authored code.
- Later: better module boundaries, descriptive fields and layout cleanup.

Previously accepted gameplay fixes are not reopened without new evidence.
