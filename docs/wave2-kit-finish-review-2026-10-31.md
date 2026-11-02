# Seven-car kit finish review — 2026-10-31

Installed Linux remains **r11 / Wave2 revision 9**. This review found concrete
customization defects and rejected an isolated first Outback redesign. No
experimental geometry was installed, published or sent to RP5.

## Captured and inspected

`out/validation/linux-current-20261029/ten-car-kit-review` contains **336 new
native Radeon RX7800XT captures** of the actual installed seven-car Wave2 set.
Stock plus kit1/2/3 were captured, with carbon hood/retail vinyl8 on each kit.
For each car: eight normal yaw views and four closer cardinal views per state.
The actual CarView/GLES renderer, selector lighting and 1280x960 viewport are
used. Camera radius/height/target: 800/245/70 cm normal, 520/160/70 cm close.
The close mode hides only the 2D UI, retaining depth and the real 3D viewport.

Every PNG was compared pixel-for-pixel with its original BMP. Raw BMPs remain on
the secondary disk at `OpenJuice-builds/20261031-wave2-kits`; portable PNGs,
contact sheets, HTML galleries, hashes and process logs remain in validation.
Both native capture processes exited zero. Captures are offline presentation
evidence, not an FPS benchmark. No personal save was loaded or changed.

**Actually visually inspected in this review:** all seven close sheets (112
views) and the Outback normal sheet (32 views). Other normal sheets were
captured and retained but are not marked inspected. The prior actual retail
Focus white-paint reference and repaired Jetta four-state sheet were also viewed.
The exact inspected image hashes and decisions are in `visual-review.json`.

## Concrete findings

| Car | Current finding / next refinement |
|---|---|
| 405 | Small front/skirt trim changes; rear alternatives weakly distinguished. |
| 406 Coupé | Kit2 rear has a conspicuous flat blue lower shelf; integrate its contour. Rear wheel placement appears centred in these views. |
| Golf R32 | Lower trims dominate differences; inspect kit2 around the twin stock tips before claiming clearance. A screenshot alone is not a collision proof. |
| Jetta | Kit2 rear reads as separate blue end blocks. Retained source wheels and headlamp repair render, but that kit finish is not accepted. |
| Outback | Plate/lights/cladding retained; rear alternatives barely distinguish themselves, front kit2 reads as a thin shelf. Needs coherent bumper/rocker variants. |
| Volvo 240 | Plate and both lamps render; rear choices almost indistinguishable. |
| V70 | Kit2 front exposes a wavy lower edge. Wheels appear centred; lamp housings retain simple source detail. |

These observations **do not close** the user's requested three finished bumper,
rear-bumper and skirt choices per car. Counts, distinct mesh hashes, fitted
vinyl meshes and a passing loader do not establish an acceptable visible finish.

## Isolated Outback redesign, pilot 01 — rejected

Following the low-poly-game-models skill, the study uses actual current native
stock geometry as the measured reference, preserving a recoverable baseline.
There is no Blender MCP exposed in this session; the existing local Blender
5.1.1 CLI was used. Tools:

- `tools/street-cars-wave2/build_native_kit_study.py`: editable Blender scene,
  measured roots/clearances and GLB kit2/3 exports, Outback pilot only.
- `tools/street-cars-wave2/package_native_kit_study.py`: native candidate adapter,
  stock mesh bytes retained within each replacement; stable catalogue/save IDs.

The pilot fits only actual native bumper/rocker parts, rather than an all-car
raycast that can hit underfloor geometry. Its first frontend study uses measured
34–40 cm roots below the plate, rear 38–46 cm roots, continuous lower edges,
stock-exhaust clearance and physically closed diffuser fins. It replaces only
sixteen garage/race kit2/3 D8Ms in a private candidate. All other runtime files,
including physics, wheels, hoods, vinyls, sounds and stock/kit1, remain intact.

Technical checks pass: original part vertex/material/index bytes preserved;
exported additions have finite/unit normals, nondegenerate triangles, closed
edges and ground clearance; native full-roster checker **1238/1238**; an
additional **48 native candidate frames**, with exact PNG/BMP comparison. The
candidate's 16-view close sheet was visually inspected.

**Decision: REJECTED; NOT INSTALLED.** Rear kit2 reads as a hanging flat plate,
with an isolated left block at its exhaust notch. Making an extension larger
does not integrate it into the bumper or produce the requested Juiced finish.
Next: integrate lower surfaces with the source bumper volume and cladding
materials, retaining plate/grille/lights and the existing stock geometry.
Do not promote this candidate because the native checker is green.

External source study/candidate:
`/run/media/martin/Ediloco/OpenJuice-builds/20261031-wave2-kits/outback-study-01`
and `outback-candidate-01`. Native evidence and explicit rejection:
`ten-car-kit-review/outback-pilot-01/visual-review.json`.

The adapter's first attempt incorrectly assumed its trim material must be newly
added; the baseline already had that identical material. It stopped without
installation, was corrected to accept an existing identical material, and
resumed the isolated candidate with guards on all unrelated files. Logs remain.

All nine installed package inventories, six personal file hashes and the
production ELF were verified unchanged across both rendering runs. RP5 remains
reserved by the user. Overall ten-car acceptance remains open.
