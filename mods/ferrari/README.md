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
the generated assets. Disabling this package selects a separate career directory;
re-enabling restores the previous combination's careers.

Revision 2 (2026-10-26) adds 31 retail vinyl designs after the existing three saved
indices. Artwork loads from the owner's game archives. The carbon bonnet retains
the original surface with a clipped, clean carbon-panel edge, finer weave and a
specular material. Replacement wheels retain the stock tyre diameter and width,
including their blur meshes. Wheel fitting requires the matching updated engine
(Android .6 or this source revision); update both APK and mod package.
