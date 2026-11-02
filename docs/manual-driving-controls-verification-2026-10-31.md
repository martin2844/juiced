# Manual driving controls — 2026-10-31

Two actual normal-menu Linux workflows verify steering, brakes, gears, reverse,
pause/resume and native quit on unchanged development ELF `e8450e7d…`. Both use
the original437BHP SRT-4/Novice Sprint, ordinary clock, private natural seven-
event profile and virtual SDL pad. No autopilot, physics/race edits or forced
track. They deliberately abandon short drives through native confirmation and
return to the menu; neither is a completed race or earned progression win.

No production change/new build was needed. Nixr15/source/APK27/car-r8/launcher,
all908 critical inputs, all6personal save/creative files and installedWave2 retain
their hashes. RP5 was not accessed and nothing was committed or published.

## Measured response

The first case has43phases/3,902passive samples. Settled negative/positive stick
inputs turn the steering value appropriately and recenter to zero. Moving left/
right commands also change world heading in the corresponding directions. A
held upshift/downshift produces one change rather than repeated shifts. Neutral,
reverse, the reverse lower bound and return to first work; reverse produces
negative longitudinal speed and backwards world displacement. Actual native HUD
detail independently shows N, R, moving R and1.

Pausing while rolling freezes position while stored velocity remains nonzero;
Continue resumes motion. Native pause/Abandon Race/confirmation/menu return
states were inspected. Footbrake and handbrake fields respond and release.
First-gear coasting has strong engine braking, so that first case alone is
explicitly not accepted as an isolated proof of handbrake braking force.

The second case has24phases/2,001samples. It selects neutral using ordinary gear-
down inputs, excludes engine braking and compares nearly equal initial speeds,
headings and durations with throttle/footbrake off during both comparison windows:

| Neutral comparison | Start | End | Elapsed |
| --- | ---: | ---: | ---: |
| Coast, handbrake released |9.485m/s |9.478m/s |1.874s |
| Handbrake held |9.698m/s |4.886m/s |1.871s |
| Footbrake, handbrake released |9.496m/s |0m/s |1.258s |

Handbrake deceleration is2.572m/s² versus0.0037m/s² coasting. Its physical effect
is thus distinguished from engine braking or footbrake; release is checked too.
These are bounded SRT-4/route observations, not all-car/real-car handling or
Windows fidelity approval.

## Input and observation calibration

The out-only observer adds compiler-derived `CarBody::handbrake` and TrackView
pause offsets to the existing pose/velocity/pedals/gears stream. Game memory is
read only with `pread`/O_RDONLY; no game fields/functions are written/invoked.
An additional hook logs actual `SDL_GetGamepadAxis` results, invokes the original
once and returns its value unchanged. Missing symbols fail the diagnostic rather
than inventing input. All67 phase snapshots match exact raw telemetry frames.

The virtual joystick's signed trigger range needs proper normalization. Its
positive raw16384 is not50% actual trigger pull. The second run logs real SDL
values0/3276/16384/32767 and independently checks the original map/device percent
format/action deadzone/pedal threshold/square-or-cube contracts:

| Actual SDL trigger | Throttle | Brake |
| ---: | ---: | ---: |
|0 |0 |0 |
|3276, about10% |0 |0 |
|16384, about50% |0.140625 |0.052734375 |
|32767, full |1 |1 |

No curve/deadzone was changed. The initial case's raw3276/16384 are retained as
relative levels, not mislabeled as actual10%/50% pulls. Steering dead input at
quarter signed range and signed half-range response are recorded separately.
This is virtual Linux input calibration, not physical/Android calibration.

The verifier checks both workflows,5,903samples,67phase bindings, all62BMP/PNG
pairs, full private ModSaves sets/archives/checksums/seven completion flags and
the entire global record tables. All private save files remain byte-identical.
13negative controls per case reject false snapshots, wrong routes/missing phases,
AI driving, bad steering/pause/gears/reverse/handbrake, false real-SDL reads and a
handbrake with no measured slowing. The initial title-only attempt is an extra
rejected workflow, never accepted as manual-control evidence.

## Visual limits and retained failures

27unique states across five contact pages and four native HUD zooms were actually
seen. One early selected-event image still shows the previous blue FTO preview
immediately after SRT-4 text is selected, before asynchronous loading settles.
That image is retained and explicitly not approved as a settled selected-model
frame. Actual driven white SRT-4 and returned FTO menu are visible. No full
preview visual parity claim is inferred from this early capture.

The initial driver pressed A before the driver-menu transition settled, missed
load confirmation and closed normally. It remains failed driver evidence. A
new named case restores the established0.8s UI waits; no duplicate live process
or game workaround. The out-only layout decoder's tuple-key error is retained
in `layout-helper-first-failure.json`. The first verifier also rejected valid
rate-limited steering ramps; its failure/range review is kept. Settled amplitude/
zero now use the final0.25s tail, while moving-heading/sign and negative controls
remain. These are harness corrections, not claimed game fixes.

After both sessions terminated,62generated BMPs/228,565,356bytes were copied,
fsynced and hash-read back under
`/run/media/martin/Ediloco/OpenJuice-backups/20261031-manual-controls-bmp`.
Original paths are exact-copy symlinks; PNGs stay regular/local. Final checks
reread archived copies. Mount Ediloco for raw-BMP revalidation; game execution
and portable PNG/checkpoint evidence do not depend on it. No original asset/mod/
source/save was moved. All owned games/displays/drivers are terminal; unrelated
existing Xvfb was left alone.

Every car/device, touch, physical Android, FPS/audio, camera/recovery variants,
broader campaign/team/network, Windows fidelity and strict119-D8M/final release
review remain separate open requirements. The overall goal remains active.
