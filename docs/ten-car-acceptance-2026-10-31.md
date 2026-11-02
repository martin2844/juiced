# Ten-car acceptance — current evidence and remaining work, 2026-10-31

The task is ten new car ports, IDs 54–63; Ferrari 53 is a separate pre-existing
import with repair obligations. Native loading, a roster entry and option
counts are evidence of integration, not proof of the requested final finish.

Linux currently uses Street Cars revision8 / Wave2 revision21 / Ferrari4.
R23 replaces the approximate405 fascia with a full OEM perimeter fit and a
narrow original mounting band; stock geometry is unchanged. It also uses
genuinely opaque/double-sided kit interiors. See
`peugeot405-oem-fit-2026-11-01.md` and the before/after workshop gallery. It is
installed locally; an already-open r22 session must be reopened to use it.
Artistic acceptance remains with the user.
R22 follows the user's workshop screenshot with a405 fit/finish pass: tighter
wheel-arch returns, welded and surface-fitted mounting collars, corrected
normals, cleaned lower edges, closed intake interiors and textured fog glass.
It uses70 actual workshop captures and64 selector views, rather than relying
on distant selector views alone. See `peugeot405-fit-finish-2026-11-01.md`.
R21 corrects the user's r20 finding that405 bumper kits were sitting over the
factory shells. It separates fixed body/lights from removable factory bumpers
and rebuilds all six custom shells without inherited stock skins. The native
stock-to-kits-to-stock cycle restores eight pixel-identical views;112 final
captures and1,322 native loading/refit checks are recorded. See
`peugeot405-bumper-replacement-2026-11-01.md`. Other r20 assets and tuning remain.
The
r20 follow-up refines405 rear fascias and all three spoilers, closes the empty
upper grille cavity, and implements the user-approved model-specific boost
preparation up to400BHP/class5 while retaining factory102BHP. It includes a
native engine rebuild and cached-rating refresh for existing equipped saves;
see `peugeot405-refinement-2026-11-01.md`. Other cars keep original tuning.
The
2026-11-01 r19 rework supersedes the user-rejected r18 kit/hood styling with
three photo-derived405 bumper/rocker sets and factory-contour plain-carbon /
central-scoop hoods. See `peugeot405-photo-kits-2026-11-01.md` and its actual
native/photo comparison gallery. R18's three wings remain unchanged. The user
also rejected the generic r17 kits and wing fit, so r17's
loading checks do not establish visual acceptance. Other nine imports remain
on their preceding asset state and still need that artistic work. The preceding
r17 delivery added30 selectable wings,405 privacy glazing, a clean240 plate,
and source-derived Jetta rear-lamp/liner repairs after the user's r16 feedback.
See `car-feedback-wings-and-rear-lamps-2026-10-31.md` for evidence and limits.
RP5 now also has r17, imported through the launcher after the user freed the
device. It validated64 cars; all nine save-backup entries match before/after
import. APK19 was retained and APK27 remains local. R18 is prepared as a runtime
ZIP but has not been imported on RP5. R19/r20/r21/r22/r23 were installed only on Linux; no
device input occurred during these405 reworks. The other cars' plain bumper/skirt finish and reported wing fit need
artistic improvement; this delivery does not establish full ten-car acceptance.

| ID | Car | Custom front/rear/skirt choices | Current relevant visual evidence |
|---:|---|---|---|
| 54 | Integra DB8 | 3/3/3 | r9 eight-yaw and close views, stock/carbon/vinyl8/all three kits |
| 55 | BMW E36 | 3/3/3 | Same r9 evidence; rear edge, grille/plate and exhaust clearance iterated |
| 56 | Levin AE92 | 3/3/3 | Same r9 evidence; rocker ends and low front plate accommodated |
| 57 | Peugeot 405 GLX | 3/3/3 | r23 full OEM perimeter fit and narrow source mounting band, tapered/rounded replacement fascias, supported opaque interior material.70 workshop +64 selector views;14 stock views pixel-identical. r20 spoilers/preparation and r19 hood/skirts retained. User artistic acceptance pending |
| 58 | Peugeot 406 Coupé | 3/3/3 | r13 kit2/3 fit, eight angles/close; rear wheel/arch correction retained |
| 59 | Golf Mk5 R32 | 3/3/3 | r13 kit2/3 fit with source centre exhaust tips clear; eight angles/close |
| 60 | Jetta/Vento Mk5 FSI | 3/3/3 | r13 disconnected rear-end correction; prior lamp/trunk-liner/source wheel repairs retained |
| 61 | Outback 2002 | 3/3/3 | r12 front/rear kits retained; r14 restores skirt vinyl receivers, 48 whole-car plus12 before/after target views |
| 62 | Volvo 240 Turbo | 3/3/3 | r15 source-component body-colour kit3 bumpers, eight angles/close/alternate paint; source headlamp/hood-edge/plate fixes retained |
| 63 | Volvo V70 2.4 | 3/3/3 | r15 local lens/housing lamp fit and outward reflectors; 48 whole-car views plus24 LIGHTSON off/on views; corrected wheels retained |

R10 changes Golf dynamics and handling/specification metadata; see
`ten-car-handling-verification-2026-10-31.md` for exact changes and test limits.
R11 additionally corrects Volvo240 first/reverse from the 1984 Swedish factory
manual; see `volvo240-transmission-verification-2026-10-31.md`. Its mass/axle
estimates and the V70 market/gearbox question remain explicitly open. R12 changes
Outback kits, r13 corrects the other six Wave2 kit2/3 fits, and r14 repairs
Outback's lower-vinyl/skirt interaction. R15 corrects V70 lamp geometry/emissive
overlay and adds a source-component body-paint fascia option to405/240 kit3;
these are asset changes, not handling.
Relevant reports: `street-cars-visual-repairs-2026-10-29.md`,
`car-visual-r5-checkpoint-2026-10-29.md`, `wave2-close-review-2026-10-31.md`,
`outback-exhaust-verification-2026-10-31.md`, and
`volvo240-plate-hood-verification-2026-10-31.md`. Historical reports retain their
original limits; use the later correction report for a subsequently fixed item.

## Proven now

- All ten are present in the stable 64-car native roster. The three first-wave
  cars now have the missing catalogue options and appended geometry.
- Extracted r10 runtime checks pass: first wave 482; Wave2 1,238. Materials,
  attachment points, wheel/damage/refit/save-codec checks are bounded by their
  actual native checker implementations, not claims of complete visual quality.
  Extracted r11 adds 1,238 passing Wave2 checks; first-wave payloads match r10.
- Existing car/part identities and personal files are preserved. The current
  executable, sound and unrelated packages remain intact. R10 corrects Levin's
  automatic shift band/reverse and Golf's final drives without changing power.
- Native flat-ground acceleration, braking, coasting and bilateral steering
  observations cover all imports plus eleven retail references at 60/120 Hz.
  R10's refined changes leave the other twenty cars' 120 cases identical per
  step size. The rejected Levin shift candidate and engine timestep/arcade
  limits are retained in the handling report; this is not road-car certification.
- Newly appended first-wave choices have now been exercised through normal
  workshop purchases, real autosaves and fresh-process profile loads for both
  indices 2 and 3 on all three cars/categories. See
  `first-wave-workshop-save-verification-2026-10-31.md`: 18 distinct choices,
  six saved/reloaded sets, exact money/part-state inspection, personal files
  unchanged. Synthetic funds/unlocks do not establish natural progression.

## Still required for full acceptance

1. Audit the actual factory variant and handling evidence per car. Both Wave2
   `SPECIFICATIONS.json` and `HANDLING.json` retain provisional warnings. Resolve
   manufacturer evidence for outstanding mass/gearing conventions (notably 405,
   Golf, 240 and V70), compare emitted dynamics with those facts, and assess
   actual acceleration/braking/cornering. Do not erase warnings on the strength
   of loading tests or adjust displayed BHP to disguise poor physics.
2. Judge the source-art and customization finish per car against the user's
   Juiced look-and-feel request. Prior source simplicity, particularly Outback,
   is not automatically acceptable because a renderer shows no missing mesh.
   Three small variants do not by themselves certify three sufficiently distinct
   finished body kits. Reuse current views; target concrete deficiencies.
   New seven-car close-kit review identifies flat/disconnected rear extensions
   and V70's wavy kit2 edge. The first isolated Outback redesign was rejected
   after actual native rendering despite passing technical checks; it is not
   installed. See `wave2-kit-finish-review-2026-10-31.md` for exact findings,
   inspected frames and the next measured study.
   Outback's subsequent continuous-surface design and corrected body-paint
   assignment are now installed in r12, with final48 views inspected and1238
   extracted-package checks. See `outback-kit-integration-2026-10-31.md`.
   R13 now replaces kit2/3 on the other six Wave2 cars with fitted geometry;
   288 native views were inspected and 96 actual mesh payloads independently
   checked for unchanged vinyl receivers, face orientation and exhaust clearance.
   See `six-car-fitted-kits-2026-10-31.md`. This is a fit correction, not complete
   customization approval: 405/240 rear styles remain subtle, V70 front lamp art
   needs targeted review, and the existing kit1 choices were retained. Outback's
   grey cladding is also a vinyl receiver: r12/r13 skirt morphs did hide lower
   artwork. R14 restores the entire original receiving surfaces and mounts the
   new skirt extensions below all31 decal meshes. The2,844-case centroid audit
   changes from1,184 newly occluded cases to0; native F&F1 before/after views and
   whole-car views were inspected. See `outback-skirt-vinyl-correction-2026-10-31.md`.
   R15 resolves the concrete V70 dark-headlamp defect: existing reflector rings
   sat behind the housing and faced inward. Their local fit is now corrected,
   with a visible LIGHTSON overlay reused from those surfaces. R15 also makes
   the405/240 kit3 fascia distinctly body-coloured, retaining source components,
   grille/chassis/trim features and player paint. There are176 inspected native
   views and independent20-mesh verification; see `v70-lamps-and-period-fascia-2026-10-31.md`.
   Existing Wave2 kit1 finish, including V70's old sampled front lip, remains a
   specific next art task. These narrower corrections do not approve all styles
   merely because three catalogue choices exist.
   The rejected first pilot stays rejected and separate from delivered assets.
3. Once RP5 is available, preserve/export its actual saves, install the selected
   APK/pack, verify Ronnie's requested full garage, and visually/physically test
   the ten cars and reported defects. Linux screenshots cannot close this item.

The next work is these car-specific gaps. Original-engine byte matching,
119-D8M sweeps, broad Arcade completion and unrelated feature audits are not
prerequisites for this ten-car task. Overall goal remains active.
