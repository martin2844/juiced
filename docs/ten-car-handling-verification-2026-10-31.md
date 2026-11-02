# Ten-car handling audit and bounded fixes — 2026-10-31

Linux now uses Street Cars **revision 6**, Wave2 **revision 8**, Ferrari revision 4,
bundle **r10**. This advances the ten-port task; it does not close the full
factory-handling, artwork or physical Android acceptance items. RP5 was not
accessed, updated or controlled. Its last verified delivery remains APK19/car-r5.

## Findings and delivered changes

**Levin AE92 GT APEX, ID56.** The authored 4A-GE torque curve extends through its
7200-rpm rated-power region, but the donor Corolla's automatic band was 5400 rpm.
The game shifted at about 5510–5545 rpm. Toyota's [selected-grade catalog][toyota]
identifies the May 1989 GT APEX five-speed manual: 1030 kg, 140 PS at 7200 rpm,
147.1 Nm at 6000 rpm, final drive 4.312 and reverse 3.250. The catalog does not
establish a native-game automatic shift threshold or all forward gear pairs.

The initial candidate set `PowerRangeMax=7200`. This was **rejected**, despite its
improved acceleration: native automatic logic shifts above 1.02 times that value,
and fourth settled near 7338 rpm, below the 7344-rpm trigger. It never reached
fifth at full throttle and terminal speed fell from 198.16 to 185.74 km/h.

Delivered `PowerRangeMax=7000` is explicitly a **native-game tuning value**. It
shifts near 7140–7151 rpm, reaches all five forward gears, retains terminal speed
and improves acceleration without changing the torque curve, mass or power.
Reverse changes from the donor's 3.166 to Toyota's 3.250. Forward ratios and final
drive remain unchanged. The importer now emits these values on future builds.

**Golf Mk5 R32, ID59.** The [original VW 02Q AWD workshop allocation, mirrored][vw]
lists 72/17 for final drive I (gears 1–4), and 72/22 for final drive II (5/6/reverse)
for the 3.2-litre 184-kW application. The old candidate used 3.944/3.087.
Delivered ratios use 72/17 and 72/22. The engine has one wheel final drive, so
fifth/sixth include the secondary-to-primary factor. Individual gear pairs remain
provisional: the workshop table refers their lookup to ETKA and is not evidence
for the currently selected individual pairs. The old overall reverse ratio is
retained rather than inventing a reverse pair. `HANDLING.json`, `SPECIFICATIONS.json`
and the authoring script retain that distinction. Full gearbox certification is
still open.

No car IDs, part indices, meshes, sounds, personal profiles, engine source or
production executable changed in this delivery. The seven other installed
packages, including Ferrari, remained byte-identical. Only the two dynamics
recipes and their package metadata changed.

## Actual native measurements

The private `CarHandlingStudy.cpp` uses the shipped native `CarBody`, fitted stock
parts, actual stock-model wheel positions, axle weighting, `raceTune`, collision
contacts and flat-ground fixture. It uses a deterministic seed and fresh private
support directories, without player save input. It measures eleven imported cars
(including Ferrari) and eleven retail references: Corsa, Punto, 206, Focus,
Civic Type R, Acura Integra, 350Z, WRX, Falcon, NSX and Viper.

Six cases per car: acceleration with/without ASR, braking from 100 km/h, 15-second
coast from 100 km/h, and left/right steering ramps with a 60-km/h speed controller.
Runs use 60-Hz and 120-Hz fixed steps, tracing actual angular velocity, not Euler
heading mislabeled as yaw rate. Baseline, rejected candidate and refined candidate
each have 132 cases at each step: **792 recorded case runs** total.

| Observation, ASR enabled | Old 60 Hz | Delivered 60 Hz | Old 120 Hz | Delivered 120 Hz |
|---|---:|---:|---:|---:|
| Levin 0–100 km/h (s) | 7.417 | 6.817 | 7.250 | 6.675 |
| Levin speed at 90 s (km/h) | 198.161 | 198.161 | 198.158 | 198.158 |
| Levin forward upshifts | 4 | 4 | 4 | 4 |
| Golf 0–100 km/h (s) | 4.200 | 4.167 | 4.242 | 4.225 |
| Golf speed at 90 s (km/h) | 241.357 | 231.783 | 241.357 | 231.784 |

These are game-fixture observations, not promised road-car specifications. At each
step size the **120 cases for the other twenty cars are identical**, while all
twelve cases for Levin/Golf change. All recorded summary values are finite. The
delivered Levin's terminal-speed difference is below 0.02 km/h, with four
upshifts and the intended measured shift region. The native displayed peak power
is unchanged (Levin 136.17 mechanical bhp, Golf 246.75). Levin's curve approximation
and 250-rpm peak-power sampling explain why it is not exactly the catalog's
140 PS converted to mechanical bhp; the curve was not falsified to force the HUD.

### What the diagnostic cannot certify

The engine retains its common **2.8 torque multiplier**. Retail references also
accelerate much faster than factory stopwatch times; forcing only the imports to
those times by reducing their power would misrepresent both the cars and game.

Braking is substantially timestep-dependent in both retail and imported cars.
For example, NSX stopping distance changes from 18.88 m at 60 Hz to 27.22 m at
120 Hz; 406 changes from 27.72 to 21.33 m. Production `PhysicsWorld` does not force
these two rates through a common 120-Hz substep. These are a shared engine/fixture
limitation, not evidence that a single imported car needs an arbitrary brake
multiplier. No global physics change was made under this car-asset task.

Flat ground is not a track, and a simulator tick rate is not achieved Android
FPS. This does not prove tire-model fidelity, physical controller behavior,
track-surface handling, realistic stopping distances or Android performance.
All mass/individual-gearing uncertainties for 405, 240 and V70 remain explicit.

## Manufacturer evidence reviewed

- Toyota's selected May-1989 GT APEX page, saved HTML plus SHA256 receipt. The
  1987 heritage release's differently specified GTV is not substituted for it.
- VW original workshop final-drive allocation, saved HTML plus SHA256 receipt.
  Individual gear ratios were not claimed from a table that does not list them.
- Volvo 1984 technical brochure `MSPV535.pdf`: B21ET power/torque and four-speed
  plus overdrive. It does not settle this candidate's exact curb mass/gears.
- Original Volvo North America 2004 brochure, printed page 39, rendered and
  visually reviewed: base V70 2.4 has 168 SAE-net hp, 166 lb-ft, 3488-lb curb;
  five-speed manual is standard, automatic optional. Applying the base-column
  curb mass to the manual is identified as an inference, not a separate manual
  mass entry. European 1528-kg data uses a different market/convention and does
  not replace the selected US mass. Individual ratios remain provisional.

The retained PDF originals, text, rendered table and source/hash receipts are in
`out/validation/linux-current-20261029/ten-car-handling/references`.

## Packaging, installation and preservation

`out/releases/OpenJuice-Car-Visual-Fixes-r10.zip`: **189790575 bytes**, **3056
payloads**, SHA256
`2bdab44bb3dda55a7290a68b1fb23a996b97741a6bed907e3e6bc084ec81aac0`.
Every ZIP payload hash, CRC and extracted package manifest was checked. Native
checks ran against the extracted archive: **482 first-wave passes and 1238 Wave2
passes**, zero failed. The two extracted dynamics scripts match the measured
refined overlay byte-for-byte. All seven regenerated authoring outputs preserve
the measured numeric anchors; both edited authoring scripts parse successfully.

The first delivery script stopped before installation because it incorrectly
expected StreetCarCheck to print a final `CHECKS` line. The tool actually emitted
482 individual `PASS` lines and exited zero. That log is retained. The corrected
harness counts its actual output and resumes validation of the same ZIP without
overwriting it; this was a harness assertion error, not a failed car check.

Only after those checks did atomic package exchanges install the two new
revisions. Previous live trees remain under `ten-car-handling/*-pre-r10-rollback`.
Repository runtime defaults match the installed payloads. Actual assembly of
all nine installed packages then produced matching measured scripts and roster.
Six personal file hashes, the production ELF and all seven unrelated packages
were checked unchanged. No profile restore or save edit occurred.

Authoritative receipt: `ten-car-handling/r10-delivery-receipt.json`.
Supporting evidence: `refined-comparison.json`, `authoring-regression.json`,
`installed-composition-receipt.json`, complete traces, logs and private harness
sources in the same validation directory. The production ELF remains
`e8450e7dac66c9eb0da844be4db06a074d486ebd7e485c4403f1245f2aaa4607`.

The overall ten-car goal remains active. Finish the unresolved per-variant
specification/handling and artwork issues, then preserve/export RP5's actual
saves and test the selected APK/pack and Ronnie's full garage when the user makes
the device available. Linux proof does not close that physical-device item.

[toyota]: https://toyota.jp/ucar/catalog/brand-TOYOTA/car-COROLLA_LEVIN/198905/1002380/
[vw]: https://workshop-manuals.com/volkswagen/golf-mk5/power_transmission/6-speed_manual_gearbox_02q/technical_data/four-wheel_drive_gearbox_identification/four-wheel_drive_identification_codes_assembly_allocation_and_capacities_golf_4_motion_2005_%3F/
