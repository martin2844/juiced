# Outback integrated kits — Linux r12, 2026-10-31

Historical r12 report: the skirt/vinyl compatibility findings are superseded by
`outback-skirt-vinyl-correction-2026-10-31.md` (r14). Read that correction for the
currently installed geometry and the limits of the earlier painted-face proof.

Linux now uses bundle **r12**, Wave2 **revision 10**, Street Cars revision 6 and
Ferrari revision 4. The installed change is limited to Outback kit2/3 geometry
and kit3 cladding material assignments. Stock, kit1, the other cars, handling,
hood/vinyl/wheel/sound assets and all car/part/save IDs retain their r11 values.
RP5 was not accessed. Six other Wave2 kit finishes remain open.

## Result and rejected attempts

The first appended-valance pilot produced hanging rear plates and was rejected
(CP51). The replacement uses continuous deformation of actual current native
bumper/rocker surfaces, retaining their original texture coordinates and details.

- **Kit2:** deeper factory grey cladding, with blended lower shaping.
- **Kit3:** fuller lower profile and native body paint on selected grey cladding
  faces. Plate, grille, lamps and textured trim retain their original regions.
  Existing materials/textures are reused; no texture files or allocations are
  added for the paint treatment.

The first painted trial (03) left unpainted grey triangles. It was not installed.
Trial04 corrects the face selection using the source atlas interior, not its UV
padding. Its normal eight-yaw and closer four-cardinal sheets were both actually
inspected: **48 final native views**, stock and all three kits, carbon/vinyl8
on each kit. The presentation is accepted for Linux integration within that
sample; this is not a claim of Android acceptance or all 31 visual combinations.

Evidence: `out/validation/linux-current-20261029/ten-car-kit-review/outback-pilot-04`.
Open `gallery-study04/index.html` or `close-clear-study04/index.html`; the exact
inspected image hashes and limits are in `visual-review.json`. Native renderer:
Radeon RX7800XT, real CarView/selector lighting. PNGs match the original BMP pixels.
Two other 48-frame trials (02 and03) are retained separately, with explicit
superseding decisions. Captures are offline presentation tests, not FPS results.

## Geometry and data checks

The measured Outback-only deformation leaves upper surfaces and wheel arches
in place, retaining original geometry around the actual stock exhaust outlet.
Normals use the deformation Jacobian, preserving source smooth/hard seams.
The exported study checks finite coordinates/normals, positive Jacobians,
triangle orientation and ground clearance. The native adapter changes only
position/normal streams and appropriate bounds, preserving UVs, colour and
unknown vertex data. Original stock meshes are never replaced.

The paint adapter changes only eight kit3 D8Ms. An independent verifier compares
the multiset of ordered triangle vertex bytes, including positions, normals,
UVs and colours, before/after coating: every corner matches. All original
painted/decal-bearing surfaces retain numerically identical positions, normals
and UVs. The adapter does not move existing decals onto different geometry.
The otherwise unchanged rear paint stream has six signed-zero differences per
kit; this is recorded as numerical identity, not byte identity. Stock part files
themselves remain byte-identical.

The final candidate passes **1238/1238 native full-roster checks**. The actual
distributable ZIP is separately read back, every payload/CRC/manifest verified,
and its extracted overlay passes **1238/1238** before the atomic Linux exchange.
Ferrari and first-wave payloads match r11; their previous 482 first-wave passes
are retained evidence, not represented as a newly rerun checker.

Only sixteen Outback kit D8Ms plus README/mod.ini/SHA256 metadata differ from
r11. All dynamics recipes, the other eight installed packages, six personal
files and the production ELF were verified unchanged. Repository defaults match
the installed payloads. Actual assembly of all nine installed packages contains
the delivered meshes. The previous live Wave2 tree remains at
`ten-car-kit-review/outback-delivery/wave2-r9-rollback`.

Authoritative package hash, size and exact changed-file inventory:
`ten-car-kit-review/outback-delivery/r12-delivery-receipt.json`.

## Editable source and reproduction

Final `.blend`, kit2/3 `.glb`, native vertex arrays, face-selection receipt and
tool hashes: `tools/street-cars-wave2/assets/outback-kits-20261031`.
Original model attribution remains in the Wave2 package. All work uses the
existing local Blender5.1.1 CLI; no Blender MCP tools were exposed in this session.

Run from the port repository, with fresh destinations and the preserved r11
runtime package as `BASELINE`. `REPO` is the absolute repository path. The
original code can be reproduced from the preserved checkpoint versions.

```sh
blender --background --python-exit-code 1 \
  --python tools/street-cars-wave2/build_morphed_kit_study.py -- REPO STUDY
python tools/street-cars-wave2/package_morphed_kit_study.py REPO BASELINE STUDY MORPH
python tools/street-cars-wave2/coat_outback_kit_study.py REPO MORPH PAINTED COATING.json
blender --background --python-exit-code 1 \
  --python tools/street-cars-wave2/apply_kit_coating_blender.py -- STUDY COATING.json FINAL_SOURCE
```

The adapters build isolated candidates, not installed releases. Native assembly,
checker, visual review, manifest refresh, ZIP roundtrip and preservation checks
precede installation. Do not run the legacy whole-source importer over the
repaired runtime; it can overwrite unrelated later fixes.

The full ten-car objective remains active. Apply measured, car-specific work to
the other six Wave2 kit defects, retain explicit handling estimates, and verify
the selected APK/pack and Ronnie garage on RP5 when the device is available.
# Later correction: r14 lower vinyl compatibility

This report records the original r12 evidence. A later audit found that grey
cladding also receives vinyls and the r12 skirt morph could cover their lower
fragments. R14 restores those receivers and replaces only the skirt extensions;
the r12 front/rear design remains. See
`outback-skirt-vinyl-correction-2026-10-31.md` for the actual defect, correction,
before/after captures and bounded all31 geometry proof. The earlier painted-face
proof alone must not be used as general vinyl compatibility evidence.

