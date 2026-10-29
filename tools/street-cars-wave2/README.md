# Wave 2 authoring and review

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

Handling uses documented power/torque anchors with approximated curves and some
provisional gearing. See specifications.json and packaged HANDLING.json. Android
driving/performance approval remains pending; this is not a certified simulation.
