# Peugeot405 photo-driven kit and hood revision — 2026-11-01

User rejected the r18 styling while accepting its consistency and asked for
three good real tuned405 references, modeled in Blender, better hood lines
and another local Linux installation. This revision addresses that405-only
scope. It does not certify all ten imported cars or a complete visual remake.

## References and design

1. [Gutmann-look complete kit](https://www.kustomorphose.com/kit-carrosserie-367/kit-carrosserie-peugeot-405-look-6801.html):
   fitted blue405 plus loose front/rear and skirt photographs. Four rectangular
   foglights, shallow central trapezoid, five low apron slots, straight period
   bands and a concave sill replace the old swollen/rounded fascia.
2. [Stratos / Arjo kit](https://www.avb-sports.be/peugeot/405/1987-1996/bodycomponents/bodykits/13218/),
   supplemented by [the fitted white405](https://www.comptoirdutuning.fr/405-994/kit-peugeot-405-stratos-50005487.html):
   three large tapered openings with thin curved bridges, clean skirts and
   two low rear recesses. The poster's central twin exhaust is deliberately
   adapted to the game's existing left-hand exhaust and its measured location.
3. [Blue405 in Morette's own photo gallery](https://www.morette.com/gb/peugeot/96-peugeot-405.html):
   a single shallow central opening, four round foglights, outer ducts, double
   lower lip and a central bonnet scoop. Morette supplies the photographs and
   headlights; its page does not identify the body-kit maker. No rear picture
   was available: this kit's rear is a clean original adaptation, not an
   asserted reconstruction of an unseen part. Original Juiced headlights stay.

These are photograph-derived game-scale interpretations, not exact commercial
kit scans. The source405 remains the geometric authority:266.9cm wheelbase,
native centimetres with X across/Y up/+Z forward; Blender metres `(x,-z,y)/100`.
References, observations and adaptations are recorded in
`tools/peugeot405-photo-kits/references.json`; downloaded photos/URL/hash receipts
are in `out/validation/peugeot405-photo-kits-20261101/references`.

## What changed

- Twelve405 kit D8Ms: three fronts, three rears and three left/right skirt pairs,
  mirrored byte-identically into workshop/UI and race assets. Each uses an
  individual constrained silhouette with real recessed openings, painted rolled
  edges, a diamond screen and separate foglight geometry. Rear lower corners
  taper inward instead of keeping the full bumper width down to the ground.
- Three optional hood D8Ms keep their existing catalogue identities. Plain
  carbon reproduces the factory pressing and source normals exactly. Painted
  and carbon alternatives replace the rejected paired rectangular louvres with
  one tapered centre scoop. Clipping interpolates original positions, normals
  and UVs instead of rebuilding the panel as a flat-shaded surface. All exterior
  source vertices and normals outside the small central cut are preserved.
- Metadata and photo provenance advance Wave2 revision16 to17 / bundle r19.
  No parts are appended or reordered. No changes to existing wings, original
  factory body, chassis, wheel nodes, handling, sounds or production code.

The original source normals include lengths0.815..1. Their supplied values
are retained for unchanged source vertices; authored geometry has unit normals.
Normalizing all source normals was rejected by the independent seam comparison
and corrected before the final export. An initial intake pass also left old
fascia fragments visible; those were removed before final native review.

## Verification and limits

The final candidate is `study05` under
`/run/media/martin/Ediloco/OpenJuice-builds/20261101-peugeot405-photo-kits`.
Only the selected final payload was installed; earlier studies are not releases.

-64 native CarView captures: eight angles for stock, each of three blue kits,
  silver Gutmann, plain carbon on silver, red/painted scoop/vinyl8 and
  silver/carbon scoop/vinyl3. Images were visually inspected, not merely counted.
-Independent D8M parsing checks finite geometry, vertex/index bounds, material
  resolution, UI/race identity and preservation of factory hood positions and
  source normals. Input/output inventory proves other cars and catalogue stay
  identical. `geometry.json` records measured bounds and triangle counts.
-Native checker:1,322 passed,0 failed, including actual loading/refit and all31
  existing vinyl meshes. This proves technical integration, not artistic approval.
-Complete editable Blender assembly has four scenes, all four correctly posed
  wheels, packed source textures and actual exported meshes. A separate authoring
  file retains construction objects/modifiers. Blender's lighting/materials are
  approximations: native screenshots govern the in-game appearance.

The source cabin remains incomplete and is hidden by the previously installed
opaque privacy glass. Floor reflections retain existing renderer artifacts.
No claim about60FPS, Android execution or road-car physics follows from these
views. Existing r18 wings remain as before. User aesthetic approval is pending.

## Local delivery and rollback

Installed and repository-default Wave2 revision17 match exactly. The usual
`~/.local/bin/openjuice` launches the existing keyboard-fixed ELF and updated
mods. No production rebuild was necessary. Engine SHA256:
`488bc91b5c934372ca7da9fb07ea0d388b93f88b9451a95a3242c4b93e2a64ee`.

Runtime: `out/releases/OpenJuice-Peugeot405-Photo-Kits-r19.zip`,
131,357,470bytes, SHA256
`1974f29b87a2c7f0cc10b5941c9cefcb09c713822c493acd556192895090736f`.
Every ZIP payload hash and CRC was read back and checked.

Installation uses reversible directory exchanges. Both original r18 packages
are preserved in the evidence directory as `r18-installed-rollback` and
`r18-repository-rollback`. All six personal files, other mods and the production
ELF were guarded by before/after hashes. Tony's unlocked405 and garage remain
unchanged by this installation. No RP5 input, Android installation or Git push.

Evidence: `out/validation/peugeot405-photo-kits-20261101`.
Open `gallery/index.html` for source photos and actual game views.
Editable sources: `tools/peugeot405-photo-kits/assets`.
Scripts, source manifest and final evidence are preserved in the r19 checkpoint.
