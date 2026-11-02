# Six-car kit fit correction — Linux r13, 2026-10-31

The requested end state remains ten coherent, fully customizable car ports and
verified RP5 delivery with Ronnie's garage/saves intact. This checkpoint advances
the actual kit geometry; it does not close the full ten-car task.

Linux now has Street Cars6 / Wave2 11 / Ferrari4, bundle r13. The production ELF,
six personal files and all eight other installed packages match the pre-change
hashes. No Android/USB access, public release or Git push was performed here.

## What changed

Kit2/3 front bumpers, rear bumpers and both rocker parts were replaced for 405,
406, Golf R32, Jetta, Volvo240 and V70, including their UI variants: 96 D8Ms.
Original car/part IDs, catalogue, stock and kit1 payloads, dynamics, wheels, hoods,
all31 decal meshes per car, materials/textures and sounds are unchanged.

The old sampled add-ons could hit unrelated underbody geometry, leave a flat
blue shelf on406 or detached blue rear ends on Jetta. New geometry is authored
against each actual native part in centimetres, with real BOH axle positions,
wheel radii and exhaust meshes. Existing lower bumper surfaces get a bounded
continuous shape adjustment; added skirts and lips have closed beveled sections.
Rear additions split around the measured exhaust intervals. Golf also reserves
the source model's visible centre twin tips as well as its shared donor slot.

Every original side-facing triangle in the possible vinyl band stays fixed,
including normals and UVs. Coincident material seams share the same protection
field. A 6cm smooth transition and one bounded deformation strength for each
whole assembly prevent folds at fixed/moving boundaries. Lamps, chrome, plates
and small fitted hardware are protected. New exposed additions remain entirely
below that car's lowest existing vinyl vertex.

## What was actually verified

Evidence root: `out/validation/linux-current-20261029/six-car-kits`.

- Final six-car candidate: 192 eight-angle normal captures plus96 close cardinal
  captures through the actual CarView/GLES renderer, AMD Radeon RX7800XT.
  Ten contact sheets were directly inspected;406's final48 frames are pixel
  identical to the directly inspected preceding pilot05. The exact comparison
  and inspected-image hashes are retained, not inferred from a successful render.
- Independent parsing of the final96 D8Ms confirms101,896 protected vertex
  entries with byte-identical receiver positions/normals, unchanged original UVs,
  positive original-face orientation, additions below every source vinyl and
  rear triangles outside the measured exhaust intervals. This checks exported
  native files rather than trusting the Blender report.
- r13 ZIP: 3,056 payloads checked individually with SHA256, CRC and extracted
  manifests; the extracted package passed1,238 native Wave2 checks. First-wave
  assets are unchanged, so the prior482 first-wave results are retained rather
  than reported as newly executed.
- Only the96 intended meshes and README/revision/manifest changed. The installed
  package was atomically exchanged with a verified rollback copy retained. The
  actual normal launcher overlay contains the measured96 mesh hashes.

The visual sample uses retail vinyl8. The file/geometry proof covers31 retained
designs per car, but is not a newly inspected all31-design screenshot sweep.
Offline capture load/refit times are not Android performance measurements.

## Decisions and remaining findings

-406: accept removal of the flat shelf and better continuous lower bumper shape.
  The small blue rear lip remains a restrained addition, not a new full rear shell.
-Jetta: accept removal of the disconnected blue ends; rear treatment uses existing
  black trim. Previous lamp/trunk-liner repairs are retained.
-Golf: centre twin tips remain visible and clear; styles are conservative.
-405/240: original lamps, chrome and plates remain intact. Rear style differences
  are still subtle; do not use catalogue counts as proof of strong finished
  customization variety.
-V70: the old kit2 sampled extension is replaced. Front lamp art is still very
  dark/simple and needs a source/material investigation. Kit1 was retained and
  does not acquire a new approval from these kit2/3 corrections.
-Outback: unchanged in r13. Its lower grey cladding, not only blue paint, receives
  some retail vinyls. Earlier r12 proof of fixed blue-painted faces therefore
  does not settle lower-vinyl compatibility; a targeted actual-surface audit is
  underway. A generic skirt pilot without a real safe mounting point was rejected.

Earlier failed406 studies are retained: hard fixed/moving boundaries inverted
four rear triangles. A later all-car trial also exposed thin Golf/V70 faces and
Jetta hardware that could fold under deformation. These were corrected through
seam-wide protection and bounded whole-assembly strength; assertions were not
removed. Blender can return0 on script failure by default, so the final batch
uses `--python-exit-code 1` and also requires its completed study report. The
earlier trial's exit-only status is not acceptance evidence.

## Editable sources and delivery

Author: `tools/street-cars-wave2/build_fitted_car_kits.py` and measured
`native_kit_profiles.json`. Adapter: `package_fitted_car_kits.py`.
Each of six editable Blender scenes, GLBs and native array/provenance reports is
under `tools/street-cars-wave2/assets/fitted-kits-20261031/<model>`.
Original references remain in those scenes; no generated car component was
substituted for source-accurate lamps, wheels, trim or chassis.

Bundle: `out/releases/OpenJuice-Car-Visual-Fixes-r13.zip`, SHA256
`45a9d550e4dd6ffe3bba916dd27eb87e2a77c9f9d2035955bc0644a6159e7d15`.
Delivery receipt: `six-car-kits/delivery/r13-delivery-receipt.json` beneath the
evidence root. RP5 is still reserved for the user's use; its last verified build
remains APK19/r5. APK27 and r13 have not been installed there.
