# Saves of the stock game

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## Where

The save root is `<CSIDL_APPDATA>\THQ\<title>` (`SHGetSpecialFolderPathA(..., CSIDL_APPDATA, TRUE)`, the title
"Juiced"; `0x005A14F0`). In it:

| path | contents |
|---|---|
| `reg.cf1` | 0x4C bytes of global settings |
| `SAVE\P<driver>\juicey.dat` | the driver's save (an archive of entries) |
| `SAVE\P<driver>\USERDATA` | the driver's name |
| `SAVE\HS\USERDATA` | the routes' records (below) |

The folders are made by the game: the save device's file object (`0x00502E60`, made with its manager at the game's
start) makes `<root>\SAVE\` with each folder above it below the special folder (`0x005A12E0`: `CreateDirectoryA` at
each `\`, then the path; one there or one that cannot be made passed over), and reg.cf1's writer (`0x00481630`) makes
the root so before it writes the file anew. The device itself makes only `SAVE` and the save's folder (`0x00502B30`,
`0x00502C50`): without the root its write ends with no flag ("You will not be able to save your progress"), as on a
player's AppData where the folders were never made.

## `reg.cf1` (`0x00502D20` writes, `0x004816A0` reads)

```
u32 version         0x10000
u32 sum             the 16 dwords after it added up (0x004815F0)
u32 values[16]      (seen: 2, 3, 1.0f, 1, 1, 1, 0.5f, 0, ...)
u32 pointer         the game's in-memory pointer to `values`, written along, not used when read
```

A file whose version or sum does not hold is ignored.

## `USERDATA` of a driver (`0x00502490` reads)

`f32` (1.38 in the saves seen), then the driver's name in UTF-16 ending with 0. The folder is "P" + the name.

## `SAVE\HS\USERDATA` (`0x0040A370` writes, `0x0040A440` reads)

The routes' records of the save system object at the game object's `+0xA8` (`0x0040A0E0`, 0x1128 bytes): the id
(`+0x1118`, 8 bytes: four `rand()` values at the start, as a driver's), then the 109 records of the routes' table
(`0x00711A3C`, `+0x0C`), 0x28 bytes each:

```
u16 name[16]   the driver's name, at most 15 characters and a 0 (a new table: "-")
f32 time       the best lap (300 s in a new table; 0 for a route of type 3, where the most counts)
i32 car        the car model's id
```

Reading or writing sets the object's `+0x08`; a new record clears it. `+0x1120` is set once `loadhighscores` has
read the file (the main menu's first run). A lap is a new record (`0x0040A1C0`) on a route of type 3 when greater,
on another when under the record and at least 1 s; `0x0040A220` / `0x0040A290` keep it with the driver's name (its
first 15 characters) and the car's model.

## `juicey.dat` (`0x00501700` writes, `0x00501500` reads)

```
u32 entryCount
u32 directoryOffset     8 + the entries' sizes
u8  entries[]           the entries' bytes one after another, in the order of the directory
directory[entryCount]:  char name[128] (zero padded), u32 size; names in byte order (the game's map)
u64 hash                of all bytes from offset 8 to the directory (0x00503030, vtable 0x006F70A4):
                        state = 0x5A5A5A5A5A5A5A5A; per byte: state = rotl64(state, 1) ^ byte
```

A save whose hash does not hold is not loaded. The save's files container packs every file but `USERDATA`
(`[0x00778A00]`) into `juicey.dat` (`[0x00778A10]`) through its `+0x1C` (`0x00501700`), the files taken out of it,
and unpacks it through `+0x20` (`0x00501500`): a file of 8 bytes or less is refused; the directory's records are
read from the offset while the next one ends before the hash's 8 bytes, each entry's bytes the next ones after the
last entry's (as many as there are); the directory must end where the hash starts and the hash must be the one of
the entries read (the entries made before a refusal stay in the container). A name of more than 128 characters runs
past its field when written (stock problem; OpenJuice cuts it).

Entries the stock game writes (`0x0040A610`), each opened by name in the archive:

| entry | writer | size seen |
|---|---|---|
| `settings.dat` | `0x00409A30` | 781 |
| `ArcadeUnlock.sav` | `0x0042ABF0` | 1256 |
| `UIHelp.sav` | `0x00485210` | 52 |
| `CMSGlobal.sav` | u32, then `0x004BD650` (career: the profile, garage, dealer, ... see the CCM notes) | 48214 |

Entries of other names are kept: mods store their own (J1CustomCars: `J1CustomCars.sav`,
`J1CustomCarsGearbox.sav`) and the stock game ignores them.

## `UIHelp.sav` (`0x00485210`)

A u32 per help page of the table at `0x00715D48` (records of 0x4C bytes: u32, the window name), non-zero once the
page was shown: CMSCalendar, CMSCarMenu, CMSRespectOverview, CMSCrew, CMSMobilePhone, CMSGridwalk, CMSRaceSetup,
CMSCrewRaceSetup, CMSCalendar, CMSAtMeeting, instant_selcar, tuningmenu, stylingmenu (13, 52 bytes).

## `ArcadeUnlock.sav` (`0x0042ABF0`)

```
per arcade series (the arcade object's count at +0x4664; 15 in the release), 5 races:
    u32 race+0xCC, u32 race+0xD0          (races 0xD4 apart from the series' +0xCC)
u8  arcade+0x52F0[0xD4]
u8  arcade+0x513C[0x1B4]
u32 arcade+0x5138
u32 arcade+0x5134
```

## `settings.dat` (`0x00409A30`)

Fields of the settings object in this order (offset: size): +0x00:4, +0x04..+0x24 nine u32, +0x2C:4, +0x28:4,
+0x30..+0x44 six u32, +0x78..+0x8C six u32, +0x90:15, +0xE4:62, +0x122:102, +0x188:62, +0x208:16, +0x218:4,
+0x21C:56, +0x254:20, +0x268:72, +0x2B0:36, +0x60:8, +0x58:8, +0x50:8, +0x68:8, +0x70:8, +0xA0:68, +0x4C:4,
+0x2D4:64, +0x314:64 (781 bytes).

The settings object is `+0x54` of the save system object. Its constructor (`0x00409750`) sets a paint at `+0xA0`
(`+0xA0` -1, `+0xA4` 0xFF000000, `+0xA8` -1, `+0xAC` 0.5, `+0xB0` 0.1) and the values below; the values for a new
driver (`0x004097F0`):

| offset | value |
|---|---|
| +0x00, +0x04, +0x08 | 0.9, 0.75, 1.0 (float) |
| +0x0C, +0x10, +0x14, +0x18, +0x1C | 1, 3.0 (float), 3, 2, 1 |
| +0x20 (the crew logo), +0x24 | 0, 0 |
| +0x28, +0x2C, +0x30, +0x34 | 1 |
| +0x38, +0x3C | 0 |
| +0x40 | 1 when the game's language is English (index 0), else 0 |
| +0x44, +0x48 (not saved) | 1 |
| +0x4C | 0 |
| +0x50, +0x54, +0x70, +0x74 | 0.5 (float) |
| +0x58, +0x5C | 0 |
| +0x60, +0x64, +0x68, +0x6C | 1 |
| +0x78 .. +0x84 | 0 |
| +0x88, +0x8C | 2, 1 |
| +0x90 (the crew name, 8-bit, 15 bytes saved) | "The Z's" |
| +0xB4, +0xB8; +0xBC, +0xC0; +0xC4 .. +0xD0; +0xD4; +0xD8 .. +0xE0 | -1; 0; -1; 0; -1 |
| +0xE4, +0x122, +0x188, +0x220, +0x26C (u16) | 0 |
| +0x208 .. +0x214 | -1 |
| +0x21C, +0x240, +0x268, +0x28C, +0x29C .. +0x2A8, +0x2C0, +0x2CC | 0 |
| +0x24C, +0x250, +0x254, +0x294, +0x2AC, +0x2B0 | 1 |
| +0x248, +0x258 .. +0x264, +0x2B4, +0x2B8, +0x2C4, +0x2C8, +0x2D0 | 9999 |
| +0x298, +0x2BC | 7 |
| +0x2D4, +0x314 (64 bytes each) | "none" |

The other bytes are not set; the saves written by the game carry whatever memory held there (seen: pieces of text
read earlier). The crew name is set by `0x00409F30`: the low byte of each character of the name entered, then 0.

## `CMSGlobal.sav` (`0x004BD650`)

The career of the driver; the objects are those of the profile manager at `0x00728C40` (`0x00727C20` in Juiced.exe
1.0).

Reading it (`0x0040A9EF`) takes the head into `[[game + 0x7C]]`, then `0x004BD790` makes a new career over the
objects first (`0x004A2300` with the driver's name the save system has just taken, all its `rand()` draws,
`08-career.md`) and reads the parts below over it, in the same order; what the file does not hold stays as the new
career made it. The phone's reading (`0x004B6070`) also clears the ten message slots' `+0x28`. The driver's
settings choose the music again only after the whole driver is in (`0x00408D20` from `0x00467D8B`).

```
u32 head                          written by the save system (0x0040A610)
block +0x2C (0x004A6380):         u32 x4, u8[0xF78], u32, u32                         0xF90 bytes
profile (0x004B7770), the profile object (obj4):
    u32 +0xC, u8 +0x500[16], u32 +0x510 (count), 4 x u32 (the values the 4 pointers at +0x500 point to, the
    first `count` of them; default values for the rest), u32 +0x620, u32 +0x624, u32 +0x10, u32 +0x14,
    u16 +0x4E, u8 +0x18[14], u8 +0x26[18], u8 +0x38[18], u16 +0x4A, u16 +0x4C,
    table A +0x50[53][10], table B +0x262[53][10], car level +0x474[53], unlock step +0x4A9[53],
    u32, u32 +8, u32 +0x4E8..+0x4FC (6), u32 +0x628
garage: u32 count, count x car, u32 (the vector's +0x10)
padding: u32 count, count x car (default cars filling the garage up to 55), u32
obj5C: u8 +0x83C[0x750], u32 +0xF8C, u32 +8, u32 +0xFA4
obj70 (dealer): u8 +0x27EC[0x27C0], u32 +0x4FAC, u32 +0x27E0
obj78: u8 +0x24[0x36], u32 +0x60
obj7C: u32 +0x708, u8 +0[0x708]
9 crews (0x004A77E0): 0x004BB240 (u8 +0x2C[0x28], u32 +0x70, u8 +0x54[0x10], u32 +0x64, +0x68, +0x6C);
    when the crew's u16 +0x78 is 0 (the player's crew, the first) u8 +0x7A[100] and u8 +0xDE[100]
    (4 member names of 25 characters each); then u32 +0x2C0, +0x2C4, +0x2C8, +0x2CC, +0x2F0, +0x30C, +0x328,
    +0x330, +0x224, +0x2D4, +0x2D8, +0x334, +0x338, +0x2D0
4 rivals (0x004A9EB0): u32 +0x8C, u8 +0x94[0x18], u32 +0xAC..+0xC0 (6), u32 +4
```

A car (`0x0048DD40` writes, `0x0048E020` reads; the car object is 0xBC bytes):

```
char model[0x40]                  the car model's name (+0x9C of its CarDef), how the game finds the model
u32 x12                           +0x18, +0x1C, +0x24, +0x20, +0x28, +0x2C, +0x30, +0x34, +0x38, +0x4C, +0x3C, +0x40
u8  +0x04[10], u8 +0x0E[10]
u32 +0x58, +0x5C
u16 (+0x44 as float x 100, to the nearest; read back x 0.01)
u16 +0x98, +0x9A, +0x9C, +0x9E, +0xA0, +0xA2, +0xA4, +0xA6
u32 +0xA8, +0xAC, +0xB0, +0x50, +0x54, +0x7C, +0x80, ... (14 in all)
u32 grid flag                     +0xB4 is set
u8  grid[16][16]                  only with the flag: the grid object (vtable 0x006FA1D8, 0x41C bytes), per cell
                                  the cell's +0x24 (after adding +0x28) x a constant, truncated; 0 for an empty cell
u32 +0x60, +0x64, +0x68, +0x6C, +0x70, +0x48
```

## The save device

The windows do not read or write saves themselves: they give requests to the save device and show its answers.
The profile device (`[game + 0x60]`, 0x30 bytes, vtable `0x006F6DAC`, made by `0x004FA560` at the game's start) and
the records' device (`[game + 0xA4]`, vtable `0x006F6E10`, `0x004FA9B0`) share one manager (`[0x00728D98]`, 0x40
bytes, `0x004FDB30`) whose file object (`0x00502E60`) works in `SAVE\` of the save root.

The request block (`0x00767FE8`): `+0x00` the request (-1 none, 0 list, 1 write), `+0x04` a question's answer (-1
none, 0 no, 1 yes), `+0x08` go on, `+0x0C` the list left, `+0x10` delete, `+0x14` / `+0x18` set by the idle state,
`+0x1C + 4 k` the state flags, `+0x88` the list's slot, `+0x8C` 1.0 after each step. A device's step (`0x004FA430`
profile, `0x004FAB20` records): its kind into the manager's `+0x3C` (0 profile, 1 replay, 2 records); the files to
write made when asked (`0x004FA190`: USERDATA, 1.38 and the driver's name, and the save system's entries packed into
`juicey.dat`, `0x0040A610`; the records' USERDATA, `0x0040A370`), or an empty set to read into; the manager's step
(`0x004FDE50`): every flag cleared, the state's update, the next state; then `+0x00` .. `+0x10` back to -1 / 0; a
read set taken in (`0x004FA2B0`: the save system's name, folder and entries; the records, `0x0040A440`): when it does
not hold flag 18 instead of 11 and the manager starts again.

| state | update | what it does, a step a frame |
|---|---|---|
| 0 idle | `0x004FDF30` | request 0 → 1 (the records: 2), 1 → 3 |
| 1 list | `0x004FDF70` | the folders of SAVE one a step (FindFirstFile / FindNextFile): drivers "P*" with a USERDATA (at most 11), replays "R*", records "HS"; then sorted (`0x004FF290`: the latest USERDATA first) and numbered: flags 14, 24, 25 (none: 5, 24); `+0x0C` → 0, `+0x08` → 2 (read the slot), `+0x10` → delete: flags 20 and 26 until answered, yes deletes the folder's files and the folder (`0x005029F0`): 21, else 22, and the list again |
| 2 read | `0x004FE510` | the slot's folder ("HS" for the records) listed, then one file a step (CreateFile / ReadFile, at most 1 MB; a driver's file also reads `reg.cf1`): 11 read, 18 a file failing, 15 no folder of the records; 24 |
| 3 write | `0x004FEA10` | the files made; a folder there (not the records'): flags 7 and 26 until answered, no ends; flag 9, SAVE and the folder made (`0x00502C50`), then one file a step in the order of the names' bytes (USERDATA, juicey.dat; `0x00502D20`: `reg.cf1` written first, then the file made anew); flags 12 and 24 each step until `+0x08` |

The replays' device (`[game + 0xA0]`, 0x30 bytes, vtable `0x006F6DC8`, made by `0x004FA8E0` at the game's start right
after the replay, `0x0040C597`) shares the manager as well: `+0x04` (`0x004FA5E0`) gives the replays listed, `+0x08`
(`0x004FA860`) is its step with kind 1, `+0x0C` (`0x004FA8D0`) starts the manager again, `+0x10` (`0x004FA8B0`) takes
its replays' object (`+0x0C`; none is refused) and `+0x14` (`0x004FA5D0`) gives it back. Before the manager's step a
write's set is made (`0x004FA600`): named "R%04d%02d%02d%02d%02d%02d" by the local time (year, month, day, hour,
minute, second) and handed to the replays' object's first method with the text "%d/%d/%02d %2d:%02d" (month, day,
year, the hour less 12 past noon, minute; its answer is not looked at); after it a read set goes to the object's
second method (`0x004FA810`), and when that refuses it flag 18 stands instead of 11 and the manager starts again. The
replay object's own methods `+0x34` and `+0x38` take these very arguments (`24-replay.md`, "Saved replays"). Nothing on
the PC gives the device a replays' object (the constructor leaves `+0x0C` and no code calls `+0x10`), so the stock
game's write or read of a replay calls through address 0; OpenJuice gives it the game's replay.

The flags `k` (`[0x00768004 + 4 k]`) match the lines `k` of the PC block of `scripts\jettext<language>.txt` (comment
lines `#` left out, lines 75 .. 98 of the English file; `[0x00728DA0 + 4 k]`), which the windows show: 7 "Driver
already exists. Overwrite previous Driver?", 9 "Saving data...", 11 "Load Successful", 12 "Save Successful", 15
"Load failed! Data read error.", 18 "Load failed! Save game is corrupt", 20 "Are you sure you want to delete driver ",
22 "Could not delete driver."; 5, 14 and 21 have no text. Their kinds for the windows (`0x006ED9F0`): 2 (a question)
for 6, 7 and 20, 0 for 1 .. 3, else 1. Lines 0 .. 9 and 13, 14 of the file are the globals `0x0075F068` ..
`0x0075F098` (the autosave's questions and notes, "Unable to load hi-scores!", "You will not be able to save your
progress.").

Answering no to question 7 in `saveprofile` (`0x00463FEC`) only opens the name window again ("NEW DRIVER NAME" 472,
"Player" 167, letters and digits, back to `driver`, on to `cmschooselogo`); the game's line 1513 "A driver with that
name already exists!" (`[0x00765940]`) is loaded and shown by nothing.
