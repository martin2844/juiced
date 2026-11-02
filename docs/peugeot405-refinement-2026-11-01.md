# Peugeot405 r20 refinement — 2026-11-01

Follow-up: the user correctly identified that the custom bumpers retained
factory skins underneath. R21 supersedes that part of this delivery; see
`peugeot405-bumper-replacement-2026-11-01.md`. The r20 spoilers, opaque grille
and approved preparation remain unchanged. R20 checks did not establish
correct bumper replacement.

The user accepted the r19 direction and requested better rear bumpers and
spoilers, an opaque backing behind the see-through front grille and an upgrade
scaling review. A native probe showed the original catalogue correctly produces
101.9177BHP stock and215.7393BHP with all prototype parts (class7). The user then
explicitly chose a special preparation up to about400BHP, preserving factory power.

## Delivered visual changes

Three rear fascias now have continuous moulded surfaces from the upper stock
seam through the lower lip, a proper number-plate pocket and actual left-hand
exhaust clearance. The original plate's two triangles, position, UVs and source
material remain; original rear lamps and outward-facing vinyl receivers remain.
The isolated lower chrome remnants were removed. These are interpretations of
the r19 references, with the same documented third-rear/exhaust adaptations.

The three spoilers were reauthored for this car: a low bonded lip whose underside
follows the deck, a period painted bridge and a restrained painted GT foil with
small shaped endcaps. Moulded supports have individually surface-fitted rubber
shoes. The very tall carbon foil/white supports from r18/r19 are no longer used.
This changes only405 wings; wings on the other imported cars remain as before.

A shaped opaque radiator backing and roof/floor/side returns now close the
empty source body behind the grille. The existing outer slats and badge stay.
The cavity returns face inward so the GPU sees their front faces through the
aperture.126 traced rays through exported native geometry are blocked with
backface rejection;66 were open in the r19 negative control. Twelve actual
native low-front captures supplement64 whole-car views. The original chassis
vertex/normal/UV records and every original chassis triangle remain intact.

## Approved405-only preparation

| Build | Native measured BHP | Displayed class |
|---|---:|---:|
| Factory |101.9177|8|
| Full street |148.6571|8|
| Full sports |218.1364|7|
| Full competition |320.1899|6|
| Full prototype |400.0001|5|

The model opts into `TurboUpgradeScale 1.10 1.40 1.75 1.85409`. The four values
multiply the fitted turbo's catalogue value by its level, representing the
approved built-engine/boost preparation. Exhaust and induction still contribute
their original catalogue factors. Removing the turbo removes the special
preparation. Factory torque153Nm, source power curve, gearing and body dynamics
stay unchanged. Every other model has unity defaults, retaining original fitting
arithmetic;16 stock/reference output cases remain identical at logged precision.

This is a game balancing choice approved by the user, not a claim that a stock
GLX engine tolerates this boost or a real-car performance certification. Native
CarBody flat-ground steps confirm the added power affects driving, not just UI
rating. Those synthetic acceleration times are not road or full-race benchmarks.
The legacy rating formula displays roughly401BHP for a measured400BHP engine;
its class5 threshold is satisfied without modifying the shared class rules.

Sources changed: `src/Core/Physics/Dynamics.h/.cpp`,
`src/Core/Cars/CarPhysics.cpp`, `src/Game/Frontend/FrontendSaves.cpp` and
`tests/Unit/Physics/TuningPreparationTests.cpp`. The optional field is not in
the serialized Car/save. Configured cars refresh their derived rating at profile
load, for both the player's garage and opponents; the existing zero-rating
recovery stays in place. Unconfigured models retain saved ratings as before.
No parts, vehicles or catalogue indices were added/reordered. Original saves
need no migration or replacement of their installed upgrades.

A rejected configuration comment used space+tilde; the retail script lexer joins
that line with the next, swallowing the instruction. The end-to-end power probe
caught the unchanged215.7BHP despite passing parser unit tests. The final payload
uses a plain comment and was re-read by the real loader, then power/classes and
driving were verified. A separate early grille-only ray sample also counted
backfaces: the final check culls those and includes original bonnet/bumper faces
which close samples outside the visible aperture. Final receipts document both
the positive and negative controls instead of treating either early check as final.

## Validation and local installation

-415 unit checks passed; optional-field parsing, invalid complete curves,
  stock isolation, unknown tiers, removed turbo and rating/simulation agreement
  are covered by the new focused tests.
-Native keyboard/gamepad/controller-reconnect check passed, preserving the
  earlier title/profile keyboard fix after rebuilding the ELF.
-1,322 native Wave2 loading/refit checks passed. Independent parsing verifies
  all seven changed race/UI mesh pairs, source chassis/plate retention and
  byte-identical other cars/catalogue/fronts/hoods/skirts.
-64 eight-direction native images and12 grille close-ups, plus editable complete
  Blender scenes and authoring objects. Native rendering governs acceptance;
  Blender lighting/materials approximate the game.
-The habitual launcher loaded Tony through the title/profile flow to the menu
  using a private copy of profiles. Personal profiles were never used for writes.
-A second, controlled private fixture kept prototype parts but deliberately
  stored the old216.1844 rating. Loading it through the habitual launcher refreshed
  the derived rating to401.25235 (class5), without a purchase or save migration.

Installed and repository-default Wave2 revision18 match the runtime ZIP. All six
actual personal files and other packages remain identical to the pre-task hashes.
A verified save backup is in
`~/.local/share/openjuice/save-backups/before-405-r20-20261101-182548`.
Both prior r19 packages remain in the evidence directory; the previous ELF is
`out/validation/peugeot405-refinement-20261101/juiced-r19-rollback`.

Runtime: `out/releases/OpenJuice-Peugeot405-Refinement-r20.zip`,131,828,308bytes,
SHA256 `b34397fecacd7408efacde6014027efa09f48a8685ca8f7cfac85460b530d79b`.
Native ELF SHA256:
`5df97d1c286a8066ff784aa2f01fde658cde860a5dd5e714b00d5041d90de91f`.
The runtime is assets only: older executables safely ignore the optional tuning
instruction and retain legacy power until rebuilt. No Android/macOS build or
device installation and no public push were performed in this local refinement.

Evidence and comparison gallery: `out/validation/peugeot405-refinement-20261101`.
Editable sources: `tools/peugeot405-refinement/assets`.
The incomplete source cabin/opaque glazing and existing floor-reflection artifacts
remain. No FPS, full-race stability or final artistic-approval claim is made.
