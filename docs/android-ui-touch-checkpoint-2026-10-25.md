# Android UI and touch checkpoint — 2026-10-25

Base commit: `a06a2a6`, branch `linux-nixos`, OpenJuice 1.02.5.
Delivered version: **1.02.5-android.2**, version code **3**.

## Result

The Android launcher now has a consistent navigation rail on wide screens and
horizontal navigation on phones, with distinct Play, Mods, Controls and Settings
pages. The initial installation page gives one clear ZIP-import action. Play
separates the main game from quick race and stock mode. Mods show descriptions
and explicit enabled/disabled states. Settings separates game installation from
save backups. Gamepad setup keeps its live monitor visible beside assignments
in landscape. Touch setup provides a visual layout editor with pinned Save/Back.

The original in-game Juiced menu art is retained. This changes the Android shell
and adds a control overlay over the native game's menus and races; it does not
redesign every original career/garage screen.

## Touch controls

- **Controls → Set up touch controls**: Auto (no gamepad), Always, or Off.
- Drag driving buttons to reposition them; tap a button and choose its action.
- Global size 75–135% and opacity 25–90%; Save persists these settings.
- Menu preview shows a fixed pad, Select/Back, Tab, page keys and the `0` paint
  purchase shortcut. Menu directions send arrows alone, so navigating the name
  keyboard does not accidentally type WASD characters.
- During a race the overlay automatically switches to driving controls. Pausing
  switches it back to menu controls. Steering, accelerator, brake, nitro,
  handbrake, gear changes, camera and reverse are available.
- Logical driving actions are resolved from the effective DRIVER1 map after stock
  controls, optional Modern Controls and saved keyboard bindings are applied.
  The overlay therefore does not assume that the controls mod is enabled.
- Pointer ownership is aggregated: lifting one finger does not release another
  finger's action. Dragging outside releases a button. Cancellation, activity pause,
  view detachment and layout changes release held controls.
- The old invisible SDL finger zones are disabled on Android to avoid duplicate
  input. Physical gamepad input remains available alongside touch in Always mode.

Settings are in app-private `touch.properties`. Updates retain game files, mods,
controller settings and saves. Back leaves the editor without saving changes.

## Verification performed

**Physical Retroid Pocket 5, Android 13, 1920×1080:**

- Signed APK installed as an update with the existing signing key. Six enabled
  mods and game installation were retained.
- Reviewed final Play, Mods, Controls, Settings, gamepad editor and touch editor
  screenshots. Fixed horizontal preview clipping, phone navigation wrapping,
  editor Back-button wrapping and status-bar contrast found during inspection.
- Dragged a driving button and saved it; read back its changed normalized position
  (`0.x=0.1562092`, `0.y=0.6485911`). Restored the default layout afterwards.
- Android instrumentation dispatched two-finger MotionEvents through TouchView,
  checking simultaneous throttle/steering, partial release, cancellation, layout
  transition and menu key isolation.
- Injected two simultaneous touches into a native quick race. Screenshots show
  both buttons highlighted; after release the car is moving at 24 mph with neither
  button highlighted. These captures were visually inspected, not inferred from
  the instrumentation exit status alone.
- In the signed release, resumed through the touch Select button, held accelerator,
  and paused through the touch Pause button. The release capture shows 78 mph;
  the next capture shows the menu layout over the pause menu.
- Restored **Auto** mode and left the launcher open. On the Retroid this hides touch
  while its built-in analog controller is connected; choose Always to try touch.

**API 35 x86_64 emulator without an analog controller:**

- Auto displays the menu pad. Touch Select progressed from title to profile manager
  and name keyboard; navigation moved the selection without inserting letters.
- Reviewed phone layout at 720×1280 / 320 dpi, including first installation, Play,
  Controls and both editors. The initial-install state was produced by temporarily
  moving the emulator's game directory and restoring it in a `finally` block.
- Instrumented TouchView tests also passed on the emulator.
- Removed only redundant ZIP copies from this test emulator's Download directory
  when its internal disk filled. Host installers and Retroid files were retained.

**Automated/build checks:**

- Pure Java tests: multitouch ownership, shared actions, dragging outside, release,
  menu key isolation, saved configuration round-trip and invalid-number bounds.
- Four Android host CTests pass, including a new touch-binding resolver test for
  stock, modded and custom keys, plus isolation from UI maps.
- ARM64/x86_64 debug and ARM64 release builds succeed.
- Release APK signature verification and 16 KiB ZIP alignment checks pass.

## Artifacts and reproduction

APK: `out/releases/OpenJuice-1.02.5-android-arm64.apk`

SHA-256: `804266a9d708cff77d9f109cd0718c7aabf468a517e06f829ffd514f1c3331b6`

Screenshots and logs: `out/validation/android-ui/` (local, not tracked).
Useful captures: `play.png`, `mods.png`, `settings.png`, `controls.png`,
`gamepad-editor.png`, `touch-editor.png`, `phone-first-install.png`,
`phone-touch-editor.png`, `touch-two-fingers.png`, `touch-released.png`,
`release-touch-race.png`, `release-touch-pause.png`,
`touch-no-gamepad-profile.png`, `touch-no-gamepad-keyboard.png`.

Build commands remain in [android-port.md](android-port.md).
For the installed debug/test APK pair, run:

```sh
adb -s DEVICE shell am instrument -w -e mode touch \
  org.openjuice.android.test/org.openjuice.android.SmokeInstrumentation
```

`-e mode touch-race` additionally opens a manual quick race and captures two-finger
input on a **1920×1080 unlocked Retroid with Always mode and default positions**.
It is a device-specific capture harness, not a general race-completion test.

## Limits

Steering/pedals are digital touch buttons, not an analog virtual stick. Driving
positions/actions are editable; menu positions are fixed. This is not a complete
career playthrough, a TalkBack accessibility audit, or an exhaustive test of every
Android display, GPU, third-party mod or in-game shortcut. The earlier 58 FPS
measurement belongs to the previous performance checkpoint; this UI pass did not
repeat that benchmark or establish a new frame-rate claim.
