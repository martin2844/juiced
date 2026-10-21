# The arcade progress

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## The arcade object

Kept at `0x00728AF4` (and `[game + 0x8C]`):

| offset | meaning |
|---|---|
| `(c x 5 + r) x 0xD4` | race `r` of championship `c` (17 championships at most) |
| +0x4664 | the number of championships read |
| +0x4668 | per championship 20 routes that `[UnlockTracks]` opens (`Track`) |
| +0x4BB8 | per championship 20 bonus routes (`BonusTrack`) |
| +0x511C, +0x5120 | the race and championship being read |
| +0x512C, +0x5130 | a car was just opened, and which |
| +0x5134, +0x5138 | (saved) |
| +0x513C | 109 route flags, 0 = open |
| +0x52F0 | 53 car flags by car id, 0 = open |
| +0x53C4 | a race result waits |

A race (0xD4 bytes): `+0x00` its kind (4 circuit, 3 point-to-point, 5 sprint, 0 a challenge), `+0x04` extreme nitrous,
`+0x08` the developers', `+0x0C` the challenge's kind (0 lap time, 1 perfect lap, 2 speed, 3 showoff), `+0x10` the
challenge's value (float), `+0x14` its route, `+0x18` the laps, `+0x1C` the weather (1 dry, 2 wet), `+0x20` the time of
day (1 .. 4), `+0x24` the AI drivers' skill (float), `+0x28` the number of cars, from `+0x2C` cars of 0x14 bytes
(`+0x00` the number, `+0x04` the player's, `+0x08` the opponent's number, `+0x0C` the car id, `+0x10` its parts
level), `+0xA4` the base paint, `+0xA8` metallic, `+0xAC` pearl, `+0xB0`, `+0xB4` their amounts, `+0xB8` the wheel
colour, `+0xBC` the wheel, `+0xC0` the neon colour, `+0xC4` the decal, `+0xC8` its colour, `+0xCC` locked, `+0xD0`
completed. A championship is 5 races (0x424 bytes). `+0x5108` lists the cars of the races (`0x0042AD40` one for each
race, `0x0042B000` the cars of the race being raced); `+0x5120`, `+0x511C` the championship and the race chosen;
`+0x53C8`, `+0x53CC`, `+0x53D0`, `+0x53D4` the news of a result: the final race unlocked, the series complete, the next
series unlocked, the arcade complete.

## `ArcadeRaces.txt` (`0x0042A960`)

The count, `+0x53C4`, `+0x5134`, `+0x5138` and both route lists are cleared; the reading starts with race -1 of
championship 0 and a count of 1.

| keyword | effect |
|---|---|
| `[Race] n` | the next race; after the fifth the next championship (the count goes up). The race is cleared, then `+0xA4`, `+0xA8`, `+0xAC` = -1, `+0xB8`, `+0xC8` = 0xFF000000, `+0xC0` = 0xFFFF0000 |
| `Location <route>` | `+0x14` = the route of that name (`0x004264E0`: route names only, 0 when none) |
| `Car <who> <car> <level>` | a car: `player` marks the player's, else the number; the car id (`0x0042A8F0`, 11 when unknown); the level |
| `Type <word>` | `Circuit` 4, `P2P` 3, `Sprint` 5; `ShowOff`, `Speed`, `PerfectLap`, `TimedLap` 0 with `+0x0C` 3, 2, 1, 0 |
| `ExtremeNitro`, `DevChallenge` | `+0x04`, `+0x08` = 1 |
| `NumLaps n`, `Challenge f`, `AILevel f` | `+0x18`; `+0x10`; `+0x24` = f x 0.01 |
| `Time <word>`, `Weather <word>` | `+0x20`: Morning 1, Afternoon 2, Evening 3, Night 4, else 0; `+0x1C`: Dry 1, Wet 2, else 0 |
| `CarColour r g b r g b f r g b f` | base `+0xA4`, metallic `+0xA8`, its amount `+0xB0`, pearl `+0xAC`, its amount `+0xB4` (each colour 0x80 alpha, blue in the low byte) |
| `CarDecal n r g b`, `CarWheel n r g b`, `CarNeon r g b` | `+0xC4` and its colour `+0xC8`; `+0xBC` and `+0xB8`; `+0xC0` |
| `[EndRace]` | no race |
| `[UnlockTracks] n` | the lists of championship n - 1 |
| `Track <route>`, `BonusTrack <route>` | the next entry of the championship's route list / bonus list |

## A new driver

After reading: every race of the championships read is locked and not completed; the first four races of
championship 0 are open; the 53 car flags and the 109 route flags are set; then (`0x0042B8A0`) the routes of those
four races and the non-zero routes of championship 0's list are opened, and (`0x0042B960`) for each of the four
races the player's car (the race's first car when none is marked) is opened; opening car 48 also opens car 22.

`ArcadeUnlock.sav` (`03-saves.md`) keeps the races' locked and completed flags, the car and route flags, `+0x5138`
and `+0x5134`.

## Unlocks

`0x0042B8A0` (championships under 17) opens the routes of a championship's first four races and of its list;
`0x0042B960` the player's car (`0x0042AFB0`: the first entry marked the player's, else the first) of each of them,
`0x0042B920` its bonus routes. Opening a car clears its flag, keeps it in `+0x5130` and sets `+0x512C`; car 48 opens car
22 too. `0x0042B5F0` sets `+0x5138` and opens the first four races of each developers' championship (`+0x08` of its
first race), `0x0042B650` the same for the extreme nitrous ones (`+0x5134`, `+0x04`); the cheat `0x0042B6B0` opens every
car, route and race, then both. `0x0042B9F0`: every race completed but the developers' and the extreme nitrous.

## A race won (`0x0042B350`)

`+0x53C4` set (a result waits: the arcade's windows ask for the autosave); winning the fifth race not yet completed sets
"series complete" and opens the bonus routes; the race completed. With the fifth race locked and the first four
completed it opens ("final race unlocked"), with its player's car and its route. With a next championship whose first
race is locked and is neither the developers' nor the extreme nitrous, three races completed set "next series
unlocked" and open its routes, cars and first four races. The first car of the race (the player's: the files list it
first) opens its model. The arcade finished by this race sets "arcade complete" and opens the developers'
championships.

`0x0042B290`, for an arcade race (mode 0) off the network: the cheat "win all races" (the career's `+0x64` + 4) wins
always; else a race or point-to-point (types 5, 7) whose first car finished first, a sprint (6) by its results' place
1, a showoff (4) by its points over the target (`0x00420390` against `0x0042B240`); other types not here. It runs at
each finish of a car (`0x0049FF30`, with 100) and a quit from the pause; a challenge's results window
(`hud_challenge_results`, `0x00450C28`) turns a challenge outcome of 0 into 1 and wins the race.

## The windows

`Arcade_Championship` (`0x0042DA40`, `0x0042DD00`): the championships as the menu's items (a developers' one with
`+0x5138`, an extreme nitrous one with `+0x5134`, another disabled while locked), the one kept highlighted; the picture
`UI_ARCADE_IMAGE<n>`; Locked and LockedBox for a locked championship, Completed "<n> / 5 Completed" or, the fifth race
won, "Completed" in green. SELECT on an open one goes to `Arcade_Race`, BACK to `menu`.

`Arcade_Race` (`0x0042C5B0`, `0x0042CB90`, `0x0042CB00`): the championship's five races ("<n> " and the kind; a tick for
a completed race, the cross until the fifth opens; locked ones disabled; the first open one not completed
highlighted), the city map at its first place with each race's route model, the car view of the race's car (all made
by `0x0042AD40`), the race's details (`0x0042CE80`: laps, time, weather, opponents, place, route, the car and its
rating, the championship, the challenge's target; Car and Challenge over several lines, their `+0x128` 1). Each frame with the view idle the map goes to the place named as
the route's location (the nine names at `0x00767DC0` against the location's text) with the route's model shown and
coming down, and the car shows turned 2.64. SELECT on an open race (a showoff first asks "VIEW SHOWOFF TUTORIAL?" once)
starts it (`0x0042D7D0`: the race's type by its kind, its laps, track, route, time and weather, one player,
`loading`); BACK `Arcade_Championship`. After a race the front end opens `Arcade_Results` when any news is set, else
`Arcade_Race`.

`Arcade_Results` (`0x0042E150`, `0x0042E330`): Title2 and Comment by the news (Comment over several lines, its `+0x128`
1; emptied for the developers' and the extreme nitrous); SELECT or BACK to `Arcade_Championship` after a series completed, else `Arcade_Race`.

The race (`0x00495DE0` for mode 0): the session of the race's entries (`0x004B93C0`: crew 0 for the player's, else the
entry's number), the challenge's outcome 0, the cars made (`0x0042B000`), each on its grid place, named by the first
member of its crew; the AI drivers take the race's skill (`+0x24`) and nitro (`+0x04`).
