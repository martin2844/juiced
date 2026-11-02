# Peugeot405 OEM perimeter fit — 2026-11-01

R23 / Wave2 revision21 is installed and verified locally in Linux. A running
r22 session keeps its old immutable overlay and must be reopened to see r23.
The guarded installer exchanges entire directories without changing the old
asset inodes: both hard-linked old overlays were verified identical before
and after installation, and the existing game process remained open. The
initial request to close the game became unnecessary once this isolation was
verified. No game process was terminated and no actual profile was edited.

The user's assessment of r22 was correct: closing some holes did not make the
kit follow the car convincingly. The former approximately single-valued Z(X)
surface could not represent the bumper returning behind its widest point.
Repeatedly adjusting that surface left a conspicuous transition under the
lights and imperfect corner returns. Loading counts and close vertex distances
were insufficient art-acceptance criteria.

The new builder samples the original full U-shaped cross-section around the
axle, including the overhang and return. It uses a controlled convex lower
section, tapered sides and a turned-under lower edge. A narrow0.6cm original
mounting band remains as the single exterior skin at the joint, with the new
fascia as the single exterior skin below it. There is no full factory bumper
under a custom skin. Original factory slots and fixed chassis are unchanged.
The mounting band's lower edges are frontY57.65 / rearY63.9cm. Source factory
slot tops remain frontY58.25 / rearY64.5cm. Native units remain centimetres,
X across, Y up and +Z forward; Blender uses metres `(x,-z,y)/100`.

Only the fascia's unperforated mounting region absorbs additional height;
fog lamps and intakes retain their proportions. The original fluted-headlight
texture supplies the fog glass. The lower authoring edges are front17/15.5/17cm
and rear19/17.5/18cm for kits1/2/3. The33cm wheel-clearance envelope is an
authoring fit around unchanged wheel centres, not new suspension or physics.

The former Constant Colour material class does not expose a Double-sided
parameter: `ConstantColour::setParam` handles only index0 and its bind uses the
renderer default cull. Appending that setting to its PMD did not enable it.
New kit interiors use a dedicated Specular Opaque material, a1×1 opaque dark
texture and supported double-sided rendering. Recess walls and a fitted inner
lining now block the visible workshop background at the duct ends. This is
an asset correction; no renderer/engine changes were needed.

The native workshop views govern acceptance. Early radial candidates were
rejected for copied stock undertray discontinuities, distorted details or
overemphasized original mouldings. Study08 is the selected candidate. It uses
only a thin original mounting band and a smooth complete fascia beneath it.
The source model, existing photo-derived kit directions and prior attribution
remain. The reference images were checked again against the actual workshop
captures, rather than treating a loading check as evidence of realism.

## Verification and delivery

- Six bumper race/UI mesh pairs plus two copies of the local interior material
  and texture change. Chassis, stock bumpers, all other405 assets, other cars,
  catalogue, saves, engine and the approved102-to-400BHP preparation are intact.
- Independent native read-back verifies finite attributes, indices, outward
  centre-facing fascia triangles, race/UI identity, the exact asset scope and
  opaque texture alpha. Fourteen stock workshop views are pixel-identical to
  r22, including the six close-up cameras.
- 70 selected actual GarageView workshop captures and64 native selector views
  include all three kits, dark/light paint, close front/rear cameras, existing
  hood/wing/vinyl combinations. All1,322 native load/refit/damage/compatibility
  checks pass on the final candidate.
- The habitual launcher loads a private Tony profile through title to menu
  with hidden-window test flags. Installed and repository-default packages
  match the runtime ZIP; all six actual personal files and other packages
  remain identical to the pre-install snapshot. R22 rollbacks are retained.
- Editable construction and a complete four-scene Blender assembly with packed
  textures are in `tools/peugeot405-oem-fit/assets`. Original references/credits
  continue in `tools/peugeot405-photo-kits/references.json`.

Runtime: `out/releases/OpenJuice-Peugeot405-OEM-Fit-r23.zip`,133,673,817bytes,
SHA256 `d42b0e6261f6ea977c06ec2fe994aa0f384b31b00ae776ec330494d2c30c1950`.
Every ZIP payload and CRC was read back and verified. The unchanged native ELF
SHA256 is `5df97d1c286a8066ff784aa2f01fde658cde860a5dd5e714b00d5041d90de91f`.
The preparation receipt includes the live baseline inventory and an additional
verified save backup. The installer never restores that snapshot over a live
profile. Current and new sessions retain separate assembled asset overlays.

Gallery: `out/validation/peugeot405-oem-fit-20261101/gallery/index.html`.
This is a visual asset revision for in-game artistic assessment, not a claim of
photorealism or final user approval. Source texture/interior limitations,
existing glazing and renderer reflection artifacts remain. No Android update,
public push, race-stability or FPS certification is included.
