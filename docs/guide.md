# Getting started

A short guide for playing and building OpenJuice. The details of what the game does are in `docs/spec`, how the code
is put together in `docs/code`.

## What you need

- Windows 7 SP1 or later.
- Your own installed copy of Juiced (PC, 2005). OpenJuice carries none of the game's files: it reads the
  installation, packed (`scripts.dat`, `ui.dat`, `tracks\<track>.dat`, ..) or unpacked. It needs no `Juiced.exe`: an
  installation of either version, protected or not, will do.
- To build it yourself: Visual Studio 2026 with the C++ desktop workload.

## Which version

The game had two retail builds, and OpenJuice follows each:

| version | branch | what it is |
|---|---|---|
| 1.00 | `retail/1.00/dx9` | the retail build 1.00 |
| 1.01 | `retail/1.01/dx9` | 1.00 with the official patch 1.01's changes (`docs/spec/patch-1.01.md`) |

Both read the same installation and the same saves; they differ only where the patch changed the game.

## Downloads

The releases are on the project's [Releases](https://gitlab.com/sidecr3w/openjuice/-/releases) page. Each release
has, for each version:

- the release build: `OpenJuice.exe` (32-bit) and `OpenJuice64.exe` (64-bit), and the settings tool
  `OpenJuiceConfig.exe` and `OpenJuiceConfig64.exe`;
- the debug build: `OpenJuice_d.exe`, `OpenJuice64_d.exe`, `OpenJuiceConfig_d.exe` and `OpenJuiceConfig64_d.exe`
  with their `.pdb` files, slower, for tracking problems down;
- the source of that version.

Either the 32-bit or the 64-bit executable will do. The 32-bit one plays the game's movies with the game's own
`binkw32.dll`; the 64-bit one decodes them itself.

To see which build you have: the executable's Properties, Details: the description reads "OpenJuice for Juiced 1.00
(64-bit)" (or 1.01, 32-bit, debug), the product version "1.00.2 (Juiced 1.00, Bug fixes)": the game's version, then
OpenJuice's release. The first line of `OpenJuice.log` says the same. What each release changed: `CHANGELOG.md`.

## Building

Open `OpenJuice.slnx` in Visual Studio, or from a developer prompt:

```
MSBuild OpenJuice.slnx -p:Configuration=Release -p:Platform=x64
MSBuild OpenJuice.slnx -p:Configuration=Release -p:Platform=x86
```

The executables land in `bin\x64\Release\` and `bin\Win32\Release\` (`Debug` for the debug builds), the settings
tool's and `OpenJuice.Tests.exe` beside them.

## First start

Put the executable in the game's folder, the one holding `tracks\`, and start it there. It can take the place of
`Juiced.exe`: renamed `Juiced.exe` (keep the original under another name), it starts as the game did, from its
shortcuts too. Away from the game's folder it says the folder was not found and stops, as the stock game does not
start there either. The first start writes `OpenJuice.ini` next to the executable with the defaults.

The game's own settings (resolution, windowed, antialiasing, ..) are those of the game's `JuicedConfig.exe`; when
none are saved yet, OpenJuice starts `JuicedConfig.exe` first, as the stock game does.

## The settings tool

`OpenJuiceConfig.exe` (and `OpenJuiceConfig64.exe`) is OpenJuice's copy of `JuicedConfig.exe`, the same window
writing the same settings (`docs/spec/30-juiced-config.md`). Put it in the game's folder too; renamed
`JuicedConfig.exe` (keep the original under another name), it is the one the game starts. Its texts are in the
game's language, picked as the game picks it (`[Game] Language` of `OpenJuice.ini` too). As the original, it opens
only where DxDiag reports DirectX 9.0c or later by its version letter; Windows 11 reports DirectX 12 with a blank
letter, so there it shows that DirectX 9.0c is needed, as the original does there. The debug builds (`_d`) take
`--skip-directx-check`, which opens the window anyway.

## OpenJuice.ini

| key | what it does |
|---|---|
| `[Game] Language` | `english`, `french`, `german`, `italian`, `spanish`, `czech`, `polish` or `russian`; empty picks it as the game does (the registry's language, else Windows'). A language whose files the installation lacks is missing in the game too |
| `[Game] SkipIntroMovies` | `0` (the default) plays the opening movies, `1` passes them at once |

The saves are the stock game's: OpenJuice reads and writes the same `SAVE` folder, in `%APPDATA%\THQ\Juiced`. The
debug builds (`_d`) also take `[Saves] Folder`, another folder holding `SAVE` (empty: the stock game's).

## Command line

The stock game's switches work as in the game (`docs/spec/01-startup.md`: `-i` an instant race, `-wet`, `-name`,
..). OpenJuice adds one:

| switch | what it does |
|---|---|
| `-setup` | opens the game's configuration tool, `JuicedConfig.exe`, and starts no game |

The debug builds (`_d`) also take OpenJuice's switches for working on the engine (`--frames`, `--seed`, `--capture`,
`--test`, ..); `src/Game/Platform/CommandLine.h` describes them.

## Reporting a problem

Problems go to the project's [Issues](https://gitlab.com/sidecr3w/openjuice/-/issues). OpenJuice writes
`OpenJuice.log` next to the executable. A report helps most with:

- the version and the build (the first line of the log);
- what you did, step by step, and what you saw;
- `OpenJuice.log`, and a screenshot when it shows something;
- how the stock game behaves at the same place, if you can check.

OpenJuice aims to behave as the stock game does, its own bugs included: `docs/spec/stock-problems.md` lists the ones
it keeps on purpose.

## Known limits

- Online play: GameSpy's servers are closed, so the online modes cannot connect. The LAN (link-up) mode is there.
