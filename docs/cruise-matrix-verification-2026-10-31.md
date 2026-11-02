# Complete current Linux Cruise/Showoff route matrix — 2026-10-31

All nine original Cruise routes now have completed normal-menu Showoff
workflows on the unchanged development executable: natural three-minute expiry,
results, Continue, normal menu return and exit0. Two prior pilots are retained;
seven new CR-X cases complete the matrix. This is verification, not a new build.

Executable SHA-256:
`556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f`.
Nix r14, APK26, car bundle r8 and the usual Linux launcher are unchanged.
RP5 remains APK19 / pack r5 and was not accessed.

| Original route | Car | Final score | Evidence |
| --- | --- | ---: | --- |
| Angel_North_cruise01 | Stock Punto | 0 | Retained timer pilot, checkpoint41 |
| campbell_cruise01 | Stock CR-X | 1,231 | New case d02-r08 |
| Downtown_cruise01 | Stock CR-X | 36 | Retained scoring + fresh native record reload, checkpoint42 |
| East_Angel_CRUISE01 | Stock CR-X | 573 | New case d04-r08 |
| Engstrom_cruise01 | Stock CR-X | 4,954 | New case d05-r08 |
| SOUTHSIDE_CRUISE01 | Stock CR-X | 279 | New case d06-r08 |
| Anderson_CRUISE01 | Stock CR-X | 29 | New case d07-r08 |
| AngelWest_CRUISE01 | Stock CR-X | 6,528 | New case d08-r08 |
| Speedway_cruise01 | Stock CR-X | 2,700 | New case d09-r08 |

Names and casing above come from original TDF/SPL references, rather than
inferences from UI district labels. The seven new natural finishes range from
180.01 to180.06 game seconds. Observed Go-to-results wall time is approximately
185.0–185.2seconds; menu/loading/settling time is separate. These timings do not
measure FPS or establish physical-device performance.

## Method and independent checks

Each new case boots to the title, loads an owned synthetic Player, selects
custom race, the stock152BHP CR-X, no auto-mod, Showoff, no tutorial and the
district through normal menus. The existing private fixture unlocks the routes
and only the CR-X car flag; it does not represent natural career/Arcade progress.
No personal profiles are copied into these tests. Virtual SDL pad, autopilot,
private Xvfb/llvmpipe and dummy audio are explicit limitations. There is no
`--test`, `--track` or fixed-step clock.

Evidence under `out/validation/linux-current-20261029/cruise-sweep-r14`:

* `plan.json`: all nine original TDF/SPL hashes, route order, grids and per-line
  point counts; hashes of the two retained receipts, fixture and launch helpers.
* `check_case.py`: independently checks ordered normal phases, actual route,
  sole CR-X grid position, movement and spline bounds, natural expiry, results,
  return, terminal game/display processes and seven exact BMP/PNG RGB pairs.
  No virtual-pad inputs occur between the first HUD and settled results.
* All seven positive scores appear in the native record log and new global
  `ModSaves/SAVE/HS/USERDATA`. Every complete109-record table is decoded: only
  the tested route changes to Player/CR-X model4/the correct score; the other
  108 records retain exact defaults. Existing seeded files are unchanged.
* Six altered traces per case reject wrong route, missing Go/finish/results/
  return and a warning for an existing mesh. The exact original factory-absence
  policy from checkpoint40 permits only its established missing names; warnings
  remain in the raw logs. No parser or production diagnostics were changed.
* Seven contact sheets were manually reviewed: initial HUD, late HUD/driving
  and settled results, with readable crops,21 sampled states total. Each final
  visible score matches its independently checked record. Southside's late
  capture shows the autopilot stopped beside a wall; its timer and results still
  complete. No autopilot recovery, continuous traversal or full-art approval is
  inferred from that case.

`final-verification-receipt.json` records nine total workflows, no failed,
active, pending or terminal-evidence-pending cases. All three runner handles,
games and private displays exited zero. All908 critical source/build inputs,
six personal Linux save/creative files and every installed Wave2 file match
their established baselines. No source, runtime asset, APK, device or public
release changed.

## What this closes and what remains

Together with the existing studies, completed route workflows now cover all
**108 distinct original routes**:72circuits,18P2P,9Sprint and9Cruise. Sprint
includes27heats. The circuit study uses9normal-clock and63fixed-step cases;
P2P/Sprint/Cruise use ordinary clocks. These are differing evidence scopes,
not a claim that every route received the same timing, car or weather test.

Positive Showoff scoring and saved records are observed on eight routes; the
retained Angel North Punto pilot scored zero. Only Downtown has a separate
fresh native record-reload probe. Full trick/formula/Windows parity, every
camera/material/weather/light state, all later campaign/Arcade/team/network
branches and physical Android remain open. The strict original119-D8M inventory
failure remains. See `verification-closure-2026-10-29.md`; the goal is active.

Checkpoint43 follows checkpoint42, SHA-256
`526bae330bbc4cad9f208aa19539e5eb42114a7a6dc9cf5c5547f4b8fbc0dbf1`.
It also retains the bounded original mesh-factory follow-up documented in
`legacy-factory-callers-verification-2026-10-31.md`.
