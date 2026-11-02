# Arcade persistence and Linux mod-menu input — 29 October 2026

Fixed an actual Linux usability failure found while checking Arcade: the optional
driver-photo menu consumed raw keyboard events only. A gamepad could create a
driver and then could not navigate or dismiss that menu. The original executable
and captures retain the failed Down/Select attempt at the first row.

## Correction

Linux now polls the selected player's normal UI action map while the creative
menu is open. Up/Down, Select and Back reuse the existing menu actions. Mapping
is established if `--mods-menu` starts before a device is acquired. Held input
must be released on entry and before closing, preventing an opening press from
activating an importer or a closing press from also selecting the next screen.
A prior driving map is restored after closing.

The retail input manager routes keyboard actions to a separate device. The
overlay preserves raw keyboard access when a gamepad owns the player, and uses
only mapped UI events for a keyboard-owned player to avoid double movement.
Physical keyboard navigation/release is checked as part of the release barrier.
Android's existing overlay gamepad-to-key bridge and macOS input routes are
unchanged; the new mapped-event entry is used by Linux only. No APK was rebuilt
or replaced by this Linux correction.

Changed sources: `src/Game/App/GameLoop.cpp`, `src/Mods/CreativeMenu.{h,cpp}`.
Current production `out/linux/juiced` SHA256:
`e9f493d2027ebd1cd54dd4dec230ec372f612c08da82b0369563c41089ac857c`.
The desktop launcher still directly selects this development-build path.

## Runtime checks and failed attempts

- New-driver onboarding with the selected virtual pad: one Down selects
  Continue; holding A keeps the overlay until release; release reveals the
  crew-logo screen without selecting its item. Profile creation then completes
  through logo/phone/crew name and Save Successful, using only the pad.
- Final executable, `--mods-menu` before acquisition: acquire a pad, Down moves
  from Modern Controls to Custom Logo. An actual private-X11 keyboard Down moves
  once to Driver Photo; keyboard Up returns once; pad Up returns to Modern
  Controls. No package was toggled.
- Holding keyboard Escape for three seconds keeps the menu visible, then
  releasing closes it to Profile Manager without activating a profile.
- A separate keyboard-only process navigates to Back before acquiring a player.
  Holding Enter for three seconds keeps that menu visible; release reaches
  Profile Manager without creating a driver. Its neutral virtual pad received
  no button presses.

Keyboard input here uses XTEST against the owned private X server, allowing SDL
to observe real X keyboard state. This is OS input injection, not a physical
keyboard certification. The earlier SDL_PushEvent fixture remains unsuitable
for proving keyboard-release behavior. An initial XTEST helper chose a 32-bit
library and failed before sending any event; it was corrected to require ELF64.

The first attempted correction stopped producing swaps/probe acknowledgements
on menu entry. A modal importer request caused by the held opening button is
suspected, not independently proved. SIGTERM did not stop that owned process;
it was deliberately killed and is excluded from passing evidence. Only its
recorded derived mod overlay was removed after its PID was terminal, reclaiming
1,050,912,743 bytes. No user package/profile was removed. Another candidate
accepted the pad but ignored the keyboard while the pad owned the player; its
unchanged-row screenshot is retained as a second failed candidate.

The image checker measures the actual orange highlight at fixed row bands.
Nine expected selected-row checks pass. The original missing-gamepad case and
the mixed-input candidate both fail their expected next-row checks. These are
independent runtime negatives; the ordinary CTest suites alone did not detect
these overlay failures.

## Actual Arcade progression and persistence

An exclusively owned Player/MyCrew profile was created with installed mods.
Normal menu selection and diagnostic autopilot drove these actual events:

| Event | Track / conditions | Result |
| --- | --- | --- |
| Novice 1, FTO | Engstrom_R1, evening/clear, 3 laps, 6 cars | First, 140.39 s |
| Novice 2, Celica SS-I | Speedway_R2, morning/clear, 2 laps, 6 cars | First, 77.46 s |
| Novice 3, SRT-4 | Engstrom_Sprint01, night/clear, 3 heats, 4 cars | Second in each heat; 12 points against winner's 18; event correctly stays unwon |
| Novice 4, Punto | Anderson_R1_rev, evening/clear, 3 laps, 6 cars | First, 97.26 s; NEW SERIES UNLOCKED |

Every event returned through its real results UI. These are ordinary frontend
paths without `--test` or a direct `--track` entry. Autopilot uses actual physics;
neither finishing positions nor save completion flags were assigned by the
fixture. The unlocked message is captured on the final executable.

An independent read-only archive parser verifies the archive checksum and exact
ArcadeUnlock fields. Saved completion counts move **0 -> 2 -> 3**. The losing
sprint stays incomplete. Series 2's first four events change from locked to open
only after the third win; its fifth stays locked. The first series itself is
not complete: its sprint and fifth event remain unwon and the fifth is locked.
Only ArcadeUnlock.sav differs between the initial/final profile archives:
CMSGlobal.sav, settings.dat and UIHelp.sav retain their hashes. Thus this Arcade
run did not change this profile's career money, cars or crew data.

The final executable loaded a byte-checked copy of the two-win profile in a
separate process: Load Successful and the two retained ticks are captured.
That process then earned the third win and saved its unlock. The native
UserSaves suite independently passes 20 checks on the final profile, including
metadata/archive checks and career round trip. No real user profile was loaded
or edited; seeding is restricted to owned validation support folders, whose
source hashes are unchanged after copying/loading.

A further final-binary process loaded the three-win profile, selected the now
available JUICE GAMES SERIES and opened its event list. Its four initial events
are open and its fifth is locked; Campbell Hills route 1 details and the new
series title are captured. This verifies the new-series unlock after a restart,
in addition to its serialized flags.

The earlier onboarding and first three events used binary
`6eec98f1f4fc297f6998fa429d61f3029b524c7c420b283dfaf1c70b28dfd476`, retained
as `arcade-series-pad-v2/exercised-juiced`. The final binary above adds mixed
keyboard handling and late device acquisition. Its own runtime covers startup
and mixed/keyboard-only menus, profile load and the complete reversed-route
third victory/unlock. The different scopes and binary identities are retained.

## Evidence and limits

Evidence root: `out/validation/linux-current-20261029/full-race-ui`.
`arcade-overlay-receipt.json` binds logs, image hashes, row checks, both successful
binary identities and failed candidates. `verify_arcade_overlay.py` regenerates
independent save inspections and rejects the original/candidate UI negatives.
`arcade-series-pad-v2`, `arcade-overlay-final` and `creative-keyboard-only` contain
the relevant runtime/support/capture files; `arcade-unlock-reload` contains the
third-win reload. All seven owned test-game processes
and their display servers are terminal; the single killed candidate is explicit.

The complete default Linux build succeeds. CTest passes 9/9; native UserSaves
passes 20/20. Build/test logs use the `creative-pad-*` and
`arcade-final-save-native.log` prefixes. No source tests, parser gates or resource
allowances were relaxed. The original strict 119-resource inventory failure
remains unresolved.

Rendering uses private Xvfb/llvmpipe and audio a dummy sink. This does not certify
FPS, audible output, physical input, Android/RP5 behavior or completion of Arcade
or the full campaign. The RP5 still does not appear in ADB; final physical r5/v17
deployment remains pending. The existing APK and car-pack ZIP are unchanged.
