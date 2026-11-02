# Visible tyre envelope correction — 2026-10-30

This pass follows the user's direction to continue on Linux while the RP5 is in
use. It returns to the reported shrinking aftermarket wheels, rather than
inferring that a loaded wheel mesh proves correct dimensions. Evidence root:
`out/validation/linux-current-20261029/wheel-envelope`.

## Reproduced defect

The installed composition has eleven imported cars and **202 catalogue wheels**:
74 retail plus 128 added wheels. Existing Ferrari and package checks measured
mesh metadata or tested that a wheel existed. The first new metadata-only audit
also passed. An expanded audit of actual vertex positions exposes the defect:
all 128 added wheels render at **94.1552% of factory tyre diameter** and roughly
**84.3–84.9% of factory width** after fitting, across all eleven cars. Retail
wheels match the factory envelope. The same defect affects normal and blurred
meshes in both garage and driving variants.

Independent Python extraction of a raw installed D8M, without the fitting
helper, confirms its bounds include exactly 2 cm of empty margin on every side.
For example AddWH_RRGrams56XL1 declares X -13.2013..12.4023 and Y
-34.2184..34.2186, but actual vertices occupy X -11.2013..10.4023 and Y
-32.2184..32.2186. Fitting the padded culling box produces smaller visible tyres.
See `independent-raw-wheel.json`, `all-wheels-v2.csv` and `runtime-v2.log`.

## Correction

`src/Game/Cars/WheelFit.h` now measures the actual position streams before
fitting. It validates every nonempty part before mutating any vertices, then
maps the real tyre width/diameter to the factory envelope, updates the bounds
and conservative culling spheres, and retains existing inverse-scale normal
handling. It rejects malformed, truncated or non-finite position data without
partially resizing earlier parts. The existing opt-in through
MATCH_STOCK_WHEELS remains; stock car definitions, wheel IDs, mesh assets,
handling, money, roster and save format are unchanged.

`tests/Unit/Cars/WheelFitTests.cpp` constructs tight and padded descriptions of
the same tyre, translated/nonuniform factory target, multiple parts and invalid
position buffers. Before the fix it reports **3 passed / 8 failed**; afterward
**11 passed / 0 failed**. The fixture checks actual expected coordinates, UVs,
normals, bounds and failure atomicity; it does not merely check a success flag.

## Verification

* Full native refit/blur audit: **35,552 slot cases** = 11 cars × 2 variants ×
  202 wheels × 2 blur states × 4 corners. Actual vertex extents, declared bounds,
  hub/scale transforms and material/geometry presence are checked. All cars
  restore their factory wheels after the sweep. The expanded audit has 45,056
  failed assertions before the fix and none afterward. This is geometry
  coverage, not a function count or percentage of game completion.
* All 74 retail catalogue choices retain their recorded actual dimensions.
  Final actual-vertex ratios match the factory tyre within the CSV's recorded
  precision (maximum reported width ratio error 0.000001; diameter error 0).
* A separate deliberate no-fitting control fails 16 size assertions on the
  first Ferrari replacement wheel. The original metadata-only audit's 0-failure
  receipt is retained as evidence of why that weaker check was insufficient.
* Real CarView/Radeon comparison: **36 before + 36 after** captures, Ferrari,
  Peugeot 406 and Volvo V70; factory wheels, first added wheel and last added
  wheel with carbon/vinyl/maximum kit, four camera angles. All three comparison
  sheets were manually inspected. Factory views are **12/12 byte-identical**.
  Every PNG was compared to its original BMP RGB pixels. The gallery is
  [`wheel-envelope/index.html`](../out/validation/linux-current-20261029/wheel-envelope/index.html).
* Linux CTest **9/9**, Android host CTest **4/4**; Android-host Ferrari garage
  checker also passes all 202 normal/blur sizes, 34 decals, carbon and BHP for
  both variants. Host Android is desktop validation, not physical RP5/GLES.
* Full original-data integration remains **260 passed / 1 failed** for the
  original model inventory. The 119-D8M failure is retained; no skipped assets
  or guessed parser changes are used to hide it.
* The actual installed Wave 2 package and all six personal Linux profile files
  retain their preservation hashes. Audits use exclusively owned overlays and
  fresh data objects. The normal launcher selects the updated development ELF:
  `7b5718df0044376164a3438400c286610168807f4919a4d82da0b86954836997`.

The first gallery compile lacked the Nix shell's SDL include paths and failed
before execution. Its log is retained; the shell-scoped rebuild and both final
captures pass. No failed or metadata-only run is called final approval.

## Android candidate and remaining scope

Signed local APK **v23**, `1.02.5-android.22-wheel-fit`:
`out/releases/OpenJuice-wheel-fit-arm64.apk`, 10,950,770 bytes,
SHA-256 `133bbf9d64e324166261fa061f2ca19426f39e18765042b449e27c28e5b22e26`.
It has the same update certificate as v22/v19; all seven ARM64 ELF/dependency
and ZIP 16-KiB checks pass. Only libmain.so changes from v22. No debug regression
entry is in the release. This shared code fix requires the new executable/APK;
the asset-only car pack remains r6.

The RP5 is untouched and still has APK v19 / car pack r5. Physical installation,
post-import save comparison, rendering/driving and Android FPS remain pending.
The gallery checks normal wheel views; blurred geometry is checked natively,
not visually approved in an actual race. It does not certify tyre contact,
damage behavior, all art finish or the full game. Original Outback/Jetta source
detail and the broader verification ledger remain separate open requirements.
No commit, push or public release was performed.

## Final Nix build and retained checkpoint

Nix r8 passes **9/9 CTest** and all **905 critical code/build/tool inputs**
match its actual derivation source. Output:
`/nix/store/vhsd6zahyljr2p5w3bpm3cd5wj10dh9f-openjuice-1.02.5-linux-mods1`;
actual ELF `d0bcbf2913acb18ea78935789c414e345a3eb30d564a8f204a6ea11c2180cd00`.
The actual wrapped executable also ran a 900-frame Radeon sample with installed
Linux mods and six factory-wheel Ferraris, reached Go and exited zero. Its image
was manually inspected. This is a finite engine/asset regression sample; the
aftermarket-wheel proof comes from the separate actual-vertex/CarView audits.
Inherited wording saying two players/no mods was corrected against the recorded
one-player/mods command; the original scope receipt remains preserved.

Exact source archive: `out/releases/OpenJuice-20261030-r8-nix-source.tar.xz`,
5,730,612 bytes, SHA-256
`ab2f7b35f3bb27ee7ec9896fd0c21ab2d31026a0978c22d54e8cd58426da72a0`.
All 1,321 files were independently restored and hash-checked; source NAR identity
and derivation match. No second binary rebuild, runtime closure distribution or
public release is claimed. Later documentation is retained separately.

Source/evidence delta:
`out/releases/OpenJuice-source-update-20261030-wheel-envelope.zip`, based on the
immutable split-render checkpoint. Its checksum and
`wheel-envelope-checkpoint-receipt.json` bind every payload. Private saves,
engine/APK binaries and original game assets are excluded; the APK and exact
Nix source remain separate hash-bound artifacts.

Subsequent distribution follow-up: r8 now also has a verified Linux runtime
archive, 307-path private-store import/content check and isolated installer.
See `nix-r8-distribution-verification-2026-10-30.md`. Those packaging results
extend this checkpoint; they do not broaden its original gameplay/device scope.
