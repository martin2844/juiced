# Juiced for macOS

A native macOS port of [OpenJuice](https://gitlab.com/sidecr3w/openjuice), the recreation of Juiced (PC, 2005). Uses SDL 3, Metal, MojoShader and FFmpeg. Runs the engine directly on Apple Silicon; Wine, Windows and the original game executable are not used.

**Early development build, not yet a complete or visually faithful port.** The native build loads the game's data, displays the interface, renders a race, runs physics and AI, and decodes music. Rendering compatibility and broader gameplay validation remain work in progress. See [the port status](docs/macos-port.md).

Game assets are not included. Supply your own Juiced installation or extracted data.

## Build

Requires a Metal-capable Mac, Xcode Command Line Tools, CMake, pkg-config and FFmpeg. Tested on Apple Silicon; Intel builds have not been tested. Install build dependencies with Homebrew:

```sh
xcode-select --install
brew install cmake pkg-config ffmpeg
cmake -S . -B out -DCMAKE_BUILD_TYPE=Release
cmake --build out --target juiced juiced_app juiced_tests mac_controls_check mac_controls_overlay_check mac_reflection_check -j 8
ctest --test-dir out --output-on-failure
```

CMake downloads pinned revisions of SDL 3 and MojoShader on the first configure. FFmpeg is dynamically linked from the local installation. `out/Juiced.app` is a local development app, not a self-contained redistributable bundle; it still needs those installed FFmpeg libraries. No signing identity is required to build locally.

## Run

```sh
./out/juiced --game /absolute/path/to/Juiced
```

The data folder contains `tracks/` and `carmodels.dat`. The path is remembered. You can subsequently open `out/Juiced.app`; when no valid remembered path exists, it opens a folder picker. Intro movies are currently skipped. Keyboard input is implemented; controllers and steering wheels are not yet connected. The default driving controls are W/S for throttle/brake, A/D for steering, Left Shift for nitrous, Space for handbrake, and R for reverse. Enter selects and Escape goes back.

An instant race bypasses profile creation:

```sh
./out/juiced --game /absolute/path/to/Juiced -i
```

A reproducible AI-driven race and screenshot:

```sh
./out/juiced --game /absolute/path/to/Juiced -i --autopilot \
  --seed 1 --frames 2100 --capture "$PWD/out/race.bmp"
```

Capture paths should be absolute: the engine switches its working directory to the game folder. `--frames` and scripted input switches remain enabled in this port's release build. Logs, settings and the remembered game path live in `~/Library/Application Support/Juiced/`; stock-format saves live in `~/Library/Application Support/THQ/Juiced/`. To change data folders, pass `--game` again.

## Controls editor

Open **Options → Controls** in the game (also available through the race pause menu). The editor renders inside the game window; the game is frozen while it is open. Click a binding and press a replacement key, or use Up/Down and Enter. Left/Right switch the Driving, Gears & Camera, and Crew Commands tabs. Escape cancels key capture, or closes the editor without saving when not capturing.

If a key is already assigned, the two actions swap keys. **Save** applies changes immediately and stores them in `~/Library/Application Support/Juiced/controls.ini`; **Cancel** discards edits, and **Restore Defaults** resets the draft until saved. Escape and Enter remain reserved for navigation. Bindings use physical key positions, so labels follow a US layout on non-US keyboards.

Defaults: WASD driving, Left Shift nitrous, Space handbrake, R reverse, Q/E manual gears, C camera, B rear view, Z/X look left/right, H horn, and 1–6 crew commands. These are keyboard bindings; gamepad and wheel support is still pending.

To open the same in-game editor directly:

```sh
./out/juiced --controls
```

## Source and license

Based on upstream branch `retail/1.01/dx9`, commit `bd7e638b187e999c13f85a608d007108a228a05b` (see the exact revision in [provenance](docs/macos-port.md)). The original source import is kept as the repository's first commit. Native adapters are in `src/Mac/`; Windows source and upstream engine documentation remain available.

Required Notice: Copyright 2026 Sidewinder (https://gitlab.com/sidecr3w/openjuice)

[PolyForm Noncommercial 1.0.0](LICENSE.md) applies to the engine and this port. This is source-available under noncommercial terms, not a permissive open-source license. SDL, MojoShader, FFmpeg and bundled third-party code retain their respective licenses. The game's assets are separate and are not distributed here. The original README is preserved in [docs/UPSTREAM-README.md](docs/UPSTREAM-README.md).
