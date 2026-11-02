# Street Cars for OpenJuice

Adds three ordinary street cars alongside the original roster:

| Car | Stable ID | Drive | Approx. stock power | Base price |
| --- | ---: | --- | ---: | ---: |
| Honda Integra DB8 Type R | 54 | FWD | 197 bhp | 22,000 |
| BMW M3 E36 (US 3.2 performance specification) | 55 | RWD | 240 bhp | 29,000 |
| Toyota Corolla Levin AE92 GT APEX | 56 | FWD | 136 bhp | 12,500 |

Requires the Ferrari additional-car package enabled (stable ID 53). This maintains
existing save identities; never renumber these cars when adding another package.
The cars appear under **All Cars** in Custom Race and in the career dealer pool
(subject to the game's dealer selection). Installing does not add free cars to a
career or modify its money, respect or garage.

Included: native race/garage meshes, original UVs and textures, working paint and
glass tint, animated wheels and fitted aftermarket wheels, separate damage zones
and repair, carbon bonnet and three fitted bumper/side-skirt options, all 31 retail vinyl
styles, performance sponsors, stock/source exhaust geometry with an upgrade slot,
headlight/brake switching, collision cage and calibrated donor-based physics.
Retail performance upgrades, nitrous and neon use the existing engine mechanisms.

The source BMW is already visually tuned, including its lowered stance and rims.
The aero options are mild adaptations of these models, not licensed aftermarket
kits. Engine sounds reuse the owner's existing Juiced banks (Integra/Civic, 350Z,
Celica); these are not authentic recordings of the new cars. Physics is calibrated
for Juiced's arcade model, not a validated simulator.

Revision 5 keeps stock index 0 and the existing sport index 1. Appended indices
2 and 3 add a painted valance kit and a slimmer dark lip kit. Each replacement
retains the original native part verbatim, including lights, grilles and trim.
Rear additions leave an opening around the actual stock exhaust tips. These are
restrained original aero studies, not replicas of named aftermarket kits.
Existing saves keep their previous part identities; the roster and dynamics
have not changed. This revision has Linux visual/runtime evidence; physical
Android verification remains separate.

The editable studies and addon GLBs are in `tools/assets/body-kits-20261031`.
`tools/build_body_kit_study.py` fits them to the native stock meshes in Blender.
`tools/add_body_kits.py` builds a fresh revision-5 candidate from an intact
revision-4 runtime package and those GLBs. It rejects missing/duplicate output,
open exported geometry and an obstructed exhaust opening. Keep the revision-4
baseline when reproducing this append operation; do not run the legacy full
package rebuild over an installed revision-5 package.

See [attribution](ATTRIBUTION.md) for creators, original sources and CC BY 4.0.
No original game's textures, meshes, audio or complete scripts are redistributed;
vinyls/audio are referenced and dynamics derive from local retail patch recipes.

## Rebuild

Requires Blender 5.1, Node with the pinned dependencies in `mods/ferrari/tools`,
and Python 3.13 with NumPy and Pillow (DDS encoding enabled). Source GLBs and hashes
are in `tools/assets`. Decode each to KEY-decoded.glb using `tools/decode.cjs`,
where KEY is integra, bmw or levin; run `inspect_blender.py -- WORKDIR`, then
`prepare_blender.py -- WORKDIR`. These save editable source/prepared `.blend`
files and render four views per car. The preparation removes the BMW studio floor
and reduces Levin geometry, preserving exterior detail preferentially.

```
python tools/import_cars.py --game GAME --sources WORKDIR --output OVERLAY
python tools/package.py OVERLAY GAME
```

Run commands from this mod directory, or prefix script paths appropriately.
`OVERLAY/scripts` initially contains additive catalogue fragments, not a standalone
game overlay. Use the normal package assembler with Ferrari and Street Cars to
produce the complete runtime overlay. Never point the game directly at fragments.

The reproducible native check is `juiced_street_car_check GAME ASSEMBLED_OVERLAY`;
the handling probe is `juiced_car_handling_check GAME ASSEMBLED_OVERLAY [FPS]`.
Keep diagnostic careers isolated from real profiles.

Revision 4 preserves integral boot and hatch bodywork when fitting a carbon hood, changing vinyls, or choosing no spoiler. The importer previously treated a rear/high paint cutoff as a detachable wing. These three source cars now retain their factory rear bodywork and integral wing in the chassis. Existing car IDs, handling and save entries are unchanged.

## Revision 6: Levin automatic gearing

Corrects the AE92 automatic shift band inherited from the Corolla donor.
Native PowerRangeMax is 7000 rpm, producing observed shifts near 7140–7151 rpm
around the selected GT APEX engine's 7200-rpm rated-power region. This is a
native-game tuning value, not a Toyota factory automatic specification: the
selected car is the five-speed manual. A 7200-rpm native band was rejected
because it prevented the fourth-to-fifth upshift at full throttle. Reverse is
3.250, from Toyota's selected-grade catalog; forward ratios and final drive,
stock torque/power curve, mass, geometry, sounds and stable IDs are retained.

Factory reference: https://toyota.jp/ucar/catalog/brand-TOYOTA/car-COROLLA_LEVIN/198905/1002380/
See `docs/ten-car-handling-verification-2026-10-31.md` in the port repository
for 60/120-Hz observations and their limits. This does not certify real-world
stopwatch times or Android behavior.

## Local car bundle r16 — 2026-10-31

Existing Sport1 bumper/skirt meshes retain the complete original stock surfaces, with fitted lower additions; fixes sampled retail-vinyl occlusion. Stock and kit2/3 payloads are unchanged.
Native full-roster candidate checks pass (1,238 Wave2 plus483 first-wave assertions). This is a user playtest delivery, not a claim of complete visual/handling/device approval. Additional evaluations were stopped at the user's request.

## Local car bundle r17 — wings and reported visual fixes

Three actual retail Juiced wing options are fitted per car. Previously integral source wings are detachable stock choices; boot/hatch panels stay attached. Other existing body-part and vinyl indices, car IDs, handling and saved-game formats stay unchanged. Wing choices append to the previously absent SPOILER category.
Full native composition passes1,322 Wave2 and483 first-wave checks. New wing geometry was reviewed in eight directions; the Jetta brake overlay was compared off/on. These checks do not establish Android FPS or finished bumper/skirt artistry; the user requested richer kits and that work remains open. No saves are included.
