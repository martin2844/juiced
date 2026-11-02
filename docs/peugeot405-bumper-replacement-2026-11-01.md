# Peugeot405 bumper replacement correction — 2026-11-01

Follow-up: r22 addresses the oversized clearances and rough workshop appearance
shown in the user's screenshot. See `peugeot405-fit-finish-2026-11-01.md`.
R21 proved slot replacement; its selector-only art review missed these defects.

The user's r20 feedback was correct: the front and rear options looked mounted
over the factory bumpers. The engine already swaps the selected slot mesh in
`CarModel::fill`; it does not append the new bumper. Our asset builders retained
too much original exterior skin inside each kit, partly to keep the original
vinyl receiving surfaces. Native magenta isolation revealed865 inherited front
and356 inherited rear exterior triangles in kit1. Those surfaces made the
replacement look like an extension over stock. R20's loading checks did not
prove that the authored shells genuinely replaced the factory ones.

R21 / Wave2 revision19 changes only nine405 mesh pairs: CHASS, stock front/rear
and the six custom bumper shells. It preserves catalogue IDs, all parts lists,
BOH transforms, wheel placement, hoods, skirts, spoilers, other cars and the
r20 factory102BHP / approved400BHP prototype preparation. The native executable
is unchanged, SHA256
`5df97d1c286a8066ff784aa2f01fde658cde860a5dd5e714b00d5041d90de91f`.

## Geometry ownership and construction

The source importer originally split front/rear/chassis by triangle centroid,
so the bumper slots also contained lights and fixed upper body pieces. Simply
deleting all inherited source geometry would have removed those components.
We repartition the complete source assembly with exact triangle clipping,
interpolating position, original normal and UV at the cut. Native units are
centimetres, X across, Y up and +Z forward; Blender uses metres `(x,-z,y)/100`.
The mounting planes are frontY57 / rearY63 and longitudinal cutZ±161.45.
Factory bumper shells belong only to the stock slots; fixed body, lights,
grille and rear fascia belong to CHASS. The r20 opaque radiator cavity and
underbody remain unchanged. Per-material factory surface areas are conserved
after exporting and reading back the native payloads.

Each custom option discards the inherited exterior stock skin completely,
retaining authored openings, lamp details, reveals and lips. New collars meet
the measured fixed-body seam. The source's textured trim is included when
measuring the mount envelope: paint-only measurements stop before the front
corner. Continuous side returns follow the wheel-arch end instead of copying
every ridge of the old shell into a corrugated side panel. The original two
plate triangles retain their texture UVs in the replacement rear recess.
The arch return uses a39cm authoring clearance envelope around the original
wheel centres; this is a visual fit, not a change to suspension or collision.

The first two isolated candidates were rejected for ripples and incomplete
corner coverage. Study03 is the installed candidate. The process and exact
native-to-Blender authoring sources are under
`tools/peugeot405-bumper-replacement`. The separate editable Blender assembly
contains factory and all three complete custom scenes, with packed textures.
Existing model/photo attribution remains in the r19/r20 documentation.

## Verification and installation

- Independent read-back checks cover finite attributes, indices, normals,
  race/UI identity, per-material factory area conservation and fixed-body
  exclusion of the removable factory shells. Original light areas are retained
  on the fixed body. Other assets and tuning are byte-identical to r20.
- 48 native eight-angle captures exercise stock → no bumpers → kit1 → kit2 →
  kit3 → stock. The final stock image is pixel-identical to the first in all
  eight views. With both slots removed, lights, grille and fixed panels remain;
  no factory outer bumper skin stays underneath.
- 64 further native views cover the existing painted/carbon hoods, three kits,
  spoilers and representative vinyls. The AMD renderer was inspected directly;
  Blender materials are a separate approximation.
- All1,322 native loading, parts/refit, damage and compatibility checks pass.
- The habitual Linux launcher is tested from the title through profile to menu
  with a private copy of profiles and hidden-window test flags (`--test --screen
  start`). Two earlier visible-window automation attempts lost focus and stayed
  at the title; those are not counted as a successful profile/menu check. Actual
  profiles are never used for automated writes.

Both installed and repository-default Wave2 packages are revision19 and match
the runtime archive. All six actual personal files and the other packages are
unchanged. R20 packages are kept in the evidence directory as reversible
rollbacks; an additional verified personal-save backup is recorded in
`preparation.json`.

Runtime: `out/releases/OpenJuice-Peugeot405-Bumper-Replacement-r21.zip`,
132,096,530 bytes, SHA256
`834d5f9949ee859880a030ff535d1b63fecc8a51bf13339ea77fbe9cb8dc84eb`.
Evidence and comparison gallery:
`out/validation/peugeot405-bumper-replacement-20261101/gallery/index.html`.
This verifies bumper replacement and Linux presentation, not race stability,
FPS, Android installation or final artistic approval. Existing incomplete
source interior/opaque glazing and floor-reflection artifacts remain.
