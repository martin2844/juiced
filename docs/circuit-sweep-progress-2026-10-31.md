# Complete circuit traversal sweep — 2026-10-31

**All 72 circuit routes now have full-race evidence**, retaining ten previously
completed routes and adding 62 independently verified cases. Every race records
two laps, six distinct finishers/places and normal exit. No cases are failed,
active or pending, and all three runner queues returned terminal 0. The 19-route
snapshot in CP37 is historical and remains intact in that archive. This count
is circuit coverage, not a percentage of the whole port goal.

The preceding checkpoint was progress: nine normal-timing reverse-R3 races, a fixed
step pilot and corrected results-capture timing were sealed in CP36. This pass
turns that calibration into a disjoint three-queue sweep, checks all 72 circuit
TDF/SPL references independently and adds actual new completed races.

## Plan and execution

`out/validation/linux-current-20261029/route-circuit-sweep-r14/prepare.py`
finds exactly eight circuit routes per district in StockTables: 72 unique routes.
Nine reverse-R3 normal-timing races and the fixed-step Angel North R2 reverse
pilot retain their original receipt hashes. The other 62 cases are assigned
21/21/20 to three queues; the successful one-case bootstrap is retained and
skipped when its full queue continues. No existing case is silently restarted.

The unchanged development ELF is
`556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f`.
New cases use original game data, no mods, a stock Focus, two circuit laps,
night/wet, seed 42, autopilot, private support folders and actual Radeon offscreen
rendering. The existing test-only fixed 1/60 clock/uncapped-loop method is
explicitly different from normal timing. It does not measure FPS, Android,
audible mixing, human input or frontend navigation. Other route types are not
relabelled as circuits; the remaining 36 non-circuit routes are outside this batch,
while still part of the broader verification scope.

Every completed case requires its requested scene and Go, six distinct cars and
finishing places, both laps and normal exit. A mid-first-lap framebuffer is
captured after the player crosses the independently counted spline midpoint.
Results timing uses actual frame acknowledgements from the existing SDL probe:
the final image follows a baseline results snapshot by at least 360 frames,
therefore at least 6 simulated seconds under this fixed-step path. This exceeds
the original results text's 5-second duration/delay. It avoids relying on wall
sleep while simulation is accelerated. Driving/final BMP and PNG pixels match.
The one-case bootstrap records 368 frames, 6.133 simulated seconds, and six actual
finishes before all three queues start.

`verify.py` separately checks per-line spline bounds, per-car lap observations,
all capture acknowledgements/frame differences, raw/PNG hashes and pixels,
actual six-place finish events, command/executable identity and terminal PIDs.
It records actual live executable identities rather than treating a state file
as proof that work is running. `verify.py --all` requires all 62 new cases, no
pending/active/failed cases, no STOP marker, and terminal successful main queues.
The prior real missing-route and negative-control evidence is retained in CP36;
it is not counted again as a new test here.

## Failure and lifecycle handling

The first unverified case writes an exclusive STOP.json. Other queues finish
their current owned case and stop before starting another; traces and support
folders remain. New errors/unrecognized warnings are not silently approved.
The existing generated CMSGamble missing-text warning is retained explicitly.
An observation deadline is labelled as such, not as a permanent game-stall
finding. Existing partial directories require inspection instead of replacement.

Actual exec handles: queue 0 **79448**, queue 1 **60287**, queue 2 **15328**.
The successful bootstrap handle 18852 is terminal 0. Exact handles and case PIDs
are retained in `sessions.json` and per-case process/terminal/result receipts.
All three main handles have now returned terminal 0. No games from this batch
remain active. Existing cases were reused rather than silently restarted.
The independently checked complete result is `final-verification-receipt.json`.

## Images, preservation and limits

All **62 new driving views and 62 settled results views were inspected**.
Driving views show road, stock cars and HUD. The final results screens show
all six position/time/name/car rows and Continue. Source image hashes and review
page hashes are bound in `visual-review-final-receipt.json`, the initial
`driving-review-receipt.json`, three `reviews/*/review.json` batches and
`results-review/review.json`. Some racing cars are damaged or airborne, wet
highlights are bright and night barriers are dark. These samples do not establish
original-Windows material/lighting/damage/handling parity, every track viewpoint,
menu-margin correctness, added-car art or Android rendering.

All 908 critical production source hashes, six personal Linux save/creative files
and every installed Wave2 file remain identical. No production code, renderer,
physics, APK, user-save or asset modification. RP5 was not accessed. No public
upload/commit. The user was asked asynchronously whether final closure means
full-port audit or the recent car/APK work; until clarified, the existing broader
scope remains in force and independent Linux work continues. The overall goal
is active, including the remaining original 119-D8M gate and physical device,
reference/gameplay/art/distribution requirements. No smaller success criterion
is substituted by this circuit-only progress checkpoint.
