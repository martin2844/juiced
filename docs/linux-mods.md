# Native Linux mods checkpoint — 2026-10-25

Integrated the six packages from martin2844/juiced at `1f4693b` into the
Linux/NixOS port of upstream OpenJuice 1.02.5 (`4e5ab27`). The prior Linux
checkpoint is `6dadefd`. This retains the newer engine, GL reflections and native
threading fixes. No Windows executable or Wine is used.

## Installed usage

Super+Space → OpenJuice launches with all six packages enabled: Modern Controls,
Custom Logo, Driver Photo, Ferrari 458 Spider, RGB Neon and Eagle Ultimate Wheels.
Options → Mods enables/disables packages; restart to apply. The desktop entry
also offers Manage mods and Play without mods. Options → Mod Garage imports
photos/logos. The Ferrari is an independent car, available in custom race and
the dealer. Wheels are available through the normal garage menus.

The launcher seeds missing files from repository `mods/` into
`~/.local/share/openjuice/mods`, preserving installed edits and disabled markers.
Source tools and generated caches are excluded. Original `game-data` is unchanged:
package assets and patches are assembled into temporary overlays removed on exit.
All careers live in `~/.local/share/openjuice/ModSaves`, with or without mods.
Old profiles in `saves`, `LegacyModSaves` and `ModSaves/variants` are copied into
that directory without replacing current profiles or deleting originals. Mod
catalogue compatibility is not enforced. Creative preferences and images live
in `CreativeMods` within the shared save directory.

## Native implementation and rebuild

Linux now links Lua 5.4 and GdkPixbuf through Nix. The SDL file dialog replaces
AppKit selection; bounded image decoding, orientation, crop/fit, transparency,
BGRA conversion and DDT output replace ImageIO. Shared game hooks enable the Lua
runtime, catalogue extensions, menus and creative features on both native hosts.

Build with `nix-build -j1 --cores 12 -o out/linux-mods-result`. The installed
launcher uses `out/linux-result`; `out/linux-1.02.5-result` retains the earlier
unmodded build for rollback. Source assets are separate from the Nix executable.
Explicit flags: `--mods DIR`, `--creative-mods`, `--mod-defaults DIR`,
`--mods-menu`, and `--no-mods` (takes precedence over enable flags).

## Verification

- Nix build and all four CTest suites pass; core runner: 188 passed, zero failed.
- All four Ferrari/wheels combinations and package toggles share one save directory.
- Ferrari definition, independent ownership/upgrades, model, lights, collision,
  tuning and save roundtrip checks pass against actual assets.
- All 128 imported wheels fit, select and save; total catalogue is 202 wheels.
- Image decoding/persistence and custom logo geometry checks pass across cars.
- Native GL neon preview, cancel, purchase and persistence checks pass.
- Ferrari selection rendered; a six-Ferrari race ran 900 frames with physics,
  autopilot and HUD. This is a smoke test, not a complete career playthrough.
- The installed Super+Space wrapper rendered the mod manager for 100 frames;
  its log confirms all six packages enabled in the normal XDG installation.
- Shell syntax and desktop-entry validation pass.

Evidence is local in `out/validation/mods/`: package/car/wheel/creative/unit logs,
`manager.png`, `installed-manager.png`, `ferrari-select.png`, `ferrari-race.png`
and `neon-picker.png`. The image chooser itself has not been manually clicked
through; its decoding and persistence paths were exercised directly. The car
selection capture still shows some floor-reflection artifacts; this checkpoint
does not claim every rendering defect is resolved.
