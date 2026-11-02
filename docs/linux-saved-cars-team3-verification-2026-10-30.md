# Linux saved-car and three-on-three verification — 2026-10-30

This extends the replay/pink-slip checkpoint. It adds runtime and visual evidence;
it does not change the production executable, car pack or Android APK. The user
is using the RP5 and explicitly requested Linux work. No further phone control,
installation or export was performed during these Linux checks.

Evidence root: `out/validation/linux-current-20261029/full-race-ui`.

## Ronnie's actual saved configurations

The pre-update device backup was copied into an isolated Linux support directory.
The complete backup remains intact. Only Player's files were omitted from this
owned fixture, to select Ronnie unambiguously. No original profile was edited,
restored or seeded. The fixture archive retains SHA-256
`2a504f82385f8dc893d316d5669f77da46658cdc18b147417c8c2acefc08553a`.

Normal title/profile/career navigation loaded Ronnie with **35 cars and
$1,130,449**. The current development executable is
`16813fa758afb0aa0c54d63b5e2ed2c227fd384b4d26e95d92c8b191ab001556`.
The process and its private display exited successfully. Loading did not change
the owned archive. The original full backup also retains its recorded hash.

An out-only native CarView gallery reads this exact archive through the native
save reader and retains its selected colors, parts, upgrades and damage. It does
not replace saved cars with default builds. Twelve saved imported-car
configurations (including both Ferraris) were rendered from eight fixed angles:
**96 images**, all manually inspected via twelve contact sheets. The actual
renderer log identifies the **Radeon RX 7800 XT**. PNG conversion preserves every
RGB pixel of the corresponding BMP. This diagnostic has its own recorded binary;
it is not claimed to be the packaged game's executable.

The gallery uses the nine active packages in the installed Linux collection.
The RP5 collection has eleven. This check therefore does not certify the extra
AI liveries or claim identical whole-device mod state. Installed mod inputs were
read-only symlinks; the generated overlay was confined to the owned fixture.

Observed saved settings and limits:

- Both Ferraris use carbon bonnet 1, kit 1, retail decals 30/33 and wheels 30/41.
  The reported gross bonnet distortion was not reproduced in these views.
- The 406's saved wheel upgrade 105 was inspected on both sides; no obvious rear
  hub displacement or wheel-envelope shrink was reproduced.
- The V70's rear wheels remain centered in the arches in its saved stock setup.
- Outback body/cladding and plate placement are coherent in these views; source
  surface detail remains limited. The Jetta's saved parts are coherent, and the
  240's front lamps look consistent in these views.
- The 405's glazing remains excessively dark/opaque. This is a visible finish
  limitation, not a closed retail-quality art requirement.
- Most other new cars in this save have bonnet 0/255 and decal 255. Those images
  do not certify every carbon/vinyl/kit/damage combination. Earlier r5 sweeps
  provide separate evidence for broader combinations.

The harness does not initialize menu button-icon textures. White control blocks
are excluded from launcher/menu UI claims. Its measured 241–960 ms model loads
are diagnostic Radeon timings, not an Android dealer benchmark or FPS result.

Gallery: `ronnie-profile-gallery/initial-nine-pack-radeon/index.html`.
Receipts: `ronnie-profile-gallery/verification-receipt.json`,
`saved-gallery-image-receipt.json`, `visual-review.json`.
`verify_ronnie_gallery.py` checks metadata, selected parts, all 96 pairs of image
hashes/pixels, renderer identity and archive preservation. Five negative controls
reject missing/duplicated angles or files, foreign models and wrong part IDs.
Human visual judgment is not inferred from these metadata checks.

## Six-car three-on-three career race

`team3-fixture` uses an exclusively owned synthetic career fixture: native
recruitment of Vito and Amber, the existing Eclipse/GTR, and a factory-rated 350Z
created/rated/padded using normal game operations. An eligible one-lap circuit
on Engstrom_R1 is staged on January 2, afternoon/dry. This proves this configured
workflow, not natural recruitment or a complete campaign.

The actual installed Nix r3 ELF is
`97778946dba76584115a7c1eb5180d9ce52ea484c7118cadedb2311ef48f5d25`,
bound to the live process through `/proc/<pid>/exe`. The game runs through the
normal title, profile, calendar, driver/car selection, grid, race, results,
meeting summary, respect changes, autosave and garage. Diagnostic autopilot
drives only the human Eclipse; both teammates and all opponents use ordinary AI.

Actual D-pad inputs change the two teammates independently:
Up/Down changes Vito and Right/Left changes Amber. The HUD was manually read at
baseline Medium/Medium, High/Medium, High/High, Medium/High, Medium/Medium,
Low/Medium, Low/Low, Medium/Low and Medium/Medium. Three frames per step preserve
flashing labels; the Amber-Medium case uses frame 1 because frames 0/2 are blank.
Keyboard glyphs remain visible in this diagnostic; no physical pad-glyph claim.

All six cars finish. Opponent team time is **63.28 s**, player team time
**67.53 s**, determined by each team's slowest member. The player crew finishes
second and receives $1,000: **$40,000 → $41,000**. Both Vito and Amber record one
race and zero wins. Native learning raises Vito ability/recovery to 42.5/72.5
and Amber to 52/42. The event is marked done, all three garage models remain,
and eleven added-car progress entries are preserved.

A separate Nix r3 process loads the completed archive and shows both named
teammates with one race, zero wins and $41,000. Loading leaves the archive
byte-identical. Final SHA-256:
`97633fd31e904791c22a898a86ca4e1db804398bd794521100ae18efa46fea0d`.
Both game and private-display processes exit successfully.

Diagnostic failures are retained and excluded from passing claims. The first
selection script assumed an incorrect crew neighbor, then a subsequent grid
input entered a side bet. Recovery continued in the same game; declining that
bet incurred its normal respect decrease. This is not claimed as a clean
no-betting path. The first reload's Amber-named capture actually selects Player;
it is excluded. A second reload selects and visually verifies Amber correctly.

`verify_team3_runtime.py` independently checks serialized archive/hash/bounds,
six entrants/finishers, ordered workflow, prize, both teammates, preserved
progress, process exits and separate reload. Native fixture inspection agrees.
Eighteen negative controls reject missing phases, false money/member/progress
fields, unrecorded teammate races and a corrupted archive payload.

These private Xvfb/llvmpipe, dummy-audio and virtual-pad runs do not certify RP5
rendering, performance, audible output or physical input. No production source
or asset changes were needed for these cases. The strict original 119-D8M
inventory failure and remaining global requirements remain open.
