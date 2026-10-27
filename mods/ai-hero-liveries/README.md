# AI Hero Liveries — visual preview

Five new AI-generated body-side graphics inspired by Brian's Eclipse and
Supra, Dom's RX-7 and Jesse's Jetta (The Fast and the Furious, 2001), and
Rachel's 350Z (NFS Underground 2). They are interpretations, not replicas.
No other NFSU2 cars or downloaded vinyls are part of this collection.

The artwork was generated with the built-in image generation tool using a
Juiced retail texture sheet as a composition reference. Exact prompts are
saved in `prompts.json` and `production-prompts.json`. Source PNGs with alpha
are under `art/`. Final selections and retail donor styles are in
`definition.json`.

Unlike the discarded small side-motif approach, these use the **complete
original per-car vinyl geometry and UV coordinates**, without shrinking them
into a small rectangle. They preserve native vertex data, bounds and damage
streams. The build only substitutes the material name and new texture. The
art is normalized to the retail 4:1 canvas, with near-transparent generation
noise ignored when measuring its visible bounds. Each has a transparent
256×64 workshop icon and a 1024×256 mipmapped texture.

The five graphics are registered on all 52 original cars in the same order.
Car paint remains independently selectable. Suggested paint: green for
Rachel/Eclipse, orange for Supra, red for RX-7, white for Jetta. Selecting a
graphic does not change paint, wheels, body kit or any other part.

## Building

Requires Pillow and NumPy, the original game data, and the native inventory
produced by `tools/vinyl_pack/audit_retail.py`.

```sh
python tools/vinyl_pack/build_ai_heroes.py \
  --game ../game-data \
  --inventory out/vinyl-native-audit/retail-inventory.json \
  --definition mods/ai-hero-liveries/definition.json \
  --output out/vinyl-research/ai-heroes-rebuild --cars ALL
```

Add `--reference` for research builds that also include the unchanged native
texture/material on identical geometry as a visual control. Do not ship those
control entries. The output starts disabled and never modifies the game or
saves. Use `android_vinyl_check` to test the output before packaging.

## Installation and limits

The importable ZIP is built separately; this directory holds editable source.
Disable any Underground/F&F or Universal Side Motifs package before enabling
this pack: each supplies its own `scripts/mod_vinyls.txt` and they cannot be
enabled together. None of their previous graphics are in the new ZIP.

This is a preview, not an A-tier certification for every combination. The
runtime harness exercises garage/race model loading and isolated save round
trips, and captures all 260 combinations from three angles. Render coverage
is not the same as manual review of every image. Initial manual review covers
the five designs on the Civic, the Rachel comparison on Civic/350Z/Supra/RX-7,
and the Jetta donor comparison. Further sample reviews are in the checkpoint.
Real Android hardware, body-kit combinations and crash deformation remain
unverified for this edition. Mod-added cars are not supported by this package.

No user profiles are included or rewritten. The game stores vinyl selection
as a numeric index, so replacing a catalogue can change an already fitted
graphic. No save compatibility block or separate save namespace is introduced.

OpenJuice upstream: https://gitlab.com/sidecr3w/openjuice
Juiced's original artwork and geometry belong to their respective owners;
these native resources are used as references and geometry donors, not claimed
as original AI-generated meshes.
