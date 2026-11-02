# Sprint matrix and Showoff pilot — 2026-10-31

All nine original Sprint routes completed three heats each on the unchanged
Linux development executable. Every heat has four distinct finishers: 27 heats
and 108 finishes. All nine final displayed point totals match the independently
summed finishing places. This is runtime verification, not a new engine build.

Development executable SHA-256:
`556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f`.
The normal launcher still uses this executable. Packaged Nix r14, Android APK26
and car bundle r8 are unchanged; the RP5 remains APK19 / pack r5 and was not used.

## What ran

Each case entered through title, profile and normal custom-race menus, selected
Sprint and the district, then completed all three heats, result screens,
leaderboards, Continue and menu return. There was no `--test`, `--track` or
fixed-step clock. An owned synthetic route-unlock profile, a virtual SDL pad,
autopilot, private Xvfb/llvmpipe and dummy audio were used. This does not establish
natural unlock progression, physical controls, audible output or frame rate.
Player used the stock Punto; opponents came from ordinary native selection.

| District / original route | Final displayed points, descending | Player points |
| --- | --- | --- |
| Angel North / Angel_North_sprint01 | 16, 12, 6, 2 | 16 |
| Campbell Hills / campbell_sprint01 | 18, 10, 4, 4 | 18 |
| Downtown Angel City / Downtown_sprint | 14, 14, 8, 0 | 14 |
| East Angel Island / East_Angel_SPRINT | 16, 10, 6, 4 | 16 |
| Engstrom / Engstrom_sprint01 | 18, 12, 4, 2 | 18 |
| Southside beach / SOUTHSIDE_SPRINT01 | 16, 8, 6, 6 | 16 |
| West Anderson / Anderson_SPRINT | 18, 12, 6, 0 | 18 |
| Angel Westside / AngelWest_SPRINT01 | 16, 14, 4, 2 | 16 |
| Angel SuperSpeedway / Speedway_sprint01 | 14, 8, 8, 6 | 14 |

Route names and ordering come from the original TDF/SPL references in
`sprint-sweep-r14/plan.json`; internal district names can differ from UI labels.
The tied Downtown table is observed evidence,
not a general proof of all tie-breaking behavior.

## Independent checks and visual review

Under `out/validation/linux-current-20261029`:

* `sprint-sweep-r14/final-verification-receipt.json` checks all nine original
  route references, three starts/results with two restarts, distinct cars and
  places, positive finish times, player consistency and original spline bounds.
* `sprint-sweep-r14/check_case.py` sums 6/4/2/0 points per heat. Seven copied-trace
  negatives reject wrong routes, missing phases/finishers and an unexpected
  warning for an existing original mesh. The exact original factory-absence
  policy from the preceding P2P study remains separate from the strict D8M gate.
* Every saved native BMP/PNG pair has identical RGB pixels and a capture
  acknowledgement. Nine contact sheets, each containing driving, heat results
  and leaderboard for all three heats, were manually reviewed: 81 panels.
  `visual-review-final-receipt.json` binds the review files and displayed points.
* Native Sprint legitimately adds `ModSaves/SAVE/HS/USERDATA` to each isolated
  fixture. The verifier checks its entire 109-record table: only the tested
  route changes, driver Player, Punto model, best **player** heat time; the other
  108 records retain exact defaults. Existing seeded profile files are unchanged.

All three queues, games and displays terminated normally; none remain active.
All 908 critical source/build inputs, six personal profile/creative files and
every installed Wave2 file match the established baseline. No personal saves
were seeded or overwritten. No production parser, warning or asset changed.

## Showoff: timer verified, positive scoring still open

`full-race-ui/showoff-normal-entry-r14-after-disk` is a separate normal-menu
Angel North Cruise/Showoff pilot. The original SPL was independently read;
the runtime used one Punto. Captures show countdown 02:59:93, 02:24:89 and
00:29:92. Natural finish occurred at 180.01 game seconds, approximately
185.108 wall seconds after observed Go. Final score was **0**; Continue returned
to the custom menu, and game/display exited zero. No pad commands occurred
between the start and settled-results captures. No high-score file was added.

`verify_showoff_after_disk.py` independently verifies the original route,
movement/waypoint bounds, natural finish, return, image parity, terminal
processes and unchanged profiles/source. Five altered traces are rejected.
This verifies the timer/results workflow only. It does **not** verify positive
tricks, combos, scoring or record persistence. Zero from this autopilot run
is not itself evidence of a scoring defect. Eight other Cruise maps remain open.

The earlier `showoff-normal-entry-r14-pilot` stopped at setup because the root
disk filled. It never loaded a track or started a race; its trace and scope
receipt are retained and it is not counted as a pass.

## Disk recovery and retained artifacts

Only superseded compressed Linux runtime exports r4/r5/r8/r9 were retired in
this pass (1,278,779,956 bytes). Exact source archives, checksum sidecars, prior
receipts and permanently rooted compiled Nix outputs remain. Combined with the
preceding r10/r11 retirement, 1,918,181,872 compressed-export bytes were removed.
See `out/releases/RETIRED-EXPORTS-20261031.md` and the two cleanup receipts in
`sprint-sweep-r14`. Current r14, APKs, car bundles and original assets remain.

The inactive test emulator's 8,379,432,960-byte QCOW2 was first preserved in RAM
to recover disk space. Compression barely reduced it and failed the reserve
check; no permanent compressed archive resulted. It was subsequently copied to
the secondary Ediloco disk and read back with identical SHA-256:
`7a791cea927d43580555efeedf71b7784cf86928f94e989c993387f15dc09021`.
The verified duplicate in RAM was then removed. Durable backup:
`/run/media/martin/Ediloco/OpenJuice-backups/20261031-retired-test-avd/userdata-qemu.img.qcow2`.
`sprint-sweep-r14/durable-avd-backup-receipt.json` records the copy/readback;
`out/android-avd/STATE-RETIRED.md` explains restoration. This is test-emulator
state, not RP5 data. Codex histories/database, SDK/NDK and user data were untouched.

## Remaining scope

Together with prior evidence, all 72 circuits, 18 P2P and nine Sprint routes
have completed workflows; timing and visual limits differ by study. Showoff
positive scoring and eight Cruise maps, broader campaign/Arcade/team/network,
Windows fidelity, physical Android and final distribution review remain open.
The strict original 119-D8M integration failure is unchanged. No full-game,
60-FPS, Android, complete art/handling or release-publication claim is made.

Checkpoint 41 follows checkpoint 40
(`OpenJuice-verification-checkpoint-20261031-p2p-complete.zip`, SHA-256
`dca943b3ac9d6c706fce4287b39de8bb7ab198a469d98c0a03a832f3bae55c16`).
The broader goal remains active.

Subsequent checkpoint42 closes the targeted positive-score/record/reload gap:
stock CR-X on Downtown scores36, saves the correct record and a fresh native
process shows RECORD36/SCORE0. The preceding zero-score pilot remains valid
and unchanged. Seven other Cruise maps and broader trick/formula coverage
remain open. See `showoff-positive-verification-2026-10-31.md`.
