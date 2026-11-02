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
  `OpenJuiceConfig.exe` (32-bit, for both);
- the source of that version (the debug builds, `_d`, are built from it, not released).

Either the 32-bit or the 64-bit executable will do. The 32-bit one plays the game's movies with the game's own
`binkw32.dll`; the 64-bit one decodes them itself.

To see which build you have: the executable's Properties, Details: the description reads "OpenJuice unofficial
patch 1.02 for Juiced (64-bit)" (or 32-bit, debug), the product version "1.02.7 (based on Juiced 1.01, Graphics)":
the patch's version with the release, then the game it is built on. The first line of `OpenJuice.log` says the
same. What each release changed: `CHANGELOG.md`.

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

The game's own settings (resolution, windowed, ..) are those of the settings program; when none are saved yet,
OpenJuice starts `OpenJuiceConfig` first (`JuicedConfig.exe` when it is not there), as the stock game starts
`JuicedConfig.exe`.

## The settings program

`OpenJuiceConfig.exe` (32-bit, for both executables of the game) is the settings program of JuicedConfig Extended
(https://gitlab.com/sidecr3w/juicedconfig-extended) made for the unofficial patch: put it in the game's folder, next
to `OpenJuice.exe` or `OpenJuice64.exe`. Its tabs are the settings the patch's game reads: the display (the monitor,
full screen, a window without a frame (the picture's shape kept or stretched) or a window, the resolution, the shaders, the picture's shape: 4:3, Auto or a shape of its own, VSync
and the frame rate limit, the textures' sharpness, the FPS counter, the threading), the
controller (XInput and SDL controllers, its layout and the keyboard's,
edited in pictures), the steering wheel (the wheels' map: a wheel, its pedals and a shifter apart; the dead zone, the
rotation, the response, the force feedback; MultiTurn). The display's settings are the registry's
(`HKEY_CURRENT_USER\Software\THQ\Juiced`, as `JuicedConfig.exe` writes them); the controller's and the wheel's are
in `%APPDATA%\THQ\Juiced\JuicedConfigExt\controller.ini`, the file JuicedConfig Extended's plugin reads too. Save and
play saves and starts the game (`OpenJuice64.exe` on 64-bit Windows when it is there, else `OpenJuice.exe`); the
game's Controls menu opens its layout editor. Its texts are in eight languages
(its language list, Auto: Windows'); `--lang <name>` shows one without keeping it. The anti-aliasing (MSAA, SMAA), the
texture filter and the effects are set in the game: OPTIONS, GRAPHICS, ADVANCED, each in effect at once; the sound's
EAX (with any card) in the main menu's OPTIONS, AUDIO, beside the volumes.

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
