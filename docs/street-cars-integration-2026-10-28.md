# Street Cars integration checkpoint — 2026-10-28

Request: integrate all three sourced street cars, including native customization,
and install on Retroid Pocket 5. Blender MCP was not exposed by this session's
tools; Blender 5.1.1 was installed and used locally through reproducible scripts.

## Deliverables

- `mods/street-cars`: native asset package and additive script recipes.
- `mods/street-cars/tools`: meshopt decoder, glTF reader, Blender preparation and
  render scripts, importer, package generator and build driver.
- `mods/street-cars/tools/assets`: all three acquired source GLBs and provenance.
- `tools/StreetCarCheck.cpp`: native integration check target
  `juiced_street_car_check` in `cmake/Linux.cmake`.
- `tools/CarHandlingCheck.cpp`: extends the existing Ferrari diagnostic to added
  street cars and applies each model's actual drive classification.
- `out/releases/OpenJuice-Street-Cars-r1.zip`: distributable mod ZIP (~18.5 MB).

IDs remain stable: Ferrari 53, Integra DB8 54, BMW E36 55, Levin AE92 56.
This package currently requires Ferrari enabled to provide ID 53. No retail car
was replaced. All Cars in Custom Race includes the three; career dealers can
select them. Existing profile files are not migrated, reset or seeded by installation.
The launcher reports 57 entries including the NONE entry, not 57 playable cars.

## Asset preparation and behavior

BMW's source studio floor was removed. All source transforms were baked and scale
was established from measured tyre centres and reference wheelbases (262/270/243
cm). The BMW is the visually tuned source car, with US 3.2 performance parameters;
it is not advertised as an exact EU 286 hp factory M3. Levin interior geometry was
reduced more aggressively than the exterior; weighted body-panel normals were
applied. Baseline source views and editable .blend files are retained in the audit.

Body paint/glass material names match the native model-name convention, enabling
paint and tint. Original texture UVs are retained for lenses, badges, tyres,
interior and trim. Large textures use DXT1/DXT5, reducing generated runtime assets
from ~209 MB to ~62 MB. Shapes are shared by the matching race/garage models.
Final stock render geometry is approximately 18k/37k/67k triangles respectively,
including four instantiated wheels and light lenses (excluding vinyl/sponsor shells).

Native panels: front/rear bumper, bonnet, skirts, spoiler and chassis. Optional
sport panels make mild fitted alterations; carbon uses a clipped inset so triangle
centroids cannot make a jagged outline. Raised spoiler geometry tapers to fixed
mounting edges. These are authored adaptations, not licensed aftermarket products.

All 31 owner's retail vinyl styles use fitted painted-body surfaces; sponsor
rectangles are clipped onto doors. Stock tyres set the envelope for aftermarket
wheel fitting. Original exhaust geometry is separated into the shared exhaust slot
for each car, preserving stock appearance and allowing native upgrades. Brake and
headlight overlays are separate from the permanent lenses so unlit cars retain
their lamps. Collision cages, wheel contacts, physics, paint, repair and upgrades
use existing native engine systems.

Audio intentionally uses existing owner-provided banks (Civic/Integra, 350Z and
Celica). These are not recordings of the new models. Physics remains arcade-tuned.

## Evidence and validation

Evidence root: `out/street-car-sourcing-20261028/`.

- Native integration check: 377 passed checks, zero failures, including race and
  garage loading, all stock material references, all four wheels, exact wheelbase,
  drive classification, plausible power, deform/repair, sport slots, 31 vinyls,
  removal, aftermarket wheel fit, collision meshes, lights and upgrade save round-trip.
- Stock powers shown by the native selector: ~197 / 240 / 136 bhp.
- Headless 60 Hz handling probe with ASR on/off, straight throttle and steering
  pulse; no NaNs or straight-line instability. See `handling-final.csv`.
- All three ran native Linux race probes on Angel North R1 for 900 rendered frames.
  These earlier captures exposed unlit-lens and exhaust placement defects, then
  corrected in the importer. Do not use the earlier race captures as final artwork.
- Native selector captures for all three stock cars, then forced blue paint,
  aftermarket wheels, sport panels, carbon bonnet and retail vinyls. The first
  keyboard Auto-Mod attempt only assigned input; explicit QA collection overrides
  were subsequently used. `*-styled.png` are the relevant customized captures.
- Synthetic diagnostics use isolated `qa-support` and QA overlay directories;
  no diagnostic car was added to a user's career.
- Package assembled and passed the native integration check alongside the existing
  installed Linux mod set, not only in isolation with Ferrari.

## Installation

Linux package: `~/.local/share/openjuice/mods/street-cars`; the normal launcher
already discovers this directory. No executable replacement was needed.

RP5 serial `7f3ffc8d`: imported the ZIP through launcher Mods → Add mods from ZIP.
Launcher reported successful data/active-mod validation and 57 roster entries;
Street Cars was visibly Enabled. Existing APK 16 supports these data additions.
Other attached Android devices were not targeted.

Exported RP5 save backups before and after the first import: all nine entries
identical uncompressed; ZIP integrity passed. `retroid/saves-before.zip` and
`retroid/saves-after.zip` retain the evidence. Linux's six existing save files
also matched pre-install hashes. No uninstall, clear-data, restore or save editor
write was performed. Final carbon-panel refinement was reimported successfully; `retroid/final-validation.png` records the 57-entry validation. A third export, `retroid/saves-final.zip`, again matches all nine original entries byte for byte. Final styled native screenshots were inspected for all three cars after that refinement.

## Limits

No claim of 60 fps or extensive physical-device endurance testing. Linux rendering
and native functional tests do not substitute for RP5 track performance. Existing
licensed sounds are reused, and no new bespoke audio recordings were created.
Only native retail vinyls are fitted here; other custom livery packs need their
own explicit per-car meshes. Package dependency on Ferrari is documented rather
than changing the stable roster format in this work.

Specification references:
- Honda Type R DB8: https://global.honda/jp/news/1995/4950824.html
- BMW E36 US specification table: https://bmwtechinfo.bmwgroup.com/tech_training_manual/ST041%20E46%20M%20Complete%20Vehicle.pdf
- Toyota AE92 GT APEX: https://gazoo.com/catalog/maker/TOYOTA/COROLLA_LEVIN/198905/1002380/

Model sources/licenses and individual hashes are in the package ATTRIBUTION.md
and tools/assets/provenance.json. Acquisition was independently checked against
creator metadata, not inferred solely from the redistribution library.
