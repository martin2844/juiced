# Arcade challenge runtime checkpoint — 29 October 2026

Six actual challenge runs completed through the native frontend, using the
unchanged production executable `e9f493d2027ebd1cd54dd4dec230ec372f612c08da82b0369563c41089ac857c`.
The challenge rules, results and persistence behaved correctly in these cases.
No production correction was necessary in this follow-up.

## Controlled event access

An exclusively owned asset package permutes the 75 original `ArcadeRaces.txt`
event blocks. Original events 16, 18, 19 and 32 occupy the first four positions.
Every block remains byte-identical, including its car, parts, route, target,
weather, lighting and lap count. The independent checker verifies both the full
multiset of block hashes and the exact permutation. An edited-target negative
control is rejected. Original game data and installed user mods are untouched.

The starting profile is a copy of the previously created validation Player
profile before any Arcade wins. No progress flag was edited. This deliberately
changes event access and is **not** evidence of natural progression through the
original later series. The fixture is local validation data, not a shipped mod.

## Actual outcomes

| Retail event | Runtime setup | Input | Outcome |
| --- | --- | --- | --- |
| 16, TimedLap | Corolla, Campbell_R2, evening/wet, 47 s target | Diagnostic autopilot | Out of time at 47.00 s; remains incomplete |
| 18, PerfectLap | Focus, SouthSide_R3_rev, night/dry, 80 s target | Diagnostic autopilot | Completed at 72.32 s; autosave records the win |
| 32, TimedLap | Falcon, Downtown_R4, afternoon/dry, 45 s target | Diagnostic autopilot | Out of time at 45.06 s; remains incomplete |
| 19, Speed | Beetle, Downtown_R2_REV, afternoon/dry, 60 mph target | Diagnostic autopilot | Reaches target, then drops below it; You are too slow at 12.85 s |
| 19, Speed | Same original setup | Neutral virtual gamepad, no autopilot | Target speed not reached at 10.00 s |
| 18, PerfectLap | Same original setup | Full right steering/RT through the normal pad backend after Go, no autopilot | Wall collision; Car damaged at 2.16 s |

Each run reaches Go, a real physical simulation finish, `hud_race_complete`,
`hud_challenge_results`, quit confirmation and return to `Arcade_Race`. Results
captions were visually inspected; the checker does not perform OCR. The finish
log calls every solo finish “place 1”; failure is determined by challenge rules
and displayed outcome, not by that place label.

A separate production process loads the saved profile and displays the Perfect
Lap tick. Independent archive parsing verifies exactly one completion at slot
2, no later-series unlock and the fifth event still locked. Both manual failure
runs leave that archive byte-identical. The initial timed failure also leaves
the initial archive byte-identical. `CMSGlobal.sav` and `UIHelp.sav` remain
unchanged throughout. The successful autosave also stores a changed
`settings.dat`, consistent with the Change View input used in the run; this is
not a claim that only ArcadeUnlock changed. No existing user profile was used.

## Input fixture correction and limits

Two fixed-delay Down injections during model loading were sampled as one held
button and advanced past Speed to the Falcon event. `speed-selected.png`
therefore depicts the Falcon and is excluded as Speed-selection evidence. The
Falcon race is reported as event 32 above, not as a Speed run.

The new validation helper `reliable_pad.py` waits for logged press and release
samples before issuing another command. It then selects the actual Beetle Speed
event correctly. These are diagnostic injection changes; they do not establish
or repair a production gamepad defect. Screens must still finish their own
transitions before the next dependent action.

Both games and their private Xvfb servers exit zero. Rendering uses llvmpipe and
audio uses a dummy sink. This is neither hardware/FPS nor audible-mix approval.
The successful Perfect Lap result view is partly occluded by foliage, so it is
not a director-camera-quality pass. Challenge success/failure coverage is not
all-Arcade completion or full Career challenge coverage.

## Recheck and evidence

Evidence is under `out/validation/linux-current-20261029/full-race-ui`:

- `challenge-fixture/manifest.json`, `prepare_challenge_fixture.py` and the
  exclusively owned fixture package record the byte-preserving permutation.
- `arcade-challenges` holds four autopilot runs, result captures and intermediate
  independent archive inspections.
- `arcade-challenge-manual` holds the separate-process load, two manual failures,
  collision input receipt and final archive inspection.
- `verify_challenges.py` rechecks the fixture, process/binary identities, event
  ordering, save hashes and archive contents. Thirty missing-evidence negative
  controls, an edited-target control and a false-progress control are rejected.
- `challenge-runtime-receipt.json` binds runtime logs, reviewed images and saves
  to the actual executable.

Recheck with the existing Python/Pillow environment:

```sh
python3 out/validation/linux-current-20261029/full-race-ui/verify_challenges.py
```

The current source build and CTest 9/9 from the prior checkpoint remain the
applicable build evidence: no production source or executable changed here.
The r5 car ZIP and v17 APK remain unchanged. ADB still lists no devices, so the
RP5 installation and visual/driving check remain pending. The strict original
119-resource inventory failure and the remaining global ledger items are open.
