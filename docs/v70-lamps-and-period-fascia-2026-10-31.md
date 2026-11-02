# V70 lamps and405/240 fascia — Linux r15, 2026-10-31

Linux now uses Street Cars6 / Wave2 13 / Ferrari4, local bundle r15. The normal
launcher overlay has the corrected payloads. Twenty D8Ms and release metadata
changed; no engine, catalogue/roster/save-format, physics, sound or texture edits.
Six personal files and all eight other installed packages retain their hashes.
No Android/USB access, public release or Git push was performed in this step.

## V70: actual source-layout defect

The earlier port's `detail_v70_lamps.py` had added an approximate four-reflector
assembly behind the original source lamp lenses. It placed both inner/outer
rings at one global average depth and generated their winding/normals inward.
The curved source lamp housing therefore hid254 of364 reflector vertices.
The native full-size front view showed mostly two tiny bulb spots in black
apertures. This was a geometry/layout problem, not a missing texture.

The new Blender study samples the actual native lens planes at each ring point.
It retains the existing four assemblies, conforms their depth to those planes,
and turns ring winding/normals outward. The inner rings shrink to94%/92% in XY
to fit the original lower bumper mask; outer rings retain their size. Bulbs
translate forward by about4.2,14.2,14.5 and5.0cm, separately for each lens.
The source glass/housing, lamp openings and unrelated car components stay fixed.

All ring vertices now clear the opaque housing/body; minimum measured clearance
is3.24cm, with at least0.80cm behind the lens at the sampled ring vertices.
Vertices, triangle centroids and edge midpoints were checked while fitting each
ring. These are measured source clearances, not a claim of OEM optical CAD.

The previous LIGHTSON area contained only tiny bulb spheres. It now additionally
reuses the fitted reflector surfaces with the existing native emissive material
and0.8mm normal offset. No new physical car component or texture was invented.
There is one additional mesh part/364 existing reflector vertices per LIGHTSON
mesh. This is not a measured Android performance result.

The correction covers stock, sport and kits1/2/3 front-bumper variants, LIGHTSON
and UI copies:12 D8Ms. Damage/refit mesh identities and car/part indices remain.

##405/240: distinct body-colour kit3 fascia

Kit3 front/rear bumpers now use the existing native player body material on the
actual bumper fascia. Stock/kit1/kit2 retain black plastic. Chrome, plates,
lamps, source grille textures and separate chassis materials remain intact.
No triangle position, normal, UV, colour, unknown vertex byte or silhouette
changes on either car.

The405 black-only bumper atlas is separate from its chrome, lamp, plate and
underbody atlases. Its400 front/224 rear fascia faces take body paint.
The240's material also covers grille/chassis components, so a simple colour or
height filter is insufficient. Native welded components match the source
`Cube.027_black_0` front bumper (1,290 faces) and `Cube.034_black_0` rear bumper
(940 faces), and their measured bounds. Only these complete components change
material. A native alternate red-paint capture confirms that the bumpers follow
the player's colour rather than a baked blue replacement.

Earlier pilots remain rejected:06 selected facets by normal and left patchy
black triangles;07 painted everything below a height and also coloured inner
chassis visible in the floor reflection.08 uses the complete source bumper
components and preserves that chassis. The rejected assets were never installed.

## Actual verification

Evidence root: `out/validation/linux-current-20261029/v70-lamp-layout`.

- V70:48 native whole-car views (stock/all three kits, eight normal and four
  close angles, carbon/vinyl8 on kits), plus24 actual LIGHTSON off/on UI views
  across all kits and front/quarters. Both ordinary contact sheets and both
  final on/off sheets were inspected. The earlier bulb-only on/off trial is not
  the accepted final overlay.
-405/240:96 normal/close native views across stock/all kits, plus eight actual
  alternate red-paint views. Some405 imagery is reused only after exact image
  and final-payload comparisons. The final240 component-selection views were
  inspected separately from rejected height/normal-based trials.
-176 accepted relevant views total. Captures use actual CarView/GLES on Radeon
  RX7800XT. The private LIGHTSON toggle tests the real model node in UI lighting;
  it does not prove a night race, Android frame rate or physical RP5 behaviour.
- Independent parsing of all20 final D8Ms confirms every non-lamp V70 part is
  unchanged, original UV/colour/unknown attributes survive the lamp corrections,
  winding/normals face outward and both emissive ring/bulb overlays match the
  fitted physical surfaces at the importer offset.405/240 retain exact ordered
  triangle-corner byte multisets; only intended material assignments differ.
- UI and race meshes match exactly. All31 decal payloads per car, handling,
  hoods, wheels and every other runtime payload are intact. V70's48-view off
  geometry is unchanged by the later emissive-only overlay addition; twelve
  independently rendered off frames are pixel-identical before/after it.
- The r15 ZIP was read back: all3,056 payloads, CRC and extracted manifests
  verified; the extracted package passed1,238 native Wave2 checks. First-wave
  assets are unchanged, so its prior482 checks are retained, not rerun/relabeled.
- Installed assets, repository default runtime and normal launcher overlay
  match measured hashes. Atomic replacement retains a verified Wave2 12 rollback.

Records include `independent-native-verification.json`, `reuse-proof.json`,
`visual-review.json`, `lamp-switch05/capture-receipt.json`,
`paint-red/capture-receipt.json` and `delivery/r15-delivery-receipt.json`.

## Sources, delivery and remaining scope

Editable lamp study and native-material selection/provenance are in
`tools/street-cars-wave2/assets/v70-lamps-and-period-fascia-20261031`.
Recipes: `repair_v70_lamp_layout.py` (Blender) and
`coat_period_bumper_kits.py` (material-only adapter). Reproduction begins from
the verified r14 runtime, runs lamp fitting, then the component coating step;
it does not rerun the entire importer over a newer installed pack. A second
application of either operation is intentionally rejected by source guards.
BlenderMCP tools were unavailable; background Blender5.1.1 produced the study.

Bundle: `out/releases/OpenJuice-Car-Visual-Fixes-r15.zip`,189,630,507 bytes,
SHA256 `a510b0eb2aa89aca461f95b47080b4a0ddcc5e68453a6c4d66bce9c8973ebb78`.
This is local Linux delivery; RP5 remains on its last verified APK19/r5. The
prepared APK27 and r15 have not been installed there.

Full ten-car acceptance is still open. Next art work is the retained Wave2
kit1 finish, especially the old V70 sampled front lip. Declared factory-variant
handling uncertainties and actual RP5/Ronnie-garage verification remain. The
new source-based body-colour choice improves variety; it does not retroactively
approve every existing custom part or substitute catalogue counts for finish.
