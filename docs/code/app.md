# The start and the main loop (`src/Game/App`, `src/Game/Platform`)

How the game starts, what runs each frame and how the phases follow each other. What the stock game does at its start
is `docs/spec/01-startup.md`.

## The start

| step | where | what |
|---|---|---|
| `WinMain` | `App/Main.cpp` | the log (its first line the version, `App/Version.h`), the crash filter, `OpenJuice.ini` (`Core/Base/Config.h`), the command line (`Platform/CommandLine.h`), the game's folder, the processor's features (`Core/Base/Cpu.h`), the accessibility keys off (`Platform/SystemKeys.h`), the worker threads' count and `JUICEDMUTEX` |
| `runMain` | `App/Main.cpp` | the stock's crash filter (`crash.txt`, `App/CrashLog.h`) around the main function |
| `gameMain` | `App/GameLoop.cpp` | the stock's main function (0x00418300): `rand()` stepped, the start's language, `-setup`, the car models' check |
| `runGame` | `App/GameLoop.cpp` | the game object's start (0x0040BAC0) and the loop |

The executable must be in the game's folder (`GameFiles::valid()`: a folder holding `tracks\`), as the stock one must
to find its `binkw32.dll`; elsewhere it says so and ends. The game's files are then read from the current directory.

`runGame` makes the parts in the stock order, which matters for the `rand()` values each draws:

1. `Platform::start`: the adapter saved by `JuicedConfig.exe` looked for among the system's (else the configuration
   tool started and the search done again, once), the registry's settings (`Platform/Settings.h`), a single instance
   (`Global\JuicedTheGame`), the window class `jet` and the game's window.
2. The render device (`render::createDevice`), the sound system, GameSpy Voice's window, the music's player, the debug
   pages (`App/DebugPages.h`: made, never shown, as in the stock game).
3. The language (`chooseLanguage`: the registry's id, else Windows', or `[Game] Language`), its text table, the input
   (`Platform/InputDevices.h`: DirectInput's controllers and the force feedback; the keyboard comes from the window's
   messages) and the controls file. The fixed build has no DirectInput maps: JuicedConfigExt's controller and layout
   (`Platform/Pads.h`, `Core/Input/PadControls.h`; the controls file with the layout's controls) and its steering
   wheels (`Platform/WheelInput.h`: controller.ini's `[WheelMap]` and `[Wheel]`, DirectInput's raw states beside the
   force feedback's channels, which `InputDevices::startWheels` opens for the wheels).
4. `rand()` seeded from the performance counter (or `--seed`), then `GameData::load` (`data.md`): the car models, the
   parts, the arcade, a first career.
5. The graphics settings, the 3D engine (`scene::Engine`, `rendering.md`), the movies (`movie::System`), the first
   `Frontend` with its startup window drawn over three frames, the music, the interface's speech.
6. The `TrackView` (the race's object, `race.md`), the front end's first window (`disclaimer`, or `loading` with
   `-i`).

## The loop

The game object's phases (0x0040CCA0):

```
Frontend --race asked--> Load --the load's job done--> Race --race left--> Unload --the job done--> Frontend
```

- **Frontend**: `Frontend::update` and `draw` (`frontend.md`). When a window asks for a race (`raceLoadAsked`), the
  front end goes and a loading front end (`keepLoaderWindows`, `startLoader`: `loader`, `arcade_loading` or
  `custom_loader` by the race's mode) takes its place.
- **Load**: the loading front end's frames; after three, its animation and the load as a job of the loading thread
  (`Core/Base/LoadingThread.h`), as the stock's: `TrackView::start` (the track), the grid by the race's mode
  (`Race/RaceGrids.h`: the career's racers, a custom or arcade race, a replay's, the network's), the intro,
  `beginRace`. The frames go on meanwhile (the loading window and its animation move, the network's frames keep the
  peers); once the job is done the main thread ends the load (`finishLoad`) and the race's music starts.
- **Race**: `TrackView::drive` and `draw` (`race.md`), the 3D sounds' world. When the race front end asks to leave
  (`unloadAsked`), the race ends (`endRace`) and a loading front end shows.
- **Unload**: one frame of it (three in the arcade), then its animation and a job of the loading thread making the new
  `Frontend` (`windowAfterRace`, the windows it keeps); once the job is done and the loading window's transitions
  have ended, the front end's music and its first window.

The stock's two threads share the game's objects without a lock. Here a job of the loading thread takes turns with
the frames (`FrameTurns`): a frame holds the turn from its start to its present, the job runs between the frames and
gives the turn back at its waits (the files it reads, the engine's loads) when a frame waits for it.

Each frame, in the stock order:

1. The window's messages (`Platform::pumpMessages`), the device's frame begun, the brightness.
2. The input's poll with the frame's keys (`input::Input::poll`), then the interface's event of the phase: the race
   front end's, or the front end's with its controllers watched.
3. The phase's frame and draw (above), with the music's and the speech's frames.
4. The sound system's step (`audio::system().update`), then the network's frames: the session, the online user
   (GameSpy Presence), the matchmaking, the statistics, the voice (`network.md`).
5. The present, Print Screen's screenshot, then the stock's limiter: the frame yields until 1/60.1 s has passed (none
   with `-r`).

## The platform

- `Platform` (`Platform/Platform.h`) owns the window, its messages (the keys, the activation, Print Screen, the close
  from the system menu) and the single instance. The fixed build's screen modes (`Platform/Screen.h`,
  `Platform::chooseScreen`): the process DPI aware, the adapter of the settings' `Monitor`, the full screen mode at its
  refresh rate (no mode of the resolution: the window without a frame), the window without a frame over the monitor
  (minimized when it loses the focus), the window with a frame fitted to the work area.
- `Settings` are the registry's values `JuicedConfig.exe` writes (`HKEY_CURRENT_USER\Software\THQ\Juiced`); OpenJuice
  writes none. `runConfigurationTool` starts `JuicedConfig.exe` and waits for it (the adapter check, `-setup`); the
  fixed build starts its `OpenJuiceConfig` first (`runSettingsProgram`: JuicedConfigExt's settings program,
  `third_party/juicedconfigext`; `--first-run` from the adapter check, `--layout-only` from the controls' menu).
- `startupText` is the start's messages (another instance, no `JuicedConfig.exe`, ..) read from the game's
  `jettext<letter>.txt` before the game's texts are loaded.
- `CrashLog`: a crash writes the stack to `OpenJuice.log` and the stock's `crash.txt`.
- Windows 7: the build targets it (`_WIN32_WINNT` 0x0601), but the toolset's C++ library calls three functions of
  Windows 8 directly (`CreateFile2`, `CopyFile2`, `GetSystemTimePreciseAsFileTime`), and an executable importing
  them does not start on Windows 7. `Platform/Win7Imports.cpp` (with `Win7Imports32.asm` for the 32-bit names),
  built in the executable's own project, defines their import slots, read-only as the system's: each holds the
  function that does the same with Windows 7's (`Platform/Windows7.h`: `CreateFileW`, `CopyFileExW`,
  `GetSystemTimeAsFileTime`), on every Windows alike, nothing looked up as the game runs. The test
  `Windows7.Executable` (`tests.md`) checks every import of the executable against Windows 7's functions.

## The command line and `OpenJuice.ini`

- `parseCommandLine` (`Platform/CommandLine.h`) reads the stock game's switches as `WinMain` and the main function do
  (`-i`, `-wet`, `-1` .. `-7`, `-localfx`, `-r`, `-host`, `-connect`, ..; `docs/spec/01-startup.md`) into
  `LaunchOptions`, and OpenJuice's `-setup`.
- The debug builds (`_DEBUG`) also read OpenJuice's switches of scripted runs, which start with `--`: `--frames N`,
  `--test` (a hidden windowed run that changes nothing on the system and opens no tool or controller), `--seed N`,
  `--capture <file>`, `--press`, `--screen`, `--track`, `--car`, `--cars N` (on the first N grid places),
  `--car-ahead <right>,<forward>` (the second grid place that far from the first, in cm), `--track-camera
  <right>,<up>,<back>[,<right>,<forward>]` (held through the race; looking at the first grid place or at the point the
  last two give), `--net`,
  `--players`, `--autopilot`, `--saves`, `--headlights N` (the fixed build's 0 .. 2 whatever `--test` resets),
  `--shadow-map <file>` (the headlights' shadow maps at the last frame, grey: light i's in `<file>_i`),
  `--reflections N` (the REFLECTION row's 0 .. 3, 4 the fixed build's ULTRA), `--cube-map <file>` (the cars' cubes at
  the last frame: `<file>_stock_<face>`, `<file>_cars_<face>`), `--world-light N` (0 .. 2), `--sky N` (0 .. 2),
  `--ao-map <file>` (the
  last ambient occlusion; beside it the lights' reflection `_lights`, the street lamps' shade `_lamps`, the first
  lamp's map `_lamp0` and the light in the haze `_haze`), `--glow N` (LIGHT FLARES: 0 OFF, 1 STOCK, 2 CUSTOM),
  `--user-settings` (with `--test` the registry's renderer settings and effects kept, the window still hidden),
  `--profile N` (the frames' layers timed from the N-th: each layer's GPU and CPU time, draws and triangles a frame,
  logged at the end, `Render/D3D9/D3D9Profiler`; with `-r` and no VSync a release-like run's are the frame's real
  costs), `--caster-pixels X` (the shadow maps' casters under X pixels across from the view left out, 0 none), ..
  (`LaunchOptions` names each). The release builds read them as the stock game does: as no switch.
- `Config` (`Core/Base/Config.h`) is `OpenJuice.ini`: `[Game] Language`, `[Game] SkipIntroMovies`, and in the debug
  builds `[Saves] Folder`. A missing file is written with the defaults.

## A scripted run

The debug builds can run the game without a hand on it, for checks that need the real game:

```
OpenJuice64_d.exe --test --frames 300 --seed 1 --capture shot.bmp
```

runs 300 frames hidden, with `rand()` seeded with 1, and writes the last frame (a relative name next to the
executable). `--press 120:RETURN,200-260:UP` presses keys (a frame, or the frames down and up, and a `DIKEYBOARD_`
name or a scan code), `--screen <window>` opens another first window, `--track <Track>/<Route>` shows a route instead
of the interface (`--car <car>` puts that car on its grid).
