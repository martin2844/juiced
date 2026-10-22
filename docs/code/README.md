# The code

How OpenJuice's code is put together, for someone reading it for the first time. What the stock game does, part by
part and with its addresses, is `docs/spec`; these pages are about where that lives in the port and how the parts
talk.

| page | what it covers |
|---|---|
| [`app.md`](app.md) | the start, the main loop and its phases, the platform, the command line and `OpenJuice.ini` |
| [`data.md`](data.md) | the game's files and formats, the data loaded at start, the texts and languages |
| [`ui.md`](ui.md) | the interface: `.ui` windows, their elements, the window manager, the message windows |
| [`frontend.md`](frontend.md) | the front end's windows (the menus, the garage, the career, the network) |
| [`race.md`](race.md) | the race: its load, its frame, the racers, the camera, the replay, the physics |
| [`race-frontend.md`](race-frontend.md) | the windows over a race (the HUD, the pause, the results) |
| [`rendering.md`](rendering.md) | the render device, the materials and programs, the 3D scenes, the particles |
| [`career.md`](career.md) | the career, the cars, the arcade and the saves |
| [`network.md`](network.md) | the LAN and online game: the transport, the session, GameSpy's parts |
| [`audio.md`](audio.md) | the sound library, the music, the speech, the movies |
| [`tests.md`](tests.md) | the tests: unit and integration, the fixtures and fakes, running them |

## The libraries

```
Core     the game without a screen or a sound          oj, oj::net, oj::save, oj::input, ...
Render   the render device and the stock renderer       oj::render            -> Core
Audio    the stock sound library                        oj::audio             -> Core
Game     the game: its loop, interface, front end,      oj, oj::frontend,     -> Core, Render, Audio, Speex
         race, 3D scene, cars' models, movies           oj::scene, oj::ui, ..
JuicedConfig  the settings tool's parts apart from      oj::config            -> Audio's Eax.h
         its window (modes, flags, DxDiag, EAX)
```

- Each is a static library (`src/<name>/OpenJuice.<name>.vcxproj`). `OpenJuice.exe` (`src/Game/OpenJuice.vcxproj`) is
  Game's object files with the version resource; `OpenJuice.Tests.exe` links the same libraries.
- The arrows are the only includes: Core includes nothing of the others, Render and Audio nothing of each other or of
  Game. Where Core's code calls into the game, as the stock code calls the sound library from the race's rules, it
  calls a hook the game fills in at start: `RaceSounds.h` (the race's sounds, the speech's stop), `onlineHooks()`
  (the online objects' ends), `audio::setNodeMatrix` (where a 3D sound's node is).
- `third_party/speex-1.0.3` is the voice codec GameSpy Voice links (its own BSD licence).
- `OpenJuiceConfig.exe` (`src/JuicedConfig/OpenJuiceConfig.vcxproj`) is the port of the game's `JuicedConfig.exe`
  (`docs/spec/30-juiced-config.md`): its window (`ConfigDialog.cpp`) and `WinMain` over the JuicedConfig library,
  with the stock's resources (`OpenJuiceConfig.rc`, `res\`). It shares no code with the game but the EAX
  property sets' ids. Its DirectX 9.0c check takes any DirectX past 9 (the stock's, by DxDiag's letter alone,
  never passes on Windows Vista and later); its debug builds take `--skip-directx-check` (the window whatever the
  check finds).

## From the start to a frame

1. `WinMain` (`Game/App/Main.cpp`): the log, `OpenJuice.ini`, the command line, the game's folder (the executable's),
   the processor's features, the system's accessibility keys, the worker threads; then the game's main function.
2. `gameMain` (`Game/App/GameLoop.cpp`, the stock 0x00418300): the start's language, `-setup`, the car models'
   check; then `runGame`.
3. `runGame` makes the game object's parts in the stock order: the platform and its window, the render device, the
   sound, the music, the language and its texts, the input, `rand()`'s seed, the game's data (`GameData`), the
   graphics settings, the 3D engine (`scene::Engine`), the movies and the front end. Then the loop.
4. The loop's phases are the game object's: the front end (`Frontend`), the race's load (a loading front end, then
   `TrackView::start`), the race (`TrackView`), the race's unload (back to a new front end). Each frame: the window's
   messages, the input's poll, the phase's frame and draw, the sound system, the network's frames (the session, the
   online user, the matchmaking, the statistics, the voice), the present, then the stock's limiter to 60.1 frames a
   second.

`app.md` follows this in detail.

## Following the stock

- **Addresses.** A function's comment starts with the stock address it ports (`// 0x0046B5A0: ...`); a file's first
  comment names the stock object it is (its address, size, vtable) and the `docs/spec` page. The addresses are the
  retail build's (`Version.h` names it).
- **Stock problems.** What the stock code gets wrong and OpenJuice keeps is marked `Stock problem:` where it happens;
  `docs/spec/stock-problems.md` lists the ones a player can see.
- **Layouts.** Objects the saves copy byte for byte, or that the stock code reads past their ends, keep the stock
  layout, each field's offset in its comment (`int32_t m_answer = -1;  // +0x10`). `Core/Base/RawFields.h` reads and
  writes a field by its offset where the stock code indexes past an object.
- **Random numbers.** The stock game draws from the C runtime's `rand()`: `gameRandom()` (`Core/Base/Random.h`) is
  that generator, and every part draws from it in the stock's order (a draw out of place changes the race that
  follows; the log counts the draws). The particles and the dents have the stock's second generator (`LaggedRandom`).
- **Floats.** Sums and products are written in the game's order so that they round the same; `Core/Base/Matrix.h`
  is the engine's matrix arithmetic, `Core/Base/X87.h` the runtime's functions as the x87 unit computes them.
- **The game's files** are read only through `GameFiles` (`Core/Base/GameFiles.h`): packed (`<folder>.dat`) or not,
  from the current directory, which is the game's folder, as the stock game reads them.
- **The log.** `oj::log` writes `OpenJuice.log` next to the executable and the debugger's output; `log::debugPrint`
  is the stock's own text to the debugger, kept where it is.

## Names and files

- Namespaces: `oj` (the game's types), `oj::frontend` (the front end's windows), `oj::ui` (the interface),
  `oj::scene` (the 3D scenes), `oj::render`, `oj::audio`, `oj::net` (and `oj::net::gp`, `gstats`, `ghttp`, `gv`: the
  GameSpy SDK's parts), `oj::save`, `oj::input`, `oj::movie`, `oj::bink`; `app` is `Game/App`'s own.
- A file is one class or one part of a big one (`Frontend*.cpp`, `TrackView*.cpp`, `RaceHud*.cpp`); a family's shared
  helpers are in its `*Internal.h`.
- An enum names what the stock code compares with numbers (`RaceMode`, `RaceType`, `SwitchMode`, `RaceWindowId`);
  the few fields whose meaning is not traced are named by their offset (`valueAC`, `// +0xAC`).
- The style: four spaces, 120 columns, case labels not indented, a comment of whole sentences above what it tells.

## Where a change goes

| it is | it goes in |
|---|---|
| a file format of the game | `Core/Formats` (and its reader's test in `tests/Unit/Formats`) |
| data the game loads at start, a table | `Core/Data` |
| a rule of the career, the arcade, the race | `Core/Career`, `Core/Race` |
| the cars' physics | `Core/Physics`, `Core/Cars` |
| a window of the front end | `Game/Frontend/<family>` (`frontend.md`) |
| a window over the race | `Game/Race/Hud` (`race-frontend.md`) |
| an element of the interface | `Game/UI/Components` (`ui.md`) |
| something drawn | `Render` (the device, the materials) or `Game/Scene` (what a world holds) |
| a sound | `Audio` (the library) or the game's part that plays it |
