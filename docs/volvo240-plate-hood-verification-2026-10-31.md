# Volvo 240 plate and forward hood — 2026-10-31

Linux now uses **Car Visual Fixes r8 / Wave2 revision7**. The normal launcher
still uses the same production ELF; Android APK24 and the RP5's APK19/packr5
remain unchanged. No device access, public upload or personal-save edit occurred.

Two actual import defects were corrected in the retained source model:

* Its original `plate_letters` material is a double-sided glTF MASK with cutoff
  0.7741707563400269. The previous Coloured Spec ABlend program did not implement
  that double-sided property, hiding the inward-facing source letter planes.
  The importer now uses native Specular ATest with the source cutoff applied to
  alpha. Actual native rendering restores **ROBIN MIK**, the source author's
  lettering, on both plates. No lettering or plate geometry was invented.
* The old bonnet polygon stopped at source z2.08m, before the existing upper
  hood's forward edge at approximately2.284m. Extending its forward boundary
  assigns that existing surface to BONNET, so buying carbon covers the front
  band too. The former side/rear boundary is retained. This does not establish
  that the estimated rear seam is an exact OEM CAD boundary.

The earlier close review called blank plates a source-art limitation. For this
Volvo that was incorrect: source inspection established a native material/culling
error. That finding and the previously open front-band question are resolved
here; historical checkpoints remain immutable.

## Geometry, native graphics and scope

Evidence lives in `out/validation/linux-current-20261029/volvo240-source-study`;
open its **gallery.html** for before/after and actual workshop/reload images.

The independent native D8M reader compares old versus new CHASS + BONNET + each
of four registered front bumpers. All oriented source surfaces, materials,
interpolated UVs/normals and constant vertex colours are preserved within the
recorded serialization tolerances. All three relevant stock slot transforms are identity;
BOH bytes are unchanged. No degenerate faces were excluded. Maximum residual
triangle coverage error is0.000106454cm²; maximum position error about0.0000101cm.
Six in-memory corruptions of geometry/normal/UV/winding/face/colour are rejected.
This is surface preservation, not vehicle physics or OEM geometry certification.

One retail decal also retessellates: custom slot22, `w262_DECAL_CREW_VS7`.
The body proof's exact coplanar/normal tolerance does **not** apply successfully
to this0.08cm normal-offset overlay; its failed strict checks are retained.
A separately labelled offset-envelope analysis measures both-direction area
coverage, UVs and attribute drift: maximum position0.00031731cm, normal-component
0.00302385, UV0.000001149, per-triangle area error0.000139923cm²; no excluded
faces. This is bounded equivalence, not exact overlay identity. Five deliberate decal corruptions are rejected. Its dedicated
native before/after sweep covers22 views per version, stock and kit3/carbon,
with eight yaws and three close details per state. Both comparison sheets were
inspected; the side artwork remains continuous across the retained body.

Other native evidence:55 material-prototype views,165 final candidate views,
165 same-order baseline views, and55 extracted-ZIP views. All110 unchanged
Focus/Jetta views are RGB-identical in the correctly controlled comparison;
all55 extracted Volvo views match the reviewed candidate. An earlier comparison
with different car load order changed the parked steering pose and was excluded,
not reported as a rendering regression. Production CarView RNG was not changed.
All relevant capture processes exited0 on the Radeon RX7800XT. These offline
readbacks are not an Android performance measurement.

## Package and ordinary Career purchase

The runtime-only r8 ZIP has3002 payloads, all read back and independently
extracted/hash checked. Native package checks: **1238 passed,0 failed**.
Only22 Volvo assets plus README/revision/SHA manifest changed in Wave2; the other
2016 declared payloads remain identical. Two pre-existing lens codec differences
from source regeneration were identified with the BEFORE importer replay and
explicitly preserved from the accepted package. Handling, roster, other cars,
headlamp textures and recipes are unchanged. The new general MASK exporter also
covers future imports; earlier Street Cars MASK materials were inventoried but
those runtime cars were not regenerated or certified here.

A controlled one-car Career fixture uses ordinary title/profile-load/workshop UI,
not direct save modification for purchases. Bought front bumper kit3 for$3500
and carbon bonnet for$2500. Actual autosave and a separate fresh process preserve
both purchases, **153.04620361328125 BHP**, and **$994000**. The full reloaded
archive is byte-identical to the bought archive:

`a32bc4afe3b7600e715052980afd7fd2701cb1da85f3391f1c0480e6517a7bb6`

Independent archive/hash parsing finds exactly nine changed CMS byte values:
cash, total/car spending, front-bumper index and bonnet index. Every other
career byte and archive entry is identical; eight save negative controls reject
corruption. Two ordinary UI processes exit0. A first load-button input was
ignored during a transition; the same process was observed and retried, with
no duplicate game launch. This synthetic fixture proves persistence, not earned
unlocks or personal-profile migration. The6 personal Linux save files retain
their previous hashes. The previous installed r7 package and25-file repo delta
remain available for rollback.

## Storage recovery and reproducibility

An ENOSPC write truncated an owned verification helper; production assets,
importer and saved receipts stayed intact, and installation had not started.
The helper was restored from its exact earlier AddFile instruction, parsed as
JSON without evaluating transcript code; syntax, four source-surface proofs and
later post-install old/new replay all pass. The failure/recovery receipt is kept.

973 redundant BMPs were removed only after exact RGB equality with retained
PNGs and both hashes were recorded: about4.03GB allocated duplicate containers.
Original BMP headers/unused alpha are not claimed to be preserved. Reproducible
owned ZIP extraction/cache were removed after payload/image parity checks;
ZIPs, source scenes, receipts, PNGs and rollback assets remain. No emulator,
personal profile or unrelated project data was deleted.

Runtime ZIP: `out/releases/OpenJuice-Car-Visual-Fixes-r8.zip` (186852935 bytes).
SHA-256: `eeeeddf4011391ca740dcfcdfeb51a61fc6869561d54cf513dad0e1fb99f68f2`.
The checkpoint links to CP26 and retains source updates, prepared input copies,
verification helpers, native captures and synthetic purchase archives. A fresh
regeneration reproduces all216 diagnostic native output files byte-for-byte;
`replay_native.py` records the command and removes only its verified duplicate.
Whole-game verification remains active: original119-D8M integration failure,
later campaign/routes, physical Android, network conditions and final
source/license/publication closure are not certified by this car fix.
