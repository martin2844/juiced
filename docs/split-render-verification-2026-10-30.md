# Split rendering and interface verification — 2026-10-30

The user is using the RP5 and explicitly directed this pass to Linux. No device
commands, installations, exports or screenshots were performed. The installed
device remains APK v19 / car pack r5. This pass does not close the broader goal.

## Four defects corrected

1. Direct diagnostic `--track ... --car ECLIPSE --players 2` crashed in
   `TrackView::cameraTarget`: two views existed but the hardcoded one-player
   rules/grid left the second view without a player car. GameLoop now bounds
   requested players to 1–4, uses matching rules and assigns one player car and
   driver map per requested player. Normal frontend grid construction is unchanged.
2. Linux and Android GL `clear()` disabled scissoring and cleared the whole
   framebuffer. It now intersects the viewport, active clip and target bounds,
   preserving the appropriate backbuffer/texture Y convention. This follows
   [D3D9 Clear](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-clear)
   and [scissor semantics](https://learn.microsoft.com/en-us/windows/win32/direct3d9/scissor-test).
3. Transformed DrawPrimitiveUP silently forced depth/write/cull off. Both GL
   backends now honor the caller's states; explicit 2D helpers retain their own
   overlay behavior. Audited production callers are movies, scene round-mask and
   the poker table. The depth regression failed before this correction.
4. A completed split race exposed results compressed into the bottom viewport.
   PostEffects::take can retain an already-bound backbuffer without restoring
   its viewport. The transition to interface drawing now explicitly restores the
   full display viewport. A second complete race shows full-size results.

## Evidence and exact scope

Evidence root: `out/validation/linux-current-20261029`.

* Final development ELF: `f98216e4a114bffc4f546cc1dc1cbefedc3d191215bf95e77593ec0071f4bf3c`.
  The habitual Linux launcher already selects `out/linux/juiced`.
* Native CTest **9/9**, Android host CTest **4/4**. Six GPU contract groups pass
  on both Linux and the Android backend hosted on the Radeon RX 7800 XT.
  Checks include every pixel of viewport/scissor color clears on a backbuffer,
  non-square render target and cube face; partial depth preservation through
  both buffered and UP paths. This is not an Adreno/physical Android result.
* Baseline `split-screen-r5/clear-before.log` fails the color contract;
  `clear-after-depth-diagnostic.log` exposes the transformed-UP depth defect.
  Earlier attempts remain preserved, including diagnostics that led to fixes.
* `full-race-ui/split-final-current` uses two real SDL virtual gamepads,
  ordinary UI input, isolated support, original assets, installed Linux mods
  and diagnostic autopilot. It joins player two, selects two stock Puntos,
  runs one lap, pauses/resumes from player two, finishes **63.21 / 65.03 s**,
  shows results, returns to `multi` and quits normally. Game and Xvfb exit zero.
  No `--test`, game-memory patch or private save is used. With no profile loaded,
  the first player's result name is empty; Player2 appears normally.
* This flow starts directly at the existing internal `multi` window. The
  original PC main-menu Multiplay branch still opens LAN `link_search`; it was
  inspected separately in `split-normal-current`. No normal-main-menu access
  to split screen, physical pad driving, network peer or performance claim is
  made. Autopilot validates race/render flow, not steering responsiveness.
* `split-screen-final-two` separately reruns the formerly crashing direct-car
  command on the final ELF: 900 frames, Go, six cars and two independent views,
  exit zero. Its capture was manually inspected. This is a finite diagnostic,
  not a completed race. Four-player/boundary runtime coverage remains open.
* `split-render-final/poker-render` retains ten original BMP readbacks and
  lossless PNGs. The 52 cards, mixed cash/title/car preview, large result and
  Career re-entry checks pass. Key final captures were manually inspected.
* Full integration remains **260 passed, 1 failed** for the original model
  inventory. The 119 D8Ms remain a visible unresolved gate; none was excluded
  or parsed using guessed layouts. Final test ELF:
  `8c21787c213236adecfec3f45773b8824a8a9010cad754d28415d1f6fa9fbfb5`.
* Loading regression still services 161 worker requests in **2 frames / 36 ms**.
  Installed Wave 2 files and all six personal Linux profile files match their
  existing preservation hashes. No physics, car roster or save format changed.

Final visual comparison:
[before results](../out/validation/linux-current-20261029/full-race-ui/split-direct-current/captures/direct-split-results.png),
[after results](../out/validation/linux-current-20261029/full-race-ui/split-final-current/captures/final-split-results.png),
[two views](../out/validation/linux-current-20261029/full-race-ui/split-final-current/captures/final-split-go.png).

## Android candidate and exclusions

`out/releases/OpenJuice-split-ui-arm64.apk`: version code **22**, version name
`1.02.5-android.21-split-ui`, 10,950,766 bytes,
SHA-256 `1a15b721417af3fddef424c21d4d4cc02999b814b942d12f45aaa1faaf777312`.
Signature matches v21/v20/v19; all seven ARM64 libraries and ZIP alignment meet
16-KiB checks, API-26 dependencies resolve and the debug regression activity is
absent. Only libmain.so differs from v21. Physical APK execution is untested.
v21 remains a historical intermediate candidate without the results fix.

The initial r5 crash receipt sampled the wrapper process too early and recorded
bash. It is excluded from game identity claims; the sanitized system coredump
backtrace binds the actual r5 ELF. No raw core was exported. An intermediate
native pilot predates the final UP/viewport changes and is not final approval.

One controller helper correctly encountered a locked Corsa; ordinary Left
returned to the unlocked Punto in the same process. Two waiting capture helpers
then competed for one command file, causing one acknowledgment timeout; the
capture and game were intact. Final verification uses one sequential helper and
does not repeat that harness race. An initial integration log counter saw 259
line-start PASS entries because stderr interleaved stdout; the runner's terminal
summary was 260/1. Both the original receipt and correction are retained. The
final rebuilt test run independently reports 260/1.

APK audit invocation attempts outside FHS and with an extra `bash` argument
failed before auditing. The final script-path FHS invocation passed. An owned
Nix r6 build was canceled when the additional results defect was discovered;
its source snapshot is explicitly excluded. The final-source build is r7.

## Final Nix source and runtime binding

Nix r7 completed all **9/9** checks and matches all **904** critical current
code/build/tool inputs. Output:
`/nix/store/dfr3nn4a3dwwhgv0zqkr6crmkziaahkw-openjuice-1.02.5-linux-mods1`;
actual ELF `23ded18a149d2a5445f2e37a189a8db5a389041184b4ed9d6f070539452b35a7`.
The final packaged ELF independently ran the same 900-frame/two-player Radeon
diagnostic to Go and exited zero. Its two views were manually inspected. The
probe waits for the actual wrapped ELF rather than assuming the initial PID's
executable is already the game. This is a finite runtime sample, not the
completed UI race (which belongs to the development ELF).

Exact compiled source is retained in
`out/releases/OpenJuice-20261030-r7-nix-source.tar.xz`, 5,725,684 bytes,
SHA-256 `6025987fb844dbbbdae4ad64b9f387a57d013c3efcc036a8f2d5e921ecace820`.
All 1,319 files were independently extracted and hash-checked; restored NAR
identity and derivation match. This does not assert a second binary rebuild.
There is no r7 runtime closure archive, isolated installer/import check or
public release in this pass; earlier r5 distribution evidence is not relabeled
as r7. Source and build receipts are in `linux-package-r7`.

The source/evidence delta is retained as
`out/releases/OpenJuice-source-update-20261030-split-render.zip`, based on the
immutable Novice r5 checkpoint. Its adjacent SHA-256 file and
`split-render-checkpoint-receipt.json` bind every restored payload. Engine/APK
binaries, original game assets and private user profiles are excluded; the
signed APK and exact Nix source archive remain separate, hash-bound artifacts.

No commit, push or public release is made by this pass. Later campaign/routes,
art/customization breadth, original-resource reachability, physical Android,
LAN conditions and final source/license review remain open.

## Later normal-menu entry correction — 31 October 2026

The internal-entry restriction described above is now resolved for native
ports: MainMenu opens the existing local/LAN selector. Both current development
and actual Nix r11 wrapper/ELF complete the two-player race from normal title/
profile/main navigation, player2 pause/resume, results/return/LAN search/back.
See `split-main-menu-verification-2026-10-31.md` for failing-before regression,
source/build identities, APK25 audit and actual readbacks. Physical steering,
Android execution and wider split/render conditions remain separate.
