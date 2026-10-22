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

## [1.00.4 / 1.01.4] - 2026-10-22

### Fixed

#### Starting and Windows

- OpenJuiceConfig opens on Windows Vista and later: like the original JuicedConfig it took DirectX 10 .. 12 for
  older than 9.0c and only showed "You need to install DirectX9.0c runtime or better"; both builds. (5767ead)

#### Races

- Solo races and showoffs start behind the car with the 3-2-1 count, without the fly-by (and the black screen after
  skipping it), as in the original game. (aa8eabf)
- Speed challenge: the target shows the challenge's speed (80 KPH, not 180). (ef4c50b)

#### Sound

- The test track's intro silences the engines as it fades to black, as in the original game. (5da4a8e)

#### Career

- The 1000-respect host challenge and the pink slip races ask for the class of the player's best car, not class 1.
  (143a6bd)
- Respect Update: the descriptions are white again; only the title and the figure are coloured. (14a82c9)
- Hosting an event that fewer than 2 crews would attend is refused with "You need at least 2 crews in attendance", as
  in the original game. (4ffabef)

#### Menus and the garage

- Car Spec's mod icons no longer stay faded or unseen after a visit. (b7d55b5)
- The workshop's menu no longer vanishes when Crew Car Scheme is applied quickly. (75c6a40)
- The workshop's and the dealership's car view fade in from black after "Please wait". (25afcb9)
- The race event's PRIZE, TYPE and LAPS titles fade in with the window. (d7cd47c)
- Cars shown together (the betting window, the grid walk, split screen and the network windows) each keep their own
  neon colour. (6a6fdb6)
- The grid walk's cars turn from the original game's angle, and its loading screen leaves a frame later, as there.
  (04e90c9)
- The maps' camera range starts where the original game's does. (7e75015)

#### Online

- The crew race's driver window shows the empty slots' boxes as the original game does. (32f48aa)
- The search results' GameSpy logo fades in with the window. (7cdc597)

### Known issues

- The EAX reverb was reported to sound louder than in the original game; not confirmed.
- Windows Defender's cloud protection may still warn about the executables as new, unsigned files.
- Controls: on Windows 7 with DIMapper, the Controls option's mapping window works in the 32-bit build only.

## [1.00.3 / 1.01.3] - 2026-10-22

### Fixed

#### Starting and Windows

- Windows 7 support no longer looks system functions up by name as the game runs: the same file and clock functions
  on every Windows. This was the likely cause of Windows Defender flagging the 64-bit 1.01.2 (Contebrew.A!ml).
  (0f738a0)
- Alt alone opened the window's system menu, which held the game; as in the original game Alt+Enter minimizes and Alt
  alone does nothing. (4e67cd1)

#### Races

- The pre-race fly-by changes shots through the camera director's transition, and the sweep before the start runs
  across the route's whole grid (on the test track it stood still on the car's front). (a581fab, f6e5375)
- Career: an event the player hosts was raced as the player's crew's challenge: every car finished out of time at the
  start and the challenge failed. (6f15471)
- Career: the crew's orders open in the frame the race's chatter or a crewmate's nerves ask for them, as in the
  original game. (7a6600c)

#### Sound

- Audio options: the effects' and the speech's test sounds restarted on each step of their sliders. (59ee33c)

#### Menus and the garage

- Cars in the menus' 3D views (garage, dealer, car lists) load in the background as in the original game: scrolling a
  list loads only the car it stops on, and the windows no longer wait for each car. (a7cede1)
- A held direction goes through a list as fast as in the original game. (7074a5b)
- The menus' 3D views draw a car's glass over the spoiler behind it. (3b0138e)
- Garage: auto-mod and changed parts left the car at its old ride height in the preview. (1f3feac)
- Wheel colour: it no longer reaches the standard wheels, which the original game never colours, and the crew scheme
  window's preview no longer leaves its wheel colour on the car. (ec5ad72)
- Car models: the primer grey part of the S2000's chassis and of the Golf's farthest logo, as in the original game.
  (e13af25)
- Custom race setup: each page's menu comes in from black. (249e4fd)
- Custom race on the network: the client follows the host's race type; a network sprint has one heat. (a3656f2)
- Message windows: the brackets jump to the highlighted answer instead of gliding to it. (578ee6c)
- The legal screens at the start cross-fade as in the original game. (4e0027e)
- Going back to the title screen resets the music and effects volumes to a new driver's, as in the original game.
  (13aea90)
- Credits: the text is white. (18661c1)

### Added

- OpenJuiceConfig (`OpenJuiceConfig.exe`, `OpenJuiceConfig64.exe`): OpenJuice's copy of the game's settings tool
  `JuicedConfig.exe`, in the game's eight languages; in the release and debug packages. (021f632)

### Known issues

- The EAX reverb was reported to sound louder than in the original game; not confirmed.
- Windows Defender's cloud protection may still warn about the executables as new, unsigned files.
- Controls: on Windows 7 with DIMapper, the Controls option's mapping window works in the 32-bit build only.

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

[1.00.3 / 1.01.3]: https://gitlab.com/sidecr3w/openjuice/-/compare/v1.00.2...v1.00.3
[1.00.2 / 1.01.2]: https://gitlab.com/sidecr3w/openjuice/-/compare/v1.00.1...v1.00.2
[1.00.1 / 1.01.1]: https://gitlab.com/sidecr3w/openjuice/-/releases/v1.00.1
