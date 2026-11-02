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

## [1.02.7] - 2026-10-30

OpenJuice 1.01.7 (Juiced with the official patch 1.01) plus the patch's own additions and fixes below — this round is
mostly graphics: the settings page, the sky, the lights and the shadows. Technical details of each change:
`docs/fixes.md`.

### Added

#### Graphics options

- Options / Graphics: a preset row — Med (the original look), Hi, Ultra and Custom; Reset to Defaults returns to the
  original look from any setting. (b064422, 924d0ac)
- Options / Graphics > Advanced: Anti-aliasing, Texture Filter, Car Materials, Shadows, World Light, Ambient
  Occlusion, Sky, Headlights and Heat Haze, each with its own levels, all applied without a restart (in a race's
  pause too). (9774ea8, ed31128, b064422, b064422, b064422, b064422, b064422)
- Heat Haze: a slider of ten steps (off by default; its strongest is the old 50 %). It is your own setting — the
  presets never change it. (fe5ab5c, 924d0ac)
- The levels are named the same everywhere: Med, Hi, Ultra, Custom. (a2668e5)
- SMAA anti-aliasing, alone or with MSAA 2x. (f66f691, f66f691)
- Light Flares: Off, Stock or Custom — Custom is the glow with less of the night's brightening that burnt out the
  picture. (ea66591, 02a6fdc)
- EAX on/off in the game's Audio options (main menu), heard at once. (fe5ab5c, fe5ab5c)
- Reflections Ultra: the car you watch reflects the cars near it and their lit lights, at half the cost of the first
  version. (f13bf54, f13bf54, d1b267a)

#### Lighting

- World Light: Med the original, Hi the sky's light by the surface's direction with ambient occlusion, Ultra the
  sun's dynamic shadows by day in place of the baked ones. (9e2e8cc, 9e2e8cc, f44d173)
- Headlights: Med the original, Hi the new lights that reach far down the road like a real low beam (cars and walls
  close ahead no longer burn white), Ultra their shadows with soft edges. (ed31128, be07053, be07053, f13bf54,
  66af62b, 66af62b)
- Car Materials (the row called Car Paint before): Hi draws the paint, the glass and the chrome per pixel, with the
  sun's glare on the glass and the chrome, and the headlights' and the street lamps' light on them. (b064422,
  f44d173, f44d173, 924d0ac)
- World Light Ultra at night: the lights and the street lamps shine in the wet road, the puddles stand out, the
  lamps' cones and the headlights' beams show in the haze, and the cars cast shadows under the street lamps.
  (5f819fb, 5f819fb, 5f819fb, 5f819fb, 1254dbe, 47ddb80, 47ddb80, 47ddb80)
- World Light Hi: the trees get ambient occlusion of their own (crowns darker inside and below). (61e541a)
- World Light Ultra: the trees' crowns take the sun's shadows and glow against the sun. (61e541a)
- World Light Hi and Ultra: the nitro's and the backfires' flames light the road and the cars around them, in the
  flame's own colour (the car's neon). (248755b, d73992e)
- Headlights Hi and Ultra: on a dry day the cars switch their headlights on in tunnels and off after them. (d1b267a)

#### Sky

- Sky: Med the original sky, Hi procedural clouds that drift with the wind (stronger in the rain, slower at noon and
  at night), Ultra clouds with real shadows on the ground. (4c171eb, 0ab63e5, 4c171eb, 4c171eb, fe5ab5c, a2668e5)

### Changed

- The Juiced 2 post-processing (bloom and eye adaptation) removed. (be07053)
- Depth of Field removed (the effect and its option). (02a6fdc)
- The drive and traction control back to the original game's handling. (ec97594)
- OpenJuiceConfig: one 32-bit program for both builds; the effects, anti-aliasing and texture filter are set in the
  game now, so its Effects page is gone. (ed31128, 9774ea8)
- World Light Ultra and Headlights Ultra draw their shadows faster: up to 18 more fps on a dry day. (d1b267a)
- Ultra reflections cost half as much: up to 1.3 ms a frame less. (d1b267a)
- The lights reflected in the wet road draw faster (World Light Ultra at night): about a third of the pass's work was
  thrown away. (a2668e5)

### Fixed

#### Graphics

- Fixed the night sky looking washed out on Sky Hi and black on Sky Ultra. (a2668e5)
- Fixed the Ultra clouds looking ribbed and rippling as the view moves. (4c171eb)
- Fixed the original flat clouds still showing over the new ones. (4c171eb)
- Fixed the car paint looking almost matte at night with Car Materials Hi. (f44d173)
- Fixed dark specks and grey blocks around the trees with World Light Hi. (f44d173)
- Fixed the sun's shadows looking pixelated at their edges (World Light Ultra); they are soft now. (61e541a, 61e541a,
  61e541a)
- Fixed the wires' shadows near the view looking dotted by day (World Light Ultra). (cc5c5c2, cc5c5c2)
- Fixed tunnels drawn without the sun's shadows until you drive in (World Light Ultra). (61e541a)
- Fixed the cars' shadows under the street lamps looking pixelated, then as hard dark bands; they are soft and light
  now. (0ab63e5, a2668e5)
- Fixed the street lamps' light, its glare on the wet road and the cars' shadows from them popping in and out at
  night. (248755b)
- Fixed rows of street lamps merging into one bright blob at night, and your own headlights making a white wall in
  the rain ahead. (1254dbe)
- Fixed several cars' headlights and the street lamps blinding on the wet road at night. (1254dbe)
- Fixed the cars' brake lights shining yellow in the wet road at night; they are red now. (0ab63e5)
- Fixed far buildings looking distorted and cut out of the street lamps' haze at night, and trees' crowns flickering
  in a lamp's light. (0ab63e5)
- Fixed dark bands along the verges and across the road under the rain (World Light). (94d625a)
- Fixed the street lamps' light cones rippling far off; they show on dry nights too, a little weaker. (1254dbe)
- Fixed the rain's puddles losing their outline, and the wet road's shine drowning them out. (47ddb80, 47ddb80,
  47ddb80)
- Fixed the cars' paint losing the fog with Car Materials Hi. (61e541a)
- Fixed the nitro's and the backfires' light looking blocky on a car's body. (1254dbe)

#### Menus and settings

- Fixed the game having no controls when the installed game lacks the chosen language's files (English files are used
  in their place); the language in OpenJuice.ini is read in any letter case. (4c171eb)
- Fixed headlights, car paint and world light changed in the race's pause not taking effect. (ed31128)
- Fixed MSAA doing nothing with motion blur, light flares or depth of field on. (2fc615d)
- Fixed the Brightness slider doing nothing. (1a23810)
- Fixed the heat haze missing with motion blur or Stock light flares. (f5af13d)
- Fixed the headlights at night dimming to half once the race's countdown starts (Headlights Hi and Ultra). (5f819fb)
- Fixed the Advanced page's Back button drawn over the Heat Haze row. (a2668e5)
- Fixed switching Reflections to or from Ultra not taking effect until the options were left. (f44d173)

### Known issues

- Windows may say the executables are new and unsigned and ask to check them; this is not a detection.
- Controls: on Windows 7 with DIMapper, the Controls option's mapping window works in the 32-bit build only.

## [1.02.6] - 2026-10-27

OpenJuice 1.01.6 (Juiced with the official patch 1.01) plus the patch's own additions and fixes below. Technical details
of each change: `docs/fixes.md`.

### Added

#### Controls

- Controllers and steering wheels of JuicedConfig Extended built in, no DirectInput: layouts for Xbox and other
  XInput pads, DualShock / DualSense, Switch Pro and Bluetooth pads; steering wheels with separate pedals and
  shifters, with the game's force feedback. (b2f4869, 1dab4a1)
- The controller steers as on the Xbox: no twitchy steering around the centre, less lock at speed. On by default
  (OpenJuiceConfig, Controller tab: Steering as on the Xbox); the game's steering sensitivity still works, its middle
  is the Xbox's. The keyboard and steering wheels are unchanged. (68beaff)
- The controller vibrates as on the Xbox, stronger the harder the car turns (off by default; OpenJuiceConfig,
  Controller tab: Vibration). (c94ad22, 796429b)

#### Settings

- OpenJuiceConfig is JuicedConfig Extended's settings program: display, sound, controller, steering wheel and effects
  in one place. (1154918)

#### Graphics

- Wide screens of any shape: the Widescreen option is a list (4:3, Auto, 5:4, 16:10, 16:9, 21:9, 32:9); the race view
  gets wider instead of stretched, menus spread across the screen (up to 16:9) with nothing stretched, backgrounds
  fill the screen, the HUD sits at the screen's edges. (dda822e, 6dc3bdc, 9b2f2b7, 36fb6e4, 5eac0e7, 6a899e3,
  3f9f4f9)
- Field of view in Options / Game (also in the race's pause menu), in degrees for your screen. (bb2e068, d913b90)
- Screen modes: full screen at the monitor's highest refresh rate, borderless window (picture kept to its shape or
  stretched), window fitted to the screen, choice of monitor. (c4eccba)
- VSync, frame rate limit (high refresh rates, the sound right above 60 FPS), MSAA, anisotropic texture filtering and
  sharpness, FPS counter. (0b46904, cba70ab)
- Effects, all off by default (OpenJuiceConfig, Effects tab): bloom and eye adaptation, world light by direction,
  per-pixel car paint, glass and chrome, headlights lighting the road at night, heat haze from the exhausts.
  (6cd36ec, fc6f188, 87bb7d2, dfa0745, aaddae8)

#### Sound

- Sound through OpenAL Soft: EAX reverb with any sound card (no Creative card or eax.dll needed; turn EAX on in
  OpenJuiceConfig), music and speech without Windows' WMA codecs, voice chat without DirectSound. (0fdb730, e87b591,
  c20f637)

#### Replays

- Change View switches the camera and Rear View looks back while watching a replay (car cameras, not the TV cameras).
  (b61d229)
- Replays from Juiced's Xbox builds (May and August 2004) play on the PC: copy their folders into
  `Documents\THQ\Juiced\Replays`. (b67ad2d)
- The replays list shows a picture of the replay's district, as in Acclaim's builds. (567d7ee)

#### Online

- Lobby chat: TAB opens it in the lobby and in car selection; 12 lines; any character from the keyboard; ENTER always
  sends. (d846d18, d1c6c4c)

### Changed

- Light flares (Glow) is Yes / No again and looks as in the original game; the adjustable bloom is on
  OpenJuiceConfig's Effects tab. (c104aa0, f1d917d)
- The borderless window minimizes when you switch away from the game. (85b34ae)

### Fixed

- Fixed the car's reflections showing white through tyre smoke. (f1d917d)
- Fixed the race camera zooming out when braking in reverse. (01ec365)
- Fixed headlight glass reflecting a frozen picture while driving; with Reflections HI it reflects the road around
  the car. (feab2ee)
- Fixed white outlines around the HUD's dials and pictures with texture filtering on. (6b6bd4d)
- Fixed PLAY AS GUEST looking highlighted while another login option is selected. (974bf69)
- Fixed the save question asking about a PS2 memory card when Autosave is off, and the second player's "reconnect the
  DUALSHOCK 2" note in split screen. (0ac845e)

### Known issues

- Windows may say the executables are new and unsigned and ask to check them; this is not a detection.

## [1.02.5] - 2026-10-24

First release of the unofficial patch 1.02: OpenJuice 1.01.5 (Juiced with the official patch 1.01) plus 124 fixes of the
original game's bugs and 9 additions. Technical details of each change: `docs/fixes.md`.

### Added

#### Online

- Online play restored through OpenSpy (GameSpy's servers were shut down in 2014): quick join, search, create, friends
  and leaderboards. (f24ec9f)
- Lobby chat in online sessions: SELECT2 in the lobby (gamepad button 2, Page Down) opens it. (0c6eccf, f6e9484)
- Play as Guest: online play without a GameSpy account, under the driver's name (no friends, leaderboards or stats).
  (9bc7a5b)
- The GameSpy account is stored encrypted (Windows DPAPI) in the save instead of plain text in settings.dat; an existing
  account is converted on the next save. (b8e91d5)
- Smoother, more accurate online cars: positions are compensated for each player's ping and sent 20 times a second (was
  5-10). (5556176)
- Collisions between players running the patch are felt on both machines. (21c154d)

#### Gameplay

- Replays can be saved and watched: quitting a replay offers to save it; saved replays are under Extras > Replay, stored
  in `My Documents\THQ\Juiced\Replays`. (a53d3f8, 9a8c9cd, 831f637)

#### Graphics

- The Glow option is a 0-100 % slider (0 = off) instead of Yes / No. (587944a)

#### Modding

- Wing mirror parts (`cardefs.txt` WMIRROR) are fitted and unlock with the car's upgrades; the game's own cars have
  none. (d049e38, 7d0d588)

### Fixed

#### Stability

- Fixed a possible hang after a display reset (e.g. Alt+Tab) when a shader cannot be created. (1655e15)
- Fixed a crash in the online leaderboards (top showoff scores). (75aea7e)
- Fixed the online login freezing the game; it now runs in the background, times out after 60 seconds and reports every
  failure. (b8e91d5)
- Fixed online leaderboard requests waiting forever on a failed connection. (ba767c0)
- Fixed a rare race start where the countdown stopped at 0 and the race never started. (f65c031)
- Fixed memory leaks in the EAX check of the sound system and of OpenJuiceConfig. (5f834e8)
- Fixed oversized data files (crews, challenges, arcade races, routes, decals) writing past their tables; the game's own
  files are not affected. (16ae1b4)
- Fixed online nicknames of 16 characters or more overwriting a session setting. (edf6db1)

#### Graphics

- Fixed the saved Detail setting being ignored until the graphics options were opened. (20b493b)
- Fixed the glow blur and the camera-cut transition being drawn into an internal buffer instead of the screen. (9d62cf3)
- Fixed a black first frame with glow on, and a slightly blurred picture with motion blur, depth of field and camera
  cuts. (e34ab61)
- Fixed track lighting: the second light took the third light's colour and the third had none. (689c661)
- Fixed wrong light colours on the low shader detail (fixed-function lighting). (04bf3b7)
- Fixed rendering errors after the reflection maps are updated. (0bf3783)
- Fixed car windows vanishing behind tyre smoke and decals. (66d4849)
- Fixed wing mirrors, headlights and rear lights staying fully lit while the car is in shadow. (91ac494)
- Fixed some car parts staying primer grey (S2000 chassis piece, Golf's rear logo). (b42e329)
- Fixed crew paint schemes losing their metallic finish. (329fdea)
- Fixed the sun's reflection on cars missing in menu scenes. (fb33c22)
- Fixed the last atmosphere zone's look staying on after leaving all zones. (f17fac5)
- Fixed tyre smoke being too transparent. (f98a60b)
- Fixed uneven depth-of-field blur. (6e068be)
- Fixed rain splashes being left behind a moving camera, the last rain streak not being drawn, and raindrops on the
  camera flashing opaque. (0a9991e, 4480992, 58c8373)
- Fixed only half of the falling leaves being drawn. (29d24c2)
- Fixed the opacity of the first skid mark segment overflowing. (b52030f)
- Fixed distorted HUD dial needles. (18cadd0)
- Fixed the rolling road graph being misplaced and unscaled. (15f580b)
- Fixed the highlighted dial missing in the HUD colour menu. (e12a353)
- Split screen: fixed the lower view losing its picture with motion blur, depth of field, camera cuts and screen
  effects. (c452211)
- Fixed the default graphics settings ignoring the model of an ATI / AMD card. (5d64f16)

#### Sound and movies

- Fixed movies playing silent until the volume is changed. (c88ef30)
- Fixed volume and mute on movies with one audio track, and a buffer overrun on tiled movies. (b2f5b5e)
- Fixed some sounds staying silent for a whole race. (17bd79e)
- Fixed sound occlusion getting stuck and jumping. (9d9c9e2)
- Split screen: fixed the second player's camera turning the first player's car sound away. (f38962a)
- Fixed the third betting suggestion voice line never playing. (b888153)

#### Races and HUD

- Fixed hundredths of a second never showing 99 (HUD, results, records, challenges). (3af7458)
- Fixed the world's time jumping forward after a second pause. (a2d05bc)
- Fixed QUIT on the challenge results leaving without the "QUIT RACE?" question. (acbc322)
- Fixed empty results rows showing "--:--:--" under Team. (0362c7d)
- Fixed crew results showing another crew's time. (ea09a92)
- Fixed a sprint restarted from the pause menu keeping the old heats' times for tie-breaks. (d67a908)
- Showoff: fixed a running trick's points going to a newer trick line and deciding its multiplier. (21d999e)
- Fixed the race camera moving too fast below 20 fps. (1d3e833)
- Fixed TV cameras turning wrongly while correcting their pitch, and the passing camera's height. (50b318b)
- Fixed cable cars and other moving track objects starting with the wrong tilt. (141f4fb)
- Fixed replays sometimes never starting. (61b6139)
- Fixed the third attract-mode replay never playing. (507536e)
- Fixed replays reading the wrong throttle for a car that lost a part. (e3f2300)
- Split screen: fixed player 1's HUD messages being cleared at every player's lap. (db0b7ee)
- Split screen: each player starts solo races and cruises in their own camera view. (6db6646)
- Fixed a double space in "Player 2" in the pause menu. (bad6351)

#### Handling and physics

- Fixed cars losing grip along the wheels when sliding backwards above 8 m/s (after a spin or fast reversing). (1ce3112)
- Fixed cars and props at rest shaking slightly. (1ce3112)
- Fixed the pitch stabilizer never engaging; a car is now pushed back towards level from 49 degrees of pitch. (1ce3112)
- Fixed a turbo lost to damage coming back at 1.3 instead of the car's own value. (1ce3112)
- Fixed handbrake values from 100 to 10000 in the car files being raised to 10000: 27 cars (mostly front-wheel drive)
  now have their intended weaker handbrake. (5d3b87a)
- Fixed drive and traction control going to the wrong wheel while a wheel is in the air. (6fd9b00)
- Fixed spin limits using the wrong inertia. (6fd9b00)
- Fixed wall collisions registering up to 12 cm late on some track faces, and other collision geometry errors. (40ed1f4)
- Fixed cars sliding along the bins on Southside Beach with no grip or bounce. (5efc41a)
- Fixed the AI swerving several times harder with more cars around. (0c1e21c)
- Fixed collision feedback (wheel jolt, camera shake, rumble, smoke) always using the strongest hit: light hits feel
  lighter. (0c1e21c)

#### Career, garage and menus

- Fixed accented and non-Latin letters (Czech, Polish, Russian, ...) in names. (3ac1f44)
- Fixed the arcade and showoff maps opening at the first location in languages other than English. (3748fd0)
- Fixed metallic and pearl paints being charged as a base paint (or free) without enough money; the full price is now
  required. (21d999e)
- Fixed a car staked in a pink slip race coming back repaired. (21d999e)
- Fixed the stats graph taking two or three steps for one event. (21d999e)
- Fixed Mod Shop upgrades keeping the weaker set's parts and meters when a stronger set replaces it. (21d999e)
- Fixed privileges adding respect to your crew. (21d999e)
- Fixed cruise results writing invalid winnings for an empty crew place. (21d999e)
- Fixed calling a crewmate who has joined: the call now rings out and SELECT answers it. (96c6f16)
- Fixed crew logo 19 never being offered. (53e1744)
- Fixed the decal colour chart showing the pearl colour, and BACK in the wheel colour menu not restoring the car's own
  colour. (0997f4d)
- Fixed SWEET never being chosen as the supplier of custom parts. (b4f4bc9)
- Fixed the right side skirt being treated as the left one. (400aff5)
- Fixed BACK in the game options not restoring the difficulty and AUTO2. (b30fd73)
- Fixed custom races: a sprint route in slot 1 counted as none; the route page highlights the right route. (84f0762)
- Fixed Arcade Championship highlighting the wrong championship. (aa0af21)
- Fixed Up / Down with no letter selected in the cheats screen leaving to Extras. (c4435e6)
- Fixed the betting screen showing a wrong amount when a raise or lower is refused. (d71ac47)
- Fixed "Could not delete driver." closing immediately. (41842b6)
- Fixed screenshots overwriting an existing empty file. (aa5ac6f)
- OpenJuiceConfig: fixed resolutions 4096 pixels wide and more, the threading default overriding other settings, and an
  invalid saved shader setting. (df1bc41)

#### Online

- Fixed pausing an online race freezing the race on your screen while it went on for everyone else; the race now
  continues under the pause menu. (0829fbc)
- Fixed being logged off when a friend request is refused. (b8e91d5)
- Fixed a failed login replacing the saved account. (b8e91d5)
- Fixed account creation with email addresses of 32 characters or more. (b8e91d5)
- Fixed profiles blocking the game until the server answered. (b8e91d5)
- Fixed a second login breaking the new connection, and the friends list stopping its refresh. (b8e91d5)
- Fixed the profile's "Female" read as unspecified, and the same status resent every 60 seconds. (b8e91d5)
- Fixed ties raising both players' ratings. (6a19934)
- Fixed an online career crew race result not being sent until the next report. (6a19934)
- Fixed leaderboard responses being misread (headers, content length). (ba767c0)
- Fixed reference leaks in the online code. (b8e91d5, 6a19934)

#### Modding and internal

- Fixed the 2-Texture Modulate (Specular) shader classes, negative UV scrolling, the mip levels of `-localfx` procedural
  textures, the HUD's ABS light without a speed display, and a showoff zone's CRUISE_MESSAGE. (2532186)
- Fixed open splines' length and their riders' spacing. (1c1e868)
- Fixed the "AUTO-MOD CAR?" question repeating for cars without the first body parts. (e582315)
- Internal fixes with no visible effect on the game's own files: render tree bounds, an unused shader constant,
  crewmates' phone numbers, the credits' third page, territory places. (5b58ed9, 7b4ae3b, 38c0f90, 504e883, e46ca42)

### Known issues

- Players without the patch have no lobby chat, and collisions are shared only between players running the patch.
- Windows Defender may warn about the executables (new, unsigned files).
- Windows 7 with DIMapper: the Controls mapping window works in the 32-bit build only.

## [1.00.6 / 1.01.6] - 2026-10-27

### Fixed

#### Races

- Fixed the "!" over nervous racers and the names over cars being drawn too low, inside the car. (d393497)
- The "!" over a nervous racer blinks as in the original game. (e1f0be1)
- Fixed the speed blur staying on during the start's fly-by after restarting a race. (8e4ac7a)
- Fixed the loading screen freezing (no spinning icon) while a race loads, and tips showing on it after a race.
  (af1a384, a872c2b)
- Fixed random outcomes drifting from the original game's: career results after a race, city map routes, the
  garage. (6573c4b, 5d04710)

#### Sound

- Fixed opponents' engines going silent while your car with the same engine sound is at full revs. (b33a031)

#### Online

- Fixed the on-screen keyboard's Symbols page (TAB) showing no symbols. (cd47596)
- Fixed cars still colliding in sessions hosted with collisions off (COLLISION: NO). (7159093)

#### Graphics

- Fixed the game using the main graphics card instead of the one chosen in the settings on PCs with two cards.
  (3a2b57b)

### Removed

- OpenJuiceConfig64.exe: the settings tool is 32-bit only, like the original JuicedConfig, and works for both builds.
  The 64-bit one could not detect EAX and turned it off. (45162c0)

### Known issues

- Windows may say the executables are new and unsigned and ask to check them; this is not a detection.
- Windows 7 with DIMapper: the Controls mapping window and saved control schemes work in the 32-bit build
  (OpenJuice.exe) only.

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
