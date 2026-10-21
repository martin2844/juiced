# Changelog

All notable changes to OpenJuice are documented in this file. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Each release comes in two builds with the same changes: 1.00.x for the retail game 1.00 (branch `retail/1.00/dx9`)
and 1.01.x for the retail game with the official 1.01 patch (branch `retail/1.01/dx9`).

## [1.00.2 / 1.01.2] - 2026-10-21

### Fixed

- Windows 7: the game did not start ("The procedure entry point CreateFile2 could not be located in KERNEL32.dll").
- Saves: nothing could be saved where the original game had never run (no `%APPDATA%\THQ\Juiced` folder): "You will
  not be able to save your progress", then a PS2 memory card question. The folders are now made at start, as the
  original game does.
- Linux / Proton: no car or world sounds in races, on the dyno and in test drives.
- Pause: particles, animated materials and reflections kept moving while the race was paused.
- Dyno: a dyno run wore the brakes, so the garage then asked for a $500 repair.
- Menus: a car of "Cars in group" in the custom race's Performance Group window blinked along with the highlighted
  group; the same in High Scores and in the online arcade's create window.
- Menus: losing the window's focus showed the PS2 line "To continue, please reconnect the DUALSHOCK®2 analog
  controller..."; the screen now fades to black as in the original game.

### Removed

- The unfinished Direct3D 11 renderer stub: the original game has only Direct3D 9.

## [1.00.1 / 1.01.1] - 2026-10-21

Initial release.

[1.00.2 / 1.01.2]: https://gitlab.com/sidecr3w/openjuice/-/compare/v1.00.1...v1.00.2
[1.00.1 / 1.01.1]: https://gitlab.com/sidecr3w/openjuice/-/releases/v1.00.1
