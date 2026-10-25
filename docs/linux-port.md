# Native Linux / NixOS port — 2026-10-25

This port builds the engine source into a native Linux executable. It does not run
Juiced.exe or OpenJuice.exe. Original game assets are required and are kept outside
the source repository and Nix store.

## Source baseline

- Engine and portable SDL platform/input/audio: main `622c6071e441e3b2b8f53ee6352279cb4a61982b`.
- OpenGL renderer and case-insensitive asset lookup adapted from `origin/android`
  `4bf7f1baff95a81ff7b8ea8afc0c95c0805f4bf5`.
- MojoShader pinned to `ad5dff84830c2863c841f4b1f4e3df78c705b383`.
- Working branch: `linux-nixos`.

The newer main engine is retained. Only the graphics backend was brought across
from Android; Android lifecycle, older engine logic and device resolution caps are
not used. Existing portable helpers under `src/Mac` are shared with Linux through
`OJ_LINUX`; no AppKit, Metal, Wine or Windows runtime is linked on Linux.

## Build and run on NixOS

```sh
nix-build -j 1 --cores 8 -o out/linux-result
./out/linux-result/bin/juiced --game /path/to/Juiced
```

The result symlink keeps the package and its dependencies rooted against Nix garbage
collection. Keep it while using the desktop launcher. The wrapper makes the system
OpenGL driver available through `/run/opengl-driver/lib`.

For iterative development:

```sh
nix-shell
cmake -S . -B out/linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/linux -j 8
ctest --test-dir out/linux --output-on-failure
out/linux/juiced --game /path/to/Juiced
```

Other Linux distributions need CMake, a C++20 compiler/library, Ninja, pkg-config,
SDL3, OpenGL development headers/libraries, and FFmpeg development libraries
(avformat, avcodec, avutil, swresample). The CMake target is portable, but only
NixOS x86_64 with AMD Mesa has been exercised here.

## Data, configuration and launcher

On this machine the data is `/home/martin/Projects/juiced/game-data`. The engine
reads the original archives directly. File lookup resolves Windows separators and
case differences on Linux without renaming the user's files.

Defaults:

- Settings and remembered asset directory: `~/.config/openjuice`.
- Saved games: `~/.local/share/openjuice/saves`.
- Runtime log: `~/.local/state/openjuice/Juiced.log`.

XDG_CONFIG_HOME, XDG_DATA_HOME and XDG_STATE_HOME are respected. Setting
JUICED_SUPPORT_DIR overrides all three for isolated validation. Tests below use
that override so they do not touch normal player saves.

The managed desktop integration lives in `~/Projects/nixos-config`:
`home/.local/bin/openjuice`, `home/.local/share/applications/openjuice.desktop`
and corresponding `links.json` entries. The wrapper runs the Nix result with the
local original assets. Super+Space → OpenJuice launches it. Desktop actions also
offer an instant race and the keyboard binding editor.

Menus use arrows, Enter and Escape. Driving defaults: WASD, Space handbrake,
Left Shift nitrous, C camera. Use `--controls` to edit bindings, `-i` for instant race.
The inherited behavior pauses a race when its window loses keyboard focus.

## Validation and limitations

The development build passed CTest and its unit runner reported **122 checks
passed, 0 failed**. Linux excludes CPU-instruction, voice-chat and Windows system
command tests, as those target unimplemented platform facilities.

Runtime validation on an AMD Radeon RX 7800 XT:

1. Normal visible window: disclaimer, legal/title sequence rendered from original
   assets; 1,000 frames captured.
2. Scripted Enter navigation: reached PROFILE MANAGER / CREATE NEW DRIVER, with
   native Bink video background; 1,500 frames captured.
3. Instant race: original track and Ford Focus loaded; countdown and autopilot
   physics ran, speed increased and the lap state advanced. Visible-window
   validation also confirmed focus-loss pause behavior.
4. Final Nix package launched through `~/.local/bin/openjuice`: 1,700 frames,
   clean exit, race countdown completed and speed reached 36.3 in engine units.
   The package build also passed CTest. The installed ELF is x86_64 Linux.

Local screenshots and isolated saves are under `out/validation/` (not committed).
These checks establish startup, navigation into profile management, and a running
race. They do not establish a complete career playthrough, parity of every shader,
or support for every GPU/distribution. Some glass/material and UI rendering
artifacts are visible with the imported GL backend and remain to be corrected.
Online, voice chat, wheel/force-feedback and gamepad support are not validated.
Audio banks and music decoded during runtime; no listening-based audio QA was done.
