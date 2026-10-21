# The game's files and data (`src/Core/Base`, `src/Core/Formats`, `src/Core/Data`)

How OpenJuice reads the installed game: its folders and archives, the file formats, the data loaded at start and the
texts. OpenJuice carries none of the game's files; everything below is read from the player's installation.

## Reading a file

`GameFiles` (`Core/Base/GameFiles.h`) is the only way to the game's files, rooted at the current directory (the
game's folder):

- `read("tracks\\Angel SuperSpeedway\\...")` gives the file's bytes. As the stock game mounts its folders
  (0x004F9970), a folder with a `<folder>.dat` beside it is read from that archive (`Formats/DatArchive.h`, its
  files zlib streams, `Formats/Inflate.h`), else from the folder itself. An installation packed or unpacked reads the
  same.
- A name not found is tried again localized: `_<letter>` before its extension, the letter of the game's language
  (`setLanguageLetter`).
- `resolve` gives a loose file's path, for what the stock game opens itself (the audio, the movies: never packed).
- `valid()` is a game's folder: one holding `tracks\`.

The text files (`scripts\*.txt`, the `.cfg`, `.atm`, `.ui`) are lines of a keyword and its words
(`Core/Base/Script.h`, `parseScript`; `docs/spec/05-scripts.md`). Binary files are read with `ByteReader`
(`Core/Base/ByteReader.h`): little-endian, a read past the end marks it bad and gives zeros.

## The formats

| file | reader | what it is |
|---|---|---|
| `.dat` | `Formats/DatArchive.h` | a folder packed into one file |
| `.ddt` | `Formats/Ddt.h` | a texture: a 44-byte header and its mip levels |
| `.d8f` | `Formats/D8f.h` | a bitmap font of the interface, one per screen size class |
| `.boh`, `.d8m`, `.pmd` | `Formats/Model.h` | a model's node tree, meshes and materials |
| `.fxo` | `Formats/Effect.h` | a compiled effect (`effects\material_*.fxo`) |
| `.cld` | `Formats/Cld.h` | a route's collision: surfaces, triangles, walls, its quadtree |
| `.ocd` | `Formats/Ocd.h` | a car's or a physics object's collision mesh |
| `.dlh` | `Formats/Dlh.h` | a track's scene objects and their node tree |
| `.tdf`, `.d8g`, `.spl`, .. | `Core/Tracks/Track.h` | a track's routes, geometry layers, racing lines |
| `.atm` | `Core/Tracks/Atmosphere.h` | an atmosphere: lights, fog, sky, sun of a time of day |
| `.sqh`, `.pqh` | `Core/Tracks/SunMap.h` | a track's ground maps: the sun on moving objects, the puddles |
| `.ui` | `Game/UI/UiFile.h` | a window of the interface (`ui.md`) |
| `.cfg`, `.dat`, `.dsb` of `audio\` | `Audio/Bank.h` | a sound bank (`audio.md`) |

The byte layouts the stock game's own tools never documented are described by the Blender add-on's technical
documents, which the readers' comments name.

## The data loaded at start

`GameData` (`Core/Data/GameData.h`) is the game object's data, loaded by `load` in the stock order (0x0040BAC0),
which is also the order of its `rand()` draws:

1. `tables`: the executable's name tables (`Core/Data/ExeTables.h`). They are part of the program, not of the game's
   files, so they are built in (`ExeTables::stock()`, `StockTables.cpp`, the retail build's); no `Juiced.exe` is
   read. `ExeTables::load` reads them from an executable for the test that compares the two.
2. `defs`: `scripts\cardefs.txt`, the car models with their parts and colours (`Core/Cars/CarDefs.h`; it draws
   `rand()`).
3. `catalogue`: the parts' catalogue; `dynamics`: the cars' `scripts\dynamic_<car>.txt`, each read when first asked
   for (`DynamicsStore`, `Core/Cars/CarPhysics.h`; the file's contents `Core/Physics/Dynamics.h`).
4. The arcade's cars (`arcadecollection.txt`), the career objects, the arcade (`ArcadeRaces.txt`), the custom race's
   car groups (`ArcadeUnlock.txt`), for the first and the second player.
5. The first race (a custom race on Angel SuperSpeedway, as the stock's 0x0040D0C0), the music's songs.

`GameData` also holds what the game object keeps between the front end and the race: the race to run (`race`,
`RaceSetup`), the driver's settings, the career (`career`), the arcade, the tips, the playlist. Its methods are the
stock's operations on them that several windows share: a new career (`newCareer`), the race of the current event
(`setupRace`, `startRace`), the next day, the dealer's cars, the cheats (`career.md`).

Other data, read where it is used:

- `Core/Data/Controls.h`: `scripts\controls_dx9_<letter>.txt`, the actions of the controls' maps (UI, DRIVER1, ..).
- `Core/Data/EffectDefs.h`: the particle effects' definitions, read as a race loads.
- `Core/Data/NameFilter.h`: the names a driver may not take.

## Texts and languages

- `Language` (`Core/Data/Language.h`) is one of the eight: its name (`english` .. `russian`), its letter, its index.
  The build OpenJuice follows was made for another region; OpenJuice picks the language as the English build does
  (`chooseLanguage`, `app.md`), among all eight.
- `TextTable` reads a text file of numbered lines (`scripts\<language>.txt`, `scripts\jettext<letter>.txt`).
- `GameTexts.h` and `JetTexts.h` name the lines the code reads by number (`kTextPlayer2`, ..), each with its English
  text in the comment.
- A window's texts are its own files (`ui\windows\<language>\<window>.*`), read with the window (`ui.md`).
