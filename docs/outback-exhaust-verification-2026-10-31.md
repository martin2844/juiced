# Outback exhaust and close car review — 2026-10-31

The Linux install now uses car bundle **r7**: Ferrari revision4, Street Cars
revision4, Wave2 revision6. The Outback's original tailpipe occupied its rear
bumper mesh while the native exhaust slot also loaded the WRX donor pipe.
Stock therefore showed both; buying another exhaust left the original pipe
permanently underneath it. The retained source tailpipe now occupies Juiced's
replaceable exhaust slot. No new shape, engine change or handling adjustment.

The RP5 remains APK19 / car bundle r5. The user is using it; this pass contains
no device access. Local Android APK24 and Linux Nix r10 remain unchanged.

## Source and native proof

`tools/street-cars-wave2/repair_outback_exhaust.py` opens the retained prepared
scene into a fresh output. It parents the existing284-polygon pipe under the
importer's existing `exhaust` role. All74 source mesh world-vertex, normal, UV,
polygon/material assignments and material-name hashes remain identical; the
original source blend hash remains unchanged. Export uses local Blender5.1.1.

Independent native reader `tools/vinyl_pack/mesh.py` compares the original rear
bumper with the new rear bumper plus BOH-transformed pipe. All four bumper kits
in both driving and garage models preserve triangle count, winding, normals,
UVs and materials. Maximum position error from float coordinate partitioning:
0.0000095367431640625cm; normals/UVs exact. All other BOH nodes remain identical.
Shared pipe material and texture are byte-identical to the source car's copies.
The unused sport rear-bumper mesh changes too; exact geometric equality above
is claimed only for the four registered kit variants.

Runtime delta:12 Outback BOH/rear-bumper files,3 new shared exhaust assets,
the existing Cardef append recipe's Outback `STANDARDEXHAUST`, package revision,
README and SHA manifest. The other2022 declared Wave2 payloads remain identical,
including calibrated dynamics, roster/IDs, wheels, hoods, all31 decals, collision
and shadow meshes. Provisional authoring physics were never copied.

## Actual renderer and package checks

Evidence workspace: `out/validation/linux-current-20261029/close-car-finish`.
Open `gallery.html` there for native screenshots and full-resolution PNG links.

- Same native `CarView`, selector viewport/lighting, Radeon RX7800XT:165 views
  before and165 after, comprising retail Focus, Jetta and Outback; five states
  per car and eight yaw views plus front/rear/hood detail. Separate24-view pilot.
- All110 Focus/Jetta candidate images are RGB-identical to baseline. Only35
  Outback views change; the other20 match. All15 baseline contact sheets and
  five Outback candidate sheets were inspected, plus full-size selected views.
- Separate stock/three distinct catalogue exhaust shapes × four bumper kits:
  16 states/64 images each before and after. Native slot/material/nonempty-mesh
  guards and restored content signatures pass; all four rear comparison sheets
  inspected. Stock has only the original pipe; upgrades replace it.
- Candidate native checker: **1238 passed,0 failed**. Delivered ZIP independently
  extracted and assembled; another1238 checks pass. All64 packaged exhaust
  images are RGB-identical to the reviewed candidate.
- RGB-preserving PNG conversions checked against every BMP before removing
  redundant owned BMPs. Original BMP hashes and conversion receipts retained.

Two harness mistakes are retained and excluded: the first exhaust probe assumed
mesh pointer identity across refits, although `CarModel::mesh` creates a new
mesh; the first normal-UI attempt omitted the controller command environment
variable. Neither is reported as a production defect or successful workflow.
The content guard's initial compile error is retained too.

## Delivery and preservation

`out/releases/OpenJuice-Car-Visual-Fixes-r7.zip`, **186853705 bytes**:

`1150284cf9fb29e73ec3182ec151355b0d9d8b3f5d413c5d606a0c6732b137a6`

All3002 runtime payloads read back and extracted hashes checked; authoring tools,
models, caches and saves excluded. Previous r6 ZIP retained unchanged. Linux
Wave2 directory exchanged atomically with the validated package; previous
directory retained for rollback. Only the explicit visual delta copied into
repository assets; unrelated authoring tools retained. All six personal save
hashes unchanged. Habitual launcher still uses the same development ELF.

## Ordinary workshop purchase and fresh reload

Two accepted ordinary development-ELF processes complete normal title/driver
load/Career/workshop navigation. An owned synthetic Outback profile starts with
1M funds and seeded unlocks. Its body/neon byte0 is a test configuration, not a
strict factory appearance or earned progression claim. A $90 exhaust quote is
declined using its selected **No**, requoted and confirmed **Yes**. Final cash
999910, native rating165.2228 ->171.8517 (UI165 ->171), exhaust catalogue ID0.
Normal workshop-exit autosave completes; the next process loads the purchased
part and reaches the actual exhaust menu with the same cash/rating.

Independent whole-archive comparison: exactly8 CMS byte values change, belonging
only to cash, total spent, car spent, exhaust ID and rating. Every unrelated CMS
byte and every other archive entry remain identical. Both accepted processes
exit0; fresh-process archive is byte-identical to the purchased archive:

`0797c1d2488a4a6b1ab1ceff499b2efaf003fcdd1bdfb9da1a4dfbd5c410bda3`

Evidence: `full-race-ui/outback-exhaust-fixture/verification-receipt.json` and
`outback-exhaust-ui-r7` / `outback-exhaust-reload-r7` under the same validation
root. A screenshot named `cancelled-stock-restored` is actually still the No
dialog: B does not confirm this modal. It is explicitly excluded as stock
restoration evidence. The next A confirms No; final exact $90 debit proves no
extra charge. Fresh reload's first menu capture is the stock-removal selector
after a rejected No removal quote, not the exhaust menu; the later
`reloaded-exhaust-menu.png` and runtime transition establish that state. No
refund, stock-removal purchase or other brands/levels are claimed.

Owned assembly caches and the verified redundant ZIP extraction were removed
after their jobs finished. ZIP, source scenes, rollback package, actual PNGs and
all readback/hash receipts remain. Logical cleanup sizes count hardlinks and
are not a physical-space claim.

This closes the demonstrated duplicate-pipe defect. It does not establish full
retail art parity, every vinyl/colour combination, damage/physics fidelity or
physical Android appearance/performance. The bounded workshop purchase/save
coverage above does not cover all variants. The broad verification goal remains
active; the original119-D8M inventory failure is unchanged.
