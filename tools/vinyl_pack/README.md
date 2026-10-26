# Underground vinyl import — refined collection v2

This tool imports the recoverable artwork from three downloaded packs into an
optional native OpenJuice asset package. It does not edit original DAT files,
installed mods or saves. It requires Python, NumPy and Pillow, plus the original
Juiced data for the target car surfaces.

Sources:

- [ND4SPDWorld Underground vinyl collection](https://www.nd4spdworld.com/blog/index.php/feat/carpack-3/).
  Individual artists retain authorship; preserve the readmes inside each archive.
- [Haris Ahmad's NFS Underground 2 collection](https://www.nfs-cars.com/need-for-speed-underground-2/6/files/view/11449/).
  This is an aggregation; individual nested readmes identify additional creators.
- [AJ_Lethal's vinyl pack](https://www.nfsaddons.com/downloads/nfsug2/tools/2998/vinyl-pack.html).
- UV zone reference: [CiPH3R-88's NFS BlackBox Blender Tools](https://github.com/CiPH3R-88/NFS-BlackBox-Blender-Tools),
  with orientation and Underground 1 offsets calibrated against native captures.

## Reproduce

Extract the three packs below a working directory as `nd4/`, `fnf/`, `aj/`.
Use `unrar` for RAR files: libarchive reports CRC errors on some of these files,
and p7zip can incorrectly identify embedded ZIP data as the outer archive.
Never execute the old installers/EXEs included in the downloads.

```sh
python3 tools/vinyl_pack/inventory.py WORK/nd4 --unrar /path/to/unrar
python3 tools/vinyl_pack/inventory.py WORK/fnf --unrar /path/to/unrar
python3 tools/vinyl_pack/inventory.py WORK/aj --unrar /path/to/unrar
python3 tools/vinyl_pack/catalogue.py WORK WORK/decoded
python3 tools/vinyl_pack/refine.py WORK/decoded WORK/refined
python3 tools/vinyl_pack/build.py --game /path/to/Juiced \
  --decoded WORK/refined --output WORK/fresh-package
python3 tools/vinyl_pack/check_refinement.py WORK/decoded WORK/refined WORK/fresh-package
DISPLAY=:0 out/android-host/android_vinyl_check /path/to/Juiced \
  WORK/fresh-package/assets WORK/captures
python3 tools/vinyl_pack/thumbnails.py WORK/fresh-package WORK/captures
```

The builder requires a fresh output directory. `catalogue.json` records every
included source file, deduplicated copies, source hash, target car, mask and baked
palette. The decoded catalogue also lists exclusions. A count of texture layers
is **not** a count of complete original liveries. Version 2 composes 19 source
layers into eight complete designs using `curation.json`, leaving independent
hood, stripe, badge and alternative designs selectable. There are 252 recovered
art layers in 241 options; the numeric `2.dds` from Skyline v2 is an opacity mask,
not an extra design. The conservation check rejects missing or duplicated art.

The separate NFS mask supplies opacity for tint-channel maps: valid artwork in
these packs can have zero colour-DDS alpha. Full-colour graphics can instead use
overlapping source alpha to cut out their artwork; the importer distinguishes
these cases heuristically. Rectangular sponsor
logos receive a separate two-sided placement. Predominantly RGB-channel artwork
gets a documented baked palette; hand-reviewed overrides cover the F&F and rally
designs in `curation.json`. These choices are adaptations, not recovered NFS player
colour selections. The source readme specifies primary HUES including lighter and
darker shades, rather than only saturated RGB bytes. Full-colour artwork is retained.

Partial side graphics are trimmed and fitted with a shared aspect-preserving
scale and anchor. Both physical sides use the same coordinate bounds. Broad body
wraps retain their source layout; their contours cannot be inferred from opacity
alone. Panda Trueno has an explicit layout for its lower trim, black bonnet and
original one-sided Fujiwara lettering, avoiding black text on black trim.

Textures retain their original useful resolution (normally 512px), with a full
filtered mip chain and clamped sampling. Transparent UV regions are conservatively
culled from the decal meshes. Workshop thumbnails use real native car captures,
choosing a bonnet view when the artwork would be invisible from the side.

The meshes follow each original car's stock paint surfaces in both garage and
race variants, including the BOH transforms of bonnet, bumpers and skirts. They
do not include windows, lights or wheels. Normal-based projection is approximate:
custom body kits, damage deformation and recolouring are not validated. Some
full-body wraps retain source-car contour differences. This is an adaptation to
stock Juiced bodies, not a claim of pixel-identical NFS reproduction.

Unknown source cars are explicitly assigned to the Golf. 240SX artwork is
adapted to the 300ZX, and Tiburon artwork to the Celica. These are art transfers;
the package does not add those missing cars. The catalogue records these choices.

## Native extension and saves

The package adds `scripts/mod_vinyls.txt` lines:

```text
Vinyl SUPRA DECAL_NFS_V0123456789 "PACK: human-readable name"
```

The host appends options after retail decals, validates target cars and mesh names,
rejects duplicates/overflow, and reserves a slot for Custom Logo. Workshop entries
have short captions and full help text. Existing public binaries without this
extension do not load these extra options.

Android .10 and newer share careers across all mod configurations, including the
original-game mode. Earlier builds separated catalogue save variants. Legacy
careers are recovered by copying without overwriting originals. There is no
catalogue compatibility gate or index remapping: adding/removing vinyls may change
saved fitted decals, including Custom Logo. The collection is packaged disabled.

## Validation

`android_vinyl_check GAME ASSETS [CAPTURE_DIRECTORY] [MESH_NAME]` loads both native
car variants, requires a decal mesh/material/texture, checks a career round trip,
checks workshop menu membership and can capture three angles plus each car's
workshop selector. It runs on the desktop Android renderer; this is not a device
performance or gameplay certification. Use contact sheets for visual inspection:

```sh
python3 tools/vinyl_pack/contact_sheet.py WORK/captures \
  --pattern '*_0.bmp' --out WORK/review.png
```

Keep imported art and original car geometry in local output artifacts. Attribution
is not a replacement for the individual authors' redistribution terms.

## Restart after visual rejection (2026-10-26)

The v2/v3 bulk projection workflow above is historical, not the acceptance path
for the next release. Follow [the native-first investigation and process](../../docs/vinyls-native-first-2026-10-26.md).
Retail uses shared transparent motifs/icons and car/style-specific decal meshes;
the old NFS atlas projection and screenshot thumbnails do not reproduce that.
Run `audit_retail.py --game GAME --package EXISTING_PACKAGE --output AUDIT` for a
read-only resource inventory and comparison. No installed assets or saves change.

The complete legacy conversion has now been checked against the new prerequisites:
241 reviewed, zero admitted (including all 107 previously published options).
See [the full-pack review](../../docs/vinyls-full-pack-native-review-2026-10-26.md)
and `native_review.json` for per-option exclusions. `curate.py` requires both the
native review and visual review; its old reproduction command deliberately stops
with no output until a matching native-first acceptance exists. Legacy A/B alone
must not produce another release. No replacement pack has been installed.
