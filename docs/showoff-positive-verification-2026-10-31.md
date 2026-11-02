# Current Linux Showoff scoring and record reload — 2026-10-31

The unchanged development executable completed a normal-menu Downtown Showoff
with the stock 152-BHP CR-X, scored **36**, returned to the menu and saved that
record. A fresh native process then loaded the profile and displayed
**SCORE 0 / RECORD 36** at the beginning of the same route. Both processes and
their private displays exited zero. This closes the positive-score/record/reload
gap left by checkpoint41's zero-score Angel North timer pilot.

Executable SHA-256:
`556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f`.
No production code, assets, Nix package, APK or Linux launcher changed.
No physical RP5 access or public upload occurred.

## Conditions and observations

The prior route-only synthetic fixture correctly showed the CR-X as LOCKED;
that short process was closed at car selection, before any track/race. Its
capture and scope receipt remain separate. `prepare_crx_fixture.py` makes a
new owned fixture that changes only the CR-X car-lock field (model4,1→0),
recomputes the archive checksum and verifies every other payload byte unchanged.
The previous synthetic route-unlock flags remain. No personal profile is edited;
no natural car/route unlock or career progression is claimed.

Actual scoring run:
`out/validation/linux-current-20261029/full-race-ui/showoff-downtown-crx-unlocked-r14`.
Normal title/profile/custom/car/Showoff/location menus, auto-mod declined,
tutorial declined, one CR-X on `Downtown_cruise01`. Autopilot, virtual SDL pad,
private Xvfb/llvmpipe and dummy audio; ordinary game clock, no `--test`,
`--track` or fixed-step mode. The natural finish is **180.04 game seconds**,
approximately **185.000 wall seconds** after observed Go. The middle image
shows16points and DRIFT; the late image shows19points, DONUT as bonus and
Mega Wipeout. Settled results show36. These samples establish actual positive
scoring; they do not validate every trick or derive the scoring formula.

`verify_crx_showoff.py 36` independently reads original TDF/SPL references,
checks initial grid/waypoint bounds and movement, ordered normal transitions,
natural finish, the native route33 record log and return/terminal evidence.
All seven BMP/PNG pairs have identical RGB pixels and capture acknowledgements.
Six altered traces reject missing/wrong phases/record/route and an unexpected
existing-mesh warning. The observed Corsa spoiler warnings arose while browsing
the car list; they fall under the exact original NONE-slot absence policy
established in checkpoint40, not a new parser exclusion or invented mesh.

The only added private profile-tree file is `ModSaves/SAVE/HS/USERDATA`:
4,368bytes, SHA-256
`3412bc7ee4cb9d499ff45321358e2768df83d2cd8a353bb3c7d5c411f60b6000`.
Its complete109-record table is decoded independently: route33 has Player,
CR-X model4, value36; all other108 records retain exact defaults. All original
seeded profile files remain unchanged.

## Fresh native reload

`full-race-ui/showoff-downtown-crx-reload-r14` uses a fresh process and a copy
of the completed scoring run's support tree. Normal profile/high-score loading
and ordinary custom-race menus lead to the same route. The first HUD was
manually read: TIME02:59:91, SCORE0 and RECORD36. It then deliberately quits;
this reload probe is **not another completed race**. The entire seeded
ModSaves tree, including the record file, remains byte-identical.
`verify_crx_reload.py 36` checks that binding, image parity, normal phases and
both terminal processes. The independent scoring receipt keeps its original
`fresh_native_reload_verified:false` scope; the separate reload receipt supplies
the subsequent positive evidence.

All908 critical source/build inputs, six personal Linux save/creative files
and installed Wave2 remain unchanged after both runs. Original tests, warnings,
strict119-D8M failure and prior sealed checkpoints are retained. No Windows
score-formula/handling/art equivalence, physical input/audio, FPS or Android
approval is inferred. Two of nine current Cruise maps now have completed
workflows; seven remain, alongside broader campaign/Arcade/network and final
distribution requirements. The overall goal remains active.

Checkpoint42 follows checkpoint41, SHA-256
`42a97f3529bbea3e3ce1befd1390891f204f4a4fa935b6dfe49239c4d7ccbad9`.
It includes scripts, receipts, normal-run traces and captures, not raw profiles,
original assets or compiled binaries.
