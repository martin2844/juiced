# Naturally earned Novice completion — 2026-10-30

The isolated Player progression continues from the four-completion archive in
the r4 checkpoint: three Novice circuit wins and one Juice Games circuit win.
No race parameters, progress flags, car performance or opponent behavior were
changed. The user's RP5 remains untouched; all new runs are on Linux.

## Three-heat sprint

Three actual current-development-renderer workflows exercise retail event 3,
Engstrom_Sprint01, night/dry, the original tuned SRT-4 against FTO/Golf/Celica.
The first uses diagnostic autopilot with seed 43. The other two use ordinary
virtual SDL pad input, manual shifting and NOS, without autopilot or `--test`.

| Driver/input | Heat results | General result |
| --- | --- | --- |
| Autopilot, seed 43 | Second: 11.42 / 11.43 / 11.40 s | Loses; archive byte-identical |
| Initial manual timing, seed 42 | Second: 11.58 / 11.46 / 11.44 s | Loses; archive byte-identical |
| Earlier manual shifts, NOS from second gear, seed 42 | First: 10.83 / 10.84 / 10.85 s | Wins with 18 points; final unlocked |

The initial manual captures show late changes; the revised input timing is a
driver correction, not a production fix or easier event. Full RT and stock
native NOS/gear-up button samples are recorded. The successful final leaderboard
shows Player 18, Poppa 12, Melody 4, Carlos 2. The original opponents run normal
AI. Selected shift, results, leaderboard and unlock captures were manually read;
the full capture set is not claimed as complete visual certification.

Normal result/continue/autosave opens the Novice final. Independent inspection
validates the whole archive hash and exact entire ArcadeUnlock payload: only
event 3's completion, event 5's lock, Eclipse car flag 19 and Downtown_R1 route
flag 25 are allowed to change. Other archive entries remain identical. The two
losing runs leave the entire archive unchanged. Five missing-start/false-win
controls reject incomplete or unearned evidence. Total completions: **4 -> 5**.

Development ELF:
`e2c88a434d4b9740004ba1ceea6f3e7fdd8d95687d86d126b84e136f5d53e449`.

## Actual Nix r5 final and fresh reload

The newly built Nix r5 wrapper loads the naturally earned profile. The visible
original final specifies Eclipse GSX 377 bhp, Downtown_R1, four laps,
afternoon/clear and five opponents. Six Eclipse entrants are verified from the
runtime log. Diagnostic autopilot drives the player; the final completes first
at **209.14 s**. The UI displays **SERIES COMPLETE**.

The actual save-device autosave changes only ArcadeUnlock.sav. Independent
whole-payload comparison allows only the final completion dword and the original
bonus-route flags for Speedway_R2_rev and Anderson_R1. The rest of the 15 series,
car/route flags, developers/extreme flags and non-Arcade archive entries remain
unchanged. Total completions: **5 -> 6**: five Novice plus one Juice Games event.

A separate Nix r5 process visibly shows Novice **COMPLETED** and all five event
ticks. Loading leaves the archive byte-identical, SHA-256
`e91903228b7013f506da0d07d4d6346b3a984eca4e7e8bbeb95762d5e2f8a9a6`.
Both games and private displays exit zero. Eleven missing-phase controls reject
incomplete final workflow evidence. BMP-to-PNG conversion preserves every RGB
pixel; final/unlock/reloaded completion views were manually inspected.

One diagnostic navigation assumption needed correction in the same live process:
the controller expected Arcade_Race after Series Complete, but the original
flow correctly returns to Arcade_Championship. One additional ordinary A input
reentered the race menu before final inspection. Its separate input log is kept.
No game was restarted or save modified to hide this fixture error.

Actual packaged ELF:
`68d4fa9ec6b1a7838d90e2659ec470326877518c13015528cb934cef1510adb8`.
The source/build/tool payload matches the corrected development source. No
production code changed in this follow-up; earlier fixes remain separately
verified. Private Xvfb/llvmpipe, virtual input and dummy audio do not certify
physical controller, audible mix, hardware FPS or RP5 behavior. Later series,
complete Arcade/career and broader route/network/art checks remain open.

Evidence: `out/validation/linux-current-20261029/full-race-ui`:
`arcade-novice-sprint-{current-43,manual-42,manual-timed-42}`,
`arcade-novice-final-r5`, `arcade-novice-final-r5-reload`,
`verify_novice_sprint.py` and `verify_novice_final_r5.py`.
The package runtime receipt is in `linux-package-r5/runtime-receipt.json`.
