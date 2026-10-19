# Remaining work

- Continue manual campaign/tutorial progression, mission cycling, crowded combat,
  mission exit, menus and multiplayer checks; fix concrete reports.
- Validate complete x64 gameplay; other platforms/architectures are deferred.
- Audio is user-confirmed working. Controller and multiplayer are accepted for
  now on existing local checks. Physical controller/haptics/Bluetooth and ENet
  separate-machine/four-player/loss/reconnect checks are deferred, not verified.
- SDL3 legacy cleanup is integrated. Remove further dead code when concrete
  dependencies establish it is unused; broad cleanup is deferred. Small Windows
  file-attribute and CRT cwd/durability adapters remain deliberate.
- Manually accept advanced display settings: fullscreen mode changes, confirmation/
  timeout, restart persistence and FSR1 appearance at larger resolutions. Local
  x86/x64 windowed UI, scaling, FSR toggling and cleanup checks pass. Older-GPU
  fallback and fullscreen desktop restoration still need hardware acceptance.
- Select a license for project-authored code.
- Later: better module boundaries, descriptive fields and layout cleanup.

Previously accepted gameplay fixes are not reopened without new evidence.
