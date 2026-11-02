# Peugeot405 custom shop — one-car rework, 2026-11-01

The user rejected the almost invisible kits and badly fitted donor wings and
requested a focused Blender rework of one car, Peugeot405. This replaces that
car's custom geometry rather than propagating another generic tweak to ten cars.
It does not establish acceptance of the other nine imports or their r17 wings.

## Delivered shapes

- Street: period-inspired painted lower fascia, three recessed front openings,
  curved fog lenses, narrow rub strips, restrained rocker and a low raised wing.
- Combat: larger trapezoid intake, curved grille/slats with two dividers, painted
  stock plastic corner returns, segmented sculpted rocker, rear grille/lip and
  three diffuser vanes, plus a body-painted endplate wing.
- Touring: deeper intake and carbon lower splitter, two canards per front corner,
  rocker end fences, wider rear diffuser with five vanes, and carbon foil/endplates
  on alloy-colour supports. Each has measured rear-deck mounting shoes.
- Two new hoods cut actual openings from the retained source surface, with inset
  walls/wells and seven physical louvers per port. Painted and carbon versions
  append at indices2/3; existing stock0 and plain-carbon1 stay available.
- Fixed reflective opaque glazing now actually hides the absent source cabin in
  close workshop views. This intentionally keeps the normal tint slider from
  lightening those windows. The interior is not claimed to have been completed.
-66 existing low horizontal undertray faces receive dark material, retaining
  positions, UVs, normals and colours. Pale floor reflections still exist in the
  current renderer; this is not a claimed shared reflection-shader fix.

The shape reference is [the FIA5380 Mi16 form](https://historicdb.fia.com/sites/default/files/car_attachment/1601054701/homologation_form_number_5380_group_a.pdf), page1, which shows front/rear period details. The new
kits are original aftermarket interpretations, not exact OEM Mi16 reproductions.
Original downloaded-body/material credits remain in the package; the new405 wing
geometry is authored here rather than fitted from another retail car.

## Evidence and rejected candidates

Evidence root: `out/validation/peugeot405-rework-20261101`.
`gallery/index.html` includes native comparison and eight-angle sheets;
`gallery/overview.jpg` compares original panels with the three new configurations.
`final08` contains48 full native captures: stock, Street, Combat, Touring, red
Combat/vinyl8 and silver Touring/vinyl3. Native loading/refit exercises every new
hood and wing. The original plain-carbon hood is exercised by the unchanged
baseline checker; all31 existing vinyl meshes load through that checker.
Native checks finish **1,322 passed,0 failed**. These are presentation/slot/data
checks, not physical Android gameplay, natural career unlocking or FPS acceptance.

Early candidates were rejected after native views exposed overly tall boxy rear
panels, flat grilles intersected by the original centre, and jagged raw ray-hit
projections through existing openings. The final adapter fits smooth measured
outer envelopes, preserves original upper/arch/side receivers, and samples curved
recesses. Curved fog-lens surfaces replace intersecting flat lens boxes. An initial
Blender assembly omitted the engine's right-wheel rotation; the final file restores
it. None of the rejected candidates was installed.

## Sources and deployment

`tools/peugeot405-rework/assets/Peugeot405-Custom-Shop.blend` contains four complete
scenes, packed original textures and a studio. `Peugeot405-Authoring.blend` retains
construction collections/modifiers. `tools/peugeot405-rework/README.md` documents
coordinates, rebuild order, limitations and exact scripts. Blender renders are
material approximations; the native captures establish game appearance.

Runtime bundle: `out/releases/OpenJuice-Peugeot405-Custom-Shop-r18.zip`.
Wave2 revision16,130,681,167 bytes, SHA256:
`9ee89b4321fc84ec6004b0523320f94e94fb50fccd3e838f7e6d3ce152423de5`.
All CRCs and every runtime payload hash were read back. Only405 payloads, its two
appended hood entries and package metadata/attribution change. Handling recipes,
the other nine imported cars, sound and production ELF stay unchanged.

Linux deployment and exact save/unrelated-package guards are recorded separately
in `linux-delivery.json`. This step does not access the RP5, replace an APK, push
Git, publish a public release or modify a personal profile. Its last installed
Android car bundle remains r17. Existing indices retain their identity and no
save-compatibility or roster reset is introduced.

Visual finish is reviewable through the actual native gallery. This checkpoint
records a concrete one-car asset rework; it does not declare the whole ten-car
project finished or substitute automated loading checks for user visual feedback.
