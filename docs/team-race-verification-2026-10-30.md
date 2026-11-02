# Team race verification and updated packages — 2026-10-30

Three actual native four-car team races now complete through normal frontend
selection, one lap, results, career settlement and autosave. They exercise the
preceding development executable, the caption-corrected executable, and a fresh
Nix package. All game processes and their private display servers exit zero.
This is a controlled validation fixture, not completion of the campaign.

The owned fixture recruits Vito using the native career operation, supplies the
existing Eclipse/GTR and schedules a two-v-two Circuit on January 2. The actual
route is Engstrom_R1, afternoon/dry, one lap. Original route geometry and race
parameters are used; recruitment, calendar placement, free entry, class eligibility
and prize amounts are controlled fixture choices. The player car uses diagnostic
autopilot; teammate Vito uses ordinary AI. No existing user save is used or changed.

## Confirmed bug and correction

The retail text table has a generic `2,2,2 TEAM RACE` string for two-member teams.
Four-car events displayed three crews in the calendar, driver selector and grid.
The shared caption helper now shortens that numeric list for an actual two-crew
event. It predicts adding the player's crew before entry, including the four-car
Sprint cap, and retains translated text (German also checked). Three crews and
three-member teams retain their appropriate captions.

The real-assets HostMeeting regression fails before the correction (10 checks
pass, 7 fail), then passes 17/17. The fixed native build and fresh Nix build each
pass CTest 9/9. Supported Android-host build passes 4/4. The before/fixed logs,
binary identities and actual corrected calendar/driver/grid captures are retained.

Development executable: `38d2f54bf85a326b78b9e8f92ec651c9d965aa30a72d36477a10810ba8a88cfc`.
Prior exercised executable is retained with its original hash in the fixture.
The Nix executable is a distinct build; `/proc/PID/exe` identifies the actual
wrapped ELF as `9b9f96cbd6e334db14efb014116a68c93c320675696d073312ed354cac2cb17c`.
The wrapper hash is recorded separately, not presented as an ELF hash.

## Driving, team results and persistence

The fixed development and Nix runs visibly exercise D-pad commands after Go:
Medium, High, High (upper clamp), Medium, Low, Low (lower clamp), Medium. Three
spaced captures per state avoid the HUD's flashing interval; contact sheets and
original captures are retained. The package contact sheet was separately viewed.
This verifies the normal Linux action map with an SDL virtual pad. It does not
certify the RP5's physical buttons or Android runtime.

All four cars finish each race. Team placement follows the slowest team member;
the player finishing first alone does not win the team event. MyCrew finishes
second, earning the configured $1,000. Independent archive parsing verifies cash
$40,000 → $41,000, two retained members, the event completed, Vito races 0 → 1,
driving ability 40 → 42.5 and recovery 70 → 72.5. Archive bounds/checksum are
validated, and optional OJM1 added-car progress remains present. Arcade/help
entries remain unchanged. A separate native career reader also checked the first
completed save. Different finish times between builds are not performance evidence.

A separate execution of the Nix package loads its completed profile and visibly
shows $41,000. Merely loading and viewing the crew screen leaves that archive
byte-identical. One capture selected an empty crew slot with Down; it is explicitly
excluded as evidence of Vito's displayed statistics. Saved Vito values are instead
certified by independent parsing.

`full-race-ui/verify_team_runtime.py` verifies three complete flows and independently
re-reads final archives. It rejects 47 negative controls: missing runtime phases,
false saved cash/member/progress values, a payload-corrupted archive and the
initial head-zero fixture that never entered the intended race. The rejected
fixture is retained, not counted as a fourth passing race. Baseline blank HUD
captures and transition-fade results are likewise excluded from visual claims.

## Delivery candidates and scope

Fresh Nix store output:
`/nix/store/miwvgcil0xmzpvslrqyxa3ddqzanvaw9-openjuice-1.02.5-linux-mods1`.
Its entire 307-path runtime closure imports into an empty owned local Nix store;
the dependency graph, all store contents and imported ELF hash match. The installer
and `--help` work with an isolated Nix profile. The packaged source references
include the previous checkpoint chain plus the six-file team UI source supplement.
There is no bit-identical development/Nix rebuild claim.

Android candidate: `out/releases/OpenJuice-team-race-ui-arm64.apk`, versionCode 18,
`1.02.5-android.17-team-labels`, SHA256
`2fe6a387c9438b4c25a705317e1962331ef83af3905181a9294053531aface93`.
Signature matches the prior update certificate. ZIP 16 KiB alignment, all seven
ARM64 ELF load alignments, NDK API 26 dependency closure and absence of release
debug activity pass. Only libmain changes among native libraries. This APK has
not received a new emulator/runtime or physical RP5 test. ADB has no devices.
The separate r5 car correction package is unchanged, not silently republished.

No FPS or audible mix claim comes from private Xvfb/llvmpipe/dummy-audio runs.
The original strict 119-D8M inventory failure, remaining career/route/replay/audio
breadth and physical RP5 validation remain open in the global closure ledger.
Candidates and checkpoints are local; no new release publication is claimed.

Evidence root: `out/validation/linux-current-20261029`. Main records:
`team-label-build-receipt.json`, `team-labels-{before,after,ctest}.log`,
`full-race-ui/team-runtime-receipt.json`,
`full-race-ui/career-team-reload-r2/reload-receipt.json`,
`team-package-r2`, and `android-team-labels`.

Local Nix delivery:
`out/releases/OpenJuice-20261030-r2-linux-x86_64-nix.tar.xz`, 319,180,516 bytes,
SHA256 `0f2e7c0c4b8c175568a1ff7210d693aefb246cf8cd7b01f9cdd33a4d23ed317b`.
Every archived payload was read back and matched. Only the exclusively owned
empty-store import fixture and raw staging are removed after proof; compressed
candidate, source supplement, installed package and receipts remain.

Six-file source supplement:
`out/releases/OpenJuice-source-update-20261030-team-ui.zip`, 25,029 bytes,
SHA256 `c93cbbd569f3f4fe32691b799293a3fafa5babe3f5cfc21950f67c8d06d6a90c`.
Its manifest extends the preceding source-package checkpoint and verifies all
six replacement payloads. It contains no original game assets or user saves.

The current complete enabled integration run also finished: **251 passed, 1 failed**,
exit 1. The only failure remains the strict original model inventory. This uses
an isolated copy of the class-7 validation save and the independent inventory
fixture; `team-current-all-integration-{exit.json,log}` preserve its actual count.
It does not convert the failing integration gate into a passing release gate.
