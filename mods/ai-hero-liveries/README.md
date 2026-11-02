# AI Hero Liveries — all installed cars (revision 2)

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

The five graphics are registered on all **63 playable cars** in the current
installed catalogue: the 52 originals, Ferrari, Street Cars and Street Cars
Wave 2, including Peugeot 405. Mod-added cars reuse their existing fitted
native vinyl receiver meshes. The builder reads the assembled catalogue;
it fails explicitly if a requested car lacks a matching receiver.
Every car receives the same five graphics in the same order.
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
  --overlay /path/to/assembled-active-mods \
  --output out/vinyl-research/ai-heroes-rebuild --cars ALL
```

Add `--reference` for research builds that also include the unchanged native
texture/material on identical geometry as a visual control. Do not ship those
control entries. The output starts disabled and never modifies the game or
saves. Omitting `--overlay` builds for the original 52 cars only. Use the
native `VinylPackTest` harness and `check_ai_geometry.py` before packaging.

## Installation and limits

This directory holds both editable source and the ready-to-load `mod.ini`
and `assets/`. The Linux installed copy is enabled; restart the game to load
the updated catalogue. The importable ZIP is built separately.
Disable any Underground/F&F or Universal Side Motifs package before enabling
this pack: each supplies its own `scripts/mod_vinyls.txt` and they cannot be
enabled together. None of their previous graphics are in the new ZIP.

This is a preview, not an A-tier certification for every combination. The
runtime harness exercises all **315 combinations** through garage/race model
loading, workshop menu selection and isolated save round trips. All **630**
UI/race receiver meshes were verified byte-for-byte against their recorded
donors, except the material identifier. New Linux captures cover all five
designs on both sides of the 405, plus representative Civic and Volvo V70
views. Render coverage is not the same as manual review of every image.
Initial manual review covers
the five designs on the Civic, the Rachel comparison on Civic/350Z/Supra/RX-7,
and the Jetta donor comparison. Further sample reviews are in the checkpoint.
Real Android hardware, body-kit combinations and crash deformation remain
unverified for this edition.

Added-car entries use `VinylIfPresent`, allowing the corresponding car packs
to be disabled without blocking startup. The Linux executable built on
2026-11-02 understands this keyword. Older executables can still load the
52 original-car entries but require rebuilding for the added-car entries;
this update has not been deployed to Android or macOS. Future car packs
require rebuilding the assets against their assembled catalogue.

No user profiles are included or rewritten. The game stores vinyl selection
as a numeric index. The original entries are preserved and these five are
appended, so existing retail/Ferrari vinyl selections keep their indices.
Replacing a different imported vinyl pack can still change an already fitted
graphic. No save compatibility block or separate save namespace is introduced.

OpenJuice upstream: https://gitlab.com/sidecr3w/openjuice
Juiced's original artwork and geometry belong to their respective owners;
these native resources are used as references and geometry donors, not claimed
as original AI-generated meshes.
