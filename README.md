# OpenJuice

This checkout includes a native Linux/NixOS port of upstream **v1.02.5**.
Build, launcher, validation and platform limitations: [Linux port](docs/linux-port.md).
Native Android APK, ZIP/mod imports and Retroid controls: [Android port](docs/android-port.md).
The upstream description follows; platform-specific online account persistence
and Windows-only facilities are not available in the Linux build.

An engine that runs Juiced (PC, 2005) from the files of an installed copy of the game. The first goal is the stock
game as it is: the same tracks, cars, physics, AI, menus and look, with the same results - a base for mods to start
from. The code is written from descriptions of the game's behaviour; it carries no code and none of the game's files,
which it reads from the user's own installation.

This project is AI assisted.

This is the branch `unofficial-patch/1.02/dx9`: OpenJuice's fan patch 1.02 of Juiced. It is the retail build of
the game with the official patch 1.01 (`retail/1.01/dx9`, `docs/spec/patch-1.01.md`) and on top of it 124 fixes of
the stock game's problems - crashes, hangs, leaks, wrong pictures and sounds, career and race mistakes, the online
code's troubles - and 9 improvements of its own: online play again on OpenSpy (with a chat in the session's lobby,
a guest's login with no account, the account kept encrypted, the others' cars nearer to where they are and their
hits shared between the patch's players), replays saved and watched on the PC, a glow meter in the graphics options
and the cars' wing mirrors. Each one, and the few still to be decided, is in [`docs/fixes.md`](docs/fixes.md). Its
version is 1.02 and the retail releases' number (1.02.5 with the retail 1.00.5 / 1.01.5), drawn with Direct3D 9.
The project's branches are listed on [`master`](https://gitlab.com/sidecr3w/openjuice/-/tree/master).

To build it and play: [`docs/guide.md`](docs/guide.md).

## Layout

| folder | what it is |
|---|---|
| `src/Core` | the game without a screen: the game's files and formats, the data, the career, the cars' physics, the race's rules, the saves, input, the network (`Base`, `Data`, `Formats`, `Career`, `Cars`, `Physics`, `Race`, `Tracks`, `Saves`, `Input`, `Network`) |
| `src/Render` | `IRenderDevice`, the engine's only door to the graphics API; `D3D9/` the backend of the stock game |
| `src/Audio` | the sound system: banks, players, streams, voices |
| `src/Game` | the game: the main loop (`App`, `App/Main.cpp` its `WinMain`), the windows' interface (`UI`), the front end's windows (`Frontend`, see `docs/code/frontend.md`), the race (`Race`), the 3D scene (`Scene`), the cars' models and sounds (`Cars`), movies and music (`Media`), the platform (`Platform`). `OpenJuice.Game.vcxproj` builds it as a library (the tests link it); `OpenJuice.vcxproj` makes `OpenJuice.exe` of its object files |
| `tests` | unit and integration tests (`OpenJuice.Tests.exe`) |
| `docs/spec` | what the stock game does, by part, with its addresses |
| `docs/code` | how the port's code is put together |
| `build/OpenJuice.props` | settings shared by all projects |

Graphics: Direct3D 9, as the stock game draws (same states, same shaders).

## Building

Visual Studio 2026 with the C++ desktop workload: open `OpenJuice.slnx`, platforms x64 and x86, or

```
MSBuild OpenJuice.slnx -p:Configuration=Release -p:Platform=x64
```

Output: `bin\x64\Release\OpenJuice64.exe` and `bin\Win32\Release\OpenJuice.exe` (32-bit); the debug builds are
`OpenJuice64_d.exe` and `OpenJuice_d.exe`. The version resource (`src/Game/App/Version.h`) names the game's build the
branch follows and OpenJuice's release. Static runtime (`/MT`), Windows 7 SP1 and later.

## Running

OpenJuice runs from the game's folder, as the stock game does: put the executable in the folder of an installed Juiced
(the one holding `tracks\`) and start it there. It can take the place of `Juiced.exe`: renamed `Juiced.exe`, it starts
as the game did. Away from the game's folder it says the folder was not found and stops (the stock game stops there too,
without its `binkw32.dll`). No `Juiced.exe` is read: either version's installation, protected or not, will do. The
install may be packed or not: a folder with a `<folder>.dat` beside it (`scripts.dat`, `ui.dat`, `cars\eclipse.dat`,
`tracks\<track>.dat`...) is read from that archive, as the stock game does, else from the folder itself. The game's own
settings (resolution, windowed, antialiasing, language...) are those of `JuicedConfig.exe` in
`HKEY_CURRENT_USER\Software\THQ\Juiced`; when the adapter saved there is not found, `JuicedConfig.exe` is started first,
as the stock game does.

`OpenJuice.ini` next to the executable (written with the defaults on the first start):

```
[Game]
Language=
SkipIntroMovies=0
```

The game's build this engine follows was made for another region; OpenJuice has the region binding removed. The language
is picked as the English build 1.0.1 does, from `JuicedConfig.exe`'s `language` value (a Windows language id) or the
user's language, among all eight languages (English when none fits); `Language=english` (french, german, italian,
spanish, czech, polish, russian) under `[Game]` in `OpenJuice.ini` chooses one. It picks the texts, the windows' texts,
the controls, the speech (`audio\speech\ui<letter>`, `race<letter>`) and the movies' sound track: a language's files the
installation lacks are missing in the game as well.

The movies are played by the game's own `binkw32.dll` in the 32-bit build; the 64-bit build decodes them itself (Bink 1,
`src/Game/Media/BinkNative.*`). `SkipIntroMovies=0` (the default) plays the opening movies (THQ, Juice Games, the intro)
as the stock game does; `1` passes them at once. The saves are the stock game's, in `%APPDATA%\THQ\Juiced`; the debug
builds also read `[Saves] Folder`, the folder holding `SAVE` (empty: the stock game's).

The game's command line switches work as in the stock game (`docs/spec/01-startup.md`); OpenJuice adds `-setup`, which
opens the game's configuration tool (`JuicedConfig.exe`) and starts no game. The debug builds also take OpenJuice's
switches of scripted runs (`--frames N`, `--test`, `--seed N`, `--capture <file>`, ..;
`src/Game/Platform/CommandLine.h`). `OpenJuice.log` is written next to the executable.

## Tests

`tests/` holds the checks of the engine (`OpenJuice.Tests.exe [save folder] [--game <folder>] [--unit | --integration]
[--suite <name>] [--list]`): `Unit/` tests one part each with the data they make, `Integration/` the parts together
through the file system, sockets on the loopback, the player's saves and the installed game's files (the `--game`
folder; without one the tests of the game's data are left out). `Framework/` registers the tests, runs them and holds
what they share (`Fakes/`: stand-ins such as a render device that draws nothing and a front end for the windows' tests).

## Specification

`docs/spec/` describes the stock game's behaviour that the code follows, step by step from the entry point.

## License

OpenJuice is free for everyone under the [PolyForm Noncommercial License 1.0.0](LICENSE.md): it may be used,
changed, forked and passed on for any noncommercial purpose, without asking; it may not be sold or used commercially.
Whoever passes a copy on keeps its `Required Notice` line (the author's credit) with it. The game itself is not part
of OpenJuice: it needs your own installed copy. Speex (`third_party/speex-1.0.3`) keeps its own license.
