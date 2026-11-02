# Manual Sprint controls and live-heat grid correction — 2026-10-31

The current Linux development executable now rotates the starting lane between
Sprint heats. An actual normal-menu, three-heat manual-input run exposed that
`restartRace()` increments `RaceState::rules().heat`, while `gridPlace()` read
the original `TrackView::rules.heat`. That initial copy remains zero. The
StartingGrid helper was already correct; its callers supplied a stale heat.

`TrackViewLoad.cpp` now uses the live race rules when available, with the initial
rules as the pre-race fallback. The network/empty-grid early return is retained.
The default Linux launcher already points to `out/linux/juiced`; the next normal
launch uses this correction. No personal profile or game/mod asset was edited.

## Failing before and passing after

Both runs enter through title, load the same naturally earned private seven-event
profile, select Novice3 Sprint normally and use the original437BHP SRT-4,
Engstrom/San Ricardo night/dry course, four competitors and three heats. Neither
uses `--autopilot`, `--test`, a forced track, accelerated clock or edited race
parameters. Virtual SDL gamepad inputs supply throttle, upshifts and NOS. The
player remains `CarKind::Player` through driving; the game's normal post-finish
AI hand-off is distinguished from the manual samples.

| Observation | Before | After |
| --- | --- | --- |
| Development ELF SHA-256 | `556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f` | `e8450e7dac66c9eb0da844be4db06a074d486ebd7e485c4403f1245f2aaa4607` |
| Observed player grid indices |3,3,3 — defective repeat |3,0,1 — expected rotation |
| Logged player times, seconds |10.81 /10.83 /10.85 |10.84 /10.86 /10.86 |
| Player finishes / final points |1st /1st /1st;18 |1st /1st /1st;18 |
| Passive telemetry samples |3,907 |3,839 |
| Progression archive |Unchanged, seven completed events |Unchanged, seven completed events |
| Global route record |Route58/model42 becomes10.81s; other108 records unchanged |Entire existing record table unchanged |

The original SPL's six grid points independently establish the expected lanes;
positions, neutral velocity/throttle/brake, heading and full15-unit NOS tank are
checked at every grid capture. Physical settling moves the model origin slightly
behind/below the authored grid, so bounded x/z/y tolerances are explicit. In the
baseline, applying the rotating-lane contract rejects heat2. With the correction,
all three grid comparisons pass. No observed failure was excused in the new run.

The read-only observer is an out-only `LD_PRELOAD` helper. A compiler-derived
native layout and exact ELF vtable bind one stack-local TrackView via its embedded
host's self pointer. All game memory is read with `pread` on an `O_RDONLY`
`/proc/self/mem` descriptor; it neither writes game fields nor invokes game
functions. It forwards SDL's swap once and writes diagnostic JSONL. The layout's
velocity field is `CarBody::localVelocity`; the logged `speed_mps` is its forward
component. The two observer binaries have identical hashes. This helper is not
installed in the normal launcher or packaged with the game.

Observed throttle reaches1, internal gears advance2→3→4→5→6 (displayed1→5), NOS
activates and consumes its tank at the Sprint2.5-unit/s rate, and longitudinal
speed exceeds60m/s. An extra upshift request is correctly clamped at the SRT-4's
fifth forward gear. Timed virtual shifts are deliberately ordinary inputs and
are not a claim of optimal shift timing; the native late-change notices remain.

## Evidence and preservation

`out/validation/linux-current-20261029/manual-input-r15/verify.py` independently
checks full phase/finisher/player/point contracts, original grid/controls/NOS,
whole profile archives, global high-score tables and all52 native BMP/PNG pairs.
Twelve trace/telemetry negative controls per run reject wrong routes, missing
phases/finishers, AI driving, wrong positions, missing throttle/shifts/NOS use and
missing acceleration; the stale baseline rotation is a separate negative case.
All six contact pages were actually inspected:28 selected grid/HUD/results/
leaderboard/menu panels. Displayed times may truncate one centisecond relative
to rounded log times; observed final points agree with the independent totals.

Full native build and18/18 CTest pass. One of908 previously bound critical inputs
changes: `src/Game/Race/TrackView/TrackViewLoad.cpp`; the other907 remain exact.
The exact pre-fix source and ELF are retained under `manual-input-r14` for local
reproduction. The new `manual-input-r15/source-receipt.json` binds all908 current
development inputs and the new ELF; it is not a Nix derivation receipt.

The initial layout helper link attempt failed because inline header globals
required game libraries. Extracting its constant layout section from the object
avoided executing/linking those constructors; the failure log remains. The first
verifier draft also used lowercase `sprint` where the native TDF log spells
`Sprint`; the exact literal and corresponding wrong-route negative were corrected.
Neither was a game runtime failure. Both actual race drivers, game processes and
owned Xvfb servers closed normally. The pre-existing unrelated Xvfb was left alone.

Every installed Wave2 file and all six personal save/creative files retain their
previous hashes. RP5 was not accessed. Linux's installed car bundle remainsr8.
The signed APK26 and Nixr14 distribution remain unchanged and predate this new
development correction; carrying the verified fix into those candidates remains
pending. No public upload or commit was made in this pass.

This closes a manual throttle/upshift/NOS workflow and a concrete Sprint grid
defect. It does not approve braking, steering, downshifts, handbrake, physical
gamepad/touch, FPS/audio, Android runtime or Windows fidelity. Later natural
Arcade/career/team/network and the strict119-D8M legacy gate remain open. The
overall verification goal remains active.
