# Native Linux / NixOS port — updated to 1.02.5 on 2026-10-25

This port builds the engine source into a native Linux executable. It does not run
Juiced.exe or OpenJuice.exe. Original game assets are required and are kept outside
the source repository and Nix store.

## Current release

The base is upstream [v1.02.5](https://gitlab.com/sidecr3w/openjuice/-/releases/v1.02.5),
commit `4e5ab27` (October 24). The changes from upstream v1.01.2 to v1.02.5 were
merged onto the existing platform port; conflicts in engine logic were resolved
using v1.02.5. The previous Linux port is preserved in commit `bde984b`.

Linux-specific additions for this release:

- Marshal background-loader graphics resource operations to the OpenGL context
  owner, including servicing pending work while the game waits for a load.
- Specialize Shader Model 1 pixel samplers for the actual bound cube textures,
  with cached variants and the matching uniform layout. This fixes the white
  glass/reflection corruption without inventing materials or changing assets.
- Use POSIX local time for replay dates and iconv for Windows-1250/1251/1252 names.
- New `juiced_gl_check` regression: six independently coloured cube faces,
  repeated cube/2D switching on one shader, and texture creation from another
  thread. Run it manually in `nix-shell` on a real OpenGL driver. It is intentionally
  not part of sandboxed CTest, which has no GPU/display.

The menu's blue footer is present in the original `ui.dat` → `windows/driver.ui`:
BOTTOMPANEL and BLUEBOX1 explicitly specify `Colour 255 0 0 255`. It is not a colour
conversion defect and has not been recoloured. The original data files are intact.

Windows DPAPI credential encryption is unavailable on Linux. Like upstream on
DPAPI failure, the Linux port does not persist online credentials and never falls
back to plaintext. Guest/network behavior has not been validated against a live
service. Original Windows encrypted credentials cannot be read here.

Validation for 1.02.5: **178 unit checks passed, 0 failed**; the manual GL regression
passed; a 1,700-frame autopilot race and 1,500-frame title/profile-navigation run
completed. White glass patches and black road artifacts from the previous captures
are absent in the new race capture. No full-career or all-cars parity claim is made.
The async car-selection preview also completed (700 frames), and the fly-by was
captured separately at 900 frames. The installed Nix package passed CTest.

Installed package: `/nix/store/f5hmisn2xwfihcjqy9j3wlvm4dfdgbaw-openjuice-1.02.5-linux`.
The launcher resolves it through `out/linux-result`; `out/linux-previous-result`
retains the previous package as a rollback root. Source and runtime logs, screenshots
and the asset audit are in `out/validation/update-1.02.5/`. See the separate
[installed-mod audit](mod-audit-2026-10-25.md).

## Initial port provenance

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

## Initial port validation and limitations (historical)

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
