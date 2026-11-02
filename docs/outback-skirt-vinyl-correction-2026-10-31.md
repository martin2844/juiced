# Outback lower vinyl correction — Linux r14, 2026-10-31

Linux now uses Street Cars6 / Wave2 12 / Ferrari4, bundle r14. The normal launcher
composition has the corrected skirt payloads. No RP5 access, upload, engine or
personal-save modification was performed.

## Confirmed defect and cause

The r12 Outback study preserved original blue-painted faces, but lower grey
cladding also receives the retail designs. Moving the lower cladding outward
left some vinyl triangles behind the new skirt surface. The previous48 views
using vinyl8 did not justify a general all-design compatibility claim.

An independent side-ray audit sampled opaque triangle centroids in the lower
door band of all31 source meshes. Thirteen designs have opaque samples there.
There are1,422 stock-visible sample points, compared against kits2/3 for2,844
cases. R12/r13 geometry newly occluded1,184 cases, with some surfaces over1.8cm
in front of the original vinyl. Retail F&F1 was among the clearest affected
designs. Samples use native UV row order and texture alpha>=192/255; this is a
finite geometric sample, not a whole-image pixel-coverage percentage.

## Correction

Only kit2/3 left/right skirts, including UI variants, change: eight D8Ms. Every
original stock skirt triangle, including positions, normals, UVs, colours and
unknown vertex data, is retained. Kit3 applies the existing native body-paint
material to the measured cladding faces, without changing their geometry.
The r12 front/rear kits, lamps, plates, wheels and exhaust are unchanged.

New closed beveled lower sections mount on the real lower flange. Their actual
maximum exported heights are about30.85cm; the lowest source vinyl is31.4598cm.
Thus the exposed additions have over6mm vertical clearance below all31 designs.
The previous failed generic pilot stopped its root search at30.46cm and missed
the actual flange near30.67cm; it was rejected rather than allowed to float.

The Blender study also checks exported triangles against the31 decal meshes in
their side projection. The final added triangles have zero projected overlaps
because their height intervals are disjoint. This is an explicit separation
proof; it is not evidence that the general polygon-intersection routine has
been calibrated for arbitrary overlapping surfaces.

## Verification and evidence

Evidence root: `out/validation/linux-current-20261029/six-car-kits`.

- Independent native parsing confirms byte-for-byte inclusion of all original
  stock triangle corners in all eight files. Material-only splitting for kit3
  changes the grouping, not those corner bytes. All31 decal payloads are intact.
- The same2,844 stock-visible opaque-centroid cases now have0 new occlusions.
  Saved rows identify each original point, design and kit. Texture resources and
  mesh inputs have recorded hashes.
-48 native whole-car views were directly inspected: eight normal angles and
  four close angles for stock and all three kits, with carbon/vinyl8 on kits.
-12 additional before/after views isolate retail F&F1, index15: both sides on
  stock, kit2 and kit3. The private gallery uses actual CarView/GLES and the same
  camera/colour, without carbon changes. Both comparison sheets were inspected.
- The extracted r14 archive passes1,238 native Wave2 checks. Every3,056 payloads,
  ZIP CRC and extracted package manifests were read back and verified. The six
  other cars' r13 fit corrections are unchanged, retaining their288-view proof.
- The installed package, repository runtime and normal launcher overlay match
  the measured payload hashes. Six personal files, all eight other installed
  packages and the production ELF retain their pre-change hashes. Rollback from
  Wave2 11 is preserved in `six-car-kits/outback-delivery/wave2-r11-rollback`.

Relevant files: `outback-safe-independent-verification.json`,
`outback-r13-lower-vinyl-depth.json`, `outback-safe02-lower-vinyl-depth.json`,
`outback-safe-visual-review.json`, `outback-vinyl-proof/capture-receipt.json` and
`outback-delivery/r14-delivery-receipt.json` beneath that evidence root.

Editable Blender/GLB sources and provenance are in
`tools/street-cars-wave2/assets/outback-safe-skirts-20261031`. Reproduction uses
`build_outback_safe_skirts.py`, `package_outback_safe_skirts.py` and the
material-only `coat_outback_kit_study.py --skirts-only` step. Native sources and
BOH axle/wheel measurements remain the geometry authority.

Bundle: `out/releases/OpenJuice-Car-Visual-Fixes-r14.zip`,189,521,107 bytes,
SHA256 `d123a1adfe51c48b17171c9290f47b45304dd5fc6b7f02eb878e6b744be82393`.
It is local and installed on Linux; it has not been installed or tested on RP5.

Full acceptance remains open: customization variety on some models, V70 lamp
source/material quality, declared handling-variant uncertainties and the real
RP5/Ronnie-garage verification. No Android FPS claim follows from these offline
Linux captures, and all31 designs were not individually screenshot-reviewed.
