# Packaged Arcade progression — 2026-10-30

The actual Nix r4 ELF completes the first event in the naturally unlocked second
series, **Juice Games Series**: retail event 6, Campbell_R1, three laps,
afternoon/wet, four Corsas. The player finishes first at **167.16 s**. Ordinary
opponent AI runs throughout; diagnostic autopilot drives only the human car.
The event parameters and progress flags are not patched or manually granted.

The normal title/profile/Arcade/results/menu flow performs the actual save-device
autosave. Independent archive inspection shows **three -> four completions**.
Only ArcadeUnlock.sav changes. Within its entire payload the sole change is the
completion dword at offset 44, 0 -> 1: second series, first event. No later event
or series is incorrectly awarded. The archive hash validates.

A fresh packaged process loads this save and visibly shows 1/5 for Juice Games
and the first-event tick. Loading leaves the archive byte-identical, SHA-256
`93b0af0101b0a338478ac71fd0c336548d64b0325fb6e9c552ee499991f8a2f1`.
Both game and owned private-display processes exit zero. Thirteen missing-phase
or false-progress controls reject incomplete evidence. Source fixtures remain
unchanged; the profile is exclusively owned synthetic Player data.

ELF: `71d6760f224a2f06f046e8c0593ae21d70893f86fad00a1d4328bd7f141944a4`.
This race precedes the later Linux upload-queue correction and does not certify
that different binary with a full race. The initial preview captures preceded
completion of asynchronous car/mirror loading and are excluded from art approval;
the subsequent dedicated loading study distinguishes delay from missing models.
The first results capture is during its UI entrance animation; it supports the
visible first-place/time fields, not complete results-layout approval.

Evidence: `out/validation/linux-current-20261029/full-race-ui/arcade-regular-r4`,
`arcade-r4-progress-reload`, `arcade-r4-receipt.json`, and `verify_arcade_r4.py`.
The provisional scenario filename says regular-r4; the retail UI name is Juice
Games Series. This is not all-Arcade completion, physical input, audible output,
hardware/FPS, Android or RP5 approval. User saves/device were not used.
