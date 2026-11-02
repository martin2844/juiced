# Complete P2P traversal matrix — 2026-10-31

**All 18 point-to-point routes now complete through the normal menu**, with
six distinct finishers/places, settled results, Continue back to the custom
performance-group menu and normal exit. One prior Angel North forward pilot
is retained by receipt hash; this sweep adds 17 unique routes. No cases are
active, pending or failing under the explicitly reviewed factory-absence policy
below. This establishes these P2P workflows, not full-game completion.

## Actual execution

Evidence: `out/validation/linux-current-20261029/p2p-sweep-r14/` and the
corresponding `full-race-ui/p2p-sweep-r14-dXX-rYY/` case directories.
The development ELF remains
`556cc99f3fd2286da1bb11e1837e5eb1eca96dffa9f7ca5abf2e17c190456b3f`.
There is no `--test`, `--track`, fixed clock, production rebuild or changed race
logic in these runs. The player selects the Punto in group G, declines automatic
modification, chooses Race, the actual district/P2P route, then ordinary setup
defaults. Opponent models vary through normal custom-race selection; they are
not all assumed to be Puntos. Six car IDs and places are checked in each race.

The native profile loader loads a copy of the existing owned route-unlock
fixture; all other fields/entries and its original seed remain unchanged.
This is not a natural campaign/unlock progression claim. Three disjoint queues
reuse completed cases and never silently restart a partial directory. Each
game uses its own support directory, SDL virtual pad and private Xvfb/llvmpipe
display. LP_NUM_THREADS=4 limits this software-rendering test only. Dummy audio
does not certify listening; these runs do not measure hardware FPS, human input
or physical Android performance. The usual development launcher is unchanged.

`prepare.py` independently reads all 18 original TDF/SPL references and their
hashes, TDF route-menu order, six grid points, POINT2POINT last-point metadata
and every spline's point count. Each new game traverses title/profile/custom
menus, captures the selected route, starts the requested actual scene and Go,
captures driving, waits for all six finishes/results, waits another 6.5 wall
seconds on the normal-clock path, captures settled results, selects Continue,
captures the returned menu and quits normally. Every press and release and
capture/quit acknowledgement remains in the original logs.

## Factory-absence review, with the original stop preserved

The initial strict scheduler stopped at Campbell reversed when an AI MX-5
requested MX5_SKIRT_L/R. Two already-running cases finished and also recorded
the MX-5 warnings; East Angel reversed additionally recorded CORSA_SPOILER.
The warnings occurred during opponent loading before Go, not at menu return.
All three games actually had six finishers, results, return and exit 0, but
their original strict `result.json` rejections, queue exits and STOP.json are
retained unchanged. No failed trace was replaced with a clean rerun.

A fresh independent `tools/audit_stock_body.py` run exactly matches the previous
retail audit: 54 car blocks, 610 factory references, 535 found, 75 absent, all
75 absent references pointing to original BOH slots named NONE. This is separate
from the 119 rejected D8M files: no bytes are being accepted by a guessed parser.
`review_stock_absences.py` rereads original cardefs/BOH hashes and slots and
rechecks missing meshes in all native fallback folders. It qualifies only exact
standard-part names absent in **both** race/UI variants with both original
slots NONE and no direct or packed active-package overrides. That yields 37
eligible diagnostic strings; six occur across the completed matrix (MX-5
skirts, Corsa spoiler, Beetle skirts/spoiler). All remain visible in the logs.

`reviewed-cases.json` binds the three original rejections and log hashes to this
explicit evidence. A separate `run_after_review.py` resumes the remaining 11
new races; separate phase start/terminal receipts and STOP marker preserve the
initial stop. No renderer, car geometry, warning output, parser or original
archive changes were made. This classification says the named files are absent
in the supplied original data; it does not assert Windows art equivalence or
close the strict EveryModel inventory failure.

## Independent verification and visual limits

`verify_after_review.py --all` passes: 18 total / 17 new, no failed/active/pending
cases, no second-phase STOP, three second-phase queues terminal 0. It rereads
the actual route/ordered normal-menu/Go/results/return logs, all six finish
events and waypoint bounds, game/wrapper/display terminals, probe/executable
identities, original references and seeded ModSaves hashes. Five BMP/PNG pairs
per new case have exact RGB equality and successful native capture acknowledgements.
Six copied-log negative variants reject wrong route, missing Go/finisher/results/
return and a new warning for a mesh that actually exists (MX5_BONNET). The last
control ensures the documented absences do not become a general warning bypass.

All 17 selected-menu/driving/settled-results triplets were inspected: 51 sampled
views, bound to source PNGs and eight contact pages in
`visual-review-final-receipt.json`. The selected P2P labels/maps, player/road/HUD
and six settled result positions/times/names are visible. Other start/return
snapshots have file/pixel/transition checks, not blanket manual visual approval.
Some driving samples include skids, damage or fence contact/sparks. This does
not certify collision recovery, every viewpoint/menu/animation, handling,
material/lighting/damage/HUD parity with Windows, audible mixing or Android.
The native HUD shows LAPS 1/1; no original-runtime HUD comparison is claimed.

All original and second-phase runner handles are terminal; no games/displays
from this matrix remain active. Handles and exact PIDs are in `sessions.json`
and queue/case receipts. All 908 critical production inputs, six personal Linux
save/creative files, installed Wave2 and the development executable remain
unchanged. RP5 was not accessed. No public upload/commit or APK/asset update.

The 72-circuit matrix remains separately verified. Sprint/cruise route breadth,
later campaign, remaining reference/art/audio/network/distribution work, the
strict original 119-D8M gate and physical Android requirements remain open.

## Disk reserve

With less than 1 GB free, removed only two superseded generated Nix runtime
export copies (r10/r11), reclaiming 639,401,916 bytes. Their exact source
archives, checksum sidecars, prior receipts/checkpoints and rooted compiled
Nix outputs remain; current r14 runtime/source/media and all APKs/car packs,
original assets, saves and validation captures/logs remain. No Nix store GC.
`superseded-export-cleanup-receipt.json` records exact before/after hashes,
paths and free space; the retirement is listed in the release directory.
