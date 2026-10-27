# AI Hero Liveries checkpoint — 2026-10-27

Replaces the small-sticker direction rejected by the user. Five new graphics
were generated with the built-in image tool using Juiced's original texture
sheet as a composition reference. Source PNGs, exact prompts and definitions
are in `mods/ai-hero-liveries/`. They interpret the Eclipse, Supra, RX-7 and
Jetta from The Fast and the Furious (2001), and only Rachel's 350Z from NFSU2.
These are new side graphics, not exact replicas or full hood/roof wraps.

The decisive change is preserving the full retail per-car mesh and UV data.
The prior small rectangle fitting and extracted NFS artwork are not used.
Four designs use `decal_fandf_vs1`; Jesse uses `decal_tribal_vs4` after the
first donor visibly truncated its composition. Native geometry, vertex data
and bounds are unchanged; material names and texture resources are new.
Near-transparent AI noise is ignored when measuring the artwork bounds so it
does not accidentally shrink the visible design. Thumbnail and texture use
the same artwork.

The first Rachel generation was rejected as thin and busy. The second uses a
larger coherent dragon silhouette. Both original and new graphics were rendered
on identical geometry for the Civic, 350Z, Supra and RX-7. Comparison:
`out/vinyl-research/ai-heroes-pilot2-shots/comparison.jpg`.

## Evidence

- Final assets: `out/vinyl-research/ai-heroes-v2/assets`.
- Final test: `out/vinyl-research/ai-heroes-v2-validation.log`, **260 PASS**
  combinations. Each loads both garage/race models with textures and geometry
  and round-trips the selected part through an isolated in-memory career.
- Captures: `out/vinyl-research/ai-heroes-v2-shots/`, **780 model views** plus
  52 workshop captures. Uses the actual renderer, not image-generation mockups.
- Gallery: `out/vinyl-research/ai-heroes-v2-gallery/index.html`, all combinations
  with both sides and an oblique view.
- Manual review: all five designs, both side views, on Civic, 350Z, Supra,
  RX-7, Eclipse, Golf, MX-5, NSX and Beetle: **45 combinations / 90 side views**.
  This is a sample, not a claim to have inspected all 780 model views.
- Package: `out/vinyl-research/ai-heroes-preview1/OpenJuice-AI-Hero-Liveries-preview1.zip`.
  Contains only the five AI designs; none of the old imported liveries.
  The unchanged retail references are excluded from this package.
- `seal_native.py` checks the expected car/mesh PASS pairs and completed test
  summary, writes file hashes and verifies ZIP integrity.

## Scope and limits

The work is a **reviewable visual preview**, not a declaration of final quality
for all cars. Large side coverage is now comparable to the selected retail
donors in the inspected cars. Paint colours remain independent. No hood/roof
panels, sponsors, wheels or complete replica cars are included. Dark graphics
need a contrasting paint colour just as fixed-colour native designs do.

Android hardware, crash deformation and body-kit combinations remain untested.
Preserving native data is not proof of visual compatibility under those states.
The package covers only the 52 original cars, not Ferrari or other mod-added
models. No production engine code was changed.

Nothing was installed on Android or published as a release during this turn.
No user saves were touched. Disable older vinyl packs when importing this one,
because they all supply `scripts/mod_vinyls.txt`. Their working files are retained
outside the new mod as research history; they are not mixed into the deliverable.
There are no old vinyl packages in the local NixOS launcher mod directory.

Build/review tools: `build_ai_heroes.py`, `gallery_ai_heroes.py`, existing
`android_vinyl_check`, and `seal_native.py`. Exact build instructions are in
the source directory's README. The old side-motif checkpoint remains historical
and must not be mistaken for the accepted artistic direction.
