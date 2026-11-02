# Linux title to profile black-screen fix — 2026-11-01

The ordinary launcher reached the title, then the driver selector went black.
The user's log showed `frontend: note of a lost controller ""` next to the
transition to `driver`. Native car gallery/load checks had not exercised raw
keyboard player acquisition at the title; their success did not validate this
normal launcher path.

## Cause and correction

Linux exposes one keyboard device followed by four stable SDL gamepad slots.
The shared raw poll copied physical keyboard state into every device, including
absent gamepad slots. `takePlayer` scans every device and keeps the last one with
input. Enter therefore let the last absent pad slot claim the player. Its first
UI frame was disconnected, opening the inherited empty critical disconnect
message and suppressing navigation.

`src/Core/Input/Input.cpp` now reads the raw keyboard only on its own device when
Linux has that dedicated port. The original combined keyboard/controller behavior
remains when no dedicated port exists, and non-Linux builds keep their prior
behavior. Mapped keyboard fallback and SDL pad buttons/triggers are unchanged.
This does not disable real controller-disconnect detection or alter car assets.

`tests/Linux/ControllerTest.cpp` now takes the keyboard automatically from raw
Enter with an idle virtual driving pad plus empty slots, registers the UI map,
checks connected status and navigates/selects. It repeats after all driving pads
are removed. The old explicit `setPlayerDevice(0,0)` keyboard check alone missed
this acquisition failure. Existing menu/driving/crew/remap/two-pad/reconnect tests
remain and pass.

## Verification and limits

Evidence: `out/validation/launcher-black-screen-20261101`.

- The new regression failed against the original implementation with
  `Title Enter must acquire the keyboard, not a gamepad/hot-plug slot`.
- Final Linux controller integration passes; unit suite:388 passed,0 failed.
- The original production launcher/executable with current installed r18 assets
  reproduced the blank screen in a private copy of the user's config/profiles.
  The fixed visible-window run reaches Driver, loads Tony and opens the main menu.
- A final run uses the normal launcher without `--screen` or `--test`, starting
  through disclaimer/legal/movies and title, then Driver/profile load, main menu,
  Career and the car collection. It renders1,600 frames and captures a non-black
  career collection. Keyboard inputs are scripted through normal input polling;
  no internal menu state is forced. Its window is focused and sized through
  Hyprland for the bounded check.
- A deliberate focus-cycle run also reaches the main menu. An earlier unattended
  capture lost focus and opened the legacy disconnect note; it is not counted as
  a successful run. Temporary ownership tracing was removed from the final source.
- All six personal save files remain byte-identical. All runtime checks use cloned
  profiles in a private support folder. There is no profile restoration/reset,
  Android device interaction, APK replacement or asset-pack downgrade.

The normal launcher already points at `out/linux/juiced`; rebuilding it installs
this fix for the next launch. The old binary is retained as `juiced-before` in the
evidence folder. Before SHA256:
`e8450e7dac66c9eb0da844be4db06a074d486ebd7e485c4403f1245f2aaa4607`.
Fixed SHA256:
`488bc91b5c934372ca7da9fb07ea0d388b93f88b9451a95a3242c4b93e2a64ee`.
This checkpoint validates Linux keyboard/title/menu behavior; it is not a new
Android release, complete career playthrough or hardware performance claim.
