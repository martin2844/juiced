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
and repair, carbon bonnet and mild fitted sport-body options, all 31 retail vinyl
styles, performance sponsors, stock/source exhaust geometry with an upgrade slot,
headlight/brake switching, collision cage and calibrated donor-based physics.
Retail performance upgrades, nitrous and neon use the existing engine mechanisms.

The source BMW is already visually tuned, including its lowered stance and rims.
The aero options are mild adaptations of these models, not licensed aftermarket
kits. Engine sounds reuse the owner's existing Juiced banks (Integra/Civic, 350Z,
Celica); these are not authentic recordings of the new cars. Physics is calibrated
for Juiced's arcade model, not a validated simulator.

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
