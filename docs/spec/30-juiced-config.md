# The configuration tool

`JuicedConfig.exe` (139264 bytes, built May 2005, 32-bit), the game's settings tool: one window that writes the
settings the game reads at start (`01-startup.md`, "Game object: platform start") into
`HKEY_CURRENT_USER\Software\THQ\Juiced`. The game runs it when no saved adapter matches one of the system's, and
`-setup` opens it. OpenJuiceConfig (`src/JuicedConfig`) is its port: `OpenJuiceConfig.exe` and
`OpenJuiceConfig64.exe`, with the stock's resources byte for byte (the window, its pictures, its icon) but four
languages' texts (below, "The texts") and OpenJuice's version resource. The stock tool speaks through the ANSI
functions throughout.

## The start (`0x004053C0`)

1. `setlocale(LC_ALL, "Russian")`.
2. The window class (`0x004020B0`): a count of its users (`[0x00412B78]`); the first registers the game's own class
   "jet" as the game does (style 0xB, a procedure returning 0, `game.ico` from the file at 64 x 64 else the icon
   "AppIcon", the arrow, the black brush, the menu "AppMenu"), its window never made. With a "jet" window up (the
   game running) that window is enabled, focused and shown instead and the tool ends.
3. `InitCommonControlsEx` with the tab control's class; Direct3D 9 (`Direct3DCreate9(32)`).
4. The DirectX version (`0x004021B0`): DxDiag's provider (COM started and stopped again, the header version 111, no
   WHQL checks), its `DxDiag_SystemInfo`: `dwDirectXVersionMajor`, `dwDirectXVersionMinor` (`VT_UI4`) and the first
   letter of `szDirectXVersionLetter` (a `BSTR` through the ANSI code page); `S_OK` only when all three are read.
   DirectX 9.0c or later (`[0x00412B74]`): `S_OK`, the major 9 or more, the letter (signed) `'c'` or later; the minor
   is not looked at.
5. With Direct3D and DirectX 9.0c: the window (`DialogBoxParamA`, DIALOGEX 102, `0x004049C0`); after it Direct3D
   released, the class's count lowered and the class unregistered at 0. Else `MessageBoxA(NULL, text 318, "Juiced",
   MB_OK)` ("You need to install DirectX9.0c runtime or better ...").

Windows 11 (checked on the stock under a debugger): DxDiag reports 12.0 and the letter a space (32), so the tool shows
the message and never its window (a stock problem).

## The texts

The string table (language 0x0815) holds the texts 110 .. 349 (and 350 .. 360, each "a", unused), eight ids a text
in the order English, French,
Italian, German, Spanish, Czech, Polish, Russian. A text's id for a language (`[0x00412040]`) is the first id plus the
language's number (English 0 .. Russian 7), but German takes the fourth (+3) and Italian the third (+2)
(`0x00401260` and each `LoadStringA` of the tool, into `[0x00412F98]`, 0x200 bytes). The reference build was made
for another region: its tool fixes the language to that region's, 7 (the default, set again as the window is filled,
`0x004025EA`). The French, Italian, German and Spanish texts went in through the Cyrillic code page: their accented
letters read as Cyrillic ones ("Paramиtres"). Through the ANSI functions a language outside the system's code page
reads as question marks.

OpenJuiceConfig has the region binding removed, as OpenJuice's game has: its language is the one the game picks
(`02-frontend.md`, "Languages and texts": the registry's `language` when it is not negative, else the user's
language id, by its primary language; `[Game] Language` of `OpenJuice.ini` beside the tool names one instead). The
four languages' texts have their own letters back ("Paramètres"), and the window takes its texts through the wide
functions, so each of the eight reads whole on any system.

| first id | text | first id | text |
|---|---|---|---|
| 110 | Settings (a tab) | 238 | Music |
| 118 | Troubleshooting (a tab) | 246 | Hardware Mixing |
| 126 | Display | 254 | All Audio |
| 134 | Display Adapter | 262 | Shader 1.0 |
| 142 | Display Mode | 270 | Shader 2.0 |
| 150 | Antialiasing | 278 | Shader 3.0 |
| 158 | Quality Level %d | 286 | No Shaders |
| 166 | Windowed | 294 | the full screen's validation asked |
| 174 | Widescreen | 302 | not validated |
| 182 | OK | 310 | validated |
| 190 | Reset To Defaults | 318 | DirectX 9.0c needed |
| 198 | Cancel | 326 | Input |
| 206 | Video | 334 | Multi-turn wheel |
| 214 | Shaders | 342 | MultiThreading |
| 222 | Audio | | |
| 230 | Speech | | |

## The window

DIALOGEX 102 "Juiced Config", 454 x 207 units, two tabs (1018): Settings (0) and Troubleshooting (1). The tab shown
(`[0x00412B80]`) is looked at first on every message: when it changed, the controls are turned on and off for it
(`0x00401EB0`). Its first look, before the tabs are made, finds none (-1).

| id | control | tab |
|---|---|---|
| 1021, 1026, 1001 | the Display group; Display Adapter's label and list | Settings |
| 1025, 1002 | Display Mode's label and list | Settings |
| 1013, 1012 | Antialiasing's label and list | Settings |
| 1003, 1004 | Windowed, Widescreen | Settings |
| 1023, 1015, 1014 | the Video group; Shaders' label and list | Troubleshooting |
| 1022, 1020 | the Audio group; All Audio | Troubleshooting |
| 1030, 1024, 1019 | Speech, Music, Hardware Mixing (on only with All Audio) | Troubleshooting |
| 1033 | EAX (on only with All Audio and eax.dll's EAX) | Troubleshooting |
| 1027, 1034 | the threading group; MultiThreading (on only with two processors or more) | Troubleshooting |
| 1032, 1031 | the Input group; Multi-turn wheel | Troubleshooting |
| 1010, 1017, 1011 | OK, Reset To Defaults, Cancel | both |
| 1028, 1029 | the pictures 107 (Juice Games) and 106 | both |

The labels' and boxes' texts are set from the string table (`0x00401C70`) each time the window is filled; the
dialog's own texts are the English ones.

### Filling the window (`0x004025E0`, the window's start and Reset To Defaults)

1. The language 7; the adapters by their descriptions (the item data their ordinals), the first selected. A saved
   adapter that is none of them, or no key (`0x00401120`), takes the defaults.
2. The shaders' list (`0x00401950`) with the adapter's level - 1 (0 above 3) as the selection kept.
3. The key opened (`KEY_QUERY_VALUE`); the mode 800 x 600 x 32, not windowed, no antialiasing; the shaders the level
   - 1 (3 above 3). Without the key straight to step 6.
4. Unless the defaults are asked: `RezWidth`, `RezHeight`, `RezDepth`, `Antialiasing`, `Shaders` (the selection, at
   most the list's count), `Windowed`, `Adapter` (the matching item selected, else the first), then `audioflags` and
   `widescreen`; the sound's boxes from the flags (below; EAX cleared without eax.dll's EAX), `MultiTurn`;
   `threading` (0 with one processor). A missing `threading` takes the defaults over all of it.
5. The defaults: no antialiasing, speech, music, hardware mixing and all audio on, EAX by eax.dll, threading with two
   processors or more, the flags 2, no widescreen or multi-turn wheel. The boxes set; the key closed.
6. The mode kept (`[0x00412044]`) from the width, height and depth; Windowed checked as read; the adapter's
   `CBN_SELCHANGE` sent (the mode list); the antialiasing's list, at 0 with the defaults.
7. The first time only: the tabs, Troubleshooting inserted at 0 and then Settings at 0, Settings selected.
8. The boxes set again (`0x004024A0`, the controls by the tab) and the texts.

### The lists

- **Display Mode** (the adapter's `CBN_SELCHANGE`, also sent by Windowed): with Windowed checked the table
  `[0x004120D8]` (640 x 480, 800 x 600, 1024 x 768, 1280 x 960, 1600 x 1200) within the desktop's display mode, at its
  depth (16 for `X1R5G5B5`, `A1R5G5B5` and `R5G6B5`, else 32). Full screen: the adapter's `X8R8G8B8` modes then its
  `R5G6B5` ones, each from the last down, from 640 x 480 up, each size and depth once, sorted by width, height and
  depth (`0x00402030`). An item's text is "%dx%d %d-bit", its data the key: the width 12 bits up, the height, bit 31
  for 16 bits. The item selected is the nearest to the mode kept: the least sum of the squared differences of depth,
  width and height, the first of equals. Then the antialiasing's list. A mode picked from the list is kept.
- **Antialiasing** (`0x004012C0`): the quality levels of `D3DMULTISAMPLE_NONMASKABLE` for the adapter (HAL), the
  mode's format (`R5G6B5` for 16 bits, else `X8R8G8B8`) and Windowed; items "Quality Level n" for n 0 .. the levels
  (n is the registry's value: the game asks for quality n - 1, 0 is none). The value kept is the list's selection,
  else the registry's (`[0x00412B84]`), 0 with the defaults; the first item is selected, then the kept one.
- **Shaders** (`0x00401950`): by the adapter's caps (`0x00401000`, the level: 1 below pixel shaders 1.0, 2 below 2.0,
  3 below 3.0 or below vertex shaders 3.0, else 4): No Shaders (0), then Shader 1.0 (1), 2.0 (2), 3.0 (3) as the level
  allows. The selection is the item data of the one selected (none after the list is made anew: the value kept),
  taken as an index.

### The buttons

- **OK**: the four lists must each have a selection (`0x00401530`); full screen asks first (text 294, OK and Cancel,
  "Juiced Config"; Cancel keeps the window). The mode's test (`0x00401080`) is built to nothing, so every mode
  passes; full screen then says so (text 310). Then the key (`KEY_SET_VALUE | KEY_CREATE_SUB_KEY`) gets the values
  below and the window ends (1); the window ends too when the key does not open.
- **Cancel** and the window's close end it (0). **Reset To Defaults** fills the window with the defaults. **All
  Audio** reads the boxes and sets them again (the dependent boxes on or off).

| value | type | what |
|---|---|---|
| `RezWidth`, `RezHeight`, `RezDepth` | DWORD | the mode's width, height and 16 or 32 |
| `Adapter` | BINARY, 16 bytes | the adapter's `DeviceIdentifier` |
| `Windowed` | DWORD | 1 checked, else 0 |
| `Antialiasing` | DWORD | the antialiasing item's n |
| `Shaders` | DWORD | the shaders item's data |
| `audioflags` | DWORD | the sound's flags (`0x004017D0`) |
| `widescreen`, `MultiTurn` | DWORD | 1 checked, else 0 |
| `threading` | DWORD | 1 checked (with two processors or more), else 0 |

`audioflags`: bit 0 no sound (All Audio clear), bit 1 hardware mixing else bit 2, bit 3 no music, bit 4 no speech,
bit 5 no EAX. Read back, the boxes are the bits' opposites, hardware mixing bit 1 alone.

## EAX (`0x004058D0`)

`eax.dll` loaded from the current folder (never freed), its `EAXDirectSoundCreate8` asked for a device (never
released). With one, the probe (`0x00405550`): a 64-byte 3D buffer in the hardware (mono, 22050 Hz, 16 bits), its
`IKsPropertySet`; the EAX 4.0 context, the four FX slots and the source asked for, the device's flags from its
interface `kDeviceFlags` (cleared without bit 0x10). The answer is the flags' bit 0x10. The window asks it as it is
filled and each time the controls are turned on or off with All Audio checked.

## Processors (`0x004010D0`)

More than one bit among the first 32 of the system's affinity mask. The controls' turning on and off counts again
each time.
