# macOS 0.3.0 (2026-10-21)

- Synchronize upstream OpenJuice fixes through `2805fd2`, retaining the native controls editor and platform backend.
- Preserve frame colour for menu transitions; clear new Metal targets on creation/resize.
- Increase paint fill lighting and reduce car-glass reflected colour. This is native-port visual tuning, not a physics change.
- Add GPU material/frame regressions and enable the upstream audio and garage tests on macOS.

# Changelog

All notable changes to OpenJuice are documented in this file. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Each release comes in two builds with the same changes: 1.00.x for the retail game 1.00 (branch `retail/1.00/dx9`)
and 1.01.x for the retail game with the official 1.01 patch (branch `retail/1.01/dx9`). The codes in brackets are the
commits of `retail/1.00/dx9`; the 1.01 branch carries the same changes under commits of its own.

## [1.00.2 / 1.01.2] - 2026-10-21

### Fixed

#### Starting and saving

- Windows 7: the game did not start ("The procedure entry point CreateFile2 could not be located in KERNEL32.dll").
  (15ff843)
- Saves: nothing could be saved where the original game had never run (no `%APPDATA%\THQ\Juiced` folder): "You will
  not be able to save your progress", then a PS2 memory card question. The folders are now made at start, as the
  original game does. (3248dc9)

#### Races

- A crash loading a race after a wet one (seen entering arcade series such as Challenge Series 3 and Modern V8 Series
  2). (feb2b9f)
- The cars start in the original order, back to front in arcade, custom and split screen races (the player's car was
  on pole); a sprint's heats move the grid on. (a7f3638)
- Pause: particles, animated materials and reflections kept moving while the race was paused. (326e8bc)
- HUD: it showed during the pre-race fly-by after a pause; as in the original game it now stays hidden there and
  while looking back or panning from the bumper or bonnet view. The lap time challenge's target time no longer fades
  out after the start. (bd90a1b)

#### Sound

- Linux / Proton: no car or world sounds in races, on the dyno and in test drives. (9507220)
- The engine stuttered when two cars shared an engine sound (both FTOs of the first arcade race): its voice switched
  between the cars every other frame. Stopped sounds now fade out as in the original game. (0156bd7, f05b282)
- 3D sounds swung from speaker to speaker as the camera turned (spinning the car in place, most heard with EAX); they
  now keep their side. The voice manager follows the original game in a few more places. (7014f4d, 1dec11a)
- After a skid on tarmac the tarmac tyre squeal kept sounding while the car stood off the road. (a7a91d3)

#### Menus and the garage

- A car of "Cars in group" in the custom race's Performance Group window blinked along with the highlighted group;
  the same in High Scores and in the online arcade's create window. (25a2c63)
- Losing the window's focus showed the PS2 line "To continue, please reconnect the DUALSHOCK®2 analog
  controller..."; the screen now fades to black as in the original game. (d37e9ef)
- Arcade: long texts wrap as in the original game (the results' comment, the race screen's car name and challenge,
  the loading animation's text) instead of running off the screen. (20b7ced)
- Custom race: clearing the auto-tuning left its vinyl on the car. (bd945ca)
- Garage: the HUD colour window showed a plain box of the dial's colour in place of the highlighted dial. (5043f37)
- Dyno: a dyno run wore the brakes, so the garage then asked for a $500 repair. (c368667)

#### Controls

- The Controls option opens DirectInput's mapping window as the original game does (Windows 7 with DIMapper,
  Wine/Proton), and the keyboard follows the control scheme saved there, also one saved by the retail game (the 32-bit
  build; 64-bit DirectInput keeps its own). (a1c1018)

### Removed

- The unfinished Direct3D 11 renderer stub: the original game has only Direct3D 9. (fdf0093)

## [1.00.1 / 1.01.1] - 2026-10-21

Initial release.

[1.00.2 / 1.01.2]: https://gitlab.com/sidecr3w/openjuice/-/compare/v1.00.1...v1.00.2
[1.00.1 / 1.01.1]: https://gitlab.com/sidecr3w/openjuice/-/releases/v1.00.1
