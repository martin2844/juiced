# macOS 0.3.0 (2026-10-21)

- Synchronize upstream OpenJuice fixes through `2805fd2`, retaining the native controls editor and platform backend.
- Preserve frame colour for menu transitions; clear new Metal targets on creation/resize.
- Increase paint fill lighting and reduce car-glass reflected colour. This is native-port visual tuning, not a physics change.
- Add GPU material/frame regressions and enable the upstream audio and garage tests on macOS.

# Changelog

All notable changes to OpenJuice are documented in this file. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

The unofficial patch 1.02 (branch `unofficial-patch/1.02/dx9`) is built on OpenJuice for Juiced 1.01 (branch
`retail/1.01/dx9`): the patch 1.02.N contains the retail release 1.01.N and the patch's own changes. The patch's
sections list its own changes, with the commits of `unofficial-patch/1.02/dx9` in brackets; the retail releases'
sections follow.

Each retail release comes in two builds with the same changes: 1.00.x for the retail game 1.00 (branch `retail/1.00/dx9`)
and 1.01.x for the retail game with the official 1.01 patch (branch `retail/1.01/dx9`). The codes in brackets are the
commits of `retail/1.00/dx9`; the 1.01 branch carries the same changes under commits of its own.

## [1.02.5] - 2026-10-24

First release of the unofficial patch 1.02: OpenJuice 1.01.5 (Juiced with the official patch 1.01) plus 124 fixes of the
original game's bugs and 9 additions. Technical details of each change: `docs/fixes.md`.

### Added

#### Online

- Online play restored through OpenSpy (GameSpy's servers were shut down in 2014): quick join, search, create, friends
  and leaderboards. (7a06aa7)
- Lobby chat in online sessions: SELECT2 in the lobby (gamepad button 2, Page Down) opens it. (22ffea2, ed26a4a)
- Play as Guest: online play without a GameSpy account, under the driver's name (no friends, leaderboards or stats).
  (04517be)
- The GameSpy account is stored encrypted (Windows DPAPI) in the save instead of plain text in settings.dat; an existing
  account is converted on the next save. (a3fbf9f)
- Smoother, more accurate online cars: positions are compensated for each player's ping and sent 20 times a second (was
  5-10). (17c4311)
- Collisions between players running the patch are felt on both machines. (6def657)

#### Gameplay

- Replays can be saved and watched: quitting a replay offers to save it; saved replays are under Extras > Replay, stored
  in `My Documents\THQ\Juiced\Replays`. (8b12a7d, 4fa08bf, 2ab2ae6)

#### Graphics

- The Glow option is a 0-100 % slider (0 = off) instead of Yes / No. (a5fb7e8)

#### Modding

- Wing mirror parts (`cardefs.txt` WMIRROR) are fitted and unlock with the car's upgrades; the game's own cars have
  none. (d0b601d, fca5863)

### Fixed

#### Stability

- Fixed a possible hang after a display reset (e.g. Alt+Tab) when a shader cannot be created. (d1a615a)
- Fixed a crash in the online leaderboards (top showoff scores). (be6aeb6)
- Fixed the online login freezing the game; it now runs in the background, times out after 60 seconds and reports every
  failure. (a3fbf9f)
- Fixed online leaderboard requests waiting forever on a failed connection. (bed38d3)
- Fixed a rare race start where the countdown stopped at 0 and the race never started. (77962f2)
- Fixed memory leaks in the EAX check of the sound system and of OpenJuiceConfig. (889e779)
- Fixed oversized data files (crews, challenges, arcade races, routes, decals) writing past their tables; the game's own
  files are not affected. (3bfadc8)
- Fixed online nicknames of 16 characters or more overwriting a session setting. (0c4b094)

#### Graphics

- Fixed the saved Detail setting being ignored until the graphics options were opened. (f4f80e4)
- Fixed the glow blur and the camera-cut transition being drawn into an internal buffer instead of the screen. (6074bf0)
- Fixed a black first frame with glow on, and a slightly blurred picture with motion blur, depth of field and camera
  cuts. (7e95ec5)
- Fixed track lighting: the second light took the third light's colour and the third had none. (a49b5cf)
- Fixed wrong light colours on the low shader detail (fixed-function lighting). (410d9da)
- Fixed rendering errors after the reflection maps are updated. (6a9b830)
- Fixed car windows vanishing behind tyre smoke and decals. (da2d118)
- Fixed wing mirrors, headlights and rear lights staying fully lit while the car is in shadow. (a814b8c)
- Fixed some car parts staying primer grey (S2000 chassis piece, Golf's rear logo). (c3d356d)
- Fixed crew paint schemes losing their metallic finish. (b1f8714)
- Fixed the sun's reflection on cars missing in menu scenes. (50146fe)
- Fixed the last atmosphere zone's look staying on after leaving all zones. (682856c)
- Fixed tyre smoke being too transparent. (c06473c)
- Fixed uneven depth-of-field blur. (45a8f95)
- Fixed rain splashes being left behind a moving camera, the last rain streak not being drawn, and raindrops on the
  camera flashing opaque. (0adc767, 6a258d2, a211db3)
- Fixed only half of the falling leaves being drawn. (4ec2fc7)
- Fixed the opacity of the first skid mark segment overflowing. (bc65977)
- Fixed distorted HUD dial needles. (06ec1b1)
- Fixed the rolling road graph being misplaced and unscaled. (39c6655)
- Fixed the highlighted dial missing in the HUD colour menu. (81c822c)
- Split screen: fixed the lower view losing its picture with motion blur, depth of field, camera cuts and screen
  effects. (b81e733)
- Fixed the default graphics settings ignoring the model of an ATI / AMD card. (b186399)

#### Sound and movies

- Fixed movies playing silent until the volume is changed. (7461223)
- Fixed volume and mute on movies with one audio track, and a buffer overrun on tiled movies. (12a14bc)
- Fixed some sounds staying silent for a whole race. (56451cf)
- Fixed sound occlusion getting stuck and jumping. (ad0f787)
- Split screen: fixed the second player's camera turning the first player's car sound away. (79a43df)
- Fixed the third betting suggestion voice line never playing. (21baea9)

#### Races and HUD

- Fixed hundredths of a second never showing 99 (HUD, results, records, challenges). (f20765b)
- Fixed the world's time jumping forward after a second pause. (7bef76e)
- Fixed QUIT on the challenge results leaving without the "QUIT RACE?" question. (8c938e7)
- Fixed empty results rows showing "--:--:--" under Team. (d5792a7)
- Fixed crew results showing another crew's time. (6782573)
- Fixed a sprint restarted from the pause menu keeping the old heats' times for tie-breaks. (3eab40d)
- Showoff: fixed a running trick's points going to a newer trick line and deciding its multiplier. (0f49d2b)
- Fixed the race camera moving too fast below 20 fps. (166d29f)
- Fixed TV cameras turning wrongly while correcting their pitch, and the passing camera's height. (30f27e5)
- Fixed cable cars and other moving track objects starting with the wrong tilt. (1f6074c)
- Fixed replays sometimes never starting. (9965496)
- Fixed the third attract-mode replay never playing. (27848a5)
- Fixed replays reading the wrong throttle for a car that lost a part. (9fc7499)
- Split screen: fixed player 1's HUD messages being cleared at every player's lap. (5f456e6)
- Split screen: each player starts solo races and cruises in their own camera view. (7153f43)
- Fixed a double space in "Player 2" in the pause menu. (515db08)

#### Handling and physics

- Fixed cars losing grip along the wheels when sliding backwards above 8 m/s (after a spin or fast reversing). (b89c206)
- Fixed cars and props at rest shaking slightly. (b89c206)
- Fixed the pitch stabilizer never engaging; a car is now pushed back towards level from 49 degrees of pitch. (b89c206)
- Fixed a turbo lost to damage coming back at 1.3 instead of the car's own value. (b89c206)
- Fixed handbrake values from 100 to 10000 in the car files being raised to 10000: 27 cars (mostly front-wheel drive)
  now have their intended weaker handbrake. (cde1f2d)
- Fixed drive and traction control going to the wrong wheel while a wheel is in the air. (389826f)
- Fixed spin limits using the wrong inertia. (389826f)
- Fixed wall collisions registering up to 12 cm late on some track faces, and other collision geometry errors. (c363ff8)
- Fixed cars sliding along the bins on Southside Beach with no grip or bounce. (f030beb)
- Fixed the AI swerving several times harder with more cars around. (da6948d)
- Fixed collision feedback (wheel jolt, camera shake, rumble, smoke) always using the strongest hit: light hits feel
  lighter. (da6948d)

#### Career, garage and menus

- Fixed accented and non-Latin letters (Czech, Polish, Russian, ...) in names. (ba10c28)
- Fixed the arcade and showoff maps opening at the first location in languages other than English. (7627fd6)
- Fixed metallic and pearl paints being charged as a base paint (or free) without enough money; the full price is now
  required. (0f49d2b)
- Fixed a car staked in a pink slip race coming back repaired. (0f49d2b)
- Fixed the stats graph taking two or three steps for one event. (0f49d2b)
- Fixed Mod Shop upgrades keeping the weaker set's parts and meters when a stronger set replaces it. (0f49d2b)
- Fixed privileges adding respect to your crew. (0f49d2b)
- Fixed cruise results writing invalid winnings for an empty crew place. (0f49d2b)
- Fixed calling a crewmate who has joined: the call now rings out and SELECT answers it. (948dcb8)
- Fixed crew logo 19 never being offered. (349be75)
- Fixed the decal colour chart showing the pearl colour, and BACK in the wheel colour menu not restoring the car's own
  colour. (6883f5d)
- Fixed SWEET never being chosen as the supplier of custom parts. (d404415)
- Fixed the right side skirt being treated as the left one. (ac33868)
- Fixed BACK in the game options not restoring the difficulty and AUTO2. (c6d2082)
- Fixed custom races: a sprint route in slot 1 counted as none; the route page highlights the right route. (cc00cc0)
- Fixed Arcade Championship highlighting the wrong championship. (f1877ed)
- Fixed Up / Down with no letter selected in the cheats screen leaving to Extras. (9d90e66)
- Fixed the betting screen showing a wrong amount when a raise or lower is refused. (76cb161)
- Fixed "Could not delete driver." closing immediately. (400c1a2)
- Fixed screenshots overwriting an existing empty file. (f9c3231)
- OpenJuiceConfig: fixed resolutions 4096 pixels wide and more, the threading default overriding other settings, and an
  invalid saved shader setting. (249913d)

#### Online

- Fixed pausing an online race freezing the race on your screen while it went on for everyone else; the race now
  continues under the pause menu. (c9bd82d)
- Fixed being logged off when a friend request is refused. (a3fbf9f)
- Fixed a failed login replacing the saved account. (a3fbf9f)
- Fixed account creation with email addresses of 32 characters or more. (a3fbf9f)
- Fixed profiles blocking the game until the server answered. (a3fbf9f)
- Fixed a second login breaking the new connection, and the friends list stopping its refresh. (a3fbf9f)
- Fixed the profile's "Female" read as unspecified, and the same status resent every 60 seconds. (a3fbf9f)
- Fixed ties raising both players' ratings. (f392c65)
- Fixed an online career crew race result not being sent until the next report. (f392c65)
- Fixed leaderboard responses being misread (headers, content length). (bed38d3)
- Fixed reference leaks in the online code. (a3fbf9f, f392c65)

#### Modding and internal

- Fixed the 2-Texture Modulate (Specular) shader classes, negative UV scrolling, the mip levels of `-localfx` procedural
  textures, the HUD's ABS light without a speed display, and a showoff zone's CRUISE_MESSAGE. (837112b)
- Fixed open splines' length and their riders' spacing. (e7ad1d8)
- Fixed the "AUTO-MOD CAR?" question repeating for cars without the first body parts. (ca8e92a)
- Internal fixes with no visible effect on the game's own files: render tree bounds, an unused shader constant,
  crewmates' phone numbers, the credits' third page, territory places. (acc0afb, 1ee6f2e, 89574a7, 93095a2, 326495d)

### Known issues

- Players without the patch have no lobby chat, and collisions are shared only between players running the patch.
- Windows Defender may warn about the executables (new, unsigned files).
- Windows 7 with DIMapper: the Controls mapping window works in the 32-bit build only.

## [1.00.5 / 1.01.5] - 2026-10-24

### Fixed

#### Online

- Fixed brakes having no effect in online races (only the handbrake and engine braking worked). (6530bc5)
- Fixed a softlock after Alt+Tab or a controller disconnect at the start of an online race. (959fcd2)
- Fixed players being disconnected from a network game when loading the track takes longer than 25 seconds.
  (0e5a94b)
- Fixed returning to the LAN browser instead of the Online menu after being dropped from an online lobby or after an
  online race with no players left. (295a5ca, 98c6450)
- Fixed the session's Mods setting being ignored: Standard sessions allowed the prototype auto-mod, None sessions
  allowed parts. (e097d4b)

#### Video and audio

- 64-bit: fixed menu movies playing too fast for a few seconds after switching menus. (c4271c1)
- Fixed the engine pitch jumping when repeatedly looking back. (7e84a7a)

### Removed

- Debug builds are no longer included in releases.

### Known issues

- Windows Defender may warn about the executables (new, unsigned files).
- Windows 7 with DIMapper: the Controls mapping window works in the 32-bit build only.

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
