# Completed reverse-route matrix — 2026-10-31

All nine reverse-R3 diagnostic races complete two laps with all six stock Focus
cars finishing: 54 actual finish events across nine districts. This is new
full-race evidence for the unchanged development-r14 executable, not complete
coverage of all 108 routes or a new Nix/APK/car release.

Executable SHA-256:
`556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f`.
Original game data, `--no-mods`, `--test`, seed 42, Focus_2, autopilot and
night/wet (4/2) use separate owned support directories. SDL offscreen reports
AMD Radeon RX 7800 XT; no desktop window, unlock or physical device interaction.
The `--track/--car` diagnostic prepares Circuit/two laps, bypassing frontend
selection. Player input, audio and FPS claims are excluded. The actual packaged
Nix ELF has prior separate runtime evidence; these nine runs use development ELF.

| District / route | Player place | Player time (s) | Last finish (s) |
| --- | --- | --- | --- |
| 1 / Angel_North_R3_rev | 1 | 222.99 | 233.50 |
| 2 / Campbell_R3_rev | 1 | 245.29 | 295.38 |
| 3 / Downtown_R3_REV | 1 | 247.53 | 262.46 |
| 4 / East_Angel_R3_REV | 1 | 161.04 | 169.72 |
| 5 / Engstrom_R3_rev | 1 | 131.84 | 151.90 |
| 6 / SouthSide_R3_rev | 1 | 172.26 | 202.81 |
| 7 / Anderson_R3_rev | 3 | 143.65 | 145.26 |
| 8 / AngelWest_R3_rev | 1 | 203.29 | 221.40 |
| 9 / Speedway_R3_rev | 1 | 90.14 | 98.27 |

Every process reaches the requested scene and Go, records six distinct car IDs
and six distinct finishing places, logs both racing laps, captures a native
framebuffer and exits 0. All three runner queues are terminal 0. The first pilot
and the partial two-case CP35 are superseded by the final nine-case receipt;
those immutable historical checkpoints are retained.

## Independent references and controls

Python archive extraction retains original TDF/SPL hashes for these nine routes
and counts each six-slot grid and fast/attack/centre spline. Runtime next-point
indices remain inside the independently counted fast-spline bounds. The initial
Southside extraction's case-sensitive archive-name issue is corrected by
resolving the actual archive stem; the reader never writes game data.

A real nonexistent route boots and exits 0, but never loads the requested scene,
creates racers or reaches Go; the verifier rejects it. Seven copied-log variants
also reject missing finish, duplicate place/car, wrong scene, missing Go,
missing second lap and missing results. Actual resource/log/image files are not
mutated by these controls. `verify.py --all` requires all nine terminal cases.

## Results images and capture timing

The reviewed gallery has six visible results positions for each district, with
exact BMP/PNG RGB equality. Primary captures and complete race logs remain
unchanged. The original West Anderson capture is an entry transition: rows 4–6
are still fading. It is explicitly excluded from settled UI approval.

The original `hud_race_results.ui` declares final text fades with duration 2 s and
delay 3 s. UiScreen initializes those properties; Transition::step consumes the
delay before the duration. A separate normal-timing West Anderson repeat waits
6.5 s after readiness and produces an inspected, fully populated results view.
Its six finishes and times differ from the original run; it is identified as a
separate capture in the gallery, never substituted as primary race-log evidence.
The source/properties and both screenshots are retained. No engine/UI fix was
needed or made. Future capture helpers should wait beyond the authored 5 s fade.

Open `out/validation/linux-current-20261029/route-completion-r14/gallery.html`.
`all-nine-results-reviewed.jpg` uses the labelled repeat for West Anderson;
`all-nine-results-original.jpg` preserves the premature original image.
The art approval concerns these table views only, not every track/material view.

## Accelerated traversal calibration

A separate Angel North R2 reverse pilot uses an existing test-only path:
`--test --replay-capture-frame 4294967295 -r` (arguments are separate words in the
actual recorded command). GameLoop uses fixed 1/60 updates with that test flag;
`-r` removes its frame limiter. No replay is playing, so replay auto-capture
cannot fire. No executable, physics, renderer or source was patched.

All six cars finish two laps. The last finishes at 184.35 simulation seconds,
while the whole process takes 107.929 wall seconds. Its 11640 rendered frames
represent 194 simulated seconds including non-racing phases. The 1.708 ratio is
simulated race seconds per whole-process wall second; it is not an FPS number,
Android performance result or same-route before/after benchmark. Source hashes,
actual arguments, events, frame count, PNG/BMP equality and terminal 0 are bound
in `route-fixed-step-r14/verification-receipt.json`. This pilot supports broader
fixed-step traversal work; it does not replace normal-timing validation.

## Preservation and remaining scope

The final receipts verify all 908 critical production inputs, all six personal
Linux save/creative files and every installed Wave2 file against established
hashes. The nine primary games, negative control, accelerated pilot and capture
repeat are terminal. No engine/assets/APK change, save editing, RP5 access,
public upload or commit. Linux still has car bundle r8; RP5 remains APK 19 / pack r5.

The overall goal remains active: remaining routes/gameplay/art and reference
breadth, original 119-D8M strict gate, physical Android/input/audio/network and
final source/license/distribution work are not declared complete here. The
unchanged last full integration result is 407 pass / 1 fail, independently scoped
from this out-only runtime matrix. No additional unit/CTest claim is inferred.
