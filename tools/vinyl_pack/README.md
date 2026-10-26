# Underground vinyl import (development preview)

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
python3 tools/vinyl_pack/build.py --game /path/to/Juiced \
  --decoded WORK/decoded --output WORK/fresh-package
out/android-host/android_vinyl_check /path/to/Juiced WORK/fresh-package/assets
```

The builder requires a fresh output directory. `catalogue.json` records every
included source file, deduplicated copies, source hash, target car, mask and baked
palette. The decoded catalogue also lists exclusions. A count of texture layers
is **not** a count of complete original liveries: some designs require several
layers in NFS, and this preview exposes those layers separately.

The separate NFS mask supplies opacity for tint-channel maps: valid artwork in
these packs can have zero colour-DDS alpha. Full-colour graphics can instead use
overlapping source alpha to cut out their artwork; the importer distinguishes
these cases heuristically. Rectangular sponsor
logos receive a separate two-sided placement. Predominantly RGB-channel artwork
gets a documented baked palette; these choices are adaptations, not recovered
NFS player colour selections. Full-colour artwork is retained.

The meshes follow each original car's stock paint surfaces in both garage and
race variants, including the BOH transforms of bonnet, bumpers and skirts. They
do not include windows, lights or wheels. Normal-based projection is approximate:
custom body kits, damage deformation, fine seam alignment and recolouring are not
validated. Do not describe this preview as a finished faithful conversion.

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

An asset package changes the catalogue save variant. Enabling it may therefore
show a separate career list; disabling it restores the old combination. It does
not delete saves. Never copy an existing Custom Logo career into the new variant
without remapping that dynamically appended slot. The preview is packaged disabled
and is not installed onto the Retroid automatically.

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
