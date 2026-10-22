# Ferrari 458 Spider — additional native car

A real car absent from Juiced's original roster. This opt-in mod adds
**FERRARI 458 SPIDER** as model ID 53, alongside all 52 retail cars. The Falcon
keeps ID 46 and its original assets. Launch without `--mod` for the retail roster.

Base price: **185,000 credits**; dealer prices include the normal condition,
mileage and small price variations. It unlocks at respect level 36, alongside
the retail supercars, and is immediately available in custom races.

Additional-car upgrades use independent progress in a versioned career-save
extension. Retail-only saves retain their original format. The installer uses
`Juiced Mods/Ferrari458-Added` saves, separate from the earlier replacement
prototype; do not reuse that prototype's saves as if its Falcon were a Ferrari.
The added roster uses explicit sequential IDs from `scripts/mod_cars.txt` (up to
59). Keep those IDs stable when updating a mod.

The open-roof source is distributed by Three.js under the title “Ferrari 458 Italia”,
credited to **vicent091036**. We label its visible open-roof configuration Spider.
See [attribution and licence provenance](assets/ATTRIBUTION.txt). No ripped assets
from another commercial racing game are used. The original model is CC BY 4.0;
retain attribution when sharing the adapted meshes. The repository's code licence
applies to the importer and engine changes.

## Build

Requires Node.js, FFmpeg and Python with NumPy support. From the repository root:

```sh
tools/car_mod/build.sh /path/to/Juiced /path/to/Ferrari458
cmake --build out --target juiced mac_car_mod_check -j8
out/mac_car_mod_check /path/to/Juiced /path/to/Ferrari458
out/juiced --game /path/to/Juiced --mod /path/to/Ferrari458 --screen instant_selcar
```

`--mod` defaults to a separate `Saves` directory inside the mod. Keep modded
careers separate so the retail-only game is never asked to load an added car.
The mod unlocks this car in Custom Race, including after loading or creating a
profile. In the normal menu, it is under **Class B** and **All Cars**. It also appears in the career dealer pool,
with the normal purchase, upgrade, damage and repair flows.

At Car Select, press Enter once if the keyboard has not yet been assigned.
Page Down (Fn+Down on a compact Mac keyboard) applies Auto-Mod, Tab clears mods,
and Up/Down changes paint. Enter proceeds to race setup. Auto-Mod uses Juiced's
own upgrade selection, prices and performance calculations. WASD, Shift NOS and
Space handbrake use the port's current editable bindings.

## Included

- Detailed exterior and interior, animated wheels, working paint and glass tint.
- Separate front/rear bumper, bonnet and left/right skirt damage zones, using the
  native dent/scratch grid. Collision and repair operate on the imported vertices.
- Authored sport splitter, rear diffuser, woven carbon bonnet, skirts and rear wing,
  fitted through the existing body-part slots. These are custom test parts, not
  replicas of a named real-world aftermarket kit. Accessory edges are chamfered.
- Two native tintable decal designs (twin longitudinal stripes and side graphics),
  plus a custom-image option with fitted bonnet and door surfaces. Sponsor decals
  show all nine installed performance brands. Both decal shells use native damage.
- Native neon purchase, colour, lowering, glass tint and wheel upgrades.
- Independent standard/sport engine banks built from a licensed real Ferrari 458
  recording, with native RPM crossfades and processed overrun/exhaust variants.
- Native performance catalogue support: engine/induction/exhaust, nitrous,
  gearbox, suspension, brakes, tyres and replacement wheels.
- Authored collision cage and wheel contacts; 2.65 m wheelbase, rear-wheel drive,
  seven gears, 540 Nm torque parameter and 1,535 kg mass. Torque curve and ratios
  are an approximation calibrated for Juiced, not a validated vehicle simulator.
- Separate brake/headlight nodes and flares, matching stock exhaust geometry.
- Garage and race meshes, and a matching high-speed wheel mesh.

The game currently displays about **567 bhp** stock. Published 458 specifications
inform the engine and dimensions. Wheels use one
shared native wheel mesh, as the stock game does. High-speed wheels retain this
mesh instead of a donor's unrelated blur geometry. Damage uses the game's existing
deformation system; detachable panels are not implemented by the original game.

## Implementation and validation

`decode.cjs` decompresses the pinned GLB and simplifies with normal attributes
included, preserving shading seams. `import_ferrari.py` bakes transforms, converts
handedness and units, partitions panels, exports D8M/BOH/PMD/DDT/OCD assets, and
patches a local copy of the owner's retail scripts. All paths in the overlay are
lowercase. `retail.py` reads DAT archives without modifying them.

`mac_car_mod_check` verifies both garage and race loading, all four wheels,
wheelbase, collision meshes, vertex deformation/repair, each sport-part swap,
independent engine-bank PCM loading, decal fitting/removal/damage, safe Auto-Mod
with a small decal catalogue, and increased power with fitted performance parts. Runtime captures exercise the
stock selector, native Auto-Mod (567 → 1,018 bhp), a 1,800-frame dry driving run
and a 900-frame wet/night run. Regular CTest checks cover the file
overlay and the port's existing regressions.

Generated `scripts/cardefs.txt`, `dynamic_ferrari_458.txt` and `arcadecollection.txt`
contain locally derived retail data. Do not include those files in a public
source release. Share this importer and its licensed model so each owner can
build the mod against their own copy.

For a local standalone test app, use `install_macos.py --base-app /Applications/Juiced.app --exe out/juiced --mod /path/to/Ferrari458 --output "/Applications/Juiced Ferrari 458 Added.app"`. This preserves the base app and uses a separate save directory under Application Support/Juiced Mods/Ferrari458-Added.

Sound source: [Edvvc's Ferrari 458 recording](https://commons.wikimedia.org/wiki/File:Ferrari_458_Italia.ogg),
CC BY-SA 3.0. `ferrari_audio.py` crops, filters and loops four RPM layers, resamples
them for the native 48 kHz playback ceiling, and generates standard/sport `.cfg`,
`.rev`, `.vol` and PCM WAV banks. Source and derivative audio retain CC BY-SA 3.0;
see `assets/AUDIO-ATTRIBUTION.txt`. This is an exterior drive-away recording,
not a studio multi-load library. Overrun and sport exhaust are processed variants,
not separate recordings of an upgraded Ferrari. RPM estimates and pitch shifts
are approximate. No Corvette sample remains in the Ferrari engine banks.

The third decal option is a transparent custom-logo shell until an image is
provided by the optional creative mod. Its mesh is `DECAL_F458_CUSTOMLOGO`, material
`decal_f458_customlogo`, texture slot 0; the default remains clean paint.
