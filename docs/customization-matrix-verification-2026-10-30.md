# Imported-car customization matrix — 2026-10-30

Verified the installed Linux Wave 2 package in a fresh, exclusively owned mod
assembly. The seven cars are 405, 406 Coupé, Golf R32 Mk5, Jetta Mk5, Outback,
Volvo 240 and V70. No production code, runtime assets, saves or APK were changed.
The normal development ELF remains the r9 media-policy build; RP5 was not accessed.

The prior 774-view sweep covered all retail vinyls with stock kits. The prior
four-state gallery covered the maximum kit with vinyl 8. This follow-up crosses
the actual stock/three kit choices with both hood choices and no vinyl or retail
IDs 0, 8 and 30: **224 configurations, 616 actual Radeon CarView readbacks**.
Each state has both side views; no-vinyl states also have three front angles.
The manifest records actual bonnet/decal/front/rear/skirt indices. Each model
must have exactly four kit choices, two hood choices and 31 decal choices.

The diagnostic uses the actual car-selector viewport and normal CarView
load/refit/rendering, fixed camera radius 800 cm, elevation 245 cm, target 70 cm,
1280×960. It assembles from read-only installed package symlinks into an owned
cache. No physics runs, user profiles, normal purchase UI or save operations.
The diagnostic's source/binary/link commands and exact current game hash are
recorded. Offline rendering with vsync disabled is not an FPS benchmark.

All **21 contact sheets were inspected**: both hood matrices and the front
sheet for each car. In these views, refitting the three kits preserves body
panels, hood outlines and wheel placement; the sampled vinyls remain on the
side surfaces rather than crossing windows. The Jetta shows its restored lamps
without the previously protruding trunk liner. Both Volvo 240 lamps remain
visible; Outback plate artwork remains on its original material. Source model
detail and restrained kit styling still differ from original retail cars; this
is not a new claim of retail-quality art or exact OEM panel geometry.

Each of 616 BMP readbacks was converted to an RGB PNG and checked pixel for
pixel before removing the redundant BMP. The 336 vinyl/baseline image pairs
all change visibly (minimum 3,833 pixels over a 24-channel-value threshold).
That measurement catches absent art, not artistic quality; visual review is
separate. The three sampled designs do not prove all 31 designs on every kit.
Front/corner coverage applies to no-vinyl states; oblique vinyl, in-race damage,
normal workshop purchase/save/reload and physical Android remain separate work.

All manifest-bound files of installed Ferrari, Street Cars and Wave 2 were
hashed (262 / 697 / 2,037 entries). Copied runtime assets in the actual diagnostic
overlay were also checked before deleting only that owned assembly cache.
All six existing Linux profile files retain their prior hashes. Current APK
v23/car bundle r6 and the physical RP5 v19/r5 were not replaced.

Evidence and full-size gallery:

- `out/validation/linux-current-20261029/customization-matrix/gallery/index.html`
- `verification-receipt.json`, `image-receipt.json`, `difference-metrics.json`
- `build-receipt.json`, `terminal.json`, `owned-cache-cleanup.json`
- `CustomizationMatrix.cpp`, `build_and_run.py`, `build_gallery.py`, `verify.py`

Recheck the preserved current bindings with:

```sh
python3 out/validation/linux-current-20261029/customization-matrix/verify.py
```

Reproduction must use a new owned output directory because captures and logs
are deliberately retained. The general verification goal remains open; the
strict original 119-D8M inventory failure was not altered or exempted.

The later normal frontend workflow for the 406 is now separately verified:
actual kit/hood/wheel/vinyl purchases, cancellations, autosave and fresh-process
workshop reload, with an independent whole-save comparison. See
`workshop-save-reload-verification-2026-10-30.md`. This adds one actual UI workflow;
it does not extend the matrix to all workshop permutations or damage.
