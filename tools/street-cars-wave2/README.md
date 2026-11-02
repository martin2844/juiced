# Wave 2 authoring and review

Current r17 targeted adapters and their required preceding inputs are documented
in `docs/car-feedback-wings-and-rear-lamps-2026-10-31.md`. The legacy bootstrap
below predates detachable native wings. Run the measured wing adapter after a
fresh pre-wing package; keeping the old high/rear paint cutoff disabled still
protects the boot and hatch. Editable r17 studies live under
`assets/user-feedback-20261031`.

Adds IDs57–63 after Ferrari53 and Street Cars54–56. Runtime assets are in
`mods/street-cars-wave2`; original model downloads and editable Blender scenes
stay in the local authoring workspace. Source credits accompany the package.

Requirements: Blender5.1, Python3.13 with NumPy/Pillow DDS support, native Linux
build with tests. The prepared scenes are inputs, not generated from a fresh
checkout alone. They retain source dimensions, material tags and named wheels.
The source cleaning scripts are model-specific authoring operations; in particular
`finish_geometry.py` supports only the406/Jetta. Do not apply its metre-space
normalization to the other five cars. Keep original source scenes unchanged.

Rebuild after editing prepared scenes (all seven `KEY-prepared.blend` and `.glb`
files must exist in WORK):

```sh
python tools/street-cars-wave2/rebuild_candidate.py \
  --work WORK --game GAME --base-overlay BASE_OVERLAY --blender blender
```

BASE_OVERLAY is an assembled Ferrari + Street Cars overlay. Supply its actual
path; no transient launcher cache is hardcoded. The checker must be built at
`out/linux/juiced_wave2_car_check`, or passed with `--checker`.

Package to a fresh destination:

```sh
python tools/street-cars-wave2/package.py \
  --source WORK/native-stock-probe --output NEW_PACKAGE --game GAME \
  --model-sources MODEL_SOURCES --archive OUTPUT.zip
python tools/street-cars-wave2/test_pipeline.py
```

Use the native package host to assemble Ferrari, Street Cars and Wave2, then run
`juiced_wave2_car_check GAME ASSEMBLED_OVERLAY`. This verifies the delivered
recipes as well as the raw authoring output. Save-editor migration and arcade
roster tests run separately; a model-loading test does not exercise those paths.

Capture actual Linux rendering, isolated from user profiles:

```sh
python tools/street-cars-wave2/capture_native.py --game GAME --work WORK \
  --cars JETTA_MK5_FSI --levels 0 1 2 3
```

Failed captures discard old images and exit nonzero. Blender renders are useful
for geometry review but do not prove native material rendering or device FPS.
The kits apply their solidify/bevel modifiers before export; diffuser thickness
is checked in the exported GLB. Rear body panels are not detachable spoilers.

The Jetta headlamp/trunk-liner follow-up uses the retained source assemblies:

```sh
blender -b --python-exit-code 1 \
  --python tools/street-cars-wave2/repair_jetta_lamps.py -- \
  SOURCE/jetta-vento-mk5-source.blend \
  PREPARED/jetta-vento-mk5-prepared.blend FRESH_OUTPUT
```

This writes a separate prepared blend/GLB and a source-hash/vertex-movement
receipt. Copy the existing three kit GLBs into that candidate directory before
running build_probe.py for jetta-vento-mk5. Keep the calibrated runtime dynamics
recipes; build_probe emits provisional physics. Use a full native regeneration
and package check for delivery. The fast body-only and material/object-colour
probes in the validation folder are diagnostic inputs, never install packages.

Handling uses documented power/torque anchors with approximated curves and some
provisional gearing. See specifications.json and packaged HANDLING.json. Android
driving/performance approval remains pending; this is not a certified simulation.


The 405 glazing repair keeps the four existing connected window sheets and
assigns them to `oj_glass`. It uses a fresh output directory and preserves
original geometry, normals, UVs and prepared source scene:

```sh
blender -b --python-exit-code 1 \
  --python tools/street-cars-wave2/repair_405_glazing.py -- \
  PREPARED/peugeot-405-glx-prepared.blend FRESH_OUTPUT
```

Copy the three retained kit GLBs into FRESH_OUTPUT before the geometry probe.
For a material-only update, keep the existing calibrated script/physics recipes;
`build_probe.py` emits provisional dynamics. Update both `cars/peugeot_405_glx`
and `cars/peugeot_405_glx_ui`, then validate a freshly assembled package and
review actual native menu and driving rendering. Menu previews use the `_ui`
folder; updating only the driving folder does not prove the UI changed.
Runtime ZIPs include the SHA256 manifest's declared payloads and the manifest,
not authoring `tools`, original models, dependency directories or caches.

The Outback exhaust repair classifies the original 284-polygon tailpipe as an
`exhaust` group. The importer then exports it into the native replaceable exhaust
slot instead of leaving it inside every rear bumper alongside the donor pipe:

```sh
blender -b --python-exit-code 1 \
  --python tools/street-cars-wave2/repair_outback_exhaust.py -- \
  PREPARED/subaru-outback-2002-prepared.blend FRESH_OUTPUT
```

All74 source meshes retain their world vertices, UVs, normals and material names;
the original scene is never overwritten. Copy the three existing kit GLBs into
FRESH_OUTPUT, then run `build_probe.py` for `subaru-outback-2002`. Preserve the
calibrated dynamics and roster recipes. Update both driving and `_ui` BOHs/rear
bumper variants, the shared exhaust mesh/material/texture, and only the Outback's
`STANDARDEXHAUST` to `w261_exhaust` in the existing Cardef append recipe. Verify
all four kits with stock and catalogue exhausts through native `CarView` refits;
source-only or stock-only checks miss duplicate fixed pipes during upgrades.

The Volvo240 forward carbon repair changes the existing source-space clipping
outline in `mods/street-cars/tools/import_cars.py`: the oldz2.08 boundary stopped
short of retained hood surfaces whose paint mesh ends atz2.283699. Two extra
corners extend it toz2.284 without changing the former side/rear segments.
There is no Blender/source-mesh movement or replacement. Regenerate240 into a
fresh native probe, preserve calibrated recipes, and update both garage/race
hood/chassis/front-bumper variants plus any changed transferred decal. Review
the complete stock/body/hood surface together across all four kits, since faces
move between native slots while their world surface must stay unchanged.

glTF `MASK` materials now use Juiced's `Specular ATest`, which implements the
source `Double-sided` flag; `Coloured Spec ABlend` ignores that parameter. The
source alpha cutoff is baked into the texture mask. This restores the original
back-facing240 plate lettering without reversing faces or inventing text.
The delivered update covers240 only. The prepared-source audit also finds
masked materials in the Integra/Levin; those runtime packages need their own
regeneration/review before claiming this fix there. Keep the accepted240 lens
DDTs for this selective correction: pre-change regeneration already changes
their codec, independently of the hood/plate repair. Do not silently bundle that
unrelated re-encoding with the selected asset delta.
