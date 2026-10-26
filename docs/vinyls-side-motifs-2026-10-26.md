# Native-style side motif adaptation — 2026-10-26

## Deliverable and scope

The replacement is **40 standalone side motifs on all 52 original cars**, in
the same order, with transparent workshop thumbnails. It is not a conversion
of 241 complete NFS liveries. The 201 other source options are excluded from
this edition. No invented artwork or AI repainting was used.

The previous full-car projection pipeline was not adequate. NFS atlases carry
wheel cutouts, panel backgrounds, roof/hood islands and proportions belonging
to another car. Juiced's native system uses shared motifs with per-car meshes.
Simply accepting an attractive flat texture did not establish compatibility.

The new process extracts an independent lateral motif from each prepared
atlas, isolates the principal connected artwork, retains its proportions,
and fits it to a measured body region. It deliberately loses the rest of the
source livery. Text reads forwards on both sides. Material colours are fixed;
these are not claimed to support native recolouring.

## Geometry and thumbnails

Native meshes establish placement candidates. Both garage and race variants,
and both sides, constrain coverage. Candidate rectangles are scored by world
area rather than texture pixels. The painted chassis supplies the final
surface: copying native donor geometry alone left holes where that donor's
original alpha hid them. The result is clipped to the motif and offset from
the body, avoiding the windows entirely for this stock-body placement.

Specific overrides avoid handles, mouldings and the Peugeot fuel cap. The
Civic uses a small door region between its handle and moulding. This means
some motifs are smaller than their original NFS appearance. That is an
intentional limitation, not a full-wrap reconstruction.

Shared artwork is 1024×256 with mipmaps; each transparent workshop thumbnail
is 256×64 and shows that same motif, rather than a car screenshot. Each car
has separate garage and race meshes. The MR-S's MRS_DECAL alias is supported.
31 stock designs plus 40 imports and the custom-logo slot fit the existing
catalogue capacity without engine changes.

## Selection and evidence

All 241 old options were inventoried. The extraction review produced 59
candidates. Reviewing both sides of their actual Civic renders removed 19
whose details became small, fragmented or hard to distinguish. The remaining
40 are retained as side decals; no blanket A-tier/original-game-quality claim
is made. Exact decisions and provenance are in
`tools/vinyl_pack/native_art_selection.json`.

Local evidence under `out/vinyl-research/`:

- `native-art-review/`: extracted-art review sheets.
- `native-v1-civic-shots/`: 59 candidate designs, three views each, plus a
  workshop-menu capture. Both side views were visually reviewed.
- `native-v1-placement-shots/`: initial 52-car text-placement review.
- `universal-side-v1/placement-shots/`: final 52-car text-placement captures,
  including revised MX-5 and Peugeot 206 positions. Both sides were reviewed.
- `universal-side-v1/validation.txt`: **2,080 PASS** entries, garage and race
  geometry/material loading, isolated save-index round trips, and workshop
  membership check for the first imported item of each car.
- `universal-side-v1/placement-validation.txt`: **52 PASS** plus rendered views.
- `universal-side-v1/gallery/index.html`: retained designs, both Civic sides
  and transparent motif preview.

This is not visual inspection of all 2,080 combinations. It combines a
per-design Civic review with a per-car placement sample. Race *loading* is
tested; driving, crash deformation, replacement body kits, mod-added cars
and this edition on Android hardware are **not verified**.

## Reproduction

Use Python with Pillow, NumPy and SciPy. Source archives/prepared textures
remain local research inputs; source hashes and original credits accompany
the output. Paths below are relative to the repository.

```sh
python tools/vinyl_pack/native_adapt.py \
  --game ../game-data \
  --decoded out/vinyl-research/refined-v2-reviewed \
  --catalogue out/vinyl-research/v2-reviewed/catalogue.json \
  --output out/vinyl-research/rebuilt-side-motifs \
  --ids ALL --selection tools/vinyl_pack/native_art_selection.json --cars ALL

python tools/vinyl_pack/package_native.py \
  --candidate out/vinyl-research/rebuilt-side-motifs \
  --selection tools/vinyl_pack/native_art_selection.json \
  --captures out/vinyl-research/native-v1-civic-shots \
  --sources out/vinyl-research --output out/vinyl-research/rebuilt-package

out/android-host/android_vinyl_check ../game-data \
  out/vinyl-research/rebuilt-package/universal-side-motifs/assets \
  > out/vinyl-research/rebuilt-package/validation.txt 2>&1

python tools/vinyl_pack/seal_native.py \
  --package out/vinyl-research/rebuilt-package/universal-side-motifs \
  --validation out/vinyl-research/rebuilt-package/validation.txt \
  --output out/vinyl-research/rebuilt-package/OpenJuice-Universal-Side-Motifs-v1.zip
```

All output directories must be fresh. The seal step verifies every expected
car/mesh PASS pair and the completed test summary before producing a ZIP.
It does not substitute for visual review. Changed assets require new tests
and changed geometry/art requires new captures.

## Installation and saves

The mod is staged separately and starts disabled. Disable the old
Underground/F&F vinyl package before enabling this one: both supply
`scripts/mod_vinyls.txt`. They do not merge. No profiles were read or modified
for this work, and nothing was installed on the Retroid during this adaptation.
No compatibility gate or separate save namespace is introduced.

The game stores fitted vinyls by numeric index. A different catalogue can
therefore change the appearance of an already fitted vinyl; it does not
require deleting a save. A future stable-ID migration would be separate work.

OpenJuice upstream: <https://gitlab.com/sidecr3w/openjuice>.
Original source-pack attribution and individual readmes are preserved in
the package's `sources.json` and `original-credits/`.
