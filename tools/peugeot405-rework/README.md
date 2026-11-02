# Peugeot405 Custom Shop — 2026-11-01

This is the one-car styling rework after the user rejected r17's generic kits
and donor wings. It modifies405 only; its native car ID stays57. Three new
front/rear bumper and rocker designs replace kit indices1/2/3. Three bespoke
rear wings replace the405's wing indices1/2/3. Original hood0 and carbon hood1
remain; genuinely cut/louvered painted/carbon hoods append at2/3.

The car still has no complete source interior. Its glazing uses an opaque,
reflective dark native material to hide the absent cabin from close views;
the ordinary tint slider does not lighten it. No hidden engine was modeled.
The handling, collision cage, sound, original car identity and save format are
unchanged. The three kits are aftermarket interpretations, not homologated
Mi16 parts. Period silhouette reference: [FIA form5380](https://historicdb.fia.com/sites/default/files/car_attachment/1601054701/homologation_form_number_5380_group_a.pdf), page1, dated1989-03-01.

## Editable files

- `assets/Peugeot405-Custom-Shop.blend`: complete native export assembled into
  four scenes: FACTORY, STREET, COMBAT, TOURING. Four wheels, native UVs and
  seven packed original textures; studio lights/camera and ground. Right-hand
  wheels reproduce the180deg native display rotation omitted from the BOH.
- `assets/Peugeot405-Authoring.blend`: named editable construction collections,
  retained reference surfaces, bevel modifiers and separate details. Geometry
  is authored in metres; the source reference is not all the assembled scenes.
- `assets/build-receipt.json`: original input inventory, source-envelope samples,
  output hashes, measured deck contacts and authored component names.

Blender's Principled materials approximate the Juiced shaders. Actual native
CarView captures in `out/validation/peugeot405-rework-20261101/gallery/index.html`
are the authority for game appearance. The pale showroom floor reflections
remain a property of the existing render, despite66 low source undertray faces
being assigned a dark material. No performance claim follows from offline frames.

## Rebuild

Use an untouched r17 Wave2 input, retained in the verification rollback. All
outputs must be fresh paths. Running the author on its own r18 output is invalid.

```sh
out/blender-car-tools/bin/blender --background --python-exit-code 1 \
  --python tools/peugeot405-rework/build.py -- "$PWD" /path/to/r17-wave2 /fresh/study
out/blender-car-tools/bin/blender --background --python-exit-code 1 \
  --python tools/peugeot405-rework/studio.py -- "$PWD" /fresh/study/street-cars-wave2 /fresh/studio
```

`build_preview.py` compiles a private native review harness against the existing
Linux libraries. It does not relink the production executable. `capture.py`
assembles the candidate with the other installed packages in a private support
folder and captures six configurations in eight directions, including red/silver
paints and two original vinyls. The stock carbon index1 is also covered by the
baseline native checker. `finish_studio.py` adds the Blender studio floor and
renders a verification image. `present.py` freezes sources and builds the gallery.

`prepare_delivery.py` and `install_linux.py` are checkpoint-specific scripts:
they deliberately assert the r17 revision/input hashes, require fresh output
paths and retain exact rollback directories. They are not generic mod installers.
No Android device was accessed during this rework. The runtime ZIP can update
Wave2 through Add mods from ZIP while retaining other installed packages/saves.

## Native adapter

Native D8M centimetres use X across, Y up and+Z forward. Blender uses metres,
X across, Z up and-Y forward. The bumper envelopes are smoothly fitted from
the actual stock exterior; raw per-height ray hits were rejected because they
followed inner geometry at pre-existing openings. New intake walls/grilles have
real depth. Original side-facing vinyl receivers, upper seams, lamps, trim and
arch returns are retained. The two new hoods subtract rectangular ports from
the original source triangles, then add inset wells/walls and seven louvers per
port. Wing shoes sample the actual rear deck; the foil is behind the rear glass.

Sixty-six original low, horizontal undertray faces are split by material only;
positions, original per-vertex attributes and total source triangle count remain
intact. New part materials reuse the car's native body-paint material where
appropriate. Main and UI payloads are identical for every changed mesh.
