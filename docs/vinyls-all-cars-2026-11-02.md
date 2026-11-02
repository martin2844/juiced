# AI Hero Liveries: complete installed roster — 2026-11-02

The Linux launcher did not discover the AI pack because its installed folder
contained authoring material without `mod.ini` or runtime assets. The old
builder also selected only the 52 retail cars. This revision generates and
registers the same five existing AI designs for all 63 playable installed
cars, including Peugeot 405, and supplies a real enabled asset package.
The placeholder `NONE` is excluded from that count.

## Implementation

`build_ai_heroes.py --overlay` reads the assembled active `cardefs.txt` and
uses each added car's registered, fitted native `decal_fandf_vs1` or
`decal_tribal_vs4` receiver. Missing or ambiguous receivers fail the build.
No art was regenerated and no car body, kit, handling or receiver geometry
was modified. The five textures and thumbnails retain their existing IDs
and order. Original entries stay at their existing indices; additions are
31–35, or 34–38 for Ferrari, which already has three extra vinyls.

The native loader now recognizes additive `VinylIfPresent` records, used
for the 11 mod cars. They are skipped when the corresponding car is absent.
Ordinary `Vinyl` records still require a valid installed car. Both retain
name/path validation, duplicate detection and the 125-entry capacity limit.
This avoids making the pack depend on every optional car pack being enabled.
There is no save namespace change, migration or compatibility gate.

## Evidence

Evidence directory: `out/validation/all-car-liveries-20261102/`.

- `unit-tests.log`: 422 passed, zero failed, including additive optional
  registration, stable indices, missing targets, duplicates and capacity.
- `all-cars-native.log`: 315 car/design combinations loaded with material
  and texture in both garage and race, appeared in the actual workshop menu
  and round-tripped their selected index through an isolated career.
  The complete baseline car IDs and existing part indices were compared.
- `geometry-audit.json`: 630 per-car UI/race meshes match their exact
  recorded donors after restoring only the material name. Vertex/UV/damage
  data, bounds, declarations and indices are unchanged. All five full-size
  textures and matching transparent thumbnail dimensions were checked.
- `registry-negative.log`: absent registry, required unknown target,
  optional absent target, unsafe ordinary/optional mesh, duplicate and
  overflow cases exercised against the native loader.
- `matrix-retail.log`, `matrix-ferrari.log`, `matrix-street-cars.log`:
  260, 265 and 280 combinations respectively when later optional car
  packs are absent. Full installed roster: 315.
- `captures/`: actual hidden-window Linux renderer, three angles per
  design for all five on Peugeot 405, plus Rachel on Civic 97 and Brian
  Steel Warrior on Volvo V70. `405-all-five.jpg` compares both sides.
  Workshop capture shows the imported thumbnail and selected design.

These are load/save and sampled visual checks, not certification of every
paint, body-kit combination or crash deformation state.

## Deployment

Target: Linux habitual launcher, repo defaults and installed
`~/.local/share/openjuice/mods/ai-hero-liveries`. Other car packages remain
unchanged. Runtime pack: `out/releases/OpenJuice-AI-Hero-Liveries-all-cars-r2.zip`.
Final hashes and installation/smoke results are recorded in
`out/validation/all-car-liveries-20261102/deployment.json`.

Private backups are under
`/run/media/martin/Ediloco/OpenJuice-builds/20261102-all-car-liveries/`;
no personal save payload is included in the release ZIP. Actual save hashes
are compared before and after installation; launcher smoke uses a private
copy of those profiles. A running session must be restarted to discover
the new catalogue.

Only Linux is built/deployed here. Android/macOS source shares the loader
change but their binaries need rebuilding before added-car optional entries
are recognized. No public push, APK installation or macOS release is included.
The October 27 checkpoint remains a historical record of the original preview.
