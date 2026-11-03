# Build and run OpenJuice on macOS

The current engine source is OpenJuice 1.02.7; the Mac app bundle version is
1.2.7. See [device validation](native-effects-device-validation-2026-11-03.md)
for the actual Apple Silicon tests and remaining coverage limits. Compile
locally on a Mac; a GitHub Actions runner is not required.

## Build

Requires Xcode Command Line Tools, a Metal-capable Mac and the dependencies below.
Apple Silicon was tested; Intel was not.

```sh
xcode-select --install
brew install cmake ninja pkg-config ffmpeg lua
git submodule update --init external/JuicedConfigExt
cmake -S . -B out/macos -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/macos --target juiced juiced_app juiced_tests \
  juiced_native_effects_check mac_controls_check mac_controls_overlay_check \
  mac_reflection_check -j 4
JUICED_SUPPORT_DIR="$PWD/out/macos/test-support" \
  ctest --test-dir out/macos --output-on-failure
out/macos/juiced_native_effects_check "$PWD/out/macos/gpu-captures"
```

CMake fetches the pinned SDL and MojoShader revisions. The unbundled development
executable links the installed FFmpeg/Lua libraries. Native Metal effects include
lighting, shadows, procedural clouds, SMAA and heat haze; the pixel tests exercise
their GPU paths. This is not a guarantee of parity across every scene/weather
combination or a frame-rate target. MSAA is not advertised by the native backend.

## Run

Supply your own extracted Juiced data, including `tracks/` and `carmodels.dat`:

```sh
out/macos/juiced --game /absolute/path/to/Juiced
```

The path is remembered. `out/macos/Juiced.app` opens a data-folder picker when
there is no valid remembered path. To enable the repository's optional packages:

```sh
out/macos/juiced --game /absolute/path/to/Juiced --mods "$PWD/mods"
```

Use **Options → Mods** to select packages, then restart. The collection includes
the Ferrari, ten everyday cars, AI Hero Liveries and optional gameplay/UI mods.
Mods share careers; adding a package does not deliberately invalidate a save.
Import validation is not a promise of compatibility with every third-party mod.

Settings, controls and the remembered data path live in
`~/Library/Application Support/Juiced/`. The native default save directory is
`~/Library/Application Support/Juiced/ModSaves/`. The packaged Mod Garage launcher
retains `~/Library/Application Support/Juiced Mods/ModGarage` for existing users
and loads editable packages at `~/JuicedMods/mods`.

**Options → Controls** opens the keyboard editor; `--controls` opens it directly.
Enter selects and Escape goes back. The optional Modern Controls package provides
WASD driving, Shift nitrous, Space handbrake, Q/E gears and 1–6 crew commands;
saved bindings override defaults. Android's Retroid controller tests do not
establish Mac controller/wheel/rumble support.

For an isolated diagnostic race, keep its saves and settings away from real careers:

```sh
JUICED_SUPPORT_DIR="$PWD/out/macos/race-support" out/macos/juiced \
  --game /absolute/path/to/Juiced --track 'Angel Westside/AngelWest_R1/1/1' \
  --car Focus_2 --cars 3 --test --autopilot --frames 600 \
  --world-light 2 --sky 2 --headlights 2 --capture "$PWD/out/macos/race.bmp"
```

Capture paths must be absolute because the engine changes its working directory
to the game data folder. Screen exports are opaque RGB; diagnostic render-target
readbacks retain the effects' original alpha channel.

## Package a local application

Start with an unused staging directory:

```sh
mkdir -p out/staging
ditto out/macos/Juiced.app out/staging/Juiced.app
cmake -DAPP="$PWD/out/staging/Juiced.app" -P cmake/BundleMac.cmake
python3 tools/mod_bundle/install_macos.py \
  --base-app "$PWD/out/staging/Juiced.app" \
  --exe "$PWD/out/staging/Juiced.app/Contents/MacOS/Juiced" \
  --output "$PWD/out/staging/Juiced Mod Garage.app" \
  --scripts "$PWD/mods" --creative-mods
```

The first helper bundles and relocates non-system dylibs, then signs and verifies
the app. The second creates the mod launcher with package defaults and inherits
the current app version. Keep copies of installed apps, editable mods and saves
before replacing a working installation. These are locally ad-hoc signed builds,
not Apple-notarized distribution artifacts. Game data is not included.

## Upstream and license

Engine reconstruction by [Sidewinder / sidecr3w](https://gitlab.com/sidecr3w/openjuice).
Required Notice: Copyright 2026 Sidewinder (https://gitlab.com/sidecr3w/openjuice).
See [the engine license](../LICENSE.md) and component license notices. The native
ports and mods are integration work built on that upstream engine.
