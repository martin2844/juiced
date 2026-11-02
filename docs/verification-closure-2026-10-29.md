# Verification closure — 2026-10-29

**Scope clarification, 2026-10-31:** the user states the active objective is the
ten added-car ports. Use `car-port-scope-2026-10-31.md` as the active acceptance
scope. This broader engine ledger remains historical supporting evidence;
unrelated engine/119-D8M/Arcade work must not displace the ten-car finish.

The user's current goal is to verify everything and finish this work. This
tracks completion of the native Linux Juiced port, including the existing
features and mod integration. Evidence is from the actual worktree and original
game data. The broader conversation also includes Android/mod visual defects;
those remain requirements, not automatically closed by Linux checks.

This document is a closure ledger, not a declaration of completion. Detailed
commands and evidence live in linux-current-verification-2026-10-29.md and
out/validation/linux-current-20261029. Existing evidence must be tied to its
binary; a test-only rebuild does not certify a different released executable.

| Requirement | Evidence currently available | Outstanding work |
| --- | --- | --- |
| Current sources build natively on Linux/NixOS | Current development and clean Nix r14,18/18 CTest,908 critical inputs; actual development and Nix full six-car races/menu-race PCM,1345 exact source files restored;307-path fresh private import/content check, isolated installer/help and all23 runtime archive payloads verified | Broader runtime and final source/license/publication review |
| Reliable verifier |119 unit cases/388 checks and133 integration cases/407pass+1fail; every selected case executes checks;9 omitted portable sources now compiled; calibrated case/wire/keepalive negative controls | Strict119D8M failure remains; Windows-only/microphone backend exclusions explicit; deeper game coverage |
| Boot/profile/menu navigation | Normal boot, profile creation/load; virtual SDL gamepad through normal menu/career UI | Other menu branches and physical gamepad |
| Career progression and persistence | Intro win/bet/dealer, showoff, hosted P2P, class-7 race, upgrade/repair/save/reload; delegated pink-slip confirmation/autosave, actual AI win/loss races and independent ownership/reload checks | Later career, further host/crew/pink-slip variants and final progression |
| Race modes | All9 Sprint routes complete3heats/4finishers each, displayed cumulative scores independently checked; all9 Showoff routes naturally complete3minutes/results/return;8 positive scores/complete saved-record tables, Downtown fresh native record reload; prior Career showoff and six timed/perfect-lap/speed challenge runs; three four-car and one six-car controlled team races with independent teammate intensity/results/autosave/reload checks | Further Showoff tricks/scoring-formula coverage, team configurations, multi-round/career breadth and challenge targets/routes |
| Arcade progression | Naturally complete Novice: manual three-heat sprint win/unlock and actual r5 four-lap final win/save/fresh reload with all five ticks/bonus flags; two Juice Games wins including current RSX/actual save/fresh-load tick3; three current losses preserve exact progression archives; 0 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 completions | Later series/challenges and all-Arcade completion |
| Track coverage | 9 tracks/108 routes parse/ground maps; 18 Radeon morning/dry and night/wet samples; all72 circuit routes complete2laps/6finishes/exit0 (9 normal-clock,63 fixed-step),62 new driving/results pairs inspected; all18 P2P normal-menu/clock6finishes/results/return; all9 Sprint normal-menu/clock3heats/4finishers,81 sampled panels reviewed; all9 Cruise normal-menu/clock timer/results/return,21 new sampled panels; all108 distinct route workflows complete | Further tricks, weather/light and normal-timing breadth; Windows art/handling/reference parity and physical Android |
| Cars and parts | Stock BOH paths: 104 trees/624 references; 5626 custom geometry requests; corrected visible tyre fitting with 202 wheels × 11 imported cars × garage/race × normal/blur × four corners, actual vertices/hubs/restoration checked; 72 before/after Radeon images | Remaining material/deformation/customization breadth and physical Android |
| Original resources | DAT/inventory audits; 22 original native function dossiers including file-factory callers/stream dispatch parent; bounded review of 18 refs/8 global BOHs; complete current particle-material chain with 4 scripts, 13 PMDs/13 DDTs and 30 surface refs checked | 119 rejected D8Ms remain a strict failing inventory gate; original whole-program/runtime reachability unresolved |
| Rendering | Linux/Android six GPU contract groups; corrected clear/UP/viewport behavior; complete two-player split races from normal title/profile/main/local selector on development and actual Nix r11 wrapper/ELF, player2 pause/results/return/LAN/back | Other post-effects/split-screen breadth, physical input and physical Android GPU |
| Movies | Production CPU Bink decoder matches independent FFmpeg for all472 supplied clips/all88,631 frameSHA256 values at original planar resolution;3 pilot raw frames directly equal;10 negative controls; prior one-clip visual/timing/PCM correlation | Conversion/scaling/GPU/UI timing/audio/long-run A/V sync and physical Android; external Bink revisions/flags outside supplied corpus |
| Music/effects/spatial audio | Corrected logarithmic pan, mixer33/5 before38/0 after; actual development/Nix menu/race PCM; independent extraction/decoding of41 buffers from31 original PCM payloads, raw-voice replay error2.384e-7, exact clamp/SDL delivery in six-second observation; listener/voice/cutscene9/0 | Original Windows3D gain/panning, music codec/reference and audible/spatial/long-run breadth; saturation localized to source-bus addition, not quality-approved; microphone/voice capture unavailable |
| Replay | Storage round trips and deletion checks; actual circuit replay saved/loaded through Extras; r3 normal UI confirmation/cancel/delete/relist/return with localized prompts | Camera/exposure quality and controls breadth |
| Keyboard/gamepad | Linux SDL backend; two virtual pads/remap/reconnect; normal menu/career/race; creative/photo/mod menus now accept mapped pad input and mixed/keyboard-only input with release guards | Physical gamepad racing/latency; full input coverage |
| Career Gamble/poker | Re-reviewed monetary/title logic;55 Poker +48 CareerPoker checks; actual cash win and mixed-title loss/uncalled return; current production ELF heads-up title WIN transfers TK's exact211BHP Eclipse+$1000, original two cars unchanged, actual autosave/fresh reload/garage display;8 negative controls | Other actual title-transfer/tie/side-pot/AI variants; physical Android touch/gamepad/audio/rendering and art finish |
| Multiplayer/network | Existing socket/session checks and two-frontend loopback race, discovery/readiness/finish/scored lobby/guest leave; now also HTTP/leaderboard/auth/list/search/NAT/peer/presence protocol stand-ins with calibrated receiver/order fixtures | Physical LAN, latency/loss, further rosters/player counts and online; live service availability separately |
| Mod runtime | Core/package/creative checks; pack data checks; normal-frontend V70 stock and modified 406 races; Linux upload latency corrected with failing-before/passing-after GPU regression and visible menu previews | Further enabled-pack/save roster combinations and physical RP5 |
| Reported added-car visual defects | Current Linux bundle r8: Ferrari/Street Cars revision4, Wave2revision7; source-preserving tyre/glass/lamp/panel/exhaust fixes and close multi-angle galleries, packaged readback parity and ordinary purchase/save/reload; RP5 still packr5 | RP5 current-pack import, post-import save equality and visual/driving sweep; source finish limits |
| Android packaging/launcher/save/touch | Local signed ARM64 v26 APK with shared pan correction and normal native Multiplay selector, same certificate,7 libraries/API26/ELF+ZIP16KiB audit,4/4 host tests; v19 installed on RP5; prior emulator SAF/editor/storage checks | Current APK runtime/listening and current-pack import/export parity, physical car/rendering/input/performance; native release ARM64 racing |
| Distribution and provenance | Upstream OpenJuice attribution in existing docs | Clean reviewable source snapshot, reproducible final builds/releases, license audit |
| User data preservation | Isolated pilot saves and RP5 before/after export receipt | Continue verifying deployment does not overwrite user profiles |

Do not turn unknowns into passes by excluding assets or reducing scope. Source
parsers, fake-host tests, GPU diagnostics and complete runtime workflows support
different claims and must remain distinguished. Completion requires the open
items to be resolved or an explicit user scope change; no such narrowing is
assumed here.

## Current-source closure run

Full default Linux build completed successfully (`closure-build.log`). CTest
passes 5/5 (`closure-ctest.log`). The newly included CarModelParts,
SoundListener, VoiceManager, CutScene and OnlineCrewRaceSetup suites pass
17 checks independently. Their hosts and SDL dummy audio sink validate logic,
not audible output or real online play. Only test window-handle arguments were
adapted for Linux; production logic was not altered.

All enabled integration suites together, with the independent inventory fixture
and a temporary copy of the class-7 save: **228 passed, 1 failed**, exit 1.
The sole failing assertion is still EveryModel's strict inventory check.
`closure-all-integration.log`, `closure-all-integration-exit.json`,
`closure-added-results.json`, and individual suite logs preserve the results.
Binary identities and native library dependencies are in
`closure-binaries-sha256.txt` and `closure-ldd.log`. These are the current-source
results; earlier counts describe earlier builds and must not be combined into
an invented completion percentage.

## Replay storage and sprint runtime follow-up

Added ReplaysDevice to the Linux build and CTest. Its three existing checks pass:
metadata listing and missing replay-object failure, replay header/car/block
write/read equality, and two dated folders with load/delete of one. Tests now
allocate exclusively owned temporary directories instead of deleting fixed
folders beside the executable. Numeric wide scanning uses portable std::swscanf;
the original build failure for swscanf_s is retained. CTest is now **6/6**.
Evidence: `replay-device-{build,rebuild,run}.log`, `replay-ctest.log`.

Started a real UI session to verify replay playback, with an isolated support
directory. The -i diagnostic creates a 99999-lap solo race, so it was abandoned
normally and is not counted as a completed event. Through performance group G,
selected the available Punto, no auto-mod, Sprint, afternoon/clear and four cars.
Completed three physical heats using --autopilot; first heat Player 14.94 s,
T.K. 15.16, Biggi 15.50, Sue 15.71. Final points: Player 14, T.K. 12, Sue 8,
Biggi 2. Results and transitions worked through Continue.

The authoritative runtime loaded `ENGSTROM DISTRICT Engstrom_sprint01` (route
58), despite an early transitioning menu capture showing San Ricardo. Do not
label this a San Ricardo race; why that early label differed remains unproven.
Sprint results provide Leaderboard/Next heat/Continue, without a replay choice.
Thus this run adds sprint/multi-heat evidence but does NOT certify UI replay
playback or saving. Those remain pending through a circuit's replay screen.
No existing user replays were written or deleted by this UI run. Owned game and
Xvfb exited zero. Evidence: `replay-runtime.log`, `sprint-result.png`,
`sprint-final-leaderboard.png`. Runtime rendering/performance breadth remains open.

## Circuit replay saved and replayed from disk

Resolved the district-label question at source level: StockTables.cpp explicitly
maps the display text SAN RICARDO to internal name ENGSTROM DISTRICT. Thus the
label/internal-name difference follows this table; no evidence of a route change
during loading was established. Original-executable parity of this mapping has
not been independently verified in this follow-up.

Native --screen menu session, original data, --no-mods --test --autopilot,
isolated replay-circuit-support. Configured Group G Punto, no auto-mod, Race,
route 1, one lap. Actual race loaded Engstrom_R1, afternoon/clear, six cars.
Player finished fifth at 1:04.76 (Biggi first 1:00.67). Selected Replay on the
results screen; the recorded cars/cameras advanced and replay controls rendered.
Escape -> Quit Replay? Yes -> Save replay? Yes -> Save Successful. A new folder
was created under the actual game's Documents replay location:
`/home/martin/Documents/THQ/Juiced/Replays/Replay_29-10-2026_15-09-58`.

A separate native process started at Extras, selected Replays, listed the saved
race and loaded it: log confirms ENGSTROM DISTRICT / Engstrom_R1, six cars,
75.60 seconds recorded. The track loaded and hud_replay rendered the moving cars.
Escape returned from this already-saved replay. Both processes and owned Xvfb
stopped with exit 0. The longer recording duration includes the race frontend's
recorded sequence, not just the displayed player finish time.

Three new files: USERDATA 121 bytes, replay.hdr 3088, replay.dat 63456. The saved
files and backup in `replay-runtime-saved` have identical hashes. Baseline replay
directory contained no files; nothing preexisting was overwritten or deleted.
The new user-visible replay is retained as a useful reproducible artifact.
Receipt: `replay-runtime-receipt.json`. Logs: `replay-circuit.log`,
`replay-from-disk.log`; captures: `replay-circuit-result.png`,
`replay-playing.png`, `replay-save-success.png`, `replay-listed.png`,
`replay-from-disk-playing.png`. These verify the actual UI storage/playback path.

Some director-camera frames are heavily overexposed, including the reloaded
capture. This is an unresolved visual issue; successful replay decoding does not
certify image quality. A replay reproduces this condition and can support the
next renderer/post-effect investigation. Runtime deletion remains untested;
synthetic storage tests already cover deletion.

## Linux constant blend correction

Found a concrete renderer defect while investigating exposure: blend values 14
and 15 (constant/inverse constant) fell through to GL_ONE, and render state 193
was never sent to glBlendColor. Linux did not advertise the capability, so
PostEffects::glow fell back to overwriting the blur history instead of retaining
its intended fraction. Implemented GL_CONSTANT_COLOR / ONE_MINUS_CONSTANT_COLOR,
ARGB constant upload, and enabled only the additional 0x2000 capability bit.
The same capability covers both factors, per Microsoft's D3DBLEND reference:
https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dblend .

Extended the actual GPU regression with four known ARGB constants (zero, white,
red with opaque alpha, cyan with zero alpha), used as both source and destination
factors and their inverses. Independent expected framebuffer values come directly
from those masks. Before the production correction the zero-constant case failed
white != zero. Afterwards every case passes, as do existing cube/2D, loader-thread,
neon and blend-transition checks. Native Radeon driver used. CTest 6/6 passes
after relinking juiced_tests. This is a behavior correction, not an image filter.

Rebuilt juiced and loaded the saved six-car replay through Extras again. It plays
and renders with visible scene/car detail (`constant-blend-replay-runtime.png`).
This screenshot uses a different director camera/time from the earlier blown-out
capture, so it is only a runtime smoke check. The constant-blend defect is fixed;
full resolution of the exposure symptom remains unproven until matched scenes
or other controlled post-effect evidence establishes it. The timed --press attempt
instead opened the tutorial and is explicitly rejected as replay evidence.

Artifacts: `constant-blend-before.log` (expected red), `constant-blend-final.log`
(green), build logs, `constant-blend-regression.log`,
`constant-blend-replay-interactive.log`, `constant-blend-binaries-sha256.txt`.
The desktop wrapper already selects the rebuilt out/linux/juiced. Android and
macOS renderers were not modified. Owned runtime and Xvfb stopped, no saves edited.

## Controlled replay comparison at 20 seconds

Added debug-only --replay-capture-frame N to LaunchOptions. It is effective only
with --test: simulation/UI updates use fixed 1/60 seconds, capture occurs at the
first rendered replay time >= N/60 and the process exits. Ordinary gameplay
timing is unchanged. The existing --frames limit remains a timeout fallback;
only the explicit replay-capture log establishes a successful timed capture.
This does not make movies, animated textures or OS/audio clocks deterministic.

Invocations used --screen loadreplay --seed 42 --press
200:RETURN,300:RETURN --replay-capture-frame 1200 --frames 6000, identical saved
replay/assets, fresh support directories. One Return is insufficient when the
first input only takes the player's device; the failed single-Return attempt is
retained and excluded. Correct runs capture at 20.016533 seconds / global frame
1513, with the same camera and logged car states. A repeated current-renderer
run has mean absolute RGB difference 2.219/1.691/0.652, so these are controlled
scene comparisons, not claims of total pixel determinism.

Built an isolated baseline by removing only the three constant-blend changes
from a copied GlesDevice.cpp and linking that object against the same current
engine libraries. No production files were reverted. Build recipe and source
remain in evidence (`build-blend-baseline.py`, `GlesDevice-before.cpp`). Both
baseline/current captures terminate normally at the same replay instant.
Before/after RGB difference averages 32.023/31.725/30.441; corrected blending
reduces the excess glow, but the washed-out scene persists. Therefore the full
exposure defect is NOT closed by the constant-blend correction.

Further isolated builds of PostEffects.cpp omit additive glow submission, or
disable both glow and exposure application (`build-post-isolation.py`). They
also capture the same replay instant/camera. Mean RGB in the fixed image region
x=0..1279/y=200..799: baseline 207.56/208.79/205.50, corrected
170.68/171.62/169.03, no additive glow 169.55/170.44/167.94, no glow/exposure
165.20/166.10/163.67. Visually the wash remains even without glow/exposure;
disabling these globally would conceal part of the symptom without resolving
the cause. Next isolate depth-of-field, camera cuts and scene fog against the
same replay before changing any more production rendering.

Evidence prefixes: blend-controlled-{before,after,repeat}, no-glow-add,
no-glow-exposure; logs, BMP/PNG and JSON metrics retained. Build/source identity
receipt: post-isolation-binaries-sha256.txt. CTest remains 6/6 after rebuilding
the test runner with the capture option. All diagnostic processes exited.
These runs read the saved replay without changing it or user profiles.

## Depth-of-field / buffered transformed-vertex correction

Two further isolated PostEffects variants captured the same 20.016533-second
scene: omitting the camera cut retained the white wash; omitting depth-of-field
restored contrast. Fixed-region RGB means: no cut 172.74/173.37/170.43, no DOF
100.44/102.74/92.93. Recipes/source/binaries are retained as no-dof and no-cut
under the evidence directory; they are diagnostic builds, not shipped settings.

Found the concrete backend cause: PostEffects::blurQuad supplies an XYZRHW /
Diffuse / Tex4 stream using setFvf, then calls draw. Linux's draw always used the
last programmable vertex shader/declaration, ignoring this FVF. Thus the DOF's
quad vertices were interpreted using an unrelated declaration instead of their
screen-space layout. Direct3D's transformed POSITIONT/XYZRHW data skips vertex
processing (Microsoft reference:
https://learn.microsoft.com/en-us/windows/win32/direct3d9/mapping-fvf-codes-to-a-directx-9-declaration).

Implemented a GPU stream path for buffered XYZRHW, preserving render-state
depth tests/writes, start vertex, diffuse colour, reciprocal W and texture
coordinate layouts. No CPU buffer readback or full geometry shadow copies were
introduced. The existing immediate-mode path explicitly resets the new screen
transform uniform; explicit declarations leave FVF mode. This fixes the actual
vertex submission rather than disabling depth-of-field.

New GPU check: draw a red buffered quad starting at vertex 1, then a farther
green quad with depth enabled; the centre must stay red. Uses the DOF's
XYZRHW/Diffuse/Tex4 layout. Before correction the valid request throws map::at;
afterwards it passes. A following immediate-mode draw verifies the stream mode
does not leak. Existing cube, texture-thread, constant-blend and neon checks all
still pass (`fvf-before.log`, `fvf-final-gpu.log`). Rebuilt juiced_tests and CTest
passes 6/6 (`fvf-regression.log`).

Current production binary with DOF still enabled captures the same replay time,
camera and car states without the wash (`fvf-fixed.png`): fixed-region RGB mean
102.88/105.28/94.96, close to the isolated no-DOF control but retaining the effect.
This closes the demonstrated 20-second DOF wash failure; it does not certify
every director camera, all post effects or original-game pixel equivalence.
The overall rendering-coverage requirement remains open. See fvf-fixed-runtime.log,
fvf-scene-means.json and build logs. No original assets or user saves changed.
Additional runtime capture reaches 40.016228 seconds / frame 2713, exits zero,
and shows another director camera without the demonstrated white overlay
(`fvf-fixed40.png`, `fvf-fixed40-runtime.log`). This second view has bright
highlights on the yellow Punto and road; it is not a matched original-render
reference. Current executable/check identities are in fvf-binaries-sha256.txt.
Both finite capture processes are terminal.

## Linux native gamepad path

Found a missing production path: the shared Mac/Linux InputDevices implementation
returned no controllers. Linux initialized SDL gamepads but never exposed them to
Core::input. Added Linux-only InputDevices backed by SDL3, a separate keyboard
port and four stable analog-gamepad slots. D-pad-only devices are filtered out.
Retail menu/driver semantics map to sticks, independent triggers and buttons;
crew bindings are idempotently added to DRIVER1..4, including after the keyboard
editor reapplies maps. Linux frontend device counts use connected devices rather
than the five reserved ports, so empty slots do not enable split-screen menus.

Removal submits one neutral sample before disconnected status, clearing held
triggers. Reconnection fills free slots without replacing still-connected pads.
Optional controller.ini provides deadzone/button remap; there is no gamepad
configuration UI, rumble or wheel implementation in this change. Details and
standard button layout are in linux-port.md.

`juiced_controller_check` passes actual retail controls through the production
backend with SDL virtual devices: automatic player acquisition, A/B/D-pad UI,
independent triggers, steering/deadzone, all six one-shot crew commands, remap,
two independent pads, removal/reconnection and explicit keyboard selection.
The same checker against an isolated copy of the former empty backend fails
player acquisition, exit 1 (`controller-negative-run.log`); production files were
not reverted. Initial two-pad failures showed the test injected trigger values
before SDL opened the new gamepad; priming the handle before injection fixes
the fixture. Failed logs are retained. Current checker passes
`controller-closure-run.log`; current build succeeds, CTest 6/6 passes
(`controller-closure-build.log`, `controller-closure-ctest.log`).

Hashes: `controller-closure-binaries-sha256.txt`. Process-local virtual devices
verify the SDL adapter, not physical hardware latency, Bluetooth, audible output
or full manual-race playability. A separate normal-GameLoop runtime check is
recorded below when complete; --test disables live InputDevices, so the earlier
--test/autopilot pilots cannot certify this new path. User profiles were not
modified by the checker.

### Normal GameLoop controller runtime

The unmodified executable ran without --test and without --autopilot. A diagnostic
LD_PRELOAD helper attaches one process-local virtual SDL gamepad and changes its
buttons/axes; it does not call engine input or physics. The first session used
only controller input through Menu -> new-career flyby -> TK betting -> confirm
bet -> race. Accelerated, paused with Start, resumed with A, braked and abandoned
normally through pause/confirmation. Results show $38,000 after losing the
$2,000 bet in an isolated career; no profile was saved. This run exits 0 at its
finite frame limit. `controller-runtime.log`, controller-menu*/career*/race*
captures and `controller-abandon-results-final.png` preserve the actual UI path.
The earlier left-button confirmation attempts correctly left No selected; the
vertical Yes/No dialog requires Up. They are not used as successful confirmations.

This exposed another Linux omission: -i pinned port 0, now the dedicated keyboard,
so direct-race launch ignored a connected pad. -i now polls and prefers the first
connected analog gamepad on Linux; tests remain keyboard-only as before. Also
fixed race diagnostics to print the body's applied steering/throttle/brake rather
than the competitor's AI intent, which was misleading for the player's car.
This logging change does not change input or physics.

A second normal -i run confirms the direct path: neutral after Go; RT sets actual
throttle 1/brake 0 and the Focus accelerates (48 mph in both recorded captures); LT sets throttle 0/brake 1 and stops it; full left input produces applied
steering -0.94. Start -> hud_pause, A -> hud_race. Final neutral controls are zero.
`controller-instant-runtime-receipt.json` records arguments, stages and exit 0;
`controller-instant-runtime.py`, its injectable helper source/library, runtime
log and stage PNGs make the procedure reviewable. The first 3300-frame attempt
completed the control stages but its harness wait expired; it was terminated,
retained under controller-instant-timeout-attempt, and is not claimed to have
exited normally. The final 1650-frame repeat exits 0. This is still virtual-device
runtime evidence, not a physical-controller performance claim or full manual lap.

Final sources rebuild successfully; CTest 6/6 and the two-pad checker pass again.
Evidence: controller-runtime-final-{build,ctest,check}.log and
controller-runtime-final-binaries-sha256.txt. The normal launcher selects this
out/linux/juiced. All controller-validation saves/configs are isolated from
existing player profiles; the existing user replay was not modified.

## Installed Linux car packages synchronized

The desktop wrapper uses ~/.local/share/openjuice/mods. That installation still
had Street Cars revision 1 and no Wave 2 package; seedMissing intentionally
preserves existing files, so new source assets did not update those installed
cars. Prepared a separate candidate containing current Ferrari, Street Cars r3
and Wave 2 r3, alongside the other currently installed packages. The actual native
package assembler and rebuilt wave2 checker pass **1238/1238** against that
composition. Then moved the previous car package directories to
`installed-mods-before-update` in evidence and installed the checked directories.
Preserved existing disabled markers and collection.ini; did not rewrite profiles.

All six existing Linux profile files have identical before/after SHA256 values.
`installed-mods-deployment-receipt.json` records those hashes, all installed car
package file hashes and the backup. Reassembled the actual live directory after
installation; another **1238/1238** pass confirms that installed composition,
not just the separate candidate. Commands/scripts and logs use the prefix
installed-mods*. This is a local Linux update, not an Android deployment or new
release. The RP5 is absent from ADB; only the iPlay is connected and was not used.
Visual/driving verification remains distinct from these structural checks.

## Final source and car-asset follow-up

Current complete native build exits 0 (`final-current-build.log`), CTest 6/6
(`final-current-ctest.log`). The complete enabled integration set now includes
ReplaysDevice: **231 passed, 1 failed**, exit 1, against model-inventory-fixture
and a temporary class7-save copy. The sole failed gate remains the strict 119-D8M
inventory assertion. `final-current-integration.log` and its exit receipt preserve
that failure; it is not hidden by the green CTest subset.

The rebuilt GL checker passes all cube, loading-thread, blend and transformed
stream cases on Radeon RX 7800 XT using Wayland
(`final-current-radeon-gpu.log`). The explicitly forced Xvfb rerun used llvmpipe
and also passes (`final-current-gpu.log`); do not label that log as Radeon.
Controller-runtime sessions above were also under Xvfb/llvmpipe: they demonstrate
normal GameLoop input behavior, not desktop GPU performance or Android FPS.

Complete-angle car review found and fixed the first-wave removable-boot defect
and Ferrari inset carbon panel; details, negative controls and final delivery are
in street-cars-visual-repairs-2026-10-29.md. Final installed Linux revisions are
Ferrari 4 / Street Cars 4 / Wave 2 3. All six Linux profile files remain byte
identical. The independently unpacked combined ZIP passes 1238 full-roster,
377 first-wave and 226 Ferrari checks. The final 384-image gallery runs on Radeon;
earlier llvmpipe baselines and failed-alpha contact sheets are distinguished.
The RP5 is absent; new Android installation/visual/driving approval remains open.
Neither this car delivery nor the controller correction completes the full
verification objective or resolves the other open ledger requirements.

## Hosted/delegated pink-slip ownership contract

The shared HostPinkSlipTest now builds as `juiced_pink_slip_check` on Linux;
only its renderer header selection differs by platform. Original game UI and
GameData run through FakeCareerHost. The earlier settlement check won a rival's
car and immediately removed that newly won car, so its size-only assertion did
not prove which player car would actually be lost. Replaced it with three
colour-marked cars, staking the middle car, and the actual CharacterScreen
confirmation/autosave/reinsert flow reached from GridWalk.

The check verifies that the pre-race autosave excludes exactly the selected car,
that both other cars survive serialization, that the stake is restored at the
end with its existing grid damage, and that the delegated teammate points to
its new index. Win and loss start from the same pre-settlement garage separately.
Win retains the player's stake and adds the selected rival model; loss removes
only the player's stake. Both resulting ownership states survive save reload.
No production settlement behavior was changed by this verification.

`pink-slips-contract-build.log` and `pink-slips-contract-run.log` record exit 0.
This tests production UI/settlement logic with a fake host and generated career,
not a completed physical race or an actual on-device autosave write. Real race
completion and RP5 verification remain open. User profiles were not loaded or
modified. Existing host setup checks cover exactly-600 respect eligibility,
race-type selection, eligibility changes before commit, delegation and disabling
the mod.

## Combined car delivery r5 — Jetta headlamps and trunk liner

The current installed composition is Ferrari r4 / Street Cars r4 / Wave 2 r4,
combined ZIP r5. This supersedes the composition in the earlier r4 paragraph.
Source-derived Jetta lamp assemblies replace the opaque blank lens appearance.
Per-object colour isolation proved the rear black fins came from its trunk liner;
hiding/replacing the rear-window frame did not solve them. The original frame is
retained. Twenty-eight liner vertices now fit beneath the actual exterior shell;
217 other shared GLB parts have unchanged geometry. All 31 Jetta decal meshes
and every handling recipe remain byte-identical to the preceding package.

The full native regeneration passes 1238 checks and has a reviewed 32-view Radeon
gallery. Independently extracted combined r5 passes 1238/377/226 checks for full
roster/first wave/Ferrari. Build and CTest 6/6 still pass. Shared hosted/delegated
pink-slip checker passes 54 checks including actual confirmation/autosave UI and
independent win/loss identity and serialization checks, not physical race completion.

See car-visual-r5-checkpoint-2026-10-29.md and jetta-lamps evidence. The Linux
installation compares ModSaves and saves before/after. Android remains uninstalled:
ADB reports no devices at this checkpoint. Existing integration inventory failure
and the other global requirements are not cleared by this asset delivery.

## Android renderer, import storage and live touch follow-up

The final local ARM64 APK is version code 17, SHA256
`634ba96bf86dc27a962d7dc9f4482028ac4085883709f928f46c536d581b612b`.
Android now implements the constant blend and buffered XYZRHW corrections;
negative controls fail, final native ES regression passes in the emulator and
the Android-host Radeon regression passes. Signature, ZIP/ELF 16 KiB alignment
and independent API-26 dependency checks pass. Full Android-host CTest is 4/4.

Real launcher SAF import installs combined r5 and validates 64 cars; installed
payload hashes match the ZIP. Import and game startup remove abandoned derived
overlays only under installation.lock. Repeated forced process death reproduced
3 GiB of stale overlays; the normal Check game files action reclaims that space.
Final game startup also removes a deliberately seeded killed-stage marker before
its native renderer regression. Unreadable live package lists stop an update
before activation. Host regressions retain enabled/disabled preferences and
existing packages and verify rollback, recovery, saves and lock exclusion.

The prior fixed-delay touch-race capture showed only the fly-by and is not driving
evidence. Its replacement waits for Go, injects actual Android MotionEvents into
normal -i play, and verifies new applied CarBody samples for gas, steering,
partial release, brake and final neutral. All stages pass with HUD captures.
The final Java storage changes leave the exercised native libraries byte-identical;
separate receipts bind the live-driving and final APK startup checks.

The side-vinyl sweep covers every catalogue decal on eleven added cars plus the
retail Focus, both sides: 774 native Radeon captures, 49 visually reviewed contact
sheets. It does not cover all kit/damage combinations or arbitrary custom logos.
The emulator save is a disposable clone of Linux PTony, not Ronnie on RP5; its
original and Linux source hashes remain unchanged. The physical RP5 remains
absent, so final installation, visual/driving approval and FPS there are pending.

See android-render-storage-checkpoint-2026-10-29.md and the android-current and
all-vinyl-gallery evidence directories. A separate source checkpoint archive
records HEAD, the tracked binary patch and untracked files without staging or
resetting the existing worktree. These are additional verified deliveries;
the remaining global requirements and strict original-model failure stay open.

## Original mesh constructor and all-district runtime follow-up

The selected shipped Juiced.exe (SHA256 6e9b5d5ed5e3bd2319da5ab7d73dd396c78fc95cacbd40101427dc6f03ca9d1b)
was inspected with REA/Ghidra without execution. Its direct mesh constructors
read an initial u32 index count and double it for index-buffer bytes, matching
the port's release-layout prefix. Static projection over the rejected corpus
finds 111 impossible index payloads, four zero-index/implausible-part-count
headers and four empty files. This is not observed original-game rejection,
full loader equivalence or a whole-program reachability proof. Ghidra's partial
boundaries/calling-convention uncertainties are retained with the raw evidence.

A separate bounded current-source/resource review found no selecting entry point
for the eight global BOHs containing eighteen unresolved references. These remain
candidate legacy leftovers. The production parser and strict 119-file failure
are unchanged. See legacy-mesh-audit-2026-10-29.md and legacy-followup evidence;
the native session is closed with an exported bundle and snapshot.

Eighteen new native Radeon runtime samples cover first-circuit routes of all nine
districts at morning/dry and night/wet. Six stock Focus_2 cars load; every run
reaches Go, produces positive applied player speed/throttle under AI, captures
a nonblank HUD/scene and exits 0 at 600 frames. All eighteen images were reviewed.
A real missing-route control boots/render-exits 0 but has no requested scene,
racers or Go, and is rejected. Two earlier erroneous fixture assumptions (intro
required, lowercase AI label) are retained as failed verifier attempts.

See native-track-runtime-2026-10-29.md and track-matrix/index.html. These are
debug entry-path samples, not complete laps/events or original-render parity.
No production code changed; executable SHA256 d44153bb2ad54ded777f1cbb57af59a9c5eee0b93b48658c96f51b60d60f7fec.
Each process uses its own support/ModSaves directory and creates no career profile.
All owned finite game processes are terminal. Other ledger requirements remain
open; this is concrete verification progress, not overall completion.

## Complete races with installed mods through the ordinary frontend

The unchanged production binary d44153bb2ad54ded777f1cbb57af59a9c5eee0b93b48658c96f51b60d60f7fec
completed two one-lap Engstrom_R1 events in one process, selected through the
normal menus without --test or --track. V70 factory (168 bhp), night/wet, four
cars: third at 67.37 s. An added Jetta rival finished second with its correct
model name. Auto-modified Peugeot 406 (344 bhp, carbon/vinyl/wheels/body additions),
afternoon/clear, six cars: fourth at 57.45 s. Every competitor finished. Both
Continue actions unloaded the race and returned to instant_manu.

SDL virtual gamepad input, private Xvfb/llvmpipe and diagnostic autopilot were
used; this is actual simulation/UI flow, not manual driving or FPS evidence.
Audio is dummy. Saves are isolated and no existing profile was loaded. The
game and owned display server exited 0. The independent checker rejects three
incomplete keyboard fixtures plus boot-only, missing-finish and missing-return
negative controls. The unreliable injected keyboard releases do not establish
a production input/car-selector defect; no production fix was made for them.

See modded-race-runtime-2026-10-29.md and full-race-ui/normal-races-receipt.json.
Artifact identities were rechecked; final r5/v17 installation on the physical
RP5 is still pending because ADB lists no devices. Broader ledger items remain
open, including full customization/route/campaign coverage and the strict
original-resource inventory failure.

## Linux loopback networking included in the normal test build

Linux.cmake previously omitted the existing TcpTests, TransportTests and
SessionTests integration translation units. They now build into juiced_tests
and have explicit CTest selections. Fixed-port transport/session runs share a
CTest resource lock and have 60-second timeouts; TCP has a 30-second timeout.
The current CTest run passes **9/9** (six earlier suites plus three network suites).

Six network checks pass over actual local sockets: TCP connection / two-way
payload / close; UDP discovery / two peers / reliable and plain payloads / leave;
load-time keep-alive survival with timeout after keep-alive removal; session
names / setup / readiness / count / car-state payloads / finishes / lobby;
50 ms state synchronization and ping timing; fixed-peer contact filtering.
Session car-state hooks are synthetic. These are not two running frontends,
rendered multiplayer races, physical LAN or live GameSpy service evidence.

Production networking code and the production d44153... executable are unchanged.
The test runner SHA256 is fe3d3169435eb004ca224ed3da0793edf2c6d72fb6825aa221a033aade6ece74.
Evidence: network-followup-{build,ctest,last-test}.log and
network-followup-receipt.json. No new full default integration aggregate is
claimed; its existing strict 119-resource failure remains open.

## Two ordinary frontends complete a loopback network race

Two unmodified production d44153... processes, isolated supports/displays, actual
UDP ports directed to 127.0.0.1: discovery and joining through Multiplayer,
Group G / Angel North Central / Angel_North_R1, one lap, afternoon/clear, two
factory Puntos. Host setup propagates to the guest. Both ready, load, count down,
drive under diagnostic autopilot, finish and return automatically to link_lobby.
Guest wins (local 65.84 s), host second (66.02 s); both lobbies show 10/6 points
and one race each. Remote predicted crossing times differ; exact timing-table
parity was not verified. No named profile was created, so player names are blank.

Guest confirms Leave game session and returns to search; host retains its own
6-point slot and clears the other. Both games and both private Xvfb servers exit
0. The checker rejects missing-Go/finish/return controls for each process.
See linux-lan-runtime-2026-10-29.md and full-race-ui/lan-runtime-receipt.json.
The evidence uses llvmpipe, virtual gamepads and dummy audio, not physical LAN,
Android, six-player/latency/voice/Internet or FPS validation. Production code
remains unchanged; only Linux's test build includes the previously omitted suites.

## Arcade saves, new-series unlock and Linux creative-menu input correction

Normal new-driver onboarding exposed a Linux defect: the photo/creative menu
consumed keyboard events only and ignored the assigned pad. Linux now polls its
normal UI map in that overlay, handles acquisition before --mods-menu, preserves
keyboard use alongside a pad and waits for opening/closing input to be released.
The prior driving map is restored. Android/macOS keep their existing input paths.
The new mapped-event method is used by Linux; APK/car ZIP deliveries are unchanged.

The original ignored-pad capture and a candidate that ignored mixed keyboard
input are retained as failing row-selection controls. A separate candidate
stopped swapping on entry and required deliberate SIGKILL; a held-button modal
import is suspected, not proved. Only that terminal process's derived overlay
was removed. The final row checker passes nine cases, including mixed input,
keyboard-only pre-acquisition navigation and held-key release barriers. None of
these cases toggled installed packages. Actual XTEST keyboard events are used;
the earlier synthetic SDL key-release fixture remains excluded.

Owned Player/MyCrew profile: first two Novice circuit wins, then three sprint
heats in second place (12 points versus 18), correctly not marked won. Another
process loads the two-win profile and wins Novice event 4 on Anderson_R1_rev:
first at 97.26 s; NEW SERIES UNLOCKED. Independent archive parsing verifies
0 -> 2 -> 3 completions, the losing sprint still incomplete and Series 2's first
four events open. Only ArcadeUnlock.sav changes: career/settings/help entries
remain byte-identical. Native UserSaves passes 20 checks. A further process loads
the three-win profile and opens JUICE GAMES SERIES with those four events open.
The Novice series itself remains incomplete (sprint and fifth event unwon).

Current production executable: e9f493d2027ebd1cd54dd4dec230ec372f612c08da82b0369563c41089ac857c.
The retained 6eec98... binary covers earlier onboarding/two wins/sprint; the final
binary separately covers startup/mixed/keyboard-only menus, loads and the full
reversed-route third win/unlock. Default Linux build and current CTest pass 9/9.
All seven owned games/displays are terminal, one failed candidate explicitly
killed. Private llvmpipe/virtual-pad/XTEST/dummy-audio scope is not physical/FPS
certification. No existing user save was loaded or edited. RP5 remains absent.
See arcade-overlay-checkpoint-2026-10-29.md and
full-race-ui/arcade-overlay-receipt.json. The original inventory failure and
other open ledger items are unchanged; full project completion is not claimed.

## Challenge rules and results through the actual frontend

Unchanged production e9f493... executable, exclusively owned asset/save fixture:
75 original Arcade event blocks are permuted byte-for-byte, placing retail events
16/18/19/32 first. Parameters are unchanged; this is controlled access, not natural
later-series progression. An independent full block-multiset/permutation check
rejects a target edit.

Four diagnostic-autopilot runs: Corolla Campbell_R2 wet timed lap fails at
47.00 s; Focus SouthSide_R3_rev night perfect lap succeeds at 72.32 s under 80 s;
Falcon Downtown_R4 timed lap fails at 45.06 s against 45 s; Beetle Downtown_R2_REV reaches
60 mph, then fails TooSlow at 12.85 s. A separate process loads the one-win profile,
then without autopilot stays neutral to trigger SpeedNotReached at 10.00 s and
applies normal pad steering/RT after Go to collide/fail CarDamaged at 2.16 s.
All six show matching actual challenge-results captions and return to Arcade.

Independent save parsing: exactly slot 2 completed, no subsequent-series unlock,
fifth still locked; both manual failures leave the loaded archive byte-identical.
The first timed failure also leaves its initial archive byte-identical. Career
and help payloads are unchanged. The successful autosave additionally stores
settings.dat changes after ChangeView input; only-Arcade-entry parity is not
claimed in this run. No user profile was used.

A fixed-delay injection overshot Speed to Falcon during model loading; its
mislabeled speed-selected capture is excluded as Speed proof. The new helper
waits for observed press AND release and correctly selects Beetle. No production
input defect/fix is claimed from that fixture. Thirty missing-evidence controls,
a target-edit control and false-progress control are rejected. Both games and
owned displays exited 0. llvmpipe/dummy audio/virtual pad scope; a foliage-occluded
result camera is not visual-camera approval. No production source or delivery
artifact changed. CTest remains the prior 9/9, no invented rerun aggregate.
See challenge-runtime-checkpoint-2026-10-29.md and challenge-runtime-receipt.json.
RP5 remains absent; the original strict 119-resource failure remains open.

## Source restoration, fresh Nix package and independent import

The complete dated checkpoint chain restores the engine state into a new owned
checkout: 5977 byte-identical files, 276 correctly deleted files, no missing or
extra file. Three unchanged Windows project files differ only by Git's declared
CRLF checkout conversion. The strict comparison's exit 1 is retained; a separate
attribute/Git-blob verifier accepts those cases and rejects nine negative controls.
All C++/header/CMake files match exactly; main Git index/status remain unchanged.

Fresh isolated nix-build from that restored checkout passes CTest 9/9 and installs
/nix/store/sl0d79724qiz5s0319wqlg1llcl8q1v2-openjuice-1.02.5-linux-mods1.
Actual ELF SHA256 36ff27c5d5b5fb89eaf2203df5ae40249dc832733dfdbab9c96f8678285f6257;
it is distinct from the development e9f493 executable. No bit-identical rebuild
claim is made. The installed wrapper's actual process ELF was checked at runtime.

That package loads the owned one-win profile, displays the saved Perfect Lap
tick and runs the original Beetle Speed challenge without autopilot. Neutral
applied input leads to SpeedNotReached at 10.01 s, matching results and return to
Arcade. Archive remains byte-identical; game/private display exit 0. An initial
injection during the score-loading modal entered Career and was recovered via
normal quit without saving; its black transition capture is excluded. Seven
missing-evidence controls are rejected. llvmpipe/virtual-pad/dummy-audio scope.

All 307 runtime dependency paths export and import into an empty owned local Nix
store; graph equality, full contents verification and imported ELF hash pass.
Installer/profile/help check uses an isolated Nix profile. Existing launcher,
settings and user saves are preserved. See source-reproduction-2026-10-29.md.
The original 119-resource failure and remaining global runtime breadth stay open;
RP5 is absent from both ADB and kernel USB enumeration. r5/v17 are unchanged.

Local Nix delivery: out/releases/OpenJuice-20261029-linux-x86_64-nix.tar.xz,
319172360 bytes, SHA256
645279568816f30d34ae4de9d28a1c814d61a54979ff5eee4c6c9f709a28da30.
Every archive payload was read back and matched. This is a local candidate, not
a newly published release. Only this verification run's restored checkout,
private import store and raw package staging are cleaned up after proof is
recorded; compressed artifact, metadata, receipts and installed package remain.

## Four-car team runtime and candidate r2 — 2026-10-30

Controlled owned career fixture: Vito recruited, Eclipse/GTR, January 2,
Engstrom_R1 afternoon/dry one lap. Three real normal-frontend team races complete
on the preceding ELF, corrected development ELF and fresh Nix package. Four
finishers, results, summary, respect, prize and autosave all observed; normal
SDL quit and private display exit zero. No existing user save changed.

The calendar, crew selector and grid incorrectly used the generic three-crew
caption for two crews. A shared helper follows race setup's entrant addition
and Sprint cap while retaining translated words. Real-assets checks reproduce
7 failures before and pass 17/17 after, including German. Linux development and
fresh Nix CTest 9/9; supported Android-host CTest 4/4.

D-pad Medium -> High -> High-clamped -> Medium -> Low -> Low-clamped -> Medium
is visible in corrected development and packaged runs. Human uses diagnostic
autopilot; teammate ordinary AI. This is not physical Android input evidence.
Independent archive verification: $40,000 -> $41,000, event done, two members,
Vito races 0 -> 1, ability 40 -> 42.5, recovery 70 -> 72.5; optional added-car
progress preserved. A separate Nix process reloads and visibly shows $41,000
without changing that archive. Empty-member selection capture excluded from
Vito UI-stat claims. Forty-seven negative controls reject missing/false evidence.

Fresh Nix package actual ELF 9b9f96cbd6e334db14efb014116a68c93c320675696d073312ed354cac2cb17c;
307-path closure imports into an empty owned local store with equal graph and
full content/hash checks. Candidate OpenJuice-20261030-r2-linux-x86_64-nix.tar.xz
SHA256 0f2e7c0c4b8c175568a1ff7210d693aefb246cf8cd7b01f9cdd33a4d23ed317b.
Archive payloads all read back. Installer uses an isolated profile. Owned raw
staging/import fixtures removed after verification; metadata/artifact retained.

Android code 18 / 1.02.5-android.17-team-labels builds/signs with same update
certificate; seven ARM64 ELF/16KiB/dependency checks pass, only libmain changed.
New APK has no runtime certification yet. r5 car ZIP unchanged. RP5 absent.
Global closure remains open, including the strict 119-resource inventory failure.
See team-race-verification-2026-10-30.md and full-race-ui/team-runtime-receipt.json.

Current full enabled integration rerun after the caption correction: 251 passed,
1 failed, exit 1. Sole failure remains EveryModel's strict original inventory;
temporary validation save copy only. No resource exclusion or tolerance added.
Evidence: team-current-all-integration.log and corresponding exit JSON.

## Replay UI, delegated pink win/loss, r3 and physical update — 2026-10-30

Replay direct-Extras entry lacked translated checking/deletion messages until
Driver::enter. Corrected replay initialization: real-assets localized regression
0/9 before -> 9/9 after. Actual development and Nix r3 runs show the complete
question, preserve an owned clone on cancel, delete only that clone on confirm,
show success/relist/return; all pre-existing replay hashes retained.

Linux CTest 9/9, Android host 4/4. Full current integration 260 passed, 1 failed
(strict 119-resource inventory), exit 1. No failure exclusion or weakening.
Nix r3 build, installed-wrapper runtime, empty-store 307-path closure import,
full contents/hash verification, isolated installer/help and archive readback pass.
R3 local archive SHA256 58ebc4acb37259760b3764842ae38398d44e5ed76b2fbeba9c57638aa4105272.
Native r3 ELF 97778946dba76584115a7c1eb5180d9ce52ea484c7118cadedb2311ef48f5d25.

Actual delegated pink slip with only crew-pink-slips enabled: Vito AI vs T.K. AI,
no autopilot, owned controlled fixture on Engstrom_R1. Title preview, warning,
intermediate stake autosave, race, loss dialogue, settlement and final autosave.
Loss removes only staked Eclipse, GTR unchanged, cash $40,000; separate process
reload verifies one race/zero wins. Trained Vito (95/95) in another actual r3 race
wins at 48.99 vs 55.39 s. Final garage GTR + returned Eclipse + won Golf, no
duplication; GTR unchanged, cash $40,000, Vito 1/1 and 97.5/97.5. Separate-process
winning-profile reload visibly confirms one race/one win/100%, no archive change.
First trained run ended at frame cap on grid without racing: excluded, unchanged
save. Recruitment/calendar/training are fixture setup, not natural progression.

APK v19 (1.02.5-android.18-replay-prompts) signs/audits successfully. RP5 physical
code 16 -> 19 update uses same certificate and install -r. Real launcher exports
before/after prove all nine save entries unchanged (Player/Ronnie). No uninstall
or restore. Ronnie backup: 35 cars, four members, $1,130,449, not edited.
R5 ZIP device hash matches; launcher import validates 64 cars. User then resumed
another game and explicitly requested Linux work; no further phone control.
Post-mod export parity and physical car/input/rendering/FPS remain pending.

Later Nix filter-only change excludes generated Android output/cache paths.
Source payload 1,021,572,094 -> 15,887,785 bytes. All 899 checked source/tool
files match both worktree and r3 compiled source exactly. New source materialized
by derivation instantiation; r3 predates that filter and preserves its exact
earlier recipe in package metadata. No new binary build/bit-reproducibility claim.
Only private verified raw package staging/import store removed (2,211,602,549
file bytes); compressed candidate, metadata and installed package retained.
See replay-pink-verification-2026-10-30.md. Global goal remains open.


## Actual saved-car configurations and six-car team race

User is using RP5 and explicitly requested Linux work; no further phone control.
The owned Ronnie clone opens normally with 35 cars and $1,130,449. A separate
native CarView harness rendered twelve exact saved imported-car configurations
from eight directions on Radeon RX 7800 XT. All 96 views manually inspected;
pixel/hash/part/archive checks and five negative controls pass. Source finish
limits remain (notably excessively dark 405 glazing), and most saved new cars
have no vinyl/carbon selected. No all-combination/retail-art/Android claim.

Actual Nix r3 six-car three-on-three circuit completes through settlement, save
and fresh-process reload. D-pad independently changes Vito/Amber intensity;
both actual HUD rows reviewed. Team second place pays $1,000 ($40,000 ->
$41,000); both teammates record one race/zero wins and native learning. Native
and independent save readers agree; eighteen negative controls pass.
Diagnostic selection/bet navigation errors and the first misnamed Player/Amber
reload image are retained and excluded from passing claims. No new production
source changes; no repeat binary rebuild/test claim. See
linux-saved-cars-team3-verification-2026-10-30.md. Global goal remains open.


## Peugeot 405 glazing follow-up

The saved-config review's opaque-window limitation is corrected in Linux Wave 2
revision 5 / combined car bundle r6. Four existing glass sheets now use native
Car Glass, with 9,990 source triangles/positions/normals/UVs preserved to five
decimals. Both driving and menu folders updated; other cars, stable IDs, physics
and save format preserved. Native Radeon 32-state/angle candidate review, 32
byte-identical packaged captures and 1,238 native checks pass. A separate actual
six-car 405 driving render sample reaches Go and movement, then exits after 600
frames; no full-race/FPS claim. Source interior finish remains limited.

Linux installed package updated with rollback backup; local profiles unchanged.
RP5 explicitly left alone, still on r5 car data: r6 import and physical review
remain pending. Rejected X11/partial-interface probes and an unpublished ZIP
that included authoring tools are retained as excluded diagnostic evidence.
Corrected r6 runtime-only ZIP hashes/readback pass. See
peugeot-405-glazing-verification-2026-10-30.md. Global goal remains open.


## Poker correctness and real-save workflow

Two confirmed poker defects fixed: checking at zero no longer reopens raising
after a short opening all-in; valued-contribution intervals no longer restart
odd-chip priority independently. Both have failing-before/passing-after
regressions. Poker 55/0, CareerPoker 48/0; native CTest9/9 and Android host4/4.
Actual normal frontend completes a five-player cash win (+295) and heads-up
Eclipse+980 wager after default-No cancellation/acceptance. Short caller wins
1,000; unmatched title returns. Real save-device writes and separate reload
agree at39,295 with both cars byte-identical. Synthetic owned career only.
Full current integration remains260/1, solely the strict119 original D8M gate.
Code20 APK signed/audited, physical RP5 untouched on19/r5. New native binary
4e21db0b... is used by the usual launcher; old Nixr3 predates these changes.
See poker-closure-verification-2026-10-30.md. Wider goal remains open.


## Packaged Nix r4 runtime and empty-store import

The existing Nix build completed with9/9 CTest suites; its actual source matches
904 current compiled/build/tool inputs. Actual r4 wrapper/ELF71d6760f... boots
through title/load/Career/Gamble, completes a five-player cash hand and writes
the save device. Banner-20 agrees with40,000->39,980. Both cars/progress remain
byte-identical; fresh packaged-process reload shows39,980 and leaves the save
unchanged. All307 closure paths import into an empty owned local store, preserve
the dependency graph and pass contents verification; imported ELF matches.
Isolated-profile installer and installed --help succeed. Runtime uses the normal
store, independently from the private-store integrity check. No physical RP5 or
public-release certification. See nix-r4-verification-2026-10-30.md and dated
package receipts. Wider goal and strict original119-D8M failure remain open.

Final r4 runtime archive:319,432,640 bytes, SHA-256
0ffcbc39842e0312a06487e40e874571b115e153dd5755c194f682308accdc20.
Exact compiled-source archive:5,711,052 bytes, SHA-256
0ac7a20fb5f69e6cb9985a4e12434c529a5dc3c779b294a060cba9518177f1d1.
Complete payload readback passes. Restored source has identical NAR identity
and instantiates the exact original derivation; no second binary rebuild claim.
Only owned raw distribution staging/private import root cleaned after retaining
compressed artifacts and metadata. User explicitly continues using RP5;
no device interaction. Physical device remains APK19/car bundle r5.

## Further actual Arcade progress and Linux upload latency

Actual Nix r4 wins retail event6, Juice Games Series/Campbell_R1, three laps,
afternoon/wet/four Corsas, at167.16s. Real autosave changes only the earned
completion dword in ArcadeUnlock; total completions3->4. Separate process shows
1/5 and the first-event tick, byte-identical archive;13 negative controls pass.
See arcade-r4-verification-2026-10-30.md. This run predates the next renderer fix.

Linux's one-snapshot-per-frame upload queue reproduced161 requests in162frames /
2676ms. A bounded follow-up service loop, already used on Android, reduces this
to2frames/36ms. Same owned menu profile: initial Prelude preview log completion
5.981s on retained old dev ELF ->0.452s on corrected dev ELF; cars/reflections
visually confirmed. Native9/9 CTest and four Radeon GL groups pass; full
integration stays260/1, sole strict119-model gate. A current Radeon600-frame
installed405 sample reaches Go/movement; no second full-race/FPS claim.
Current usual launcher ELF:e2c88a43..., old4e21db0b... retained. Exactly four
code/build/test inputs differ from r4, other900 checked inputs unchanged.
Local profiles/car data preserved; RP5/APK untouched. Immutable r4 packages
predate the fix. See linux-loading-verification-2026-10-30.md. Wider goal open.

## Naturally complete Novice and corrected Nix r5

Current development ELF completes three unmodified sprint workflows: autopilot43
and initial manual42 lose each heat and preserve the archive; improved manual
shift/NOS timing wins10.83/10.84/10.85s,18points. Normal continue/autosave opens
Novice final. Independent whole-payload inspection permits only the earned
completion/lock/Eclipse19/Downtown25 flags; five negative controls pass.

Single r5 Nix build completes9/9 CTest; source input matches904 checked files.
Actual ELF68d4fa9e... wins original Novice final/fourlaps/sixEclipses at209.14s,
real autosave, separate-process reload shows COMPLETED and five ticks. Only
ArcadeUnlock changes; entire payload matches final completion and original two
bonus-route unlocks. Archivee9190322... unchanged by reload; eleven negative
controls pass. The controller's incorrect assumption of return-to-race instead
of return-to-championship was corrected with one additional ordinary A input in
the same process; no production defect/restart/save edit. Total completions6.

All307 r5 closure paths import/content-check successfully; isolated installer and
--help pass. Exact1,317-file source archive restores same NAR/derivation, not a
second binary rebuild. Runtime archive/readback/owned cleanup tracked separately.
No new production code changes or physical RP5 interaction; wider requirements
and strict original119-D8M gate remain open. See
novice-series-verification-2026-10-30.md and nix-r5-verification-2026-10-30.md.

Final local r5 runtime archive319,823,128bytes, SHA-256
f2a7b672f313a14169a84e0266ff086e8d160a11b0296116672f43b7cfc6507c;
exact compiled-source archive5,720,480bytes, SHA-256
ae15bfea5f81efbe424ecfa24912fdc21a8667a97941c80b06c962de3baf5485.
All payloads read back/hashed. Owned raw export/private import trees removed
after verification,2,211,859,858regularfilebytes; artifacts/metadata/source root
retained. Actual local profiles and installed car package still match recorded
hashes. RP5 left untouched. Broader goal remains open; no public release claim.


## Linux media dependency correction, r9 — 2026-10-30

Current development and Nix builds use LGPL-2.1-or-later FFmpeg with GPL,
version-3 and nonfree options disabled, matching Android's existing policy.
10/10 CTest, exact 38,400-frame before/after real WMA playback comparison,
missing-file and muted-movie negatives, pixel-identical character readbacks,
actual 900-frame Radeon six-car sample, 307-path import/content/installer and
source/archive restoration checks pass. Source, patches, notices and both
source companions retained. Full integration remains 260/1 (original 119-D8M
inventory); neither this nor broader verification is complete. Six personal
Linux save files preserved, RP5 untouched. See
`nix-r9-media-verification-2026-10-30.md` and `current-local-candidates-2026-10-30.md`.

## Installed-car customization matrix — 2026-10-30

Seven Wave 2 cars, all three kits plus stock, both hoods, no vinyl or retail
0/8/30: 224 configurations / 616 actual Radeon CarView readbacks; all 21 contact
sheets inspected. No reproduced panel loss/window projection in this bounded
sample. All image conversions preserve RGB; installed package/overlay asset
hashes and six personal saves verified. No production code/assets/APK/device
changes. All31×allkits, normal workshop save/purchase, damage, close art and
physical Android remain unproven. Evidence/scope in
`customization-matrix-verification-2026-10-30.md`; full goal still active.

## Native workshop purchases/save/reload — 2026-10-30

Current r9 development ELF completes normal driver load -> Career -> imported
406 workshop, three kit3 body pieces, carbon hood, retail wheel47 and vinyl8
colour/purchase, actual autosave and fresh-process workshop reload. Six buys
cost14600; two quote cancellations are uncharged; cash985400, power unchanged.
Independent whole-save proof: only16 expected CMS bytes change, all unrelated
payload bytes/entries identical; fresh reload preserves the complete archive.
Eight negative controls reject corruption/wrong fields. One wrong-entry harness
attempt and one black transition capture are retained and excluded. Six personal
Linux saves and installed car files unchanged; three owned processes exit zero,
RP5 untouched. Scope/evidence: workshop-save-reload-verification-2026-10-30.md.
Other cars/categories/colours, damage, source art finish and Android remain open.

## Physical collision, persistent damage and repair — 2026-10-30

Modified 406: actual physical boundary contacts deform body and vinyl, normal
abandonment/results autosave contains26 nonzero damage bytes. Two fresh ordinary
production processes prove damaged reload, uncharged cancelled repair,500 paid
repair, zero damage grid, normal autosave and repaired reload. Exactly38 allowed
byte values change in CMS only; every unrelated career/archive byte is preserved
in the production repair. Nine negative controls reject altered results. Three
owned games exit zero; installed car files and six personal saves unchanged;
RP5 untouched. `collision-repair-verification-2026-10-30.md` records observer
limits and the open post-race history/respect save-order discrepancy. This is
one car/configuration, not all-model damage approval or full goal completion.

## Post-race save ordering correction — 2026-10-31

Resolved the prior checkpoint's respect/history discrepancy. Canonical final
respect and one event history sample now precede the terminal summary autosave;
the subsequent continuation does not add a duplicate point. Unmodified r9 actual
UI saves568 then584, with stale568 history; fixed actual UI saves584/history584
immediately. First autosave, Career exit save and third-process reload archive
are byte-identical. Same regression tests with original summary units42pass73fail,
fixed115pass0fail; native10/10, Android host4/4. Final full integration with the
existing model inventory and matching stock-save fixture371pass1fail, still the
original119D8M. New signed local APK24 includes the shared fix and passes ARM64,
API26, certificate and16KiB audits. Physical RP5 untouched (APK19/packr5), six
personal Linux saves and installed Wave2 unchanged. Habitual Linux launcher uses
the fixed development ELF; last packaged Nix runtime remainsr9 pending refresh.
Details/limits: post-race-save-verification-2026-10-31.md. Broad goal remains active.

## Nix r10 package verification — 2026-10-31

The pending package refresh above is now complete. Actual clean Nix build10/10
CTest,907 critical inputs/all8 save-fix inputs matched,7 installed notices checked.
Actual Nix ELF runs normal controlled Career/event/abandon/results/skip/autosave/
save-exit on Radeon and exits0 after2111 frames. First and exit archives identical,
584 respect/history584, one new history point. Eight readbacks inspected; four
early UI/black transitions explicitly excluded from settled appearance evidence.
All307 paths imported into a fresh private store, graph/content check and isolated
installer/help pass. Exact1336-file source restores same NAR/derivation; runtime
tar21 payloads read back,23 preceding checkpoints bound. Only two owned temporary
distribution trees removed (2210838765 logical bytes). Six personal saves,
installed Wave2, APK24 and habitual development ELF unchanged; RP5 untouched.
Local r10 runtime/source supersede r9; no public upload or full-goal completion.
Scope/checksums/evidence: nix-r10-save-verification-2026-10-31.md.

## Close car review / Outback replaceable exhaust — 2026-10-31

Close165-view before/after native review of retail Focus, Jetta and Outback:
all110 Focus/Jetta images identical. Identified baked source Outback tailpipe
plus donor exhaust duplication. Hierarchy-only classification preserves all74
source meshes; independent native triangle/normal/UV/material comparison proves
all four kits in both driving/garage preserve original body+pipe geometry.
16 exhaust states/64 views before and after; actual slots/materials/refits pass,
four rear comparison sheets inspected. Delivered r7 ZIP extracted/reassembled:
1238 checks pass, all64 packaged images match candidate,3002 payloads read back.
Linux installed atomically, previous package retained, six personal saves intact.
Ordinary workshop No quote/requote/$90 buy/autosave/fresh-process load passes:
cash999910,171BHP, only8 expected CMS byte changes, all unrelated/archive bytes
preserved. Two accepted processes exit0. Invalid harness pointer assumption,
missing controller env attempt and misleading screenshot names explicitly
excluded. Development/Nix/APK binaries unchanged; RP5 untouched. Reproducible
owned caches/extraction cleaned; broad goal, retail art parity, physical Android
and original119D8M inventory gate remain open. Details in
`outback-exhaust-verification-2026-10-31.md`.

## Other five Wave2 cars at close scale — 2026-10-31

405/406/GolfR32/240/V70:275 actual fixed-camera readbacks,25 contact sheets and
three full-size frames inspected. Rear wheel placement406/V70 and both240 lamp
textures look intact; bounded hood/vinyl/kit3 states have no missing panels.
240 carbon front painted band remains a source seam/fold question, not an
approved boundary or guessed fix. Source plates/mirror/light simplification
remain art limits. All processes exit0,PNG RGB/hash receipts retained, six
personal saves/installedr7 verified unchanged; physical RP5 untouched. Redundant
owned full candidate clone removed after matching all installed payloads;
oldr6/newr7 independent geometry comparison replay passes with manifest guards.
No new runtime release or whole-goal completion. Details/gallery in
`wave2-close-review-2026-10-31.md`.

## Volvo240 plate / forward carbon correction — 2026-10-31

Linux now uses bundle r8 / Wave2revision7; normal development ELF and APK24
unchanged, RP5 untouched. Source glTF MASK lettering now uses native double-sided
ATest; original front/rear letters render. Forward carbon extends over the
retained upper hood surface. Independent stock+three-kit native body coverage,
normal/UV/material/winding proofs pass with six negative controls. The sole
retessellated decal has a separately measured offset envelope and44 before/after
native views; strict overlay equality was not claimed. Correctly controlled
Focus/Jetta110 views are RGB-identical; all55 extracted-ZIP Volvo views match.
Native checks1238/0; all3002 ZIP payloads read back. Ordinary controlled Career
buys kit3 front bumper/$3500 and carbon/$2500; fresh process retains153BHP,
$994000 and byte-identical bought/reloaded archive. Exactly nine expected CMS
byte values change; eight save negatives reject corruption. All6 personal Linux
save hashes match. ENOSPC recovery and RGB-verified duplicate-BMP cleanup are
recorded; rollback and source inputs retained. See
`volvo240-plate-hood-verification-2026-10-31.md`. Global goal remains active;
this does not close the original119-D8M failure or physical Android validation.

## Actual poker title win / physical prize / fresh reload — 2026-10-31

Current ordinary production ELF wins a natural heads-up title hand against TK:
+$14195 equity, consisting of$1000 cash and the exact211BHP rival Eclipse worth
$13195, while retaining the original two car records. Actual save-device write
and separate ordinary profile load/third-car workshop display pass. Independent
native exhaustive rival-pool/colour/serialized record matches; independent archive
normalization removes only242-byte prize/count/cash and finds every other CMS
byte/archive entry unchanged. Eight negatives reject corruption; both game
processes exit0 on Radeon,6 personal save hashes and installed r8/ELF/APK24 match.
Preliminary wrong-car screenshot captions are excluded. Source/tests/packages
unchanged. Fresh ModelFiles suite remains3pass/1fail with the same119 paths and
payload hashes; targeted actual-loader requests pass. Strict inventory coverage
and playability are recorded separately without exclusions or an unreachable
claim. See `poker-title-win-verification-2026-10-31.md`. Broad goal remains active.

## Normal Multiplay/local split access — 2026-10-31

Confirmed MainMenu routed directly to LAN, hiding the already implemented local
selector. Native Linux/macOS/Android now enter existing multi; LAN retains its
network startup and search action, legacy Win32 route unchanged. New routing
regression fails13/1 before; MainMenu15/0 and Multi4/0 after. Native10/10 CTest,
Android host4/4, full original integration377/1 (same119D8M gate). Development
and independently built Nix r11 both follow normal title/profile/main/local
setup, two virtual-pad car choices, complete one-lap/two-player race, player2
pause/resume, full-screen results, return/LAN search/back. Autopilot drives;
physical inputs/audio/FPS remain open. Each process exits0, synthetic archive
and all6 personal saves unchanged. Signed APK25 audited, not installed. Nix
matches907 critical inputs/three changes,1342 exact-source restoration and307
closure-path import/content checks. Details in
`split-main-menu-verification-2026-10-31.md`; broad goal remains active.

## Portable test matrix / receiver-order audit — 2026-10-31

Nine portable integration sources were absent from Linux's compiled test list.
They now build and seven protocol suites join CTest:17/17 locally and in clean
Nix r13.119 unit cases execute388 checks/0 failures;133 integration cases execute
407pass+1fail, with zero empty cases. The ordinary production test runner gives
the same407/1. The original119D8M strict gate remains unchanged and visible.
Five case-report negatives reject empty/truncated/zero-check/mismatched/duplicate
reports. Actual original-EXE table comparison runs, rather than skipping.

Presence fixture waited for sender completion before receiver output was polled:
retained TCP bytes demonstrate missing final frames. Wait now includes the final
receiver frame, retaining all payload/MD5/callback checks; independent wire parser
and seven negatives pass, suite9/0. Matchmaking initially failed intermittently:
actual packets3,8,1,3 prove a valid same-instance keepalive displaced expected
replies in the stand-in's next-datagram assumption. The fixture recognizes only
exact same-instance keepalives; all reply/session/NAT checks remain. Deterministic
injected interleaving fails2/1 with original one-read assumption and passes3/0
with correction. Four identity/boundary checks pass. Original failure logs kept.

No production engine/Android/assets changes. Development ELF/APK25 and installed
Linux r8/Wave2revision7 unchanged;6 personal files and synthetic save tree match.
New Nix game ELF and306 dependencies byte-identical to distributed r11, normalized
wrapper identical; prior actual race evidence reused by identity, not called a
new r13 race. Exact r13 source independently restored; no duplicate runtime
archive or public upload. RP5 remains untouched/APK19/packr5. Remaining platform
test exclusions and audio logic/audible-mix distinction are explicit. Details in
`linux-test-matrix-audit-2026-10-31.md`; broad goal remains active.

## Production stereo pan / actual generated PCM — 2026-10-31

Found shared native SetPan interpreted hundredths of a decibel linearly, leaving
intermediate balances too centered. For0.5 input/+2000 pan, actual left output
was0.4 instead of0.05. Corrected conversion preserves center/edges/volume/3D.
Same actual-callback/SDL-queue regression fails33/5 before, passes38/0 after;
independent measured-channel verifier rejects six bad reports. Native/Nix18/18
CTest, ordinary unit388/0 and full integration407/1; original119D8M gate visible.

Development and new actual Nix wrapper/ELF each follow normal profile/main/custom
selection and complete a six-car/one-lap race, return and exit0. Four six-second
PCM captures finite, bounded, nonzero and stereo; menus unclipped, races2.99–3.82%
at saturation, retained as an open listening/reference question. Private Xvfb/
llvmpipe/virtual pad/autopilot/dummy sink scope; locked-desktop attempt excluded,
no speaker/physical/FPS claim. Transitional results captures not labelled settled.
Both synthetic archives,6 personal files and installed r8/Wave2revision7 match.

Clean Nix matches908 inputs/four changes,1345 exact source files restored to same
NAR/derivation,306 dependencies unchanged. New runtime export/import archive is
pending. Signed APK26 audited with original certificate,7 ARM64 libraries/API26/
ELF+ZIP16KiB,4 host tests; only libmain changes, six deps/APK25 unchanged. RP5
untouched/APK19/packr5; no macOS rebuild or public upload. Details in
`native-audio-pan-verification-2026-10-31.md`; broad goal remains active.

## Nix r14 runtime distribution closure — 2026-10-31

The pending audio-fix runtime is now exported and packaged locally.307 paths
import into a fresh owned private store with exact graph/content verification;
imported ELF matches the actual r14 runtime. Installer runs under an owned
profile and its same-identity wrapper --help exits0. All23 compressed archive
payloads are independently read back; actual sha256sum -c CONTENTS.sha256 checks
22 files. Source/media/car/APK companions and31 verified preceding checkpoints
are bound. New instructions identify r14 and APK26, retaining upstream notice.
No game/mod assets, recorded audio or personal saves are embedded.

Only two owned export/import trees removed after verification:2210840681 logical
regular-file bytes. Actual store/source GC root/profile/final archives/metadata
retained. Six personal files, installed r8/Wave2revision7, development ELF/APK26
and usual launcher match prior identities. RP5 untouched/APK19/packr5. Earlier
same-identity actual Nix race/PCM is reused explicitly; no new private-store
gameplay or physical-input/audio/Android/FPS claim. No public upload/commit.
Details/checksums: `nix-r14-distribution-verification-2026-10-31.md`. Original
119D8M failure, further gameplay/art/device/network/audio reference and final
source/license/publication work remain open; broad goal stays active.

## Saturation source/replay study — 2026-10-31

Out-only observer preserves original Audio.cpp operations, four other audio
archive members and all game/render/core libraries; same38 mixer checks pass.
Independent Python extraction of five original banks/89 named entries matches
41 captured buffers/31 unique PCM payloads. Independent signed16 decoding is
bit-exact; no raw source rail hits. Logged raw-voice replay reconstructs3D and
full sum with max2.384e-7 error; actual successful SDL output is exactly the final
clamp across288000 stereo frames, zero shift. Twelve copied-data/bank negatives
reject corruption. This capture:3D peak2.236/1.663% over full scale, music1.113/
0.0026%, sum2.482/3.603%. Floating music source peak1.479 remains a separate codec/
reference question. Original Windows spatial law/listening remains unverified;
independent Wine source suggests different panning and is explicitly not a
Windows runtime measurement. No production normalization/limiter/gain change.

Normal instrumented six-entrant race: player fifth66.98s/results/return/exit0;
five finish records. Opponent4 remains low-speed near waypoint78 at results.
Needs unmodified-game reproduction, not called a proven port regression or
permanent stall. Private Xvfb/llvmpipe/virtual pad/autopilot/dummy sink; no physical
claim. Controlled archive,6 personal files, installed r8/Wave2revision7, all908
production inputs, development/Nix r14 and APK26 unchanged. RP5 untouched; no
public upload. Details: `audio-saturation-study-2026-10-31.md`. Goal stays active.

## Unmodified race follow-up — 2026-10-31

Development ELF 556cc99f completes normal title/profile/main/custom selection,
one-lap six-car race, six actual finish events, player fourth (64.82 s), return
and exit 0. Car 4 makes one deliberate AI mistake but finishes third (64.63 s);
the diagnostic low-speed trajectory is not reproduced, not dismissed or fixed.
Only input/framebuffer probes, private Xvfb/llvmpipe and owned copied saves;
no audio observer. Results capture remains an entry transition and is excluded
from settled UI approval. All 908 critical inputs, installed Wave2 and six
personal files unchanged; owned display terminates. No device/public action.
Details: `ai-recovery-followup-2026-10-31.md`. Broader goal stays active.

## Complete reverse-route pilot — 2026-10-31

Current development-r14 diagnostic route entry completes two laps with all six
stock Focus cars on Angel North R3 reverse at night/wet. Player first 222.99 s,
last car 233.50 s, native Radeon offscreen results capture shows six positions,
normal exit. Independent original TDF/SPL hashes and141-point spline bounds,
actual missing-route exit-zero rejection and seven copied-log negative controls.
All908 critical inputs, six personal files and installed Wave2 remain unchanged.
Other eight district cases were queued across three owned runner handles and
remain pending in this pilot checkpoint, not implied passes. No RP5 access or
new build/bundle. See `route-completion-pilot-2026-10-31.md`. Goal stays active.

Progress after the reverse-route pilot: Engstrom R3 reverse also completes two
laps and six finishes, player131.84 s/final151.90 s, normal exit and inspected
six-position results view. Snapshot:2/9 completed,7 pending; three actual owned
games in districts2/6/8 remain live. Completed-case-only checkpoint plus handle
receipt retained; batch and overall goal remain active.

## Reverse-route matrix complete — 2026-10-31

All nine district reverse-R3 races now have two laps, six actual finishes,
Radeon offscreen frames and exit0; all runner queues are terminal0. The previous
2/9 snapshot is historical. Independent TDF/SPL bounds, actual missing-route
exit-zero rejection and seven copied-log negatives remain calibrated. Reviewed
results gallery uses a separately identified West Anderson repeat after6.5s
wait: original UI final text fades need5s, so its premature primary image remains
excluded rather than relabelled. A separate unchanged-ELF fixed1/60 R2-reverse
pilot completes six finishes, last184.35sim s in107.929wall s (not FPS/Android).
All908 source inputs,6 personal files and installed Wave2 unchanged. No device,
production build/asset or publication action. Full details and the remaining
scopes: `route-completion-pilot-2026-10-31.md`. Overall goal stays active.

## Remaining-circuit sweep complete — 2026-10-31

All72 unique circuit references have completed races, retaining10 prior routes
and adding62 new verified cases. Every new case captures driving and results,
with at least360 actual probe frames (6 fixed-step simulated seconds) between
results baseline/final readback. Per-line spline bounds, six finishes, both laps,
native image parity and terminal PIDs/queues are checked. All62 driving views
and62 settled six-position results views inspected; no whole-route/art/Android/
FPS approval. No failed/active/pending cases. All908 inputs/6 personal files/
installed Wave2 preserved. CP37's19-route snapshot is historical. RP5 untouched.
Broader scope continues pending asynchronous clarification; no goal completion.
Details: `circuit-sweep-progress-2026-10-31.md`.

## Full supplied Bink CPU corpus — 2026-10-31

All472 original clips/all88,631 frames match an independent FFmpeg decoder by
canonical original-resolution planar frameSHA256, representing20,123,365,248
decoded bytes. Three pilot raw frames also directly match and independently
calibrate SHA; five malformed inputs and five copied-output negatives reject.
All944 decoder/reference subprocesses and corpus runner terminal0. Separate
verifier rereads every original/output/hash/header and retained production
library identity. No production rebuild/assets/user data/device/public action;
all908 source inputs, development ELF,6 personal files and installed Wave2
unchanged. This closes supplied-corpus CPU video decode comparison only;
conversion/GPU/UI/audio/timing/Android remain open. See
`bink-corpus-verification-2026-10-31.md`. Overall goal remains active.

## Normal-entry P2P pilot — 2026-10-31

Unchanged development556 ELF, no --test/--track: actual title/profile/custom
menus select Angel North P2P01; six stock Punto finish126.14–127.96s, player
fifth127.72s, settled six-row results/Continue/normal menu return/exit0. Own
synthetic route-unlock fixture changes only Arcade route flags, preserving
all other fields/entries and source seed. No natural campaign/unlock claim.
Original TDF/SPL hashes/POINT2POINT last142/per-line bounds, five copied-trace
negatives, image parity and game/display terminals independently checked.
All six view samples inspected, not original-Windows HUD/art parity. All908
inputs, run seeded ModSaves,6 personal files and installed Wave2 unchanged.
Private Xvfb/llvmpipe/virtual pad/autopilot/dummy audio; no physical/FPS/Android
approval. No new build or RP5/public action. See
`p2p-normal-entry-verification-2026-10-31.md`; broader goal stays active.

## Complete normal-entry P2P matrix — 2026-10-31

All18 P2P routes complete, one retained pilot/17 new unique races, actual normal
title/profile/custom menu, six finishers/results/Continue/return and exit0.
Player Punto, ordinary varying opponents/default setup; virtual pad/autopilot/
private Xvfb llvmpipe/dummy audio. Original strict warnings stop is retained:
three finished cases had MX-5/Corsa factory-part absence warnings. Fresh retail
audit equals prior610-reference report;37 exact absent names qualify only when
both race/UI searches fail and original slots are NONE without package overrides.
Separate reviewed verifier/resume retains logs/rejections/STOP; no invented mesh,
production warning/parser change, 119-D8M gate exclusion or Windows quality claim.
Six negative trace variants include rejecting a warning for an existing mesh.
All17 map/driving/settled-results triplets inspected,85 BMP/PNG pairs checked;
all queues/games/displays terminal, all908 production inputs,6 personal files,
run ModSaves and installed Wave2 unchanged. No APK/asset/device/public action.
Removed only two superseded compressed runtime exports r10/r11 (639401916B),
retaining exact sources, rooted binaries, checksum/verification/checkpoint
receipts and current r14/APKs/car packs/user assets. Details:
`p2p-sweep-verification-2026-10-31.md`. Broad closure remains active.

## Complete normal-entry Sprint matrix and Showoff timer — 2026-10-31

All9 Sprint routes complete3heats each, four distinct finishers each:27heats and
108finishes. Starts/restarts/results/return, original TDF/SPL bounds, seven
negative traces and native BMP/PNG pairs independently checked. All9 final
leaderboard tables match6/4/2/0 points independently summed from places;81
sampled driving/results/leaderboard panels reviewed. Legitimately added global
high-score files are fully checked across109records, only the correct Sprint
route changes to Player/Punto/best player time; original seeded files unchanged.
All queues/games/displays terminal0, all908 inputs/6 personal files/installed
Wave2 unchanged. No production build/assets/device/public action.

A separate current Angel North Showoff reaches natural180.01s expiry, zero-score
results/Continue/menu return and exit0; timer images and motion/spline bounds
checked. Positive tricks/scoring/records remain open, as do the other8 Cruise
maps. The earlier disk-interrupted setup never started a race and is retained
separately, not counted as a pass. Retired four more superseded compressed
runtime copies, preserving exact sources/rooted outputs; the inactive test AVD
image has a hash-verified durable copy on Ediloco after temporary RAM recovery.
See `sprint-matrix-verification-2026-10-31.md` and `out/android-avd/STATE-RETIRED.md`.
The strict119-D8M failure and broader closure requirements remain open.

## Positive Showoff score and fresh native record reload — 2026-10-31

Current unchanged development ELF completes stock152BHP CR-X Downtown Showoff:
natural180.04game seconds, final36points, route33 record36/Player/model4, normal
menu return/exit0. Independent full109-record table check; other108 exact
defaults. Fresh process/profile load shows SCORE0/RECORD36 on the first HUD,
then deliberately quits, not another completed race. Six trace negatives and
all7 scoring/1 reload BMP/PNG pairs checked. A separate owned fixture changes
only the CR-X lock flag and archive checksum; prior locked-car selection was
correctly rejected and is not counted as a race. All908 production inputs,
6 personal files and installed Wave2 unchanged; no build/assets/device/upload.
This resolves the preceding positive-score/record/reload gap for this route;
seven other Cruise maps, broader trick/formula/Windows/physical/Android scopes
remain open. See `showoff-positive-verification-2026-10-31.md`. Goal active.

## Complete Showoff route matrix and factory callers — 2026-10-31

All9 original Cruise/Showoff routes now complete normal-menu/clock3minute
workflows/results/Continue/menu return/exit0. Retain Angel North Punto0score
and Downtown CRX36score/fresh record reload; add7 stock CRX routes with positive
scores1231/573/4954/279/29/6528/2700. Every added109-record table independently
checked, other108 defaults; all seeded profile files unchanged. Seven native
start/late/results contact sheets inspected,21sampled panels;49BMP/PNG pairs
match. Southside's late autopilot is stopped beside a wall; no recovery or
full-route traversal claim. All3queues/games/displays terminal0; all908 inputs,
6 personal files and installed Wave2 unchanged. No build/assets/RP5/public action.
Together with earlier72circuit/18P2P/9Sprint evidence, all108distinct route
workflows complete, with differing clock/environment/visual scopes. This is
not whole-game, all weather/trick-formula/Windows/physical/Android approval.
See `cruise-matrix-verification-2026-10-31.md`.

Four additional original function dossiers (22total) cover remaining recorded
file-factory direct callers and the stream-dispatch parent. Original calls and
ASCII literals checked independently against PE bytes/objdump; all119rejected
original payload hashes and native source inputs preserved. Recovered ABI,
indirect-name producers/other dispatch branches and original whole-program
reachability remain unresolved. REA evidence exported/session closed; no
invented legacy parser/zero-empty acceptance/gate exclusion. See
`legacy-factory-callers-verification-2026-10-31.md`. Overall goal stays active.

## Current natural Juice Games win/loss persistence — 2026-10-31

Retained naturally earned6-completion profile, no progress/race/AI/performance
edits. Four normal-menu/clock six-car3lap races: Prelude second166.18s, separate
Prelude retry third168.66s, RSX first220.96s, Neon third197.96s. Only the RSX
win advances6to7; full ArcadeUnlock payload matches only dword60=1, every other
archive entry unchanged. All3loss progression archives byte-identical. Native
lap records update legitimately; entire109-record tables checked with other108
unchanged per event. Fresh Neon process shows saved ticks1and3 before racing,
then loss preserves same7-completion archive. Six trace negatives per case plus
unearned-win/checksum-corruption negatives;25native BMP/PNG pairs and15sampled
panels checked. Original erroneous autosave-after-loss driver result remains
separate from independently verified game loss/normal return/exit0. No forced
unlocks: Juice Games remains2/5; final/next series locked. All908 inputs,
6 personal files and installed Wave2 unchanged; no new build/assets/RP5/upload.
See `juice-games-progression-verification-2026-10-31.md`. Goal stays active.

Separately offloaded296 already-terminal generated BMPs (1,091,215,248B) to
Ediloco with fsync/hash readback and original-path symlinks. PNGs remain local;
all saved hashes reread and case pixel checks still pass through retained paths.
No live capture/game/profile/source moved. Keep Ediloco mounted for raw-BMP
revalidation; `full-race-ui/RAW-BMP-ARCHIVE.md` explains restoration.

## Manual-input Sprint and grid rotation correction — 2026-10-31

The first current manual-input three-heat run exposed a real stale-heat caller:
`restartRace()` advances live RaceState rules, but `gridPlace()` read initial
TrackView rules. Player spawn repeats grid3 on all heats. The minimal caller
correction reads the live rules, retaining the network/empty-grid fallback.
Recompiled normal-launcher development ELF `e8450e7d…` now starts at original
SPL grid3,0,1 in the three actual repeated heats. The original ELF/source are
preserved and explicitly reject the rotating-grid contract; this is not a
retrospective all-green baseline. Both runs use normal title/profile/Arcade
selection, original437BHP SRT-4/route/competitors, manual virtual SDL inputs,
ordinary clock and no autopilot. Native throttle/upshifts/NOS work, all12
competitor finishes/three first places/18player points per run are checked.
Post-finish kind1 hand-off is distinguished from driving kind0 by the passive
compiler-layout-bound observer; no game state writes/calls are used to drive it.

Both whole progression archives retain seven earned completions. The baseline
legitimately improves route58's model42 global record to10.81s, other108 records
unchanged; corrected run retains the entire previous record table. Full build
and18/18 CTest pass,24trace/telemetry negatives and the separate stale-heat
negative are calibrated.52native BMP/PNG pairs and28actually inspected selected
panels retained. Exactly one of908critical sources changes; installed Wave2 and
six personal files remain exact. Drivers/game/owned displays all terminal0.
APK26 and Nixr14 still predate this new development fix; distribution refresh
remains pending. RP5 was not accessed; no upload/commit. Braking/steering/
downshifts/handbrake/physical/Android/fidelity and broader campaign/network/
119-D8M/final distribution review remain open. See
`manual-sprint-grid-verification-2026-10-31.md`. Overall goal stays active.

## Sprint fix distribution: Nixr15 and Android27 — 2026-10-31

New clean Nix package incorporates live-heat grid correction;18/18 CTest,
908actual inputs, expected LGPL closure/licenses, exact1360-file source restored
to same NAR/derivation. Actual new wrapper/ELF hash checked via `/proc`; normal
title/profile/Novice Sprint/manual virtual pad/ordinary clock, three first places
11.01/10.92/10.98s,18points, normal results/return/exit0. Passive observer bound to
the new ELF through an independently calibrated typed-vtable resolver;8corrupt-
copy negatives plus12runtime negatives. Original SPL lanes3/0/1 and real throttle/
upshifts/NOS checked;3,557samples,26BMP/PNG pairs,14actually seen sampled panels.
Whole seven-completion private archive and entire global record table unchanged.

All307paths freshly imported with exact graph/content checks; isolated installer/
help,22runtime archive payloads read back and21CONTENTS checks.45prior checkpoint
hashes and source/media/car/APK companions bound. External noexec-stage direct
installer failure is retained separately; interpreting the same script with bash
passes without changing mount flags, restarting export/import or duplicating the
game run. Only fresh owned external raw export/import test trees are cleaned
after successful archive verification. Real store/source GC roots, archives,
isolated profile, source restoration and metadata retained.

Signed APK27 native compile includes the shared fix. Certificate unchanged,
sevenARM64 libs/API26/ELF+ZIP16KiB,4host tests. Complete ZIP comparison: same entry
set, unchanged Java dex/six auxiliary libs, onlylibmain.so/AndroidManifest.xml
payloads differ versus26. No physical Android install/approval. Default Linux
launcher/development ELF, all6personal files and installedWave2 r8 unchanged.
NoRP5/commit/upload. Broader gameplay/fidelity/physical Android/final distribution
and strict119-D8M scope remain open; no whole-game/60FPS claim. See
`sprint-grid-distribution-verification-2026-10-31.md`. Overall goal stays active.

## Manual steering/brakes/gears/pause workflows — 2026-10-31

Two actual normal-menu manual short drives on unchanged ELF e845: original
SRT-4 Sprint, ordinary clock/no autopilot/physics edits, native quit YES/menu
return/exit0.67phases/5,903samples: moving steering changes world heading, held
up/down shifts once, N/R/backwards displacement/lower bound/first restored.
Rolling pause freezes position with nonzero stored speed, then resumes motion.
Actual SDL return logging calibrates partial triggers to stock deadzone/square/
cube behavior. Neutral coast9.485→9.478m/s versus handbrake9.698→4.886 over1.87s;
footbrake9.496→0 in1.258s. Neutral excludes engine braking; field and physical
response checked. Phase snapshots bound to raw frames;26negatives plus rejected
initial title-only UI attempt.62BMP/PNG pairs,27reviewed states/four HUD zooms.

Early selection image still has old FTO before async preview settles: kept/not
approved as settled-model image; driven white SRT-4 correct. Premature A/menu
attempt, layout tuple error and steering-ramp verifier failure retained separately;
no game source change to mask them. Full private save sets/profiles/global records/
seven earned completions unchanged.908sources/6personal files/Wave2/current
artifacts/launcher unchanged.62generated BMPs fsynced/readback to Ediloco with
exact original-path symlinks,228,565,356B offloaded; PNGs local. All owned
processes terminal; no RP5/commit/upload/full-game or physical approval. See
`manual-driving-controls-verification-2026-10-31.md`. Goal stays active.
