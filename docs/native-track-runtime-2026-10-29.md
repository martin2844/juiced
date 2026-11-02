# Native district/weather runtime checkpoint — 29 October 2026

All eighteen finite native runtime samples pass: the first circuit route of
each of the nine districts, at time/weather 1/1 (morning/dry) and 4/2
(night/wet). This extends earlier parsing and single-route pilots. It does not
certify complete laps/events, all 108 routes or every combination of settings.

Exercised executable: `out/linux/juiced`, SHA256
`d44153bb2ad54ded777f1cbb57af59a9c5eee0b93b48658c96f51b60d60f7fec`.
No production source changed for this sweep. Each process uses original game
data, --no-mods, --test, seed 42 and a separate support directory. SDL Wayland
reports the actual AMD Radeon RX 7800 XT. Dummy audio avoids playing eighteen
parallel-to-desktop soundtracks; it provides no audible-mix evidence.

The --track/--car diagnostic starts a two-lap custom race with six stock Focus_2
cars, the player driven by --autopilot. Each process ends normally after 600
rendered frames. This is a diagnostic entry path, not a full frontend/career
setup or a human-control test. Frame counts and elapsed process times are not
an FPS benchmark. No game processes run concurrently in the sweep.

| District | Route | 1/1 | 4/2 |
| --- | --- | --- | --- |
| Angel North Central | Angel_North_R1 | pass | pass |
| Campbell Hills | Campbell_R1 | pass | pass |
| Downtown Angel City | Downtown_R1 | pass | pass |
| East Angel Island | East_Angel_R1 | pass | pass |
| Engstrom District | Engstrom_R1 | pass | pass |
| Southside beach | SouthSide_R1 | pass | pass |
| West Anderson | Anderson_R1 | pass | pass |
| Angel Westside | AngelWest_R1 | pass | pass |
| Angel SuperSpeedway | Speedway_R1 | pass | pass |

Each acceptance requires the requested track/route in the actual scene-load
log, the hardware renderer, player and opponents, Go, a new applied player
physics sample with positive speed/throttle, a nonblank 1280x720 framebuffer,
all 600 frames and exit 0. The matrix records all warnings. Every run logs its
own support/ModSaves folder; none creates a career profile. Existing player
profiles and original game assets are not edited.

All three contact sheets (all eighteen captures) were visually inspected.
Scenes, opponents, road surfaces and HUD render at these sampled viewpoints.
Campbell's wet/night frame additionally shows rain and lightning; the other
night scenes show their authored light sets and wet conditions where visible.
No new blank-scene or missing-geometry symptom was identified in these frames.
This is not an original-executable image comparison or proof for unobserved
parts of each route. The repeated warning is the existing missing
ui/windows/english/CMSGamble.eng; it remains recorded rather than filtered.

## Verifier calibration

A real negative run requests Campbell Hills/NoSuchRoute_Verification with the
same executable, GPU and data. It boots and renders 90 frames with exit 0,
but logs `track not loaded: no route ...`, creates no racers and never reaches
Go. The matrix correctly rejects this state. Process exit alone therefore
cannot count a route as working.

The first pilot incorrectly required an intro-skip log: --track starts with a
grid camera and need not play the normal frontend's fly-by. The next fixture
matched lowercase "ai" while the real log uses "AI". Both erroneous verifier
attempts are preserved separately. Correcting those assumptions did not change
production code or remove the actual scene/Go/physics/framebuffer checks. All
eighteen final samples rerun and pass using the corrected harness.

## Artifacts and remaining work

Evidence: `out/validation/linux-current-20261029/track-matrix`.

- `index.html`: eighteen full-size captures, grouped by district/conditions.
- `contact-page-{1,2,3}.jpg`: reviewed comparison sheets.
- `district-*/runtime.log`, `result.json`, `process.json`, `final.bmp/png`:
  exact commands, identities, final native pixels and terminal results.
- `run_matrix.py`, `negative_route.py`, `build_gallery.py`: reproducible harness.
- `negative-missing-route/result.json`: actual fallback rejection control.
- `gallery-receipt.json`: image identities, visual review and stated limits.

Raw BMP and PNG RGB pixels compare equal. All finite child processes are
terminal; this checkpoint leaves no owned game/emulator running. Full route
traversal, race completion breadth, further weather/light combinations,
physical controllers/RP5, audio quality and networking remain open in the
global closure ledger. The separate legacy-model audit does not suppress the
119-file strict inventory failure. Neither result declares the whole goal done.
