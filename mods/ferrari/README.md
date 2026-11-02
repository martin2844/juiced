# Ferrari 458 Spider

An additional car, with its own stable ID 53, performance/body upgrades, damage,
paint, decals, neon and engine sound. Enable through **Options → Mods**, restart,
and choose the car in Custom Race or purchase it through the normal dealer.

- `mod.ini`: package metadata.
- `assets/`: ready-to-load meshes, materials, textures, audio and added-car records.
- `patches/`: recipes that read the owner's original scripts and apply the Ferrari changes.
- `tools/`: importer, pinned source model/audio, provenance and regeneration instructions.

No separate download or compilation is required to load the committed package.
The original game data is still required. Recipes verify their base files and fail
with a named incompatibility instead of silently patching another version.

See [details and attribution](tools/README.md). Licences and credits also accompany
the generated assets. All mod combinations share the same careers; enabling/disabling this package does not move saves.

Revision 2 (2026-10-26) adds 31 retail vinyl designs after the existing three saved
indices. Artwork loads from the owner's game archives. The carbon bonnet retains
the original surface with a clipped, clean carbon-panel edge, finer weave and a
specular material. Replacement wheels retain the stock tyre diameter and width,
including their blur meshes. Wheel fitting requires the matching updated engine
(Android .6 or this source revision); update both APK and mod package.

Revision 3 (2026-10-28) calibrates the handling for Juiced: 50% effective front
weight distribution, reduced maximum steering angle, and additional rear lateral
and longitudinal grip. This is an arcade calibration, not a claim of the real
458's weight distribution. Power, mass and gear ratios are unchanged. Requires
the updated engine to use seventh gear and display the correct name in results.

Revision 4 replaces the inset carbon patch with the complete detached front lid from the pinned source model. Factory and carbon lids retain the same source shape; the badge and neighbouring fenders remain in place. Handling, engine audio, car/upgrade IDs and saves are unchanged.
