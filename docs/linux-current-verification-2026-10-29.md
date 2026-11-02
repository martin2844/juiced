# Linux current-worktree verification — 2026-10-29

The preceding turn made progress: Street Cars revision 3 was installed on RP5,
validated, and all nine exported save payloads remained identical. This checkpoint
separately verifies the current Linux source tree; it does not certify full game parity.

## Current evidence

- Rebuilt the default Linux targets from the current dirty worktree with
  `nix-shell shell.nix --run 'cmake --build out/linux -j4 && ctest --test-dir out/linux --output-on-failure'`.
- Build exited zero. All four registered CTest suites passed. The core runner
  reports 351 passed, zero failed. These suites cover core and mod behavior,
  not an end-to-end career playthrough or GPU appearance.
- The actual desktop wrapper at `~/.local/bin/openjuice` selects
  `out/linux/juiced` and repository mod defaults. Corrected the stale Linux
  guide that said it selected the older Nix result.
- `ldd out/linux/juiced` resolves native Linux SDL3, OpenGL and FFmpeg libraries.
  Upstream OpenJuice supplies the reconstructed engine; this is not a claim
  that this project independently recovered the original release codebase.
- Logs, CTest detail and executable SHA256 are retained in
  `out/validation/linux-current-20261029/`. This is a worktree checkpoint,
  not validation of a clean published commit.

## Completion remains unproven

## Native career race completed (runtime follow-up)

The corrected timed attempt was deliberately stopped after 6,046 frames:
random character-clip lengths shifted the confirmation earlier, so it still
did not enter a race. Its old session 99355 is terminal; do not poll/restart it.

Replaced blind timing with observed UI navigation on a private Xvfb display,
SDL X11, seed 42, original game data, `--no-mods --test --autopilot`. Test
support was `career-interactive` under the evidence directory. No normal user
profiles were loaded. The original race physics and finishing logic ran:

- Accepted T.K.'s $2,000 bet through the visible confirmation.
- Completed the two-lap introductory career event. The player finished first
  at 102.73 s; T.K. second at 118.12 s. `career-finish.png` shows the matching
  results and `career-interactive.log` records both physical finishes.
- Continued to CMSRaceResults: $5,000 prize, balance $43,000 after the initial
  stake. `career-prize.png` shows this.
- Won the $4,000 betting pot, balance $47,000 (`career-bet-won.png`).
- Paid the actual $796 repair bill, leaving $46,204; continued into the dealer
  (`career-postrace-dealer.png`) and purchased the $2,281 CR-X through its
  confirmation. Career menu visibly showed $43,923 afterward.

This establishes a finite race, reward, betting settlement, repair and dealer
purchase through the real UI. It does NOT establish persistence of this run:
`--screen menu` bypassed driver creation, leaving `GameData::saving == 0`.
The exit dialog correctly offered “Leave career mode?” rather than save/exit.
Next runtime test must start from normal profile creation, not this shortcut.
The process reached its 30,000-frame limit and exited zero. Xvfb was stopped.
No test process is intentionally left running.

This used a virtual display and is not a GPU-performance measurement. Source
mods embedded in the executable (e.g. the Gamble menu) remain present even
with package overlays disabled; “no mods” here means no external asset/script
packages, not a claim of an unmodified upstream executable.

Career runtime navigation follow-up: starting at the real `menu` window with
an isolated `career-runtime` support folder reaches DemoFlyby, CareerLoad and
the T.K. introductory betting screen. `career-nav3.png` was visually inspected:
the character movie, career money and betting UI render. The first long attempt
(`career-finite.log`, 12,000 frames, exit zero) did not start a race: Enter
accepted the default No on the bet confirmation. This is failed navigation,
not a completed race or a crash.

A now-superseded corrected attempt used presses
`1000:RETURN,1600:RETURN,2200:RETURN,3000:RETURN,3400:RETURN,4000:RETURN,4400:UP,4600:RETURN,5000:RETURN`,
12,000-frame limit, `--screen menu --test --autopilot --no-mods`.
Log/capture prefix: `career-confirmed`; former execution session handle `99355`.
It was stopped as described above; no race-completion pass is claimed for it.
These tests use the default
new-career data; they do not load or edit the user's normal profiles.

Prize-persistence follow-up: added `CareerRaceResults.PrizePersistsAfterReload`.
It supplies a synthetic first-place crew order to the real results controller,
checks that only the player's 1,000 prize is credited, accepts results through
the controller, writes a new driver archive to an isolated temporary directory,
then lists and reloads it. Money, accumulated crew prizes and event/round
completion survive. The complete integration run now passes **52 checks**;
all five CTest suites also pass. Evidence: `prize-integration-final.log` and
`prize-build-ctest.log`.

An initial expectation that `Career::stats.moneyReceived` persists failed.
Inspection of Career.h (+0x58 explicitly not saved) and Career::read/write
confirmed that these are transient counters, unlike profile money/crewPrizes.
The test now checks that boundary without altering the archive or engine.
The diagnostic failure log is retained. This covers results-to-disk persistence;
the supplied finishing order is not proof of completing a race through physics.

Integration follow-up: Linux previously omitted the existing Career,
CareerRaceResults and SaveDevice integration source files from its test target.
They are now compiled. Removed a byte-identical duplicate `PhoneEventsClass`
definition that prevented compiling CareerTests, and corrected its enum printf.
Running `juiced_tests <isolated-save-path> --game <original-game-data> --integration`
produced **43 passed, zero failed**, with no missing-data skips. Evidence:
`career-integration.log`. This includes actual career archive write/read,
new-driver round trip, overwrite confirmation and save-device state transitions.
The results-window test specifically checks that an unoccupied crew place does
not award money; it does not simulate winning a race.

The asset-independent SaveDevice suite is also registered in CTest so ordinary
Linux checks exercise disk persistence. The game-data-dependent suites remain
explicit runs with `--game`, avoiding a green CTest entry that silently skips
without legally supplied assets. Build and CTest evidence is in
`integration-build-ctest.log`.

Runtime follow-up: the current binary completed a 3,600-frame instant-race run
with exit zero, using `--no-mods -i --autopilot --test` and an isolated
`JUICED_SUPPORT_DIR=out/validation/linux-current-20261029/race-support`
(absolute path supplied). Log and BMP/PNG captures are beside the build logs.
The inspected final image shows the Focus, track, HUD and 120 mph after 40.90
seconds of race time; telemetry advances through route node 40. The instant-race
configuration uses 99,999 laps, as the HUD confirms: extending this same test
is not a practical career-completion test. Use a finite career event for the
remaining finish/reward/save/reload check. The run did not use normal saves.

Also corrected the guide's default save location to `ModSaves`, matching
`sharedSaveDirectory()` rather than the historical `saves` path.

Historical captures demonstrate native menus and a running race. The current
automated suite does not demonstrate an entire career, every car/track/shader,
online functionality or complete behavioral equivalence to the original.
The next runtime verification should use an isolated support directory and
exercise career race completion, save/reload and workshop transitions, preserving
the user's normal profiles. Current car visual limitations are recorded in
`street-cars-visual-repairs-2026-10-29.md`; loader checks do not close them.

The original byte-matching reconstruction workflow was superseded by the user's
explicit instructions to use upstream OpenJuice and behavioral verification.
Do not restart the abandoned binary reconstruction or treat these tests as an
original-executable byte-matching oracle. The broader goal remains active.

## Saved-profile career loop verified (2026-10-29)

This follow-up closes the first-event runtime persistence check above. It does
not close the full-campaign or all-car/track verification gaps.

The native binary booted normally, without `--screen menu`, with a newly created
Player profile in an isolated absolute `JUICED_SUPPORT_DIR` under
`out/validation/linux-current-20261029/profile-runtime`. No user saves were used.
Arguments: `--game /home/martin/Projects/juiced/game-data --no-mods --seed 42
--test --autopilot --frames 60000`. The private Xvfb display was mapped for real
SDL keyboard navigation and screenshot inspection. Autopilot drove the physical
race; no finishing order or rewards were injected. This is not an FPS benchmark.
`--no-mods` disables external overlays, not compiled-in additions such as Gamble.

Verified sequence: normal profile creation and save; first career race completed
in first place; prize and accepted bet settled; damage repaired; black Honda CR-X
purchased; Save and exit; Quit; a separate game process launched; Player selected
in Profile Manager; Load Successful; Continue Game; Workshop opened.

Balance accounting: 40,000 initial minus 2,000 stake plus 5,000 prize plus 4,000
bet pot minus 1,800 repairs minus 2,335 purchase = **42,865**. The restarted UI
shows that balance and renders the purchased CR-X (class 8, 152 BHP, FWD).
The before/after save directories were independently read using the existing
UserSaves integration suite: each passes 10 checks, including archive hash and
career roundtrip. After-save contents report `garage 1: CRX_STOCK` and money
42865. The test target now includes UserSavesTests.cpp on Linux.

Evidence under `out/validation/linux-current-20261029/`:
- `profile-runtime.log`, `profile-reload.log`: initial and restarted processes.
- `profile-created.png`, `profile-race-won.png`.
- `profile-reloaded-career.png`, `profile-reloaded-workshop.png`: inspected UI.
- `profile-before-race/`, `profile-after-race/`: isolated save snapshots.
- `profile-before-inspection.log`, `profile-after-inspection.log`: 10/10 each.
- `profile-ctest.log`: `nix-shell shell.nix --run 'ctest --test-dir out/linux
  --output-on-failure'`, five suites passed. Direct ctest was unavailable on PATH;
  the recorded successful run uses the project's Nix environment.
- `profile-evidence-sha256.txt`: content hashes of binaries, affected test/build
  sources, logs, screenshots and save snapshots at this checkpoint.

Both game sessions exited zero through the Quit confirmation. The private Xvfb
was stopped afterwards; unrelated displays were left alone. Git base is
98c929ce0ac56eff029c7a58ecc816380d65bf67 with a dirty working tree, so the base
commit alone does not identify the tested binary. Use the evidence hashes.
No new original-EXE byte matches are claimed. Next broader verification is a
normal post-intro scheduled event using the purchased car, including workshop
changes and their persistence. Whole-game completion remains unproven.

## Post-intro workshop and scheduled Showoff (2026-10-29)

Continued the same isolated Player save through normal menus. Bought the class-8
Tuning Shop package for 1,887: CR-X power changed from 152 to 161 BHP and balance
from 42,865 to 40,978. Selected the January 8 Vixens Showoff from the calendar,
entered it, declined the optional tutorial, selected the owned CR-X and drove
with the existing autopilot. Downtown/Angel City/Downtown_Cruise01 loaded and
the three-minute timer expired normally. Results showed score 46 and total
respect +12 (the respect update allocated 8 and 4 to two rivals; this was not
12 respect for Sue). Continued through respect update to the repair garage,
paid the displayed 500 repair charge, then saved and quit normally.

A separate process loaded Player through Profile Manager. Verified visually:
CR-X still has 161 BHP and its upgrade decals, balance 40,478; calendar date is
January 8 and the Showoff is marked EVENT FINISHED. Both game processes exited
zero using Quit. The private Xvfb session was stopped.

Evidence in `out/validation/linux-current-20261029/`:
`postintro-runtime.log`, `postintro-reload.log`, `postintro-upgrade.png`,
`postintro-showoff-start.png`, `postintro-showoff-result.png`,
`postintro-reloaded-upgrade.png`, `postintro-reloaded-calendar.png`,
`postintro-save/`, `postintro-save-inspection.log` (UserSaves 10 passed, 0 failed),
and `postintro-evidence-sha256.txt`.

This adds coverage of workshop purchase, a scheduled non-intro event, its natural
timer/results path, repairs, calendar advancement and persisted tuning. It does
not verify competitive Showoff scoring or original-executable equivalence.
The January 5, 18 and 24 circuits/P2P require class 7 (some also prototype parts);
the UI rejects the class-8 CR-X as expected from their advertised requirements.
A normal post-intro circuit/P2P with a qualified car remains the next runtime
coverage gap. No engine code was changed in this verification follow-up.

## Save-verifier negative controls (2026-10-29)

Found and reproduced three false-success cases in the prior UserSaves audit:
an empty root returned `0 passed, 0 failed` with exit 0; corrupt USERDATA returned
2 passed/0 failed; missing juicey.dat returned 3 passed/0 failed. These did not
invalidate the earlier positive snapshots (which had ten actual checks), but
made the audit unsuitable for rejecting incomplete evidence. Reproduction logs
are in `save-verifier-controls/*-before.log` under the validation directory.

Updated `tests/Integration/Saves/UserSavesTests.cpp` to require a readable
registry, an enumerable SAVE directory and at least one driver candidate.
Candidates are enumerated directly rather than using the game's listDrivers,
which intentionally filters out broken USERDATA. Each candidate must have
readable/parseable metadata and a readable archive; failures do not dereference
missing files. The three entries unconditionally written by Frontend::buildProfile
(settings.dat, ArcadeUnlock.sav, UIHelp.sav) must exist. CMSGlobal.sav remains
optional, matching the writer's conditional career field. This verifier change
rejects incomplete fixtures; the game loader and actual saves were not modified.

Added repeatable calibration command (all mutations happen in temporary copies):

```sh
python3 tools/validate_user_saves.py out/linux/juiced_tests \
  out/validation/linux-current-20261029/postintro-save \
  out/validation/linux-current-20261029/save-verifier-controls/after
```

Eleven cases passed: valid and no-career profiles accepted; empty root, missing
registry, corrupt metadata, one corrupt profile beside a valid one, absent
archive, corrupted archive hash, and each of the three missing mandatory entries
rejected with exit 1 and an explicit FAIL. Entry-removal controls independently
re-encode the archive and recompute its hash, so rejection tests the missing-entry
check rather than checksum failure. Crashes do not count as successful rejection.
The unchanged played-career snapshot passes 20 checks with the stricter audit.

Build: `nix-shell shell.nix --run 'cmake --build out/linux --target juiced_tests -j4'`
passed. CTest remains 5/5 passing. Logs: `save-verifier-build.log`,
`save-verifier-ctest.log`, and `save-verifier-controls/after/{results.json,*.log}`.
Content hashes are in `save-verifier-evidence-sha256.txt`. UserSaves now expects
an actual save-root fixture; callers running all integration suites should pass
one explicitly rather than relying on an absent default save directory.
These controls calibrate save inspection only, not whole-engine equivalence or
all possible corrupt-file handling. The post-intro circuit/P2P verification gap
recorded above remains open.

## Hosted career point-to-point and unlock persistence (2026-10-29)

Continued the isolated Player profile through normal UI. The current dealer only
offered CR-X/Beetle/206 class-8 cars; inspected class-7 calendar events remained
ineligible. Entered the February 8 event as an attendee and left it with the
normal confirmation (closed without racing). Calendar navigation then selected
a later empty date and opened Host Meeting. The completed hosted event's results
identify its actual date as **March 29**; it was not the February 8 circuit.

Created a class-8 point-to-point on Super Speedway (route 1, 2.9 miles), paid
1,000, selected the owned 161-BHP CR-X and started without a side bet. The grid
contained Player, Biggi's Integra (195 BHP), and T.K.'s MX-5 (124 BHP). The native
runtime loaded the rainy track; existing autopilot drove the physical finish.
Results: Player 1:31.87, Biggi 1:40.99, T.K. 1:50.11. The reward screen credited
3,000 and the following screen announced newly unlocked CR-X parts. Respect
screen showed +170 total. Paid the displayed 500 repair charge and saved/quit.

Accounting: 40,478 - 1,000 hosting + 3,000 prize - 500 repairs = **41,978**.
The snapshot `hosted-p2p-save/` passes the strengthened UserSaves suite (20/20).
A separate process loaded Player normally. Tuning Shop showed the same balance,
the existing 161 BHP, and a newly available class-7 package giving 201 BHP for
4,198. That package was inspected but not purchased. This verifies unlock
persistence and supplies the next natural route to class-7 races. Both game
processes exited zero through Quit and the private Xvfb was stopped.

Evidence under the validation directory: `circuit-runtime.log` (despite its name,
this run completed a hosted P2P), `hosted-p2p-reload.log`,
`hosted-p2p-{race,result,prize,reloaded-unlocks}.png`, `hosted-p2p-save/`,
`hosted-p2p-save-inspection.log`, `hosted-p2p-evidence-sha256.txt`.
No source edits or save editing were needed. Runtime stayed `--no-mods`, on the
same native build; this is not a hardware performance benchmark.

A possible UI concern was checked against CareerRaceResults::update: TOTAL
WINNINGS counts the money remaining to transfer and reaches zero after the
balance rises. The observed zero was not loss of the 3,000 prize. The runtime
also logged missing MX5_SKIRT_L/R meshes; the visible grid and race loaded, but
that asset warning remains to be investigated rather than treated as resolved.
The early-career loop now covers intro race, dealer, tuning, scheduled Showoff,
hosted P2P, rewards, repairs and reload. Full campaign, other event types and
all-car/track visual fidelity still require broader validation.

## Retail factory-body warning investigation (2026-10-29)

Investigated the MX5_SKIRT_L/R warnings from the hosted P2P rather than suppressing
them or inventing replacement geometry. The retail cardefs block declares
STANDARD SKIRT MX5_SKIRT, but neither race nor UI archive contains those left/right
meshes (nor the alternate MX5_SSKIRT names). Searches also covered the runtime's
carmesh and mesh fallback paths. Both original mx5_stock.boh variants explicitly
have SSKIRT_L and SSKIRT_R attached to the mesh name NONE. By contrast, both
MX5_SSKIRT_VS1 left/right custom parts exist in each variant. Individual BOH nodes,
lookup results and hashes are in `mx5-asset-audit.json`.

Added a repeatable, read-only independent reference audit:

```sh
python3 tools/audit_stock_body.py --game /home/martin/Projects/juiced/game-data \
  --output out/validation/linux-current-20261029/stock-body-reference-audit.json
```

It examines STANDARD FBUMP/RBUMP/BONNET/SPOILER/SKIRT declarations (splitting skirts
into left/right), both race and UI variants, and the same three lookup folders as
CarModel. It uses the Python retail DAT reader independently of runtime GameFiles.
For missing resources it parses the original BOH hierarchy and records slot mesh
names plus BOH SHA256. On these local assets: 54 car-definition blocks, 610
references, 535 resolved, 75 missing; **all 75 missing references map to BOH slots
explicitly named NONE**. The catalogue hash and resolved resource hashes are in
the report. The script throws on unreadable/truncated hierarchies rather than
classifying unknown structure as an optional absence.

Conclusion limited to this evidence: the observed MX-5 warning is consistent
with absent factory geometry in the supplied retail assets, not a Linux-only
failure to find an existing mesh. No engine behavior, original archive, car
geometry or save was changed. Warnings remain useful for genuinely broken mod
references. This audit does not parse/render each D8M, audit custom kits/textures,
or establish visual fidelity. The report is an inventory, not a whole-game pass.

## Expanded Linux race integration coverage (2026-10-29)

The Linux test target omitted existing upstream integration suites for most race
controllers. Added HostMeeting and RespectChanges plus all eight current files in
tests/Integration/Race to cmake/Linux.cmake. These compile into juiced_tests;
no engine implementation or test expectation was changed. The additions exercise
host eligibility and challenge state, respect notes/history, Showoff trick values,
replay exit/save decisions, split-view render-target ownership, camera sweeps and
intro transitions, brake-part propagation, race shutdown/restart state, and HUD
speed units. NullRenderDevice/FakeCareerHost checks establish controller behavior,
not screenshot fidelity or live networking.

Before enabling these files, the full configured integration run with both real
game data and hosted-p2p-save passed 72 checks. Afterwards the same inputs passed
**105 checks, zero failures** (33 additional checks). Inputs are explicit so these
game-data-dependent suites do not silently skip for lack of --game:

```sh
nix-shell shell.nix --run 'cmake --build out/linux --target juiced_tests -j4'
out/linux/juiced_tests \
  /home/martin/Projects/juiced/openjuice-port/out/validation/linux-current-20261029/hosted-p2p-save \
  --game /home/martin/Projects/juiced/game-data --integration
```

CTest remains 5/5. Rebuilt and ran juiced_gl_check in the project Nix environment
on the real AMD Radeon RX 7800 XT (radeonsi, navi32, ACO, DRM 3.64, kernel 6.18.49).
It passed six independently coloured cubemap faces, repeated cube/2D switching,
and loading-thread texture creation. This hidden 64x64 GL check is independent
of the NullRenderDevice tests and is not a game FPS measurement.

Evidence: `expanded-integration-before.log`, `race-integration-build.log`,
`race-integration.log`, `race-integration-ctest-gl.log`, and
`race-integration-evidence-sha256.txt` under the validation directory. During log
inspection a compound shell command ended at rg with exit 1 because it found no
FAIL lines; the test log itself reports 105 passed/0 failed. The build and combined
CTest/GL command exited zero. No game process or private Xvfb was left running.


## Packed track integration verification (2026-10-29)

Enabled the existing CarDefs, CarPhysics, Tracks, Replay, ModelFiles, Effects,
EffectDefs, GroundMap and SceneObjects integration files in the Linux target.
The first real-data run returned exit 1: 143 checks passed and five failed.
Effects (two assertions), ground maps, models and tracks enumerated loose files
and found zero files of their respective types in this mixed loose/DAT install.
These failures remain evidence, not an engine correctness verdict.

TracksTests now enumerates track names from the built-in route catalogue, using
GameFiles to read their TDF paths, and retains additional loose TDF discovery.
It retains the original 9-track/108-route expectation and every geometry,
material, texture/light tile, region, racing-line and collision assertion.
Missing or invalid catalogue TDFs now explicitly fail instead of being skipped.
No production loader or game asset was changed.

Rebuilt juiced_tests successfully. Targeted Tracks run passed: 9 tracks,
108 complete routes, 1,759 layer reads, 435,153 instances, 108 collisions and
1,095 corners. A temporary symlink fixture excluding the tracks directory and
archive returned exit 1 with 10 failed checks (9 missing catalogue tracks plus
the aggregate assertion). The temporary fixture was deleted; original assets
and user saves were not changed.

The full integration rerun returned exit 1: **144 passed, 4 failed**. Remaining
failures are the two Effects assertions, GroundMaps and ModelFiles, which still
require loose-file enumeration. Do not report this suite as green. The next
verification work is to obtain a trustworthy resource-name inventory for those
packed fixtures, retaining completeness assertions rather than lowering counts.
This track parser validation is not a rendered/driven sweep of every route.

Evidence under out/validation/linux-current-20261029: game-data-integration.log,
packed-tracks-build.log, packed-tracks.log, packed-tracks-negative.log and
packed-tracks-full-integration.log. Source/log hashes: packed-tracks-sha256.txt.


## Packed effects verification (2026-10-29)

Added tests/Framework/RetailEffects.h: a 74-name retail FXO inventory. Names are
from the existing Scene3d initialization catalogue plus SceneRadialBlur documented
in docs/spec/11-effects.md. Completeness was independently checked against all
90 entries in the supplied effects.dat: inflate every payload, classify FXO by
0xFEFF0901 signature, and match its directory hash to a unique inventory name.
All 74 FXO entries match, with no missing or duplicate inventory names/hashes.
The other 16 entries are not classified as FXO. No shader binaries are embedded.
The repeatable read-only checker is tools/audit_effect_inventory.py; invoke with
--game /home/martin/Projects/juiced/game-data --output <report.json>.

EffectsTests now reads the inventory through GameFiles (packed or loose), while
retaining discovery of additional loose FXOs. It retains the original exact
74-effect and 15-expression assertions, known constant-table/parameter checks,
and preshader execution assertions. Missing inventory files count as errors.
Production shader loading/rendering code was not modified.

Build succeeded. Targeted Effects: 3 checks passed, zero failed; 74 effects,
211 shaders (82 constant tables), 15 expressions, 5 preshaders, no parse failures.
Independent negative control: replace SceneRadialBlur's inventory name with
material_missing; audit rejects the uncovered FXO hash. Runtime negative control:
a temporary symlink installation excluding effects and effects.dat returns exit
1, not a false pass. Original assets and saves were not modified.

Full integration run: **146 passed, 2 failed**, exit 1. Remaining failures are
GroundMaps and ModelFiles loose-file inventories. Shader parsing/evaluation is
not proof of pixel equivalence or that all materials render correctly on Linux.
Next task: resolve the ground-map resource inventory through the track catalogue,
then model-resource enumeration; retain the unresolved failures until verified.

Evidence: packed-effects-build.log, packed-effects.log,
packed-effects-inventory.json, packed-effects-inventory-negative.log,
packed-effects-negative.log, packed-effects-full-integration.log, and
packed-effects-sha256.txt under out/validation/linux-current-20261029.


## Packed ground-map verification (2026-10-29)

GroundMaps now enumerates the route catalogue's unique tracks and four light
sets, with both SQH (sunlight) and PQH (puddles) paths per set. This naming rule
is documented in docs/spec/15-tracks.md and used in TrackScene.cpp. Independent
Python DAT directory inspection confirmed all eight expected hashes in each of
nine archives; independent inflation recorded dimensions and SHA256 for all 72
payloads in packed-ground-inventory.json. This is coverage of the expected retail
light sets, not a claim of identifying all anonymous entries in the track DATs.
Additional loose SQH/PQH files are still included. Missing expected maps fail;
the previous >0 count assertion is strengthened to >=72. Quadtree reconstruction
and sample comparisons are unchanged (the existing test samples every third
pixel, not every pixel despite its introductory comment).

Build succeeded. GroundMaps passes: 72 read, 72 reconstruction comparisons pass.
A temporary symlink fixture without track archives returns exit 1: zero of the
72 expected maps read. No asset, save or production code changed. Full integration
run returns exit 1: **147 checks passed, 1 failed** (ModelFiles).

ModelFiles still requires a loose-file inventory of BOH/D8M/PMD. Its inherited
aggregate assertion also permits up to 119 D8M parser failures; that allowance
has not been independently audited and must not be presented as complete model
compatibility. Next steps: recover a trustworthy model filename inventory and
identify/triage individual parse failures instead of accepting an unexplained
aggregate allowance. No whole-game completion claim is supported by this run.

Evidence in out/validation/linux-current-20261029: packed-ground-build.log,
packed-ground.log, packed-ground-negative.log, packed-ground-inventory.json,
packed-ground-full-integration.log, packed-ground-sha256.txt.


## Model inventory and removal of numeric error allowance (2026-10-29)

The installer ZIP contains packed DATs, not a loose model-name manifest. Located
Ekey's Juiced 1 DAT tool (JCED.DAT.Tool, distinct from JCED2.JFS.Tool) and fetched
its filename list pinned to this source:

https://raw.githubusercontent.com/Ekey/JCED.DAT.Tool/a57a4b0da5bf6093f343bfc30339c51c71707fe4/JCED.Unpacker/JCED.Unpacker/Projects/FileNames.list

Source commit and SHA256 are recorded in Ekey-FileNames-source.json. The list is
kept in ignored validation output; no external executable was run. The independent
Python DAT hash resolver matches candidate names against directory entries,
rejects ambiguous hashes and unsafe paths, and builds a symlink-only test fixture.
Original assets and saves are unchanged. Repeat with:

```sh
python3 tools/build_model_test_inventory.py   --game /home/martin/Projects/juiced/game-data   --names out/validation/linux-current-20261029/Ekey-FileNames.list   --fixture /tmp/oj-model-fixture-new
out/linux/juiced_tests --game /tmp/oj-model-fixture-new --integration --suite ModelFiles
```

Resolved inventory: 428 BOH, 8,275 D8M (including route/non-mesh layouts), 10,034
PMD; 1,588 unknown archive entries, zero ambiguous name matches. Unknown entries
are not necessarily models and are NOT covered. The tool skips audio/attract DATs
and files lacking a structurally plausible generic DAT header. It records archive
directory hashes, not a completeness proof for every DAT container. A second
execution generated a byte-identical model inventory (cmp exit 0).

ModelFiles now optionally consumes .oj-model-inventory.txt in an isolated fixture,
retains loose discovery, reads listed paths through the real GameFiles loader,
rejects traversal/absolute paths, fails missing resources and prints individual
BOH/D8M failures. The original aggregate test returned pass despite 119 rejected
meshes. Removed that unsubstantiated allowance: it now requires zero rejected
meshes. No renderer or mesh parser behavior was changed to force acceptance.

Rebuilt successfully. Strict targeted test returns exit 1, 0 passed/1 failed:
428 BOH parsed, 7,929 mesh-layout D8M attempted (119 rejected), 10,034 PMD all with
material classes. All 119 failures are under global mesh/: 4 empty files and 115
nonempty. Individual paths, sizes, header words and payload hashes are saved in
model-parse-failures.json. Other archive D8Ms retain the existing route/folder
exclusion; they are not claimed as mesh-parser coverage. Determining whether the
rejected global meshes are referenced, shadowed by per-car resources, or legacy
unused files remains pending. Do not classify them as safe exclusions yet.

Evidence: model-inventory-report.json, model-inventory-reproduced.report.json,
model-inventory-{build,test}.log (inherited tolerance),
model-inventory-strict-{build,test}.log (strict failure), model-parse-failures.json,
model-inventory-sha256.txt. The next task is reference reachability and format
analysis for these named rejected meshes, not another count-only pass.


## Rejected-mesh BOH reference audit (2026-10-29)

Added tools/audit_model_references.py. It parses every node of each inventoried
BOH independently in Python (including duplicate node labels), retains references
and source hashes, and inspects the actual CarModel::mesh search order:
car-specific mesh -> carmesh -> global mesh, stopping at the first readable file.
Generic Engine::readMesh uses caller-selected folders, so the tool deliberately
does not claim a generic global BOH is loaded from a particular folder at runtime.

Read all 428 inventoried BOHs and 1,721 mesh references. Of 48 references whose
mesh name coincides with a rejected global mesh, 30 are in car-specific BOHs.
All 30 resolve to an earlier per-car file; none selects the rejected global mesh.
These are the Celica, CRX, Golf, GTR and RX7 root/chassis/brake-light references,
in race and UI variants. This does not prove all dynamic/custom part selections.
The other 18 references appear in global models, including garage and old stock
car/bumper trees. Their caller reachability remains unresolved. No failed asset
was excluded, deleted or converted, and strict ModelFiles remains failing.

Controls: a synthetic BOH with two roots sharing a node label retains both mesh
references (avoids dictionary-overwrite loss); removing its last byte is rejected.
These check the audit parser, not renderer equivalence. Scope excludes DLH, dynamic
part selection and source-code callers. Evidence: model-reference-audit.json,
model-reference-audit-controls.log, model-reference-audit-sha256.txt.

Reproduce:
```sh
python3 tools/audit_model_references.py  --game /home/martin/Projects/juiced/game-data  --inventory out/validation/linux-current-20261029/model-inventory-fixture/.oj-model-inventory.txt  --failures out/validation/linux-current-20261029/model-parse-failures.json  --output /tmp/model-reference-audit.json
```
Next: trace generic BOH callers (especially garage), and dynamic part names before
considering any named legacy exclusion. A raw name coincidence is not runtime
reachability; a per-car override is not proof that every global use is unreachable.


## Frontend/global model caller verification (2026-10-29)

GarageView::enterSel and its workshop branch explicitly load don_garage1, not
the rejected legacy garage mesh's BOH. Reviewed all direct loadModel/addModel
call sites: fixed showroom/scrapyard/garage/rolling-road/cutscene resources,
Respect territory names, CityMap UI_<route>_GUI and RaceHud UI_<route>_MAP.
Scanned every decompressed entry of ui.dat (2,394) and scripts.dat (330), without
requiring recovered filenames. All eight textual SetModel properties name
Angel_City. Per-entry hashes and lines are in global-model-ui-audit.json.
This scan alone is not a proof of all runtime-generated names or DLH use.

Added ModelFiles.FrontendAndRouteModels: reads caller-derived global BOHs and
recursively parses their global mesh references. Initial diagnostic required
GUI/MAP models for every route and reported 18 absences: two for each of nine
Cruise routes. These are absent in retail data; CityMap tolerates load failure
and RaceHud::prepareMap hides MAP on failure. Refined this newly added check to
allow absence only for Cruise GUI/MAP paths, while still parsing any such file
that is present. Existing strict EveryModel has not been relaxed.

Final targeted suite: 216 BOHs, 247 referenced mesh names, zero failures in the
new test, 18 explicitly reported optional absences. The suite overall returns
exit 1: 1 passed/1 failed, because the existing inventory test still rejects 119
global meshes. The new test proves parser compatibility of those caller-derived
resources, not rendering/material correctness, optional-map fallback behavior,
or complete whole-program unreachability of all old meshes. Shared carmodel
folder resources are outside this new global-folder test.

Negative control: a temporary installation linked to original assets except
models.dat/models fails the new frontend/routes assertion (exit 1). No original
asset, save or production loader changed. Build and diff whitespace check pass.
Evidence: frontend-model-test.log (initial diagnostic), frontend-model-test-final.log,
frontend-model-build-final.log, frontend-model-negative.log,
global-model-ui-audit.json and frontend-model-sha256.txt. Next: expand runtime
validation of showroom/workshop/route map rendering and audit dynamic car parts;
retain the strict legacy inventory failure until individually classified.


## Selectable custom car geometry (2026-10-29)

Added ModelFiles.CustomCarParts. Uses parsed retail CarDefs and PartCatalogue,
iterates custom entries of body categories 0-4,7,8 in both race/UI folders, expands
skirts/mirrors into left/right, and checks shared catalogue wheels/exhausts.
Mirrors CarModel's first-readable-file rule: car folder -> carmesh -> mesh, with
no fallback after a parse failure; shared parts search only carmesh. Factory
parts, headlights/rearlights with no fillBody implementation, neon, blur wheels,
textures/materials and rendered fit are outside this check.

Initial diagnostic: 5,642 requests, eight missing NONE_L/NONE_R paths, zero invalid
meshes, eight global selections. Inspection found explicit NONE entries, including
Corolla/Golf STANDARD SKIRT "NONE", which represent no geometry rather than a
custom mesh. Final test reports these separately instead of requiring filenames
for NONE. Earlier commentary's inferred 5,634 count was incorrect; authoritative
final output is **5,626 geometry requests, zero missing, zero invalid, zero global
selections, 12 NONE choices**. Counts are requests, not unique mesh files. This
supports non-use of rejected global meshes by these specific catalogue choices;
it does not prove absence of all possible global references or custom/modded data.

Build succeeds. Targeted ModelFiles suite returns exit 1: 2 passed/1 failed.
FrontendAndRouteModels and CustomCarParts pass; EveryModel still rejects the 119
inventoried global files and has no numeric tolerance. Negative control removes
carmesh.dat/carmesh from a temporary symlink installation; CustomCarParts fails
and process returns exit 1. Assets, saves and production code unchanged.

Evidence: custom-parts-test.log (initial diagnostic), custom-parts-test-final.log,
custom-parts-build-final.log, custom-parts-negative.log, custom-parts-sha256.txt.
Next runtime priority: rendered dealer/workshop verification across car choices;
parser success alone does not validate carbon, vinyl mapping, materials or wheels.
The legacy global-format audit remains unresolved rather than silently excluded.


## Native car-selector capture and input-harness limit (2026-10-29)

Executed the current native out/linux/juiced with retail assets, --no-mods,
--screen instant_selcar, --test and an isolated retail-render-support directory.
The automatic screenshot at frame 1200 was converted losslessly from BMP to PNG
and visually inspected: retail Viper GTS, body, wheels, glass, lights, and floor
reflection rendered, readable selector labels and 468 bhp. The red tint/LOCKED
label belongs to this fresh profile's locked selection; no unlock data was edited.
No claim of whole-car material fidelity, all-angle approval or FPS is made.

Automatic attempts with RIGHT (at end of catalogue), LEFT and page-down did not
produce a verified car switch. PGDN is not a recognized named CLI key: the parser
silently maps it to code 0; subsequent attempt used numeric 209 (0xD1). These
attempts are not evidence of successful navigation or custom part application.
Also discovered SDL preferred inherited Wayland despite DISPLAY=:0. A separate
run explicitly forced SDL_VIDEODRIVER=x11, mapped/focused the private Xvfb window
0x200034, and sent Left via xdotool (including separate keydown/up calls).
The capture retail-left.png still shows the Viper; navigation remains unverified
for this direct-screen fresh-profile setup. The cause is not established; do not
attribute it definitively to focus or an engine input defect. Next runtime test
must boot normally with a copy of the existing verified career profile, as in the
previous career verification, rather than interpreting direct-screen capture as
proof of navigation. Existing normal-boot career evidence remains separate.

The first runs reported the real AMD RX7800XT OpenGL renderer. The private X11
capture is not a performance benchmark. All launched game sessions finished or
were stopped with Ctrl-C (exit 0), and the owned Xvfb session was stopped. Other
projects' displays were not touched. No real user saves or assets changed.
Evidence: retail-instant-car.png/log, retail-left.png, retail-navigation-x11.log,
retail-instant-modified.log, retail-left-automod.log; retail-render-sha256.txt.


## Normal-boot selector and automatic modifications (2026-10-29)

Reused the verified hosted-p2p-save by copying it to a new isolated
normal-navigation-support/ModSaves. Started native juiced with --no-mods --test,
--frames 60000, SDL_VIDEODRIVER=x11 and a privately started Xvfb display, explicitly
mapped/focused the window. No --screen shortcut. Loaded Player using the profile
manager and its Load Successful confirmation, entered Custom Race from the main
menu, selected performance group G, and reached the unlocked Punto 1.8 HGT.
Left selected the Corolla (locked) with the corresponding Toyota model; Right
returned to the Punto. This confirms real keyboard navigation through normal
startup. It does not establish the exact cause of the previous direct-screen
harness failure; that shortcut bypasses normal profile/group selection.

Sent X11 Next (PageDown) to Auto-Mod: Punto changed from 116 to 209 bhp with visible
changed wheels, body kit, spoiler, retail vinyls/sponsor logos and green neon.
Tab/Clear Mods restored the stock appearance and 116 bhp. Screenshots of stock,
Corolla, modified Punto and cleared Punto were individually inspected. This is
rendered native gameplay-controller evidence, not a synthetic asset overlay or
save edit. Does not establish all modifications on every car or on Android.

Visual follow-up: modified Punto capture has conspicuous rectangular patches at
edges of the neon ground effect; investigate alpha/depth handling against retail
reference before claiming full rendering fidelity. No correction or root cause
is established yet. Existing NONE_L/NONE_R Corolla factory warnings recur, as
expected from the audited explicit NONE catalogue entries.

Evidence: normal-navigation.log; normal-punto-stock.png,
normal-corolla-stock.png, normal-punto-automod.png, normal-punto-cleared.png;
normal-navigation-save-comparison.json and normal-navigation-sha256.txt.
Both owned game and Xvfb sessions stopped cleanly (exit 0). Actual user profiles
were never used or modified. The comparison report compares common copied files,
not a claim that no extra runtime files were created. No FPS claim from this run.
Next: isolate the neon/reflection rectangular artifact with a stable view and
reference comparison, then resume normal career workshop/render validation.


## Neon artifact: original texture/effect inspection (2026-10-29)

Inspected both retail gen_neon resources without changing them. Global texture is
128x256 A8R8G8B8, alpha range 0..162, all edge RGBA channels zero. Shared cartexture
is 128x256 DXT5, alpha range 0..255, edge maximum RGB=(5,2,5), alpha zero. Decoded
with independent Pillow raw-BGRA/DDS support into textures-gen-neon.png and
cartextures-gen-neon.png plus separate alpha views. Shared texture was visually
inspected: four blurred light bars with transparent outer edges, no opaque border.
Metadata and original SHA256 are in neon-texture-audit.json. Does not establish
which texture was bound in the observed draw or certify GPU sampling.

Both global and shared gen_neon PMDs specify Coloured Spec ABlend. CarView's
colourNeon sets parameter 4 (additive), selecting Material_ColouredSpecABlendAdditive.
Built a standalone diagnostic against Core/Formats/Effect.cpp to dump the actual
retail compiled effect. Both programmable and fixed-function techniques specify
ALPHABLENDENABLE=1, SRCBLEND=5 (source alpha), DESTBLEND=2 (one), ALPHATESTENABLE=0,
ZWRITEENABLE=0. Linux GlesDevice maps blend enums to GL_SRC_ALPHA/GL_ONE and state
14 to glDepthMask. This is consistent static setup, not proof of effective states
at the draw. No blend-state "fix" was applied without a demonstrated discrepancy.

Evidence: dump-neon-effect.cpp and binary (local diagnostic), extracted FXOs
(private validation output only), neon-effect-build.log, neon-effect-states.log,
neon-texture-audit.json, PNGs and neon-audit-sha256.txt. Original asset bytes are
not added to tracked source. The visible rectangle's origin remains unresolved:
next isolate neon on/off at identical camera/pose and inspect effective draw
state, separating the neon plane from arcade shadow and mirrored-car overlays.
No game runtime was started this turn; no save or renderer code was changed.


## Real-GPU additive blend regression (2026-10-29)

Extended tools/LinuxRenderCheck.cpp's existing real GL regression with a
controlled additive blend/readback case. Uses the same backend and texture-sample
pixel shader, retail additive states SrcBlend=5, DestBlend=2, AlphaBlendEnable=1,
AlphaTestEnable=0 and ZWriteEnable=0. A synthetic 1x1 texel 0x00050205 matches the
retail compressed neon border's nonzero RGB/zero alpha; it leaves the background
0xff204080 unchanged. A half-alpha green texel 0x80008000 produces 0xff208080.
Each texel is first drawn opaque and required to return its own bits, proving the
shader/texture path is observable instead of accepting a missing draw. Repeated
opaque -> additive -> opaque transitions verify state restoration. This test uses
synthetic data, not copied game texture bytes, and ships no proprietary assets.

Command: nix-shell shell.nix --run 'cmake --build out/linux --target juiced_gl_check
-j4 && out/linux/juiced_gl_check'. Build and execution exit 0 on AMD Radeon RX
7800 XT (radeonsi, navi32, ACO, DRM 3.64, 6.18.49). Existing six cubemap faces,
cube/2D switching and loading-thread texture creation also pass. Diff whitespace
check passes. No production renderer changes were made.

Scope: verifies base blend factors and transitions with center pixel readback,
not the actual FXO/material binder, mip/sampler behavior, depth occlusion across
multiple surfaces, primitive clipping, shadow plane or mirrored-car overlap.
The visible rectangular artifact remains unresolved. Evidence narrows the next
investigation to scene/material state and overlapping geometry rather than an
unconditional error in GL_SRC_ALPHA/GL_ONE translation. No game/saves touched.
Artifacts: neon-gpu-check.log, neon-gpu-check-sha256.txt. The test window is hidden
64x64 and exited; it is not an FPS measurement or full-game completion evidence.


## Stable native car/neon A-B-A diagnostic (2026-10-29)

Added tools/LinuxCarRenderCheck.cpp and the Linux juiced_car_render_check target.
Loads retail GameData, a synthetic auto-modified Punto and the real CarView,
Scene3d/materials/OpenGL renderer, without loading or writing a profile. Camera
autoSpeed is zero; zoom is fixed. The normal scene geometry includes mirror,
arcade shadow and neon. Captures green neon, transparent neon, green neon repeat.
Instead of the initially proposed texture replacement, uses CarView::repaint with
neon colour/alpha 0, retaining the original texture and geometry. This isolates
colour contribution, not the presence of a potentially depth-writing neon plane.

Initial 30-frame and 400-frame captures failed repeatability due to still-settling
camera/zoom and initial material setup; kept as neon-isolation and
neon-isolation-stable, neither accepted as a controlled comparison. Final tool
sets current/target zoom explicitly, repaints before baseline and renders 1,000
zero-dt frames before each capture. v3 A and repeated A are pixel-identical in
RGB. A/B difference is 15,676 pixels in bbox (219,520)-(1001,616), exclusively the
light contribution in this view. The irregular rectangular reflection/shadow
edges remain visible in transparent-neon capture, narrowing the next diagnostic
to reflection/shadow overlap rather than a blanket additive-colour bug. The neon
plane remains present in B, so depth interactions are not ruled out.

Command: JUICED_SUPPORT_DIR=<isolated-dir> out/linux/juiced_car_render_check
/home/martin/Projects/juiced/game-data <capture-directory>. Build and run exit 0.
No production renderer/CarView code or original assets changed. Generated tool
uses native resource loading, not a remake. Synthetic UI lacks initialized button
icon textures (white icon boxes logged), and is not a launcher/UI quality pass.
It is a bounded diagnostic, not a full-game replay or performance test.

Evidence: car-render-build-v3.log, car-render-run-v3.log,
neon-isolation-v3/{neon-on,neon-transparent,neon-on-repeat,neon-difference}.png,
neon-isolation-v3/comparison.json, car-render-isolation-sha256.txt. Original BMPs
retained. All three final images were compared; transparent and difference views
were visually inspected. Next: remove the neon plane entirely, then independently
remove mirror and shadow, keeping the same camera/pose to identify the offending
geometry/pass. Visible artifact remains unresolved. Diagnostic process exited.


## Native layer isolation: reflected-car contribution (2026-10-29)

Extended juiced_car_render_check to retain the same loaded models/transforms and
select subsets of CarView::attach's four worldDraw callbacks (mirror, car, shadow,
neon). Tool asserts exactly four callbacks before using that documented order.
Added no-neon-draw, mirror-car-only, car-shadow-only, car-only and all-restored
captures after the prior neon A/B/A captures. No production code changed.

Build/run exit 0. Image comparison: neon-on == neon-on-repeat == all-restored,
pixel-for-pixel RGB. neon-transparent == no-neon-draw, also exact: in this scene,
the invisible neon plane produces no detectable depth/colour difference.
Visual inspection of mirror-car-only and car-shadow-only localizes the irregular
edge/bottom geometry to the mirrored car. Removing shadow and neon retains it;
removing mirror removes it. This narrows source of the visible symptom, but does
not prove it differs from original Juiced rendering or identify a Linux bug.

Reviewed CarModel::mirror: flags every mesh node as mirrored, negates root Y
column/translation. ModelNode::draw flips defaultCull around mirrored draws and
ends the material cache on both transitions. rotateY preserves that Y reflection.
No obvious missing cull flip was established; don't remove mirror or mask geometry
merely to make the screenshot look cleaner. Next compare mirrored-car winding,
underbody geometry and stock/reference rendering before changing the renderer.

Artifacts: layer-isolation-build.log, layer-isolation-run.log,
layer-isolation/*.png, layer-isolation/comparison.json,
layer-isolation-sha256.txt. All BMPs retained. Same synthetic UI limitations from
the previous harness apply (uninitialized button icon textures); this is not a UI
quality certification. Process exited; no saves or original assets touched.

## Runner selection calibration

The native test runner previously returned success for an unknown suite, despite
executing zero tests. This was a genuine verification false positive. It now
rejects empty selections and runs that execute zero checks with exit 2; unknown
options, missing option arguments and mutually exclusive kind filters also fail.
`--list` now applies the same suite/kind filters as execution.

`tools/validate_test_runner.py` runs ten subprocess cases with temporary save
roots. Seven invalid selections/arguments correctly fail; a filtered listing,
SaveDevice execution and the unit suite succeed. All ten cases passed against
the rebuilt Linux binary. CTest also passed 5/5. Evidence:
`runner-build.log`, `runner-calibration.json`, `runner-ctest.log` under the
existing evidence directory. No player saves were used. This fixes reliability
of the verifier; it is not new gameplay or full-game parity evidence. The strict
model inventory failures and remaining visual/runtime coverage are still open.

## Native movies coverage

Added the existing Movies integration suite to the Linux build and exposed the
shared character-clip harness as `juiced_movie_check`. The first integration run
failed because the test assumed eight audio tracks: independent little-endian
header inspection showed the local Biggi clip has five. The test now compares
the decoded track count with a separately read, validated BIK header, instead of
assuming one language edition. Original failure retained in `movies-integration.log`.

Rebuilt integration run passes 2/2: hub_01 frame 30 yields nonzero image data,
Biggi exposes the five tracks, and career_01 resumes at 10 frames/0.4 seconds
after a one-second stall and 6 frames/0.2 seconds after a short stall.
On the native Radeon OpenGL driver the character harness advances both the
explicit Biggi clip and lowercase/nonexistent-number fallback to frame 3.
After clearing black, framebuffer readback contains 306560 nonblack pixels in
each case. A genuinely missing movie is reported missing. This establishes
loading, advancement and drawing, not visual equivalence or audible playback.
The lowercase bet9 request resolves to bet1 by the existing numbered fallback;
it is not evidence that a bet9 asset exists.

Evidence: `movies-build.log`, `movies-rebuild.log`,
`movies-integration-final.log`, `movie-render-build.log`,
`movie-render-rebuild.log`, `movie-render-final.log` in the evidence directory.
No gameplay, decoder or original asset changes were necessary. No save tests
were selected and no saves were written by these movie-only checks.
Negative control with an empty asset directory exits 1 on the required Biggi
clip (`movie-render-negative.log`); missing assets cannot pass this harness.

### Independent visual comparison

The movie harness now accepts an optional capture directory and waits for the
actual frame counter to reach 30 (five-second timeout). Merely issuing thirty
advance calls only reached frame 3, since the player obeys its clock. The initial
`movie-captures` native files are therefore NOT frame-30 reference evidence,
despite their exploratory filenames; `movie-captures-timed` supersedes them.

The corrected run reached frame 30 twice, with pixel-identical native captures
for the explicit filename and the lowercase numbered fallback. FFmpeg 8.1.2
independently decoded zero-based frame 29 of BIGGI_ACCEPT_BET1.bik, scaled to
640x480 (`select=eq(n\,29),scale=640:480`). Inspected both images: the same
character pose, face, scene and orientation are present; no missing head or
gross colour corruption in this sample. Mean absolute RGB errors are
2.236/1.705/2.057 on the 0–255 scale, RMS 6.444/4.569/3.534. This is not an exact
pixel match; the decoding, colour conversion and scaling paths differ. No
arbitrary pass threshold was introduced. The independent reference is
`movie-captures/ffmpeg-frame30.png`; native capture, repeat and numerical report
are in `movie-captures-timed`; runtime log is `movie-capture-timed.log`.

Audio review confirms Movies supplies the audio system's device to BinkNative,
which creates and feeds a looping PCM buffer through the shared SDL audio
backend. The diagnostic deliberately sets audioFlags=1 and therefore does not
prove audible playback, audio quality or synchronization. These remain open;
the visual evidence is limited to one clip at one frame.

### End-to-end movie PCM output

Extended the diagnostic with opt-in `OJ_MOVIE_CHECK_AUDIO=1`: initialize SDL
audio and the real shared sound system, select English track 0 (the actual
English GameLoop setting), and stop audio after the movie system is destroyed.
The normal diagnostic remains silent. `OJ_MOVIE_CHECK_MUTE=1` keeps the output
device alive but disables movie audio for a negative control.

Ran with SDL_AUDIO_DRIVER=disk and SDL_AUDIO_DISK_OUTPUT_FILE pointing to the
evidence directory. SDL confirms F32LE stereo 48000 Hz. This exercises the native
Bink audio decoder, its streaming buffer, and the SDL mixer, without output to
the user's speakers. Captured 89088 frames (1.856 seconds) while playing the two
clip openings: all samples finite, peak 0.0201416, RMS 0.001926, 175652 nonzero
channel samples. The muted run produces a nonempty all-zero PCM file.

FFmpeg independently decoded the first English audio track to stereo F32LE at
48 kHz. Comparing the first half second of its left channel against the native
capture, searching delays 0–0.25 seconds, yields Pearson correlation 0.999612 at
1024 samples (21.33 ms). This is strong evidence for this clip's audio waveform
through the complete native output path; it is not a universal decoder proof or
a measurement of lip synchronization. The two capture durations differ because
the native run contains two clip openings and the reference contains one second.

Artifacts: `movie-audio.log`, `movie-native-audio.f32`,
`movie-reference-audio.f32`, `movie-audio-analysis.json`,
`movie-audio-correlation.json`, `movie-muted-audio.log`,
`movie-muted-audio.f32`, `movie-muted-audio.json`, build logs. The SDL disk driver
prints its configuration with an ERROR prefix even on success; process exit was
zero and the PCM evidence was inspected independently. No production audio code,
assets or player saves were changed. Physical speaker playback and long-run
audio/video synchronization remain unverified.

## Expanded frontend integration coverage

Linux now compiles thirteen previously omitted frontend test files covering
fifteen suites: AudioOptions, CarStats, Character, ModShop, ColourChoose,
GraphicsOptions, GameOptions, InstantSelType, InstantSetup, MainMenu,
MenuPicture, MessageWindow, ScreenShow, Startup and WindowStack. Removed one
verbatim duplicate definition of Startup.TitleForgetsTheSavesSettings; the
remaining definition retains every assertion. No production menu behavior was
changed to obtain passing results.

Ran each suite independently against retail game-data with a temporary empty
save root: **59 checks passed, zero failed**. These exercise the actual UI data
and frontend controllers through fake hosts/NullRenderDevice, so they certify
logic and state transitions, not GPU appearance or online service connectivity.
Evidence: `frontend-expanded-build.log`, `frontend-expanded-results.json` and
the fifteen `frontend-<suite>.log` files.

Then ran all enabled integration suites together against the independently
built model-inventory fixture and a temporary copy of `hosted-p2p-save`:
**210 passed, 1 failed**, process exit 1. The sole failed assertion remains the
strict model-file inventory parser check (119 D8Ms); it was neither disabled
nor allowed as a passing tolerance. The expanded frontend checks do not expose
new failures in this combined run. CTest separately passes 5/5. Evidence:
`expanded-integration-all.log`, `expanded-integration-exit.json`,
`frontend-expanded-ctest.log`. Original game data and player saves untouched.
This result supersedes earlier full integration counts for this build; the full
integration gate is still failing and full-game completion remains unproven.

## Rejected mesh structural classification

`tools/audit_legacy_meshes.py` independently reads all 119 rejected resources
from the retail archives. **Four are empty; all 115 nonempty files exactly fit
an alternate mesh layout**, not the release layout parsed by `parseMesh`:
44-byte finite bounds (w=1), vertex count, shared 36-byte vertex records, index
count, 16-bit indices, material-record count and fixed records to exact EOF.
113 files use 96-byte material records; two use 92-byte records. All material
names are printable and nonempty, all referenced index/vertex ranges fit their
pools, and every nonrestart index lies within the vertex pool. These observations
make a legacy-layout explanation concrete; unknown record fields still lack
proven semantics. This is classification, not a renderer implementation.

Six negative controls are rejected: truncation, trailing byte, zeroed payload,
oversized vertex count, out-of-range vertex index and out-of-range material
index span. Evidence: `legacy-mesh-classification.json` and
`legacy-mesh-negatives.json`. No production parser or assets were modified;
the strict integration gate still fails. Previously unresolved global BOH
caller contexts must be resolved before labeling these resources unused or
deciding that runtime support for the alternate layout is necessary. Do not
silently treat four empty files as valid geometry.

## Stock car caller-path verification

Added ModelFiles.StockCarTrees: enumerate actual car definitions, open both
race/interface BOHs from the paths used by CarModel::load, recursively visit
their mesh references and follow CarModel::mesh's first-readable search order.
Result: **104 trees, 624 references, zero global mesh selections, zero failures**.
The NONE/id=0 sentinel is explicitly excluded, following dealer generation's
model-1 start and its empty cardefs block; the initial run identifying its two
nonexistent BOHs is retained. Stock cars therefore do not select the rejected
global legacy meshes through their BOH references in this asset set. This does
not cover arbitrary script/mod requests or certify rendering/material quality.

ModelFiles now reports 3 passed, 1 failed: EveryModel remains strict and failing.
Evidence: `stock-trees-build.log`, `stock-trees-run.log`,
`stock-trees-final-build.log`, `stock-trees-final.log`.

Negative control: an isolated fixture unmounts celica.dat, supplies its real BOH
and an empty primary celica_root.d8m; the new check detects that exact primary
mesh failure (`stock-trees-negative-final.log/json`). Two earlier unsuccessful
fixture attempts are retained: loose data could not override the mounted DAT;
then removing the archive without supplying the BOH failed before mesh loading.
All attempts used temporary folders, never original assets. The successful
negative fixture also lacks other per-car meshes, so only its explicitly
reported primary-root failure is the intended calibration evidence.

## Class-7 career progression pilot

Continued the known hosted-P2P profile through normal UI on native Linux,
using a separate copy under `class7-support/ModSaves`. Command: `--game
/home/martin/Projects/juiced/game-data --no-mods --seed 42 --test --autopilot
--frames 120000`, SDL_VIDEODRIVER=x11 and a private Xvfb. No saves or race
results were injected. The existing diagnostic autopilot drove the physical race.

Purchased the newly unlocked class-7 tuning package for 4198: CR-X changed from
161 BHP/class 8 to 201 BHP/class 7, money 41978 -> 37780. The calendar now permits
the April 10 San Ricardo point-to-point (1.8 miles, free entry, 5500 first prize).
Entered against Biggi's 273-BHP CR-X, T.K.'s 226-BHP Focus and Sue's 259-BHP Integra.
Completed in third: Player 1:02.32, T.K. first 1:01.08, Biggi second 1:01.65,
Sue fourth 1:03.03. Settlement correctly credited zero winnings and retained
37780; a new-parts notice appeared. Paid the displayed 500 repair charge,
then used Save and exit career and the game's Quit confirmation (process exit 0).

Copied the resulting save into `class7-save/`; UserSaves passes 20/20 and reports
37280 cash. A separate process booted, loaded Player normally and entered the
workshop car selector: 201 BHP, class 7, good condition and 37280 cash persisted.
Accounting: 41978 - 4198 + 0 - 500 = 37280. Reload session and owned Xvfb stopped
after capture. Actual user saves were not used. This is additional career and
persistence evidence, not full campaign completion or a performance benchmark.

Evidence: `class7-runtime.log`, `class7-reload.log`, `class7-save-inspection.log`,
`class7-{upgrade,race-start,result,settlement,reloaded}.png`, `class7-save/`,
`class7-evidence-sha256.txt` (includes exercised binary hash). The pilot exercised
the existing playable Linux binary; this turn did not alter production engine
code. The latest test-only additions do not change the played binary.
