# Street cars wave 2 — checkpoint 2026-10-29

## Scope and release status

Seven additional cars (stable IDs 57–63), stock plus three front bumper, rear bumper and skirt choices each. Blender-editable assets, working native paint/vinyls/wheels/damage, stock engine data and gameplay-compatible handling. The request is **not complete** at this checkpoint. These assets are isolated candidates, **not installed on Linux or the RP5**, not a release and not visually approved as retail quality. Existing saves have not been modified.

Sources: `../model-sources/street-cars-2026-10-29` (relative to repository parent). Original files are preserved; provenance and hashes are in that directory. Work artifacts: `out/street-cars-wave2-20261029`. Scripts: `tools/street-cars-wave2`. Existing Ferrari and first street-car pack are dependencies, not replaced.

IDs: 57 Peugeot 405 GLX 1.8; 58 Peugeot 406 Coupé V6; 59 Golf R32 Mk5; 60 Jetta/Vento Mk5 2.0 FSI; 61 Outback 2002 2.5; 62 Volvo 240 Turbo; 63 Volvo V70 2.4. Source variants and measured dimensions are in `specifications.json`.

## What the evidence establishes

All seven imported into Blender. The R32 uses the stock PedroBLR KN5 source, not the previously sourced GTI. Source scenes, source eight-direction screenshots, inspection JSON and editable cleanup/repair/prepared scenes are retained.

The Peugeot 406 and Jetta source files contain multiple alternative assemblies simultaneously: bumpers, dashboards, seats, windscreens (including damaged glass), lights and wheels. The first position/name-based cleanup was inadequate. The current cleanup retains original assembly names and selects stock alternatives by those groups. The Volvo 240 source has two cars; the Turbo assembly was selected. The V70 heading was corrected and combined rear wheel meshes split. The Outback paint atlas received a polygon paint mask. The 406 and Jetta received custom fitted five-spoke wheels because the source wheels were unmounted; these are not claimed as exact OEM castings.

The initial kit ribbons had floating geometry. They were rejected visually. Current kit construction raycasts against the actual shell to establish attachment points and uses closed ribbons and thickness for fins. These remain styling candidates. Three small lip variants are not automatically proof of the requested quality or sufficiently distinct complete body kits.

`native-check-v3.log` has **1,173 PASS, zero FAIL**, exit 0. It establishes native stock/kit loading, stock plus three catalogue choices, materials, wheels, wheelbase, damage/repair, all 31 retail vinyl fits/removal, aftermarket wheel fitting, collision cages, light nodes, and save-extension round trips. It does **not** establish final quality, real vehicle handling, Android performance or absence of visual clipping. The later stricter checker additionally checks power against specified targets and actual driven wheels; see `candidate-step-4.log` for its current result, not this earlier total.

Seven real Linux menu screenshots were captured by `capture_native.py` (logs and `<MODEL>-stock.png`). They revealed residual Jetta wheel prototypes, opaque headlamp lenses and poor Golf window materials. Repairs after those captures require new native captures before acceptance. Labels in the first captures are overlong variant descriptions; the candidate builder now uses shorter model names. Early screenshots contain provisional BHP and must not be presented as final.

## Engine and save changes

`CareerProfile::kModelCapacity` increased from 60 to 64 to accommodate IDs through 63. The stock serialized byte ranges remain unchanged. The career extension loader accepts appended car rosters and preserves saved entries when fewer mods are temporarily enabled, within supported capacity. The checker verifies old four-car progress loading into an eleven-car roster and retaining the highest added-car progress through a reduced active roster. No real save file was used.

`mods/street-cars/tools/import_cars.py` gained an optional `kit_count` path. Existing first-wave configuration does not enable it. Optional kit GLBs use exactly the prepared GLB coordinate system, then the same native wheelbase transform. Complete original bumper/skirt sections are retained with the fitted extensions. Material references use native body paint and a trim material. Existing first-wave export regression still needs a controlled comparison before final delivery.

## Handling status

`calibrate_probe.py` uses factory power/torque anchors with explicitly approximate piecewise curves. Some masses and gearbox choices remain provisional and are not validated factory specifications. `reports/handling-candidates.json` records that status. The R32 roster label alone did not enable AWD: the candidate now explicitly sets driven wheels in the dynamic script. Stock audio is reused from retail donor cars.

`handling-candidate.csv` is a deterministic 60 Hz synthetic flat-ground run, not an Android benchmark or real-world vehicle certification. It tests full-throttle starts and a steering pulse with/without ASR, with finite trajectories. Juiced uses its original 2.8 torque multiplier and arcade gravity/grip. Therefore native acceleration is much faster than brochure acceleration; do not claim the measured 0–100 values match real cars, and do not falsify displayed BHP to hide that difference. Keep game-wide physics unchanged. Calibration must judge compatibility with stock Juiced cars as well as realistic relative performance.

Primary research still to finish: selected-market mass conventions and gearing, Volvo 240 turbo brochure, and Peugeot 406 power-unit discrepancy. The Peugeot May 1997 factory technical publication (https://406coupeclub.org/media/documents/PD1203%201997%20May.pdf) gives 1,458 kg for manual V6 and 2,700 mm wheelbase. Manufacturer/mirrored brochure URLs for the other cars are recorded in the specification file. Do not present provisional gear ratios as sourced facts.

## Reproducibility and next gates

Use Blender `--python-exit-code 1`; without it a Python exception can misleadingly return success. The pipeline is source inspection → source cleanup → geometry repair → prepared export → fitted kit construction → native conversion → handling candidate → private stage → native checker. The current `finish_geometry.py` is now suitable for rerunning the 406/Jetta only; other cars retain their already-normalized repaired scenes. Do not blindly regenerate those five with the changed normalization rules.

`rebuild_candidate.py` rebuilds kits/native assets/handling/private stage and runs the checker; it assumes prepared scenes are already current and the checker binary was rebuilt. It never installs. Step logs are separate. `qa-stage` uses file-level symlinks and additive script copies; no writes may follow symlinks back into the launcher stage. `stage_probe.py` unlinks destinations before replacing them.

Required before release: stricter native checks pass; inspect revised stock and all kit combinations from multiple angles; fix remaining lamp/interior/wheel/material problems; improve any too-similar kits; validate paint/vinyl presentation visually; finish handling source validation and stock-game balancing; verify first-wave exporter unchanged when optional kits disabled; test existing-save compatibility and corrupt extension rejection; run appropriate Android build/device tests; then package with source attribution and safe backup/install workflow. Do not install the `NOT_FOR_INSTALL.txt` probe as the finished mod.


## Update — packaged candidate, 2026-10-29

The native checker now reports exactly **1,203 checks passed, zero failures**, including corrupt/truncated save-extension rejection. The independent package host assembled the private ZIP contents plus Ferrari and Street Cars, and the same 1,203 checks passed against that assembled overlay. Logs: `package-assembly.log`, `package-native-check.log`. The original three-car regression has 377 passes and zero failures (`first-wave-regression.log`).

Android arm64 release build succeeded (`android-build.log`); APK: `android/app/build/outputs/apk/release/app-release.apk`. No device install. ADB currently detects iPlay 70 mini Pro `T813128GB25526860527`, not the RP5. Do not install to this different device without user direction.

Prepared geometry now uses correct native two-sided opaque materials where needed. Golf window normals were inverted in the source and are corrected. V70 lamp housings gained simple reflector geometry; these are approximations, not OEM CAD. The 406/Jetta source GLBs have multiple untextured materials: dark interior/trim and rear-lamp colours were corrected explicitly. The Jetta exterior panel reduction showed visible seam defects; restoring its pre-reduction exterior is being checked. Earlier 61.5k triangle count therefore no longer describes the latest Jetta candidate.

224 Blender views (seven cars × stock/three kits × eight angles) were reviewed as contact sheets. Native captures additionally show actual engine rendering. A final thinner kit-three lip supersedes the earlier contact-sheet variant. Review gallery: `out/street-cars-wave2-20261029/gallery.html`; `.blend` links retain editable originals and kit collections. These observations do not establish retail-level finish for every source, or acceptable Android performance.

Updated handling data uses factory Outback 2002 five-speed ratios and 3425 lb base-manual mass. 406 rated 194 PS is converted to 191.34 mechanical bhp, rather than silently equating PS and bhp. Volvo 240 B21ET 114 kW/240 Nm is supported by the 1984 manufacturer technical sheet https://www.240.se/litteratur/MSPV535.pdf. Other unresolved ratios remain explicitly provisional. `handling-final.csv` is a finite synthetic simulation diagnostic, not real-world performance certification.

Private archive: `out/street-cars-wave2-20261029/street-cars-wave2-private.zip`, with model provenance, specification/handling JSON and SHA256 inventory. Requires IDs53–56 from Ferrari and Street Cars plus a 64-slot executable. It is not published, installed, or a claim that all requested polish is complete. Remaining gates include close-up panel/lamp finish, more distinct complete bumper designs if the current conservative lips are insufficient, handling comparison with retail cars and real-device driving/performance.

Final local verification: restoring the original Jetta exterior panel meshes removed the visible door/fender gaps in the native comparison (`JETTA_MK5_FSI-stock.png`, `native-seam-final.log`). Interior optimization is retained; exterior triangle cost increased and must be measured on RP5. Final ZIP regenerated (129,591,554 bytes); native package assembly and **1,203/1,203 checks pass** again (`package-native-check-final.log`). Archive SHA256 is adjacent. All validation uses isolated synthetic profiles; no installation or user-save writes.

## Big-review follow-up

See `street-cars-wave2-big-review-2026-10-29.md`. The earlier1203-check candidate
was superseded: fixed detachable rear bodywork, applied kit modifiers, Android
editor roster migration, arcade group capacity, stale screenshot evidence and
portable/isolated pipeline failure paths. Latest clean assembled-package result:
1224/1224; clean unit result259/259. Corrected runtime assets now live in
`mods/street-cars-wave2`. The original private ZIP filename points to the reviewed
archive, with its former contents backed up. No device installation or save edits.
