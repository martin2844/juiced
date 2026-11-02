# First-wave body kits — checkpoint, 2026-10-31

Street Cars revision 5 is installed on Linux and included in the local importable
`out/releases/OpenJuice-Car-Visual-Fixes-r9.zip`. The change addresses a concrete
gap in the ten-car request: Integra, BMW and Levin had only one custom front
bumper, rear bumper and skirt choice. Each now has three custom choices plus
stock. The seven Wave2 cars already have three choices in these categories.
The overall ten-car acceptance remains open; see `ten-car-acceptance-2026-10-31.md`.

Subsequent same-day follow-up:
`first-wave-workshop-save-verification-2026-10-31.md` now supplies real workshop
purchase/autosave/fresh-load evidence for both appended indices on all three
cars. The earlier isolated checker below retains its own narrower scope.

## Assets and source preservation

Keep stock index 0 and sport index 1. Append painted valances at index 2 and
slimmer dark lips at index 3. These are restrained aero additions on complete
replacement native parts, not licensed aftermarket-kit replicas. Counts alone
do not certify their styling as equivalent to every retail Juiced body kit.

Blender 5.1.1 fits the additions to reviewed stock D8Ms in metres, Z up and
negative-Y forward. GLB export returns Y-up metre coordinates; the native
adapter multiplies by 100 without a handedness change. Original downloaded and
prepared models remain intact. The new editable `.blend` studies, six addon
GLBs and measured JSON reports are retained in
`mods/street-cars/tools/assets/body-kits-20261031`.

`build_body_kit_study.py` authors closed, bevelled solids; `add_body_kits.py`
validates actual exported topology, finite normals/positions, triangle area,
ground clearance and the stock exhaust opening. It appends original index and
vertex/material records byte-for-byte rather than regenerating the old parts.
The 48 new D8Ms cover two levels × four parts × three cars × garage/race.
Six new trim materials complete the runtime additions.

Every previous runtime asset and dynamics recipe is byte-identical. The only
changed existing files are README, manifest, revision metadata and the appended
body catalogue. No stock/sport mesh, carbon hood, vinyl, tyre, BOH, sound or
physics setting changed. The engine executable also remains identical.

## Verification and rejected candidates

Evidence root: `out/validation/linux-current-20261029/ten-car-closure`.

- Real revision-4 baseline: **377 passes, nine failures**, exit 1, precisely
  the missing category/car combinations. Baseline runtime is preserved.
- Final candidate and extracted r9 ZIP: **482 passes, zero failures**, exit 0.
  Includes garage/race loading and materials, stock body preservation under
  carbon/no-spoiler refits, all 31 vinyl fits, and real career archive codec
  round trips for old/new kit indices 0–3 and unlock levels. Synthetic test
  archives do not replace the user's saves or prove a new workshop UI purchase.
- Wave2 regression from extracted ZIP and the actual installed composition:
  **1,238 passes, zero failures**, exit 0, full 64-car roster.
- Final actual Radeon CarView captures: **96 selector views** (eight yaws × four
  states × three cars) and **48 close cardinal views**. All six final contact
  sheets were visually inspected. Carbon and vinyl 8 are present in every kit
  state; original stock is separately shown. RGB PNGs were compared pixel-for-
  pixel with their captured BMPs; hashes and exact indices are retained.

Final galleries: `ten-car-closure/gallery-05/index.html` and
`ten-car-closure/close-clear-05/index.html`. Close review hides the selector's
2D overlay so it cannot conceal the lower bumpers; it uses the same real CarView,
materials, lighting and depth testing. Radius/height/target are 800/245/70 cm
for normal views and 520/160/70 cm for close views; resolution 1280×960.

Earlier studies/candidates are retained as rejected iterations: an uneven rear
lower edge, front roots following inset grilles, a rear valance obscuring stock
exhaust tips, and insufficient clearance for low front plates. Final additions
use continuous lower edges, front paint-surface roots, a visible front edge
below the original front envelope and rear openings measured from actual
exhaust geometry. Early close captures with UI occlusion are not final proof.

## Delivery and rollback

ZIP r9: **189,789,044 bytes**, **3,056 payloads**; SHA256
`cefca7888da6e786d957b9bccbcaca647627f9a07df9794eb3018da59d735219`.
CRC, every archived payload, extracted package manifests and native checks were
verified before atomic installation of Street Cars revision 5 on Linux.
Ferrari revision 4 and Wave2 revision 7 are unchanged. Local and durable r4
rollback paths, package hashes and all readbacks are in
`ten-car-closure/r9-delivery-receipt.json`.

All six personal Linux files are unchanged. The normal launcher still selects
the same native executable and project mod defaults. No APK change, RP5 access,
device installation, public release or Git push was performed here.

This evidence covers the new visual options on Linux. It does not certify all
31 vinyl × kit permutations visually, real-world handling, physical Android
rendering/performance, or the overall finish of the other seven source models.
