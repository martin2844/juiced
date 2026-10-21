# Ferrari 458 Spider — first native car import

A real car absent from Juiced's original roster. This opt-in test mod occupies the
Falcon's internal slot, while showing **FERRARI 458 SPIDER** in the game. The normal
game stays unchanged when launched without `--mod`. This is a replacement-slot
prototype, not a 54th save-compatible vehicle ID.

The open-roof source is distributed by Three.js under the title “Ferrari 458 Italia”,
credited to **vicent091036**. We label its visible open-roof configuration Spider.
See [attribution and licence provenance](assets/ATTRIBUTION.txt). No ripped assets
from another commercial racing game are used. The original model is CC BY 4.0;
retain attribution when sharing the adapted meshes. The repository's code licence
applies to the importer and engine changes.

## Build

Requires Node.js and Python with NumPy support. From the repository root:

```sh
tools/car_mod/build.sh /path/to/Juiced /path/to/Ferrari458
cmake --build out --target juiced mac_car_mod_check -j8
out/mac_car_mod_check /path/to/Juiced /path/to/Ferrari458
out/juiced --game /path/to/Juiced --mod /path/to/Ferrari458 --screen instant_selcar
```

`--mod` defaults to a separate `Saves` directory inside the mod. Do not point
`--saves` at an existing vanilla career: the Falcon slot has different parts.
The mod unlocks this car in Custom Race, including after loading or creating a
profile. In the normal menu, it is under the donor's **Class C** category; its
actual performance is much higher. It also appears in the career dealer pool,
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
- Authored sport splitter, rear diffuser, carbon bonnet, skirts and rear wing,
  fitted through the existing body-part slots. These are custom test parts, not
  replicas of a named real-world aftermarket kit.
- Native performance catalogue support: engine/induction/exhaust, nitrous,
  gearbox, suspension, brakes, tyres and replacement wheels.
- Authored collision cage and wheel contacts; 2.65 m wheelbase, rear-wheel drive,
  seven gears, 540 Nm torque parameter and 1,535 kg mass. Torque curve and ratios
  are an approximation calibrated for Juiced, not a validated vehicle simulator.
- Separate brake/headlight nodes and flares, matching stock exhaust geometry.
- Garage and race meshes, and a matching high-speed wheel mesh.

The game currently displays about **567 bhp** stock. Published 458 specifications
inform the engine and dimensions; the model's sound uses Juiced's Corvette V8
bank. It does **not** contain an authentic Ferrari recording. Wheels use one
shared native wheel mesh, as the stock game does. High-speed wheels retain this
mesh instead of a donor's unrelated blur geometry. No new decal kit or detachable
panels are claimed; damage uses the game's existing deformation system.

## Implementation and validation

`decode.cjs` decompresses the pinned GLB and simplifies with normal attributes
included, preserving shading seams. `import_ferrari.py` bakes transforms, converts
handedness and units, partitions panels, exports D8M/BOH/PMD/DDT/OCD assets, and
patches a local copy of the owner's retail scripts. All paths in the overlay are
lowercase. `retail.py` reads DAT archives without modifying them.

`mac_car_mod_check` verifies both garage and race loading, all four wheels,
wheelbase, collision meshes, vertex deformation/repair, each sport-part swap,
and increased power with fitted performance parts. Runtime captures exercise the
stock selector, native Auto-Mod (567 → 1,018 bhp), a 1,800-frame dry driving run
and a 900-frame wet/night run. Regular CTest checks cover the file
overlay and the port's existing regressions.

Generated `scripts/cardefs.txt`, `dynamic_falcon.txt` and `arcadecollection.txt`
contain locally derived retail data. Do not include those files in a public
source release. Share this importer and its licensed model so each owner can
build the mod against their own copy.

For a local standalone test app, use `install_macos.py --base-app /Applications/Juiced.app --exe out/juiced --mod /path/to/Ferrari458 --output "/Applications/Juiced Ferrari 458.app"`. This preserves the base app and uses a separate save directory under Application Support/Juiced Mods/Ferrari458.
