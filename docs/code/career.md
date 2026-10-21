# The career, the cars and the saves (`src/Core/Career`, `src/Core/Cars`, `src/Core/Saves`)

How the career, the arcade, the cars and the saves are kept, and how the front end's windows reach them. What the
stock game does is `docs/spec/03-saves.md`, `06-cars.md`, `08-career.md`, `09-arcade.md`, `14-career-windows.md`.

## The pieces

- **`GameData`** (`Core/Data/GameData.h`, `data.md`) holds them all for the game: the career, the arcade, the cars'
  definitions and catalogue, the race to run, the driver's settings. Its methods are the stock operations several
  windows share (a new career, the race of the current event, the next day, the dealer's cars, the cheats).
- **`Career`** (`Core/Career/Career.h`): the career object with its parts at their stock places: the calendar and its
  events, the crews and crewmates, the phone (`Phone.h`), the advisor (`CareerAdvice.h`), the race session
  (`Core/Race/RaceSession.h`: the racers, the odds and the bets), the cruise manager, the garage and the dealer's
  cars, the online pink slip's stakes. `CareerInit.h` makes a new one, `CareerEvents.h` fills the calendar,
  `CareerFiles.h` reads the crew files (`CMSCrews.txt`, ..), `Money.h` shows amounts as the windows do.
- **The cars** (`Core/Cars`): `Car` is the stock car object (its model, the parts fitted, the paint, what the saves
  keep), `CarDefs` the models of `cardefs.txt`, `CarCollection` the car lists and the automatic modification,
  `CarPhysics` a car's physics with its parts and its rating, `CarRepair` a career car's repair, `CarDamage` the dents
  (`race.md`), `CarEffects` a race car's effects' emitters.
- **The arcade** (`Core/Race/Arcade.h`): the championships of `ArcadeRaces.txt` with the driver's progress, and the
  custom race's cars by group (`ArcadeUnlock.txt`).
- **The saves** (`Core/Saves`): `Profile` is the driver being played (the save system object: name, id, settings,
  help pages seen), `Save.h` the files (`%APPDATA%\THQ\Juiced\SAVE\P<driver>\juicey.dat` and `USERDATA`,
  `SAVE\HS\USERDATA`, `reg.cf1`), `SaveEntries.h` the smaller entries of `juicey.dat`, `HighScores` the routes'
  records, `GraphicsConfig` the renderer's settings the options write.

## Layouts the saves keep

The career's objects keep the stock layout: the save writes the career's fields as they are, and a field nothing sets
keeps its value, as in the stock game. So the members are at their stock places with their offsets in the comments
(`RaceSession session;  // +0x30 (not saved)`): OpenJuice reads and writes the stock game's own `SAVE` folder. The
few fields whose meaning is not traced are named by their offset (`valueAC`).

## The save device

The stock windows do not write files themselves. They give the save device (`Core/Saves/SaveDevice.h`) a request in
a block of globals (`SaveRequest`) and it answers there with state flags (`SaveFlag`), one step a frame and a file at
a time:

1. A window (`driver`, `saveprofile`, `autosave`, `loadhighscores`, `savehighscores`, ..) sets the request: list, read,
   write or delete, and the slot.
2. Each frame the front end steps its device (`stepProfile`, `stepHighScores`, `stepReplay`): the manager moves
   through its states (idle, list, read, write); a write first makes its source (the files' bytes) from the game's
   side (`SaveGameSide`), a read applies what it read.
3. The window reads the flags (`flag(SaveFlag::..)`) and the list (`slots()`), and goes on when the state is back to
   idle.

The front end's side is `Game/Frontend/FrontendSaves.cpp` and the family `Game/Frontend/Saves` (`frontend.md`); the
race front end's is `Game/Race/Hud/RaceSaves.h` (`race-frontend.md`).

## `rand()` in the career

A new career, the calendar's events, the dealer's offers, the AI's skills, a new driver's id: all draw `rand()`, and
the order of the draws decides what the player gets. The career's functions take the generator (`CrtRandom&`) of the
thread that runs them in the stock game, and are called where the stock calls them; a change that moves a call moves
every value after it.

## Testing

- `tests/Unit/Career`, `Cars`, `Saves`: the dealer, the money's text, the parts, the damage, the archive of a save,
  the settings, the graphics settings.
- `tests/Integration/GameData`: a new career from the game's files with the values the stock game gives it, the
  arcade's progress, a new driver (`CareerTests`); the car definitions and physics.
- `tests/Integration/Saves`: the save device writing, listing, reading and deleting a driver in a folder next to the
  tests; the replays' device; the player's own saves read (`UserSaves`, when there are any).
