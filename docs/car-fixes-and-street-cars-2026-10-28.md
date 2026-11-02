# Ferrari fixes and street-car sourcing — 2026-10-28

## Report and confirmed causes

The user reported the Ferrari displaying “dumbass” in race results and feeling
slow for its displayed horsepower, with excessive oversteer.

Five result-label paths in RaceResults/SprintResults indexed the retail text
block by model ID. That block stops at 52; Ferrari ID 53 read unrelated UI text.
`carDisplayName` now preserves localized names for retail models and uses the
catalogue name for added models. A regression also covers the next added ID.

`CarBody::setGear` hard-clamped at internal gear 7 (sixth forward gear), although
the Ferrari defines seven ratios and the automatic gearbox attempts internal
gear 8. `shift` could disengage the clutch even when clamping prevented a shift.
Both methods now clamp to the actual transmission; a blocked upshift does not
disengage the clutch. HUD gear display supports seventh and beyond. The legacy
network packet still has only three gear bits: saturation at sixth avoids
wrapping seventh into reverse, but exact seventh-gear online synchronization
is not implemented by this change.

## Handling calibration

The original port mod inherited Falcon suspension/steering settings and combined
them with a 42% front-weight parameter. A deterministic native CarBody test on
FlatGround reproduced unstable straight-line acceleration and severe oversteer.
Fixing seventh gear alone did NOT eliminate the instability. Multiple isolated
calibrations were compared with the original Ferrari and retail Falcon/NSX.

Selected package revision 3 changes:
- Effective front weight: 0.42 → 0.50.
- Maximum front steering angle: 0.60 → 0.45 radians.
- Rear lateral grip: 3.0 → 3.8; rear longitudinal grip: 3.0 → 3.3.
- Power, mass, torque curve, gear ratios, geometry and saved upgrades unchanged.

This is calibration for Juiced's arcade solver, NOT a claim that the real 458 has
50/50 weight distribution. Retail NSX itself uses a game-tuned front parameter.
No claim of exact real-world acceleration, handling or top speed is made.

### Native diagnostic at 60 Hz, stock car, traction control on

| Test | Before | Revision 3 |
| --- | ---: | ---: |
| 0–100 km/h, straight | 3.55 s | 3.72 s |
| 0–200 km/h, straight | 7.33 s | 7.50 s |
| Straight-line speed after 20 s | 190.2 km/h | 297.3 km/h |
| Peak straight-line sideslip | 19.5° | 0.15° |
| Speed after 10 s, steering pulse at seconds 5–7 | 50.4 km/h | 205.7 km/h |
| Peak sideslip with steering pulse | 96.2° | 12.5° |

The benefit is maintaining speed and stability, not adding horsepower or making
the initial launch artificially faster. The same diagnostic was run with ASR off
and at 30/120 Hz. It remains a synthetic flat-ground test: bumps, damage, upgraded
cars and real-player track input still need gameplay evaluation.

## Validation and delivery

- 282 Linux unit checks passed, including naming and transmission regressions.
- Native Ferrari asset checker passed: loading, wheels, damage, upgrades, decals,
  collisions and lights.
- Actual assembled package produced the final measured handling results.
- Signed ARM64 APK built: versionCode 16, `1.02.5-android.15-car-fixes`.
- Linux executable rebuilt; installed Ferrari package updated with backup at
  `out/car-audit-20261028/installed-ferrari-before/`. No saves modified.
- Android requires BOTH the new APK and importing the Ferrari revision-3 ZIP.
  No device installation or GitHub publishing performed.

Artifacts: `out/releases/OpenJuice-car-fixes-arm64.apk`,
`out/releases/OpenJuice-Ferrari-r3.zip`.
Logs, CSVs, candidate metadata/previews and checksums:
`out/car-audit-20261028/`.
Repeat the diagnostic with
`out/linux/juiced_car_handling_check GAME ASSEMBLED_OVERLAY [30|60|120]`.

## Three additional street cars: sourcing incomplete

No new cars have been imported or declared production-ready. These are candidates
selected after reviewing creator metadata and thumbnails; source meshes must be
inspected, optimized, adapted, rendered in Juiced and driven before acceptance.
All three public Sketchfab download API requests returned HTTP 401 requiring
an authenticated download. Metadata and the responses are saved in the audit folder.
No credentials were searched for or extracted.

| Candidate | Creator / source | Declared triangles | Declared license | Main adaptation |
| --- | --- | ---: | --- | --- |
| BMW E36 coupe | [RYBY_DLA_DEBILI / SmashedBones](https://sketchfab.com/3d-models/bmw-e36-7a16309e9d104000a5ab76f288d7b07e) | 16,901 | CC BY | Wheel/panel segmentation, material conversion, damage, donor-calibrated RWD physics |
| Volkswagen Golf Mk2 | [Teofels](https://sketchfab.com/3d-models/vw-golf-2-c9f47e2e291241d2a490adab1d1ca921) | 120,396 | CC BY | Major mesh optimization/LODs, material cleanup, FWD physics |
| Toyota Corolla E180 EU | [Armored Wave](https://sketchfab.com/3d-models/2014-toyota-corolla-e180-eu-with-interior-36f95efb0585464cae43a25a3b3392e8) | 84,408 | CC BY | Reduce 55k-triangle interior, simplify exterior, FWD physics |

The Golf/Corolla triangle counts are above the intended in-game budget; a download
is not acceptance. The BMW is the strongest first pilot. The Corolla's newer
model year is an aesthetic consideration despite being a normal street car.
A free 1976 Fiesta (ImperialBlue3D, 6,508 triangles) was rejected after thumbnail
inspection: blurry baked windows/lights and faceting are below the requested
Juiced quality. Stylized/PS1 packs and NFS-ripped Golf sources were also rejected.

To unblock: download original or GLB/GLTF archives from those creator pages into
Downloads (including textures and license files), then adapt one pilot end to end.
Check scale/wheelbase and pivots, real separate wheels, native panels/materials,
paint/vinyl UVs, collision/damage, drivetrain/gear count, dealership and results
names, save round-trip and Android performance. Do not add catalogue placeholders
or use these preview images as substitute cars.

## RP5 installation — 2026-10-28

Installed the signed `OpenJuice-car-fixes-arm64.apk` using an in-place update on
Retroid Pocket 5 (`7f3ffc8d`): versionCode 16,
versionName `1.02.5-android.15-car-fixes`. Imported Ferrari r3 through the launcher
ZIP importer. The launcher reported 54 cars loaded and successful data/active-mod
validation; Ferrari 458 Spider was visibly Enabled. This verifies installation,
not a physical-device track handling test.

Exported saves through the launcher before and after the update. Both archives
pass ZIP integrity verification and all nine entries have identical uncompressed
contents. Evidence: `out/retroid-update-20261028/saves-before.zip`,
`saves-after.zip`, `save-comparison.txt`, and `ferrari-enabled.png`.
No uninstall, data reset, or save restore was performed.

The three proposed new street cars remain pending source downloads and adaptation;
they were not installed. User has authorized their RP5 installation when ready.
