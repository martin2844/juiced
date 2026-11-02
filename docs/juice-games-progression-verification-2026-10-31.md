# Current Linux Juice Games progression — 2026-10-31

The naturally earned private Arcade profile advanced from six to seven completed
events after a real RSX win. Three losses left its progression archive exactly
unchanged. A fresh native process loaded the earned RSX tick before the next
race. Juice Games remains **2/5 complete**; its final and the next series remain
locked. This is partial progression evidence, not series or game completion.

Development executable SHA-256:
`556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f`.
No production source, car asset, Nix package, APK or usual launcher changed.
RP5 was not accessed; its APK19 / car pack r5 remain unchanged.

## Earned profile and original events

The seed is the previously earned Novice-complete profile, with one existing
Juice Games Corsa win, SHA-256
`e91903228b7013f506da0d07d4d6346b3a984eca4e7e8bbeb95762d5e2f8a9a6`.
No route/car/progress flags, race parameters, AI level or car performance were
edited. Each process loads a copy of the preceding process's actual saved
profile. Original `ArcadeRaces.txt` is independently read and SHA-256 bound;
all75 event records and stock model/route indices are parsed separately. None
of the nine enabled packages supplies an asset/patch override for that file.

All four runs use normal title/profile/Arcade menus, seed42, the original
three-lap afternoon/dry events, six entrants and script-specified tuning:

| Event / attempt | Model and visible rating | Route | Player result | Total completed |
| --- | --- | --- | --- | --- |
| Juice Games2 | Prelude VTi301BHP | Engstrom_r1_rev | Second,166.18s | 6→6 |
| Juice Games2, separately identified retry | Prelude VTi301BHP | Engstrom_r1_rev | Third,168.66s | 6→6 |
| Juice Games3 | RSX Type-S350BHP | Southside_r3 | First,220.96s | 6→7 |
| Juice Games4 | Neon R/T257BHP | EAST_ANGEL_r3 | Third,197.96s | 7→7 |

Runtime logs identify the actual original routes; UI calls Engstrom San Ricardo.
Autopilot, a virtual SDL pad, private Xvfb/llvmpipe and dummy audio are explicit
test conditions. There is no `--test`, route override or fixed-step clock.
No pad commands occur between the first race capture and settled results.
Autopilot sets the player's car to AI control, so its normal manual NOS path
does not process pad NOS; no hybrid-NOS or physical-input claim is made.

## Independent save and result checks

Under `out/validation/linux-current-20261029/arcade-juice-games-r14`:

* `state.py` independently decodes each complete archive, checks its rotating
  checksum and compares the entire ArcadeUnlock payload against earned or lost
  results. All other archive entries remain byte-identical. Losses preserve
  the complete progression archive's SHA-256, not just selected flags.
* The RSX win changes only the completion dword at payload byte60, from0to1,
  plus the archive checksum. The resulting archive SHA-256 is
  `a0e6558debcfb087bd34dd7186a5b49e42a431ae8e69bbb1c6f653842f4b958b`.
  Car/route/developer/extreme flags and unrelated completions are unchanged.
* Native global lap records legitimately update even on losses. The independent
  verifier checks every109-record table: only the current route changes to
  Player/the correct model/best recorded lap; all other108 records are identical
  to the preceding table. This is separate from progression autosaving.
* `check_case.py` checks original route/time/weather, all six original models,
  player/AI roles, three laps, six distinct places/finishers, consistent player
  finish, results/return/quit, and autosave only for the real win. Six altered
  traces per case reject missing phases, incorrect routes and invalid save or
  existing-mesh-warning evidence. Separate negative controls reject granting a
  win to the unchanged loss archive and a checksum-corrupted temporary archive.
* All25 native BMP/PNG pairs have identical RGB pixels and acknowledgements.
  Four contact sheets/15 sampled states were manually inspected. The last
  process's initial event selector shows earned ticks1and3 after a fresh profile
  load; its loss leaves those ticks and the entire seven-completion archive
  unchanged. UI centiseconds sometimes truncate one hundredth below the log's
  rounded two-decimal value; places and the independent saved result agree.

The first controller script incorrectly expected a progression autosave after
losing. Its original failed observation/error remain; a separate review and
independent loss verifier establish the actual normal return and unchanged
archive. The native game/display/wrapper exited0; the erroneous finish driver
exited1. A late capture request was correctly rejected after termination; no
image is claimed. The next driver waits for progression autosave only on wins.

The same first run initially used Down on the horizontal championship selector,
leaving Novice selected. Its early captures remain; Right selected Juice Games
in that same process before the race. No game was restarted to hide this
navigation correction. The winning Novice ticks are also visible in the current
executable's early fresh-profile view.

All908 critical source/build inputs, six personal Linux save/creative files and
installed Wave2 match their baselines. All owned games/displays are terminal;
there are no active or falsely counted progression runs. These are private
test profiles, not the user's personal saves.

## Disk recovery

296 BMP captures belonging to already-terminal0 UI scenarios were copied to
`/run/media/martin/Ediloco/OpenJuice-backups/20261031-completed-ui-bmp`.
Every copy was flushed and hash-readback verified before replacing the original
regular file with a symlink. All PNGs stay in their original folders. This
offloads1,091,215,248 regular-file bytes; raw paths remain usable with Ediloco
mounted. The aggregate verifier rereads every archived copy and checks each
PNG hash, plus actual case RGB parity through the retained paths.

See `raw-bmp-offload-receipt.json` in the evidence folder and
`full-race-ui/RAW-BMP-ARCHIVE.md` for the complete mapping and restoration
information. No live capture, profile, game data, APK or source was moved.

## Remaining work

Juice Games2and4 still need real wins; final5 has not been run because it remains
locked. Next-series unlocking, full Juice Games completion and later Arcade
remain unverified. These autopilot losses are not a diagnosis of an unwinnable
event or a production bug. Broader campaign, fidelity, physical Android, source/
distribution review and the strict119-D8M gate remain open. No FPS, audio,
physical-control, full-art or Windows-equivalence approval is made.

Checkpoint44 follows checkpoint43, SHA-256
`8dffc3ba81580edcf42e9a66eb74b26bfc1196e7414e119a20d8feef7c29bd7f`.
The overall goal remains active.
