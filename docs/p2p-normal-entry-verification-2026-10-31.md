# Point-to-point normal-entry pilot — 2026-10-31

The unchanged development r14 executable completes **Angel North P2P01 through
the normal title/profile/custom-race menu**, with six stock Punto competitors,
all six finishes, a settled six-position results screen, Continue back to the
custom group menu and normal exit. Player finishes fifth in 127.72 seconds;
first 126.14, last 127.96. This is one P2P pilot, not all 18 P2P routes or a
percentage of the full game.

## Controlled setup and actual flow

Evidence:
`out/validation/linux-current-20261029/full-race-ui/p2p-normal-entry-r14-unlocked/`.
Development ELF SHA-256 remains
`556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f`.
This run has neither `--test` nor `--track`; it uses normal timing, the installed
mod collection, virtual SDL pad input, autopilot, dummy audio and an owned
private Xvfb/llvmpipe display. It does not touch the user's desktop or RP5 and
does not establish physical input, audible output, GPU performance or Android
behavior. Unlike the circuit diagnostic, route and options are selected through
the actual custom setup UI.

The initial owned Player fixture only exposed locked-progress routes. That
setup was exited normally without starting a race and is retained separately
as `p2p-normal-entry-r14-forward`; it is not counted as a P2P race or game failure.
`prepare_p2p_fixture.py` creates a separate synthetic route-unlock fixture under
`full-race-ui/p2p-route-fixture/`, setting 109 route flags to zero (103 had been
locked) and recalculating the archive checksum. All other Arcade fields and
every other archive entry remain identical, including career, cars, settings
and help. The original owned seed remains unchanged. No personal save is
edited and no natural progression/unlock claim is made.

The native profile loader accepts that fixture normally. The menu shows
POINT-TO-POINT; the actual loaded scene is `ANGEL NORTH CENTRAL
Angel_North_P2P01`. The options keep their ordinary defaults. All six cars
cross the starting line, traverse the route and finish. The last results
capture waits 6.5 wall seconds after all finishers on this normal-clock path,
then Continue returns to the performance-group selection. Each virtual button
press and release and each capture/quit acknowledgement is retained.

## Independent verification and limits

`verify_p2p_pilot.py` checks the exact executable/probes, ordered normal menu,
route, Go, results and return transitions, six distinct car/position finish
events, terminal game/display and raw BMP/PNG RGB equality. It independently
extracts the original TDF/SPL, binds their hashes, confirms POINT2POINT/last
point 142, six grid positions and all per-line waypoint bounds. All six cars
are observed advancing through the final route points. Five copied-trace
negatives reject wrong route, missing Go, missing finisher, missing results
and missing return. No substitute fixed-step/circuit run is counted as P2P.

All six captured views were inspected: unlocked route menu, selected P2P,
race start, driving, settled six-position results and returned custom menu.
The locked-route setup view was inspected separately. Images establish these
states only; they do not establish Windows art/material/HUD parity or all
animation/menu-margin states. The custom HUD displays LAPS 1/1 in this run;
original-Windows P2P HUD equivalence is not established by this pilot.

Game PID 1556916, owned display PID 1556913 and wrapper handle 51999 are
terminal; driver handle 18423 also returned 0. The earlier locked setup wrapper
89902 returned 0. Final verification records no changes in the run's seeded
ModSaves files. All 908 critical production inputs, six personal Linux files
and every installed Wave2 file remain unchanged. No executable/APK/asset
update or public upload. Broader mode/route/career/reference/device requirements
remain active; the next P2P routes are not implied passes.
