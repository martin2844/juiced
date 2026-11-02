# Street Cars — reported visual defects, 2026-10-29

This follows the RP5 report after installation of the ten-car pack. The earlier
loading/checksum checks did not certify visual finish. The corrective candidate
is revision 3 of both Street Cars packages. Evidence and rollback assets are in
`out/street-cars-repair-20261029`. No user save is edited by this work.

## Findings and changes

- **V70 rear wheels:** a rear body seam named `whitexgaps` had been classified as
  wheel geometry because the source name started with `w`. Its distant vertices
  polluted axle bounds, height and overall scale. It is bodywork again; tyre
  geometry is preferred when measuring centres. Hub heights now agree within
  0.01 cm rather than differing by roughly 3.8 cm. Factory wheelbase stays 275.5 cm.
- **406 wheel fit:** the provisional rear wheels were ahead of the rear arch.
  The source rear assemblies move back 8 cm, an authoring fit based on source
  arch geometry, before the native converter normalizes wheelbase to 270 cm.
  This is not a new claim about real-car chassis dimensions.
- **Hoods:** use the actual separate hood assemblies where present (406, Golf,
  Jetta, Outback, V70, Levin). Recover the E36's disconnected hood component.
  The 405, 240 and Integra have fused body meshes: explicit source-space panel
  outlines split triangles along continuous boundaries while retaining the
  surrounding paint. The carbon material covers the resulting panel, without
  the previous arbitrary inset rectangle. These authored outlines are a visual
  adaptation, not OEM panel CAD.
- **Carbon aliasing:** replace the single-level repeating texture with a full
  eight-level mip chain and mip filtering. Reduce weave contrast. The former
  texture generated conspicuous moire bands at normal garage viewing distances.
- **Vinyls:** transfer each of the 31 retail Focus decal footprints and its actual
  barycentric UVs onto the recipient side surfaces. Intersect both geometries
  rather than projecting the same square mapping onto sides and hood. Preserve
  native materials and catalogue indices. This is a donor-based adaptation per
  design; it is not a claim that every possible recipient/kit combination has
  been visually approved. Outback cladding explicitly accepts the lower part
  of the artwork instead of truncating it at the painted upper doors.
- **Volvo 240 lamps:** remap the front housings to the front-lens atlas region,
  removing sampling of rear-light/red areas; make the existing lenses opaque.
  The decisive final fix uses the opaque two-sided shader: the alpha shader
  ignored the mirrored lens back face. Both front lamps are visible in the
  final `lampfix` capture. Earlier captures are retained as failed evidence.
- **Outback plate:** the paint mask had classified green artwork on both Oregon
  plates as paint. Restore their original atlas material on every plate face.
  Retain the atlas at 2048 rather than 1024 so the plate and lamp details survive
  conversion. The front plate is readable in the native close-front capture.
- **Jetta:** restore a real alloy-wheel assembly from its source scene, replacing
  the provisional five bars; preserve the tyre envelope. Restore hard edge
  treatment on the Jetta and 406 instead of indiscriminate smoothing.

Original downloaded models and prepared scenes are preserved. The repair scripts
write a separate authoring directory. No global renderer or physics change is
needed for these asset corrections. Existing stable car/part indices remain.

## Evidence

- Packaged Wave2: **1,238 checks passed, zero failures**.
- First three cars, isolated 57-car catalogue: **377 checks passed**.
- Negative control: the new hub-height check fails twice on the old V70 assets
  (garage/race variants), and passes on the repaired assets.
- Four geometry tests cover UV interpolation, winding and absence of
  geometry in a gap. The fourth confirms the panel split preserves
  the original surface area (1 inside + 3 outside for a 4-unit fixture).
- Native Linux captures exercise carbon and multiple retail vinyl choices,
  including front views of the Outback plate and continuous 405 hood boundary.
  Gallery and individual full-size PNGs are in the evidence directory.
- The earlier first-wave check against the 64-car overlay failed its hardcoded
  57-car roster assertion only. The isolated rerun is the appropriate regression
  result; that initial run is retained, not represented as a pass.

## Delivery and limits

Runtime assets and Wave2 dynamics recipes are updated in the repository. The
previous runtime folders are retained under `previous-runtime` in the evidence
folder. The importable ZIP is `Street-Cars-Visual-Fixes-r3.zip`, with SHA256.
Installed revision 3 on RP5 serial `7f3ffc8d` through the normal ZIP importer.
Android reports “OK: 64 cars loaded. Data and active mods validated.”
Exported saves before and after: all nine ZIP entry payloads are byte-identical.
Backups, package hash and comparison receipt are in the evidence folder
(`rp5-deployment-receipt.json`). No restore, save edit or app reinstall was used.
The final assembled package, including the lamp shader correction, passes
1,238/1,238 native checks (`package-native-check-final.log`).

The source models still vary in detail and artistic quality. This repair does not
certify retail-quality finish, Android FPS, every body-kit/vinyl combination, or
real-world handling. In particular the Outback remains a relatively simple source
mesh. Android import and data validation passed; a full RP5 visual/driving sweep remains
unverified. The existing Ronnie profile is preserved. Blank/unfinished Linux
captures are rejected by the capture script; the heavy Golf/Jetta captures were
rerun with enough loading time. `gallery.html` contains ten selected final views.

## Revision 4 — complete-angle follow-up

The new `juiced_car_gallery` diagnostic uses the actual retail car-selector
viewport, CarView and Linux GLES backend, with fixed camera positions every
45 degrees. It captures stock, carbon, retail vinyl 8 and maximum kit plus carbon
and vinyl 8. Each CSV row records the exact bonnet/decal/kit indices; optional
model names limit a rerun. It does not load/write player saves or change physics.
The final installed composition produced 384 images: eleven added cars plus a
retail Focus reference, eight yaws and four states. The final run uses the actual
AMD Radeon RX 7800 XT through Wayland. Earlier private Xvfb runs used llvmpipe;
they are retained as diagnostic baselines, not GPU performance evidence.

This found a further defect in the first three cars: a rear/high paint cutoff
classified boot/hatch panels as removable spoilers. Carbon/refit or vinyl changes
could then remove those panels. The first-wave importer now keeps their integral
rear bodywork/stock wings in the chassis, as Wave 2 already does. It no longer
offers those body panels as detachable spoilers. Existing saved body-part values
remain readable; model IDs and other part indices are unchanged. All three
regenerated dynamics scripts are byte-identical to revision 3.

The strengthened native checker compares actual body-vertex positions across a
carbon/no-spoiler refit. It fails six times against the previous package, for all
three cars in both garage/race variants. The new isolated package passes 377
checks. Full-size rear-quarter views establish that the boot/hatch shell remains
closed after the refit; the old broken views are preserved in the baseline gallery.

The Ferrari had a separate defect: its carbon texture covered an inset rectangle
instead of the source front lid. The pinned source has a detached 2608-triangle
lid. Connected-component selection now transfers that complete panel to BONNET;
carbon retains the same surface/seams and covers the full panel. The native
checker fails four new assertions against Ferrari r3 and passes 226 checks with
r4. Only five mesh files per race/garage variant change. Its calibrated handling
recipe and audio are preserved. No new geometry or scoop was invented.

Final installed revisions: Ferrari 4, Street Cars 4, Wave 2 3. All six existing
Linux profile files remain byte-identical. Previous package directories and repo
assets are retained in the dated validation folder. The final combined ZIP was
extracted independently, every per-package SHA256 checked, and native checks
passed: 1238 full-roster, 377 isolated first-wave, 226 isolated Ferrari. These
counts describe separate checkers; they are not a percentage of game completion.

Artifact: `out/releases/OpenJuice-Car-Visual-Fixes-r4.zip` (185023891 bytes), SHA256
`b03ee745196ca6d6641dd060655cc208a79aba361a12be8784cd0f3aabbada06`.
Normal Android ZIP import can update the three asset packages; a current 64-car
APK is required. This revision has NOT been installed or visually verified on
RP5: ADB currently sees only the iPlay. The earlier RP5 r3 receipt remains valid
for that earlier deployment, not this new ZIP.

Final gallery: `out/validation/linux-current-20261029/final-car-gallery/index.html`.
The PNGs preserve display RGB pixels; framebuffer alpha was discarded because it
is not window transparency. BMP hashes and pixel-equality conversion receipts
are retained. Initial contact sheets that resampled framebuffer alpha were
rejected and regenerated. The final hardware gallery's changed four models were
reviewed again after the fixes. The prior complete baseline review includes all
12 models. Stock/carbon/vinyl8/maximum-kit coverage does not certify all 31 vinyls,
every kit permutation, in-race damage appearance, Android FPS or retail artistic
quality. Outback/Jetta still inherit differences in source model detail; no claim
of completing a bespoke remodel is made.

Reproduction/evidence:
`out/validation/linux-current-20261029/first-wave-r4`, `ferrari-r4`,
`complete-car-visual-r4`, `final-car-gallery-runtime.log`,
`final-car-deployment-receipt.json`. The archive and installed files were checked
against their hashes after packaging. No career money, cars or respect were edited.

## Wave 2 revision 4 / combined delivery r5 — Jetta follow-up

The full-angle review exposed blank Jetta headlamps and two floating black fins
near the rear glass. Restored fourteen source lamp pieces, translating the
vertically displaced reflector assemblies behind their original transparent
lenses. No new vehicle silhouette or fictional lamp mesh is substituted.

Material-colour and then object-colour probes isolate the fins to the trunk
liner. A rear-window-frame hide probe fails to remove the fins and is rejected;
the final frame is the original prepared frame. A raycast against the actual
painted/glass exterior fits 28 liner vertices 8 mm inside that shell. Final full
native captures show the corrected rear surface and detailed headlamps across
eight yaws and four stock/customization states. Shared source geometry comparison
finds 217 other parts unchanged; the 31 Jetta decals and calibrated physics recipes
are byte-identical. Other Wave 2 cars are unchanged.

The combined r5 ZIP extraction passes all 1238 full-roster, 377 first-wave and 226
Ferrari checks. It is installed on Linux with profile hashes preserved. RP5 r5
installation is pending; no Android FPS/driving approval is claimed. See
car-visual-r5-checkpoint-2026-10-29.md for artifact identity and evidence paths.
