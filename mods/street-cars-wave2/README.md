# Street Cars Wave 2 — candidate

Requires a current OpenJuice build supporting64cars, plus Ferrari(ID53) and
Street Cars(IDs54–56). Adds IDs57–63. Stock and three front/rear bumper and skirt
assemblies per car; conservative lips/valances, not OEM accessories.
Rear bodywork is fixed: no synthetic detachable spoiler.
Native donor engine sounds are reused. Power, mass and drivetrain target the
selected factory variants; gearing and torque-curve interpolation include
approximations. Juiced remains arcade physics. Real-device driving/performance
approval is pending. See SOURCE-CREDITS.json for third-party provenance.
No saves are included. Android requires the updated executable, not just this ZIP.

## Revision 4: Jetta source assembly correction

Restores the source stock headlamp frames/reflectors, with transparent outer lenses
and small switchable high-beam surfaces. The donor scene stores the reflector
assemblies 1.68 m above their installed lenses; the Blender repair places those
actual assemblies behind the lenses. Fits 28 vertices of the simplified trunk
liner inside the actual body/window shell, removing two floating black fins.
The rear-window frame is unchanged; hiding/replacing it did not fix the symptom.

Stable IDs, handling recipes and all 31 Jetta decal meshes are unchanged. Native
Linux review: eight views of stock/carbon/vinyl8/maximum-kit+carbon+vinyl8.
The full 64-entry roster checker passes 1238 checks. This does not certify Android
FPS or every vinyl/kit permutation; RP5 deployment and driving review remain open.


## Revision 5: Peugeot 405 glazing

Assigns the four existing window sheets to the native Car Glass material instead
of the opaque black plastic atlas. Window seals, pillars, mirrors, geometry and
UVs remain intact. Both driving and menu model folders are updated.

Native Radeon comparison covers stock, carbon bonnet, vinyl8 and maximum kit
with carbon/vinyl8 from eight angles. Stable car IDs, physics and save format
are unchanged. This is Linux visual validation; physical Android review remains
pending. The original prepared scene is preserved separately.

Revision6: original Outback tailpipe occupies the native replaceable exhaust slot. Removes donor exhaust duplication across all bumper kits. Geometry, textures, calibrated handling and roster IDs are retained.

Revision7: Volvo240 original plate lettering uses the native double-sided alpha-test material and source cutoff. Carbon extends over the retained forward hood surface instead of stopping20cm short. Existing calibrated handling, roster, headlamp textures and other cars retained.

## Revision 8: Golf R32 final-drive allocation

Uses the VW 02Q AWD workshop final-drive tooth counts: 72/17 on gears 1–4
and 72/22 on 5/6/reverse. The native engine uses one final drive, so fifth
and sixth include the secondary-to-primary factor. Individual gear pairs
remain provisional; the previous overall reverse ratio is retained until
the selected gearbox's reverse pair is verified. Power/torque, masses,
meshes, sounds, car IDs and save identities are retained.

Updates SPECIFICATIONS.json to distinguish installed Linux observations
from unresolved manufacturer/gearbox evidence. Factory documentation and
test limits are recorded in `docs/ten-car-handling-verification-2026-10-31.md`
in the port repository. This is not a full gearbox or Android certification.

## Revision 9: Volvo 240 M46 gearing

Corrects first gear from 3.71 to 4.03 and reverse from the MX5 donor's 3.758
to 3.68, supported by the original Swedish 1984 Volvo owners manual, printed
page 100. Retains .797 overdrive from original factory service TP30425/4 p80.
Power/torque, mass, final drive, visuals, sounds and save identities are retained.
The exact mass and axle allocation for the chosen European Turbo remain
estimates; the general four-door weight range does not certify a precise trim.

Reference: https://www.240.se/litteratur/tp2401.pdf
See `docs/volvo240-transmission-verification-2026-10-31.md` in the port
repository for measured first/reverse driving and the native engine's limits.

## Revision 10: integrated Outback kit 2/3

Replaces the old thin extensions with continuous changes to the original lower
bumper/rocker surfaces. Kit2 retains factory grey cladding with deeper shaping;
kit3 uses a fuller profile and the existing native body-paint material on selected
cladding faces. Grille, lamps and plate retain their textured regions. No new
textures are allocated. Stock/kit1, car and part IDs, wheels, hood/vinyl meshes,
sound, handling recipes and save data are retained.

Editable Blender/GLB sources and native vertex/material provenance are in
`tools/street-cars-wave2/assets/outback-kits-20261031` in the port repository.
Use the native morph/coating adapters; do not regenerate current cars with the
legacy whole-source importer. See `docs/outback-kit-integration-2026-10-31.md`
for exact reproduction commands, native renderer review and remaining limits.

## Linux r13 / Wave2 revision 11 — fitted kit2/3 geometry

Six cars (405, 406, Golf, Jetta, 240, V70) replace the old sampled strips with
measured stock-surface lower treatments and fitted closed extensions. Side
triangles receiving retail vinyls remain unchanged, including normals and UVs;
new exposed extensions remain below the lowest source vinyl. Rear lips avoid
measured exhaust intervals. Stock, kit1, handling, wheel/hood/vinyl payloads,
roster IDs and saves retain the previous configuration.

This is a fit correction, not full visual acceptance: rear style differences
are subtle on 405/240, V70 lamp art needs review, and kit1 is retained. Outback
is unchanged; its grey-cladding/vinyl interaction still needs targeted proof.
288 native offline views were reviewed. RP5 delivery is still pending.

## Linux r14 / Wave2 revision 12 — Outback lower vinyl correction

Kit2/3 skirts now retain every original stock receiver triangle. The new lower
extensions mount on the real flange and remain at least6mm below all31 source
vinyl meshes. Kit3 retains body-coloured cladding through material-only face
assignment. Existing front/rear bumper kits are unchanged. A lower-door opaque
centroid audit improved from1184 newly hidden cases to0 over2844 kit/sample
comparisons. Native before/after F&F1 and48 whole-car views were inspected.
These are bounded Linux geometry/visual checks; Android delivery is pending.

## Linux r15 / Wave2 revision13 — V70 lamps and period kit3 fascia

The existing V70 reflector rings/bulbs now fit each actual lens/housing; outward
normals/winding are corrected, and LIGHTSON reuses reflector surfaces so on/off
is visible. Stock, sport, kits1/2/3 and UI copies share the correction.
405/240 kit3 front/rear fascia now use native player paint.240 selections follow
complete original bumper components; grille, inner chassis, chrome/plates and
lamps retain their original materials. No geometry changes on405/240.
176 native views were inspected, including alternate red body paint and24
LIGHTSON off/on diagnostic views. This is not an Android/night-race/FPS claim.
No roster/part IDs, save formats, handling, sounds or engine changes. Existing
Wave2 kit1 finish and handling-variant questions still require review; RP5
verification/delivery is pending.

## Local car bundle r16 — 2026-10-31

Kit1 fitted lower bumpers/skirts for all seven cars retain original vinyl receiver surfaces. Outback grey add-ons use the correct native atlas coordinates; kit2 skirt changes are UV-only. Other kit2/3 payloads are unchanged.
Native full-roster candidate checks pass (1,238 Wave2 plus483 first-wave assertions). This is a user playtest delivery, not a claim of complete visual/handling/device approval. Additional evaluations were stopped at the user's request.

## Local car bundle r17 — wings and reported visual fixes

Three actual retail Juiced wing options are fitted per car. Previously integral source wings are detachable stock choices; boot/hatch panels stay attached. Other existing body-part and vinyl indices, car IDs, handling and saved-game formats stay unchanged. Wing choices append to the previously absent SPOILER category.
Peugeot405 uses96% privacy glass by default, retaining its window geometry. Volvo240 gets an upright JUICED plate and neutral backing. Jetta restores16 original rear lamp pieces:2,009 simplified/duplicate faces are replaced by8,348 source faces; its brake overlay uses324 real-cell faces instead of the whole outer lens. The intrusive87-face trunk liner is removed; other interior components remain. Rear flare anchors follow the real brake cells.
Full native composition passes1,322 Wave2 and483 first-wave checks. New wing geometry was reviewed in eight directions; the Jetta brake overlay was compared off/on. These checks do not establish Android FPS or finished bumper/skirt artistry; the user requested richer kits and that work remains open. No saves are included.

## r18 — Peugeot405 custom shop, 2026-11-01

The405 alone gets newly authored Street / Combat / Touring front/rear bumpers,
sculpted rockers and three purpose-built rear wings with measured deck shoes.
Two genuinely cut/louvered hoods append at indices2/3; existing carbon index1
and every other car/part identity remain intact. Native materials follow body
paint; original side-facing vinyl receiver faces remain. Opaque reflective
privacy glass hides the incomplete source cabin in close workshop views.
Sixty-six existing undertray faces receive dark material, retaining geometry.
These are original aftermarket designs inspired by the period, not exact Mi16
factory parts. No handling, sound, save format or production engine changes.
Editable Blender assemblies and authoring sources: tools/peugeot405-rework.
Review evidence: out/validation/peugeot405-rework-20261101.
Only the405 was reworked; the other imported cars' existing assets are unchanged.

## r19 — Peugeot405 photo-based kits, 2026-11-01

Replaces the r18 front/rear bumpers and skirts at the same three indices with
photo-derived Gutmann, Stratos and blue Morette-showcase interpretations.
Painted rolled intake rims, recessed diamond screens, rectangular/round fog
lamps and concave continuous skirts replace the generic inflated fascia.
The factory hood pressing and its original supplied normals are retained for
plain carbon and painted/carbon centre-scoop options. Old paired louvres are
removed. No new parts or save identities; the existing wings stay unchanged.
The third reference does not show its rear: that valance is an original
adaptation. Stock upper bumper seams, lamps, vinyl receivers and left exhaust
are retained rather than claiming exact commercial-kit copies. Source URLs,
observations and adaptations: assets/PEUGEOT405-PHOTO-REFERENCES.json.
Editable source and reproducible Blender pipeline: tools/peugeot405-photo-kits.
Only405 visuals change. Physics, engine, personal saves and other cars do not.

## r20 — Peugeot405 rear finish, wings, opaque grille and preparation

Three continuous moulded rear fascias replace the r19 lower add-on look,
retaining the original plate, source corner vinyl receivers and actual left
exhaust clearance. The three405 spoilers are now a low bonded boot lip,
a period painted bridge and a restrained painted GT foil with fitted moulded
feet. The existing upper grille has an opaque radiator backing and inward
facing roof/floor/side returns: its slats no longer look into an empty body.
The approved405-only built-engine/boost preparation leaves stock102BHP intact.
Complete street/sports/competition/prototype sets produce149/218/320/400BHP;
prototype reaches class5. The optional TurboUpgradeScale dynamics parameter
uses unity defaults for every other car. Existing parts and save identities
stay unchanged. Native builds with the r20 engine feature refresh this car's
derived rating on profile load, including previously installed parts.
Older executables safely retain legacy tuning (~216BHP maximum); this asset
ZIP alone does not upgrade an Android/macOS executable. Linux was updated.
Sources: tools/peugeot405-refinement. Earlier model/photo attribution remains.

## r21 — Peugeot405 complete bumper replacements

The three front/rear choices now replace the complete factory bumper shell.
Factory lights, grille and fixed body panels belong to CHASS rather than being
duplicated in each custom bumper. Factory bumper surfaces remain only in their
stock slots. Exact triangle clipping preserves factory geometry/UVs/normals when
stock is selected. Custom shells discard the inherited factory skins and have
new body-matching mounting collars and continuous wheel-arch returns. The plate
is retained as two original UV-mapped triangles in the new rear recess.
No catalogue IDs, parts, saves, other cars, hoods, skirts, wings, tuning or engine
were changed. The approved r20 102-to-400BHP preparation remains unchanged.
Verified in the Linux native renderer; Android installation is not included.
Sources and editable Blender assembly: tools/peugeot405-bumper-replacement.
Earlier model and photograph attribution remains applicable.

## r22 — Peugeot405 fit and finish in the actual workshop

All three front/rear kits have denser mounting contours projected onto the
actual fixed body; the collar is welded into the shell and its winding stays
outward. Front/rear mounts are58.25/64.5cm. Wheel-arch clearance was reduced
from the oversized39cm envelope to33.8cm, with turned-in returns. Redundant
intersecting lower strips were removed and the continuous skins have smooth
normals. A conformal intake liner blocks views through the duct ends. Fog lamps
reuse the original fluted headlight glass texture instead of flat colour discs.
Only the405 reveal/rubber materials become two-sided for their thin inner walls.
Stock surfaces retain their original area, UVs and normals. Catalogue identities,
other cars, other405 assets, physics and saves remain unchanged. The r20 approved
102-to-400BHP preparation is retained. Checked using actual GarageView lighting,
close front/rear views and the native selector. No Android installation included.
Sources and editable Blender assembly: tools/peugeot405-fit-finish.
Earlier model/photo attribution remains applicable.

## r23 — Peugeot405 kits fitted around the actual OEM perimeter

Six bumper replacements now follow the real U-shaped OEM contour, including
its overhang and the return toward the wheel. A narrow original mounting band
is the single outer skin at the joint; the new fascia is the single skin below
it. Lower sections have controlled convex curvature and taper, with the stock
wheel centres unchanged. Lamp/intake proportions are preserved by taking extra
height in the plain mounting section. Opaque, double-sided textured interior
walls replace Constant Colour reveals whose material class ignores a supplied
Double-sided setting. The original fluted glass texture is reused for fog lamps.
Only six bumper mesh pairs and their local interior material/texture are new.
Fixed body, stock bumpers, other405 assets, other cars, catalogue and physics are
byte-identical to r22. The approved102-to-400BHP preparation remains unchanged.
Reviewed using the real workshop and selector. Linux only installation here.
Sources and editable Blender files: tools/peugeot405-oem-fit.
The earlier source model and photograph attribution remains applicable.
