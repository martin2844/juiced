# Peugeot405 photo kits — r19

Supersedes the rejected r18 bumper/rocker/hood styling, preserving r18 itself
and its provenance. Authoring uses Blender5.1.1 and native source cm units:
X across, Y up, +Z forward. Blender uses metres `(x,-z,y)/100`.

Three photograph-derived references, observations and explicitly estimated
adaptations are recorded in [references.json](references.json). The Gutmann
retailer has fitted front/rear and loose-kit photographs; Stratos has fitted
and loose front plus a small full-car poster. The blue car in Morette's gallery
has clear front/bonnet photographs but no rear: its rear is an original
adaptation. Morette is the photograph/headlamp source, not an asserted body-kit
manufacturer. These are game-scale interpretations, not exact commercial copies.

`build.py REPO BASELINE_R18 FRESH_OUT` runs inside Blender. It copies the
baseline and replaces the three405 front/rear/left/right rocker meshes and
three existing optional hoods. Constrained silhouettes, real intake reveals,
recessed diamond screens and separate light geometry replace the old generic
fascia. Original side-facing painted vinyl receivers remain. The bonnet is
clipped with all vertex attributes interpolated, retaining its original
pressed contours and source normals, including their original nonunit lengths.
The plain carbon hood reproduces source positions and normals exactly.

`verify_geometry.py` independently checks exported indices/materials/normals,
source bonnet seams and race/UI identity. Native engine loading/refit uses
the existing `juiced_wave2_car_check`; the private `build_preview.py` +
`capture.py` harness captures64 native images in eight configurations without
relinking the production executable. `studio.py` + `finish_studio.py` build
the complete editable four-scene Blender assembly with packed source textures.
Blender's materials approximate the actual game: the native captures govern.

`prepare_delivery.py` freezes r19/Wave2 revision17 and validates ZIP payload
hashes. `install_linux.py` uses reversible directory exchanges, with exact
guards for personal profiles, the production ELF, other mods and repository
defaults. It changes no saves. `present.py` publishes the local comparison
gallery and editable sources. Scripts have deliberate fresh-destination guards;
do not rerun against installed r19 as if it were the original r18 input.

Scope:405 bumpers, skirts and hoods only. No new catalogue indices, vehicle
identity, wings, physics, sound, engine or save format changes. Source cockpit
is still incomplete and hidden by the existing opaque privacy-glass workaround.
Native floor reflections have existing artifacts. Aesthetic approval and
actual driving remain user review; no Android or performance claim is made.
