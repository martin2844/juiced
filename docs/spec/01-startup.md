# Startup of the stock game: from the entry point to the shown window

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## Before WinMain

Static initialisation detects the CPU (CPUID): 1 MMX, 2 SSE, 4 SSE2, 8 3DNow!, 0x10 SSE3 (the leaf 1 bits, leaf
0x80000001 bit 31 for 3DNow!). The result is kept as the CPU feature set.

## WinMain (`0x005A38A0`)

1. The current directory is the game's root (all game files are opened relative to it); WinMain keeps it as the game's
   folder (`[0x0075DAD8]`, `GetCurrentDirectoryA` of 256 characters).
2. The command line is split at spaces (a quoted part stays whole); then "pre if(vArgs.size() == 0)" goes to the
   debugger's output (`0x005A3CD2`, the game's debug print `0x0059AF00`: `vsprintf` into `[0x0079E9D8]`, then
   `OutputDebugStringA`; nothing of it reaches a file). A word starting with `-` is a switch:

   | switch | effect |
   |---|---|
   | `-1` .. `-7` | a scale value: 2, 4, 6, 8, 32, 64, 100 |
   | `-localfx` | the effects compiled from the developers' sources `\src\jet\jetMaterials\dx9\fx\*.fx` and their procedural textures made (`11-effects.md`) |
   | `-debugfx` | `-localfx` plus the effects compiled for debugging (flags `0x141`) and software vertex processing |
   | `-nonpured3d` | no pure Direct3D device |
   | `-mtd3d` | multithreaded Direct3D device |
   | `-rf` | with depth of field on in the settings, every view draws it, not only a camera that asks for it (its `+0x22C`; `0x005168F6`, `0x00516F02`) |
   | `-r` | presentation interval ONE instead of the default |
   | `-x` | audio flags \|= 8 |
   | `-s` | software vertex processing |
   | `-n` | audio flags = 1 |

   Every word is also passed on to the game's main function as an argument.
3. With SSE present the floating point rounding is set to truncation: x87 control word `RC = chop`, SSE `MXCSR.RC =
   toward zero`. Direct3D 9 makes its device without `FPU_PRESERVE`: it sets the x87 control word of the main thread
   to single precision and round to nearest, so from then on the game's x87 arithmetic rounds each result to a float,
   to nearest. The truncation stays in `MXCSR` (the SSE conversions the game picks with the SSE flag) and in the
   explicit conversions. Other threads run at the runtime's default, double precision and round to nearest: a value
   computed there can differ in its last bit (`07-dynamics.md`).
4. The accessibility shortcuts are saved (`SPI_GETSTICKYKEYS`, `SPI_GETTOGGLEKEYS`, `SPI_GETFILTERKEYS`); for each
   feature that is off, its hot key and confirmation (`0x4 | 0x8`) are cleared for the session. They are restored
   when the game ends.
5. Worker threads (`0x005A0CE0`): the processors of the system affinity mask are counted (at most 2); the main thread
   is bound to processor 0. With threading on (the `threading` setting, default 1) and 2 processors, the mutex
   `JUICEDMUTEX` is created (owned) and one worker thread is started.
6. The game's main function runs; afterwards the accessibility shortcuts are restored.

The main function runs under the game's unhandled exception filter (`0x005A3740`: `SetUnhandledExceptionFilter`
with `0x005A34D0`, not a Ghidra function; an empty `__finally` round the call). The filter, given exception pointers,
writes `crash.txt` in the game's folder: the folder object of `[0x0075DAD8]` (`0x004F9BE0`; one `\` and then one `/`
at its end taken off) opens `<folder>\crash.txt` to write (`0x004FD540` mode 1: `CreateFileA` with `GENERIC_WRITE`, no
sharing, `CREATE_ALWAYS`, `FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN`; a missing folder is offered to be made
with the question "Create directory?"), and each line is printed (`0x004FB590`: `vsprintf` into `[0x0079EDF8]`) and
written as it comes:

```
ExceptionCode = 0x%x            the record's +0x00
ExceptionFlags = %d             +0x04
ExceptionAddress = 0x%x         +0x0C
ExceptionNumParameters = %d     +0x10
ExceptionInformation[0] = 0x%x  +0x14
ExceptionInformation[1] = 0x%x  +0x18
Edi = 0x%x                      the context's +0x9C
Esi, Ebx, Edx, Ecx, Eax, Ebp, Eip, SegCs, EFlags, Esp, SegSs = 0x%x   +0xA0 .. +0xC8 in that order
WinMain = 0x%x                  0x005A38A0
```

every line ending in `\r\n`. The file and the folder are let go and the filter returns 0 (`EXCEPTION_CONTINUE_SEARCH`:
Windows' own handling follows). OpenJuice writes the same file, the 64-bit build the low 32 bits of the pointers and of
the registers of the same names (`Rdi` .. `Rsp`, `Rip` for `Eip`) and of its own `WinMain`; then its own record of the
crash (the stack by the program's symbols) in `OpenJuice.log`, whose filter runs before the main function.

## Main function (`0x00418300`)

1. Random seed: `srand(rand() + 1)` with the C runtime's generator (first value 41, so the seed is 42), then one
   `rand()`. The game object seeds it again early in its start (`0x0040BC25`): the low 32 bits of the platform
   timer's time, so the draws differ from run to run. All the game's draws are those of the main thread (other
   threads have their own generator).
2. `carmodels.dat` must open for reading (4 bytes are read). If it does not: `MessageBoxW(NULL, text, L"Juiced",
   MB_OK)` with the startup's text 1, and the game ends. The startup's texts (`0x0059BBE0`, also asked by the window's
   start for another instance, 1, and the setup's start for a missing JuicedConfig, 3) are the jettext lines 114 ..
   117 (`[0x0075F0B8]` .., stored by `0x004FAC20`: virtual memory, another user running Juiced, reinstall, JuicedConfig
   missing), 4 "Please reinstall Juiced" (`[0x00725794]`). Asked before the game's language is loaded, `scripts` of
   the game's folder is mounted and the texts read from `scripts\jettext<letter>.txt`: in the reference build, made
   for another region, with that region's language (a constant, `0x0059BC2C`) whatever the game's; OpenJuice, the
   region binding removed, with the game's language, chosen as the main function starts. When that file does not
   read, every index gives text 4.
3. The game object is created (`0x0040BAC0`, see below).
4. Arguments: `-i` instant race (`[0x0075ECB8]`), `-wet`, `-m` (automatic gearbox off), `-cruise` (compared, then
   nothing: the comparison's result is not used), `-automod N`; network: `-host`, `-connect <address>`, `-name <name>`, `-email <address>`, `-ppass <password>`. `-automod`
   (`[0x0075EF40]`) takes the next word as N (`[0x0075EF44]`, `atol`) and passes it over; each car a race makes
   (`0x00485E5B`) then, with N not 0, is modified automatically (`0x0048F2C0`: N 4 as a prototype with parts of any
   level, else of level N) and gets `+0x0F` 1, after its model was made; the engine's sound takes N (`0x00493970`,
   `+0x138`).
5. With `-i` (`0x004184D2`): the input manager's (`[game + 0x18]`) `+0x28` 0, a new driver "Player" (`0x0040AB80`, an
   id from four `rand()` values) in the save system object whose music volume (the settings' `+0x00`) is 0; the race
   (`[game + 0x6C]`): gearbox 1 (0 with `-m`), one player, no opponents, `+0x64` 1, not a replay's, weather 1 (2 with
   `-wet`), heat 0; the rest stays as the game object's start set it (`08-career.md`: a custom race on Angel
   SuperSpeedway's `Speedway_r1`, 99999 laps, at night).
6. The first screen is pushed: "disclaimer" (or "loading" with `-i`), then the main loop runs.

## Game object: platform start (`0x0059C5E0`, title "Juiced")

1. Direct3D 9 is created (`D3D_SDK_VERSION` 32) to find the adapter of the settings.
2. Settings key `HKEY_CURRENT_USER\Software\THQ\Juiced` (created if missing). `Adapter` (16 bytes) is the
   `DeviceIdentifier` of the chosen adapter; every adapter is compared with it. On the matching one the monitor's
   work area is taken (`GetMonitorInfo`) for centering the window.
3. Without a match the configuration tool is run: `JuicedConfig.exe` (the title + "Config.exe", from the current
   directory, `DETACHED_PROCESS`, empty command line); the game waits for it to end (checks once a second) and reads
   the key again. After two runs without a match the game ends. If the tool does not start: `MessageBoxW(NULL, text,
   L"Error", MB_OK)`.
4. With a match the settings are read (missing values keep their defaults):

   | value | default | use |
   |---|---|---|
   | `RezWidth`, `RezHeight` | 800, 600 | client size of the window, back buffer |
   | `RezDepth` | 16 | 16 or 32 bit formats |
   | `Antialiasing` | 1 | multisample level |
   | `Shaders` | 99 | shader level |
   | `Windowed` | 1 | windowed or full screen |
   | `language` | -1 | a Windows language id for the English build 1.0.1 (`02-frontend.md`); -1 also when missing |
   | `audioflags` | 2 | audio flags |
   | `widescreen` | 0 | wide screen |
   | `MultiTurn` | 0 | |
   | `threading` | 1 | worker threads |

5. Single instance (`0x0059BEF0`): if a window of the class "setup.exe" exists it is enabled, focused and shown and
   the game ends. A window of the class "jet" and the mutex `Global\JuicedTheGame`: the mutex is created; if it
   already existed or a "jet" window exists, the existing window is enabled, focused and shown (or, without a window,
   the localized "already running" text is shown with `MB_SETFOREGROUND | MB_TOPMOST` under the caption "Juiced") and
   the game ends.
6. Window class "jet": style `CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS`, icon `game.ico` from the current directory
   (64 x 64, `LR_LOADFROMFILE`) or the resource "AppIcon", arrow cursor, black brush, menu name "AppMenu".
7. Window (`0x0059C070`): `WS_EX_APPWINDOW`; style `WS_CAPTION | WS_SYSMENU` windowed, `WS_POPUP` full screen; the
   client area is `RezWidth x RezHeight` (`AdjustWindowRect`); centered on the adapter monitor's work area when it
   was found, else `CW_USEDEFAULT`. Title "Juiced".
8. The window is shown (`SW_SHOWNORMAL`) and updated.

## Renderer (`0x00513B90` → `0x00512F30`)

1. Direct3D 9 is created again; the client rectangle of the window gives the back buffer size.
2. Present parameters: `SwapEffect = DISCARD`, `Windowed` from the settings, `EnableAutoDepthStencil = TRUE`,
   `Flags = DISCARDDEPTHSTENCIL`, `PresentationInterval` = DEFAULT (ONE with `-r`), back buffer count 0.
3. Formats (`0x00512BB0`, `RezDepth` 16 or 32): the back buffer takes the display mode's format when windowed, else
   the first format the adapter accepts from a list (32 bit: X8R8G8B8, A2R10G10B10, A8R8G8B8, X8B8G8R8, R5G6B5; 16 bit:
   R5G6B5, X1R5G5B5, then the 32 bit ones); depth: D24S8, D24X8, D24X4S4, D16 (D15S1 in the 16 bit list); the clear
   flags include the stencil when the depth format has one. With antialiasing on: `MultiSampleType = NONMASKABLE`,
   quality `min(levels, Antialiasing) - 1`.
4. Behaviour flags: `HARDWARE_VERTEXPROCESSING | PUREDEVICE` (without PUREDEVICE under `-nonpured3d`); `MIXED` when
   the adapter's shader level is not above 1.x; `SOFTWARE` under `-debugfx`, `-s`, or without hardware transform and
   lighting; `| MULTITHREADED` under `-mtd3d`. If `CreateDevice` fails, it is tried again with the depth format D16; 1.01
   (`0x00514EBB`) repeats the pair up to 250 times, 40 ms apart (`Sleep`), where 1.00 gave up (`patch-1.01.md`).

## Debug pages

The game object's start makes, after the game's music (`0x0040D200`), three debug objects nothing on the PC ever shows
(`0x0040C68D` .. `0x0040C6D4`), all on the game's 2D view (`[game + 0x70]`, `0x00517850`: the screen in pixels), its
font (`[game + 0x74]`, which nothing sets: 0) and the platform (`[game + 4]`); the stock game never frees them.

- The pages' manager (`[game + 0x78]` = `[0x0075F7A0]`, 0x24 bytes, vtable `0x006FD934`; made by `0x0059CB30`, which
  counts the one there again): a list of pages (each `{draw, data, view, font, platform, 0}`, a new one put before the
  first, `0x0059CCB0`), the current page (`+0x20`, none at first) and a flag shown (`+0x1C`, 0). Its methods: shown or
  not (`0x0059CD90`: shown with pages and none current, the first), next (`0x0059CE30`, shown or forced; past the last
  the first), previous (`0x0059CE60`), remove (`0x0059CDD0`: the current one first moved on, forced), shown
  (`0x0059CB20`), a text line in a font and colour (`0x0059CE80`). The renderer's frame (`0x00514B5A`) calls the
  current page's draw while the pages are shown; the only call of shown is the race's end (`0x00425BD7` in
  `0x004259A0`), with 0.
- The profiler (`[0x0075F7A4]`, 0xC58 bytes, `0x0059CED0` / `0x0059D0B0`): 30 timers of 0x68 bytes (from `+0x14`: the
  ticks run summed `+0x18`, the most share of a frame `+0x20`, start `+0x24`, end `+0x28`, starts `+0x2C`, the share
  smoothed `+0x30`, the timer running before `+0x34`, shown `+0x38` 1, the name `+0x3C`), timer 0 "CPU Frame" (a
  second column "VU1 Frame", `[0x0075F7A8]`, is never made), the running one (`+0xC48`) and their count (`+0xC4C`, 1).
  The platform's timer (vtable `0x006FDB80`) gives the ticks: the low 32 bits of `QueryPerformanceCounter`
  (`0x005A2370`) and the frequency (`0x005A2390`). A timer is added by name (`0x0059D220`, 0 when the 30 are taken);
  started (`0x0059D270`: its start, starts + 1, the running one its parent, it the running one); stopped by its
  callers (the running one's end and its ticks summed; but for timer 0 its start and end made relative to timer 0's
  start; its parent running again). The renderer's frame ends timer 0 (`0x00514B28`: end, sum; the running one
  left as it is) and starts it again (`0x00514C4B`) before the effects' frame. "Physics" (added at the first race
  frame, `0x00425E29`) runs from the physics' steps (`0x00425E87`) to the players' frame (`0x004260A1`); "Audio - 3d"
  (`0x0040CDA0`, at the first game frame with a race) over the 3D world's frame (`0x0040CE14`). Its page (one page
  object for both columns, `[0x0075F7AC]`, `0x0059CF60`; callback `0x0059D080` → `0x0059D2B0`) would end the timers
  still running, then draw from the column's base (200 a column + 40): the marks of a frame of 60 Hz (vertical lines
  at x 61, 61 + 452 / 3 and 61 + 2 x 452 / 3 of the frequency's 60ths, 508 down, `0x40141480`), each shown timer's bar
  from its start to its end (452 pixels for three frames, from x 60; 8 high, a row of 25): `0x5AFF1414` when the frame
  took longer than 1/60 s, else `0x32801414` (blue for the second column), and each timer's line "  %s - % 3.1f :
  % 3.1f" (the name, its share of a frame smoothed 0.95 / 0.05, its most) black at (29, row - 5) and `0xFF32FF32`
  at (30, row - 6).
- The memory page (`[0x0075F7B0]`, 0x34 bytes, `0x0059D970` / `0x0059D870`): its page (`0x0059DA00` → `0x0059DA10`)
  would darken the view (`0x3C000000`) and write "Please Wait..." in red across the middle, then gather the allocation
  trackers (`0x0059DBB0`: the registry `[0x0075F7EC]` .. `[0x0075F808]`, 8 slots, counted by `0x005A4170`; each
  tracker's name and the bytes of its allocations, `0x005A41E0`; sorted by the bytes, largest first; the chosen one's
  allocations grouped by file and line, the largest that fit the screen, `0x005A4250`), then draw them as a tree
  (`0x0059DE80`: "Total" with a child for each tracker and theirs for its groups; only the root's line is drawn,
  `0x0059E590`: name, bar, "%.02fk"). Nothing registers a tracker, so it gathers nothing.

The game's other debug output goes to the debugger only: the pool's statistics as a race's load ends (`0x0040D9F0` from
`0x0046348E`: "POOL STATS:", the pool's size `[0x007116D0]` 0x02000000 and the most used of it since the last load and
ever, `[0x0075F7B8]` / `[0x0075F7BC]`; OpenJuice has no pool: 0), the race load's times (`0x00461390`: two of the
platform's timers, the whole and its parts; "TRACK LOAD TIME : %.2f secs", then SoundMgr, EffectsDefs, RaceTrack (the
race with its cars) and InGameUI (the race front end)), "LOADING PROFILE : Loading cars... " and "Done!" round the
cars (`0x00495E48`, `0x004972B9`) and "numCores %d" at the renderer's start (`0x005132AC`).

Two more debug switches nothing sets: the debug camera (`0x00721C60`: `+0x60` on, `+0x54`), which each frame of the
race's camera (`0x00406547` → `0x0049F910`) would put at its place looking straight down, or with `+0x54` step the
first player's camera from the view behind the car again with 0.02 s (which calls it again without end); and
`[0x0075F0DC]`, under which LEAVES (`0x00509890`) draws its bounds as circles on the screen (`0x0050D1B0` →
`0x005118F0`: red, yellow once near enough, olive after its draw).

The engine's processor query (`0x005A1F40`) nothing calls either. With CPUID (`0x005A1880`) it notes the features
found: MMX (CPUID 1's EDX bit 23), 3DNow! (0x80000001's EDX bit 31 when the extended leaves reach it), SSE (bit 25),
SSE2 (bit 26), SSE3 with MONITOR (ECX bits 0 and 3); each is tried by running one of its instructions under an
exception handler (`0x005A1900`: pxor and emms, xorps, xorpd, pfrcp and emms, monitor; a fault leaves it out), and an
SSE3 that runs adds bit 1 to the usable ones (MMX's, not its own 0x10: a stock problem). Given a record of 0x44 bytes
it clears it and fills the vendor (`+0x00`), the name (`+0x0D`, `0x005A1A60`: AMD Am486, K5, K6, K6-2, K6-3, ATHLON;
INTEL 486DX .. 486DX4, Pentium, Pentium-MMX, Pentium-Pro, Pentium-II, Celeron, Pentium-III, Pentium-4; Cyrix;
Centaur; else Unknown), the family (`+0x2C`; an extended one 0xF as its byte x 16 | 0xF), the model (`+0x30`, an
extended one left 0xF), the stepping (`+0x34`), the bits found (`+0x38`) and usable (`+0x3C`) and 0x1F (`+0x40`);
it gives the bits found (Core/Base/CpuInfo).

## Screenshots

The window procedure sets `[0x0075F77C]` at Print Screen (`0x0059BE9D`). At the end of the game's frame (`0x0040CFD9`,
after the present), with the input manager made (`[G+0x18]`), the flag is cleared and the application's method
`+0x30` (`0x00513C60`) saves the back buffer (`GetBackBuffer`) with `D3DXSaveSurfaceToFileA` as a JPEG:
`<My Documents>\THQ\Juiced\Screenshots\PCScreenshot<n>.jpg` (`0x005A14F0` with `CSIDL_PERSONAL`, the folders made by
`0x005A12E0`). n (`[0x0079E870]`, 0 at the start) passes each file of the name from which 4 bytes can be read, and
counts one more after the shot. OpenJuice writes the JPEG through the Windows Imaging Component.
