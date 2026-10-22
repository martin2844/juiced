# The career

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## The career manager

Made once at start (`0x004A17C0`), kept at `0x00728C40` and in the game object (`+0x80`). Its objects are allocated
by the game's allocator, which does not clear memory: a field that neither the constructor nor the new career sets
keeps what the memory held (pieces of files read earlier are seen in saves), and a new career started after another
was loaded keeps that career's values there.

| offset | size | object | constructor |
|---|---|---|---|
| +0x00 | 0x198 | flyers (crew logos) | |
| +0x04 | 0x638 | the profile | `+0x510` = 0, `+0x08` = 0 |
| +0x08 .. +0x28 | 9 x 0x348 | crews 0 .. 8 (crew 0 is the player's) | `+0x00` = 0 |
| +0x2C | 0xF94 | the calendar | |
| +0x30 | 0x37C | | `0x004B8450` |
| +0x34 | 0x34 | the race's news: the place, the lead's step, the crews' rams, the bet's state (`0x004BD130`); `+0x30` the race's respect | |
| +0x38, +0x3C | 0x324 each | | `+0x320` = 0 |
| +0x44 .. +0x50 | 4 x 0xC4 | crewmates 0 .. 3 | |
| +0x58 | 0x90 | | |
| +0x5C | 0xFA8 | phone: messages and contacts | `0x004B5F10` |
| +0x60 | 0x460 | | `0x004AA5F0` |
| +0x64 | 8 | | 0, 0 |
| +0x68 | 0x24 | | a list of 3, `+0x18` = -1 |
| +0x6C | 0x84 | | |
| +0x70 | 0x4FB4 | the dealer | `0x004B4280` |
| +0x74 | 0x68 | | |
| +0x78 | 0x74 | | |
| +0x7C | 0x70C | money and respect history | |
| +0x80 | list | the cars of the opponents | `0x00489B80` |
| +0x84 | list | the player's garage | `0x00489B80` |

A car list (`0x00489B80`, 0x14 bytes): begin, end, capacity, and `+0x10` = 0 (saved after the garage).

## When a career is made

The game object's start (`0x0040BAC0`) reads, after the device: `cardefs.txt` (`0x0048C650`), the parts catalogue
(`0x00498BC0`), the save system (`0x0040A510`: `arcadecollection.txt`), the career manager (`0x004A17C0`) and a new
career (`0x004A2300`) at once, the arcade lists (`0x0042A640`, twice) and races (`0x0042A960`), the help pages
(`0x004850C0`), a second save system object with its start driver (`0x0040AB80`: an id from four `rand()` values).
Then the race to start at first (`[game + 0x6C]`, `0x0040D0C0`): a custom race (mode 1) of type 5 on Angel
SuperSpeedway's `Speedway_r1`, heat 0, 99999 laps, automatic (`+0x54` 1), one player, no opponents, `+0x64` 1 (never
read), not a replay's (`+0x68` 0), dry (`+0x70` 1), at night (`+0x6C` 4); the custom race's group (`[game + 0x84]+0x784`)
6. The music follows (`0x0040D200`).
`saveprofile` makes a new career again over the same objects for each new driver (`[[game + 0x7C]]`, the head
value of `CMSGlobal.sav`, cleared first). All of it runs on the main thread: the draws follow one another from the
seed of the start (`01-startup.md`).

## A new career (`0x004A2300`)

In this order (all `rand()` draws are the C runtime's generator of the main thread, `06-cars.md`):

1. Phone (`0x004B5FD0`): `+0x00` = 0, `+0x04` = `rand() x 0.00030518509 + 5.0` (5 .. 15); with `[game + 0x4C]` set,
   `0x0040B600`; the ten message slots' flags (`+0x28 + k x 0xBC`) = 0; `+0x18`, `+0xF8C` (contacts), `+0xF9C`,
   `+0xFA0`, `+0xFA4` = 0.
2. Calendar (`0x004A6340`): `+0x00 .. +0x0C` = 0, the used flag (`+0x10 + k x 0x84`) of the 30 events = 0.
3. Crew `k` gets the number `k` (`+0x78`, 16-bit).
4. The opponents' cars (`0x004A4240`): the list is emptied; for 5 groups and for the CarDefs 1 .. 52 in their order
   in `cardefs.txt` (the first is left out) a car (`0x0048DB40`) of that CarDef: group 0 gets the nitro part 1, group 1
   `autoModify(level 0)`, group 2 level 1, group 3 level 2, group 4 the prototype parts; each is rated.
5. The garage: emptied, `CMSPlayersCrewCollection.txt` read into it (`06-cars.md`), the first car modified
   automatically (level -1), every car rated and given its grid (zeros).
6. The crews' colours (`+0x1A4 .. +0x1B3`) = 0x80FFFFFF.
7. Each crew (`0x004A7240`) is cleared (below) and reads `CMSCrews.txt`.
8. Crewmate `k` gets the number `k` (`+0x00`) and (`0x004A9CE0`) `+0xAC`, `+0xB0`, `+0xB4`, `+0xBC` = 0, `+0xC0` = 100,
   then reads `CMSCrewmates.txt`.
9. `CMSCrewChallenges.txt` (`0x004A7650`).
10. The profile (`0x004B7660`, below).
11. The flyers (`0x004B2C20`, the object at the career's `+0x00`): `CMSFlyers.txt`: `Crew` takes the next crew
    (from 0), `FontColour r g b` its colour (`+0x54` + 0x24 x crew, 0xFFRRGGBB), `Logo` its logo (`+0x58` + 0x24 x
    crew), `PlayerFontColour r g b` the player's next colour (`+0x00` + 4 n); then the logo picture name of the
    chosen logo (`ui_chooselogo_NN`, the table `0x00718A28`, the settings' logo) at `+0x58`. The crews' logos are
    the pictures of the career windows (`CMSRaceResults`, `CMSRespectMsg`, `CMSMeetingSummary`).
12. `career+0x6C`: `+0x00`, `+0x04`, `+0x0C`, `+0x18`, `+0x5C .. +0x80` = 0.
13. The dealer (`0x004B4BE0`, below).
14. Crewmate 0 joins the player's crew (`0x004B80D0`, below).
15. Unless `[[game + 0x7C]]` is set, the first event (`0x004A32B0`).
16. The calendar is filled (`0x004A25D0`).
17. `career+0x78`: `+0x00 .. +0x10`, `+0x20 .. +0x54`, `+0x58` (16-bit), `+0x60` = 0; `+0x18`, `+0x1C` = 13.
18. `career+0x7C`: its 0x70C bytes = 0.
19. The history (`0x004BD8A0`) takes its first values.
20. The calendar's `+0xF88` = 1.

### A crew (`0x004A7240`)

The four member names (`+0x7A + i x 25`, `+0xDE + i x 25`) get an empty first character; `+0x144 .. +0x1A3` = 0;
`+0x00` = the crew's number; `+0x04 .. +0x50`, `+0x54 .. +0x60`, `+0x64`, `+0x68`, `+0x70` = 0; `+0x6C` = 1000.0;
`+0x1C4 .. +0x1E0`, `+0x224`, `+0x228`, `+0x22C`, `+0x230`, `+0x23C`, `+0x248`, `+0x24C`, `+0x254`, `+0x258`,
`+0x260 .. +0x284`, `+0x2B8`, `+0x2BC`, `+0x2C0 .. +0x2D0`, `+0x2DC`, `+0x2E0`, `+0x2E4`, `+0x2F0`, `+0x2F8`, `+0x2FC`,
`+0x300`, `+0x30C`, `+0x314`, `+0x318`, `+0x31C`, `+0x328` = 0; `+0x338` = -1; `+0x334` (the money of the crew, a float) by
its number: 1: 2500, 2: 5000, 3: 7500, 4: 15000, 5: 40000, 6 and 8: 100000, 7: 250000, else 0. After the file:
`+0x2D4`, `+0x2D8` = 0.

### `CMSCrews.txt` (map `0x00768A04`)

`[BeginCrew] <n>` makes `n` the crew the lines are for (`0x007187A4`), `[EndCrew]` none (-1); a line applies only
while it is the reading crew's number. Counters of the reader (one reading per crew) number the repeated keys.

| keyword | effect |
|---|---|
| `CarColours r g b  r g b f  r g b f` | base `+0x1A6/+0x1A5/+0x1A4` (alpha `+0x1A7` = 0x80), metallic `+0x1AA/+0x1A9/+0x1A8` (`+0x1AB` = 0x80), amount `+0x1B0`, pearl `+0x1AE/+0x1AD/+0x1AC` (`+0x1AF` = 0x80), amount `+0x1B4` (atol to bytes, atof) |
| `WheelColor n`, `Wheels n`, `NeonColour n` | `+0x1B8`, `+0x1BC`, `+0x1C0` |
| `EntranceFee n` | `+0x288` |
| `PrizeMoney a b c` | `+0x28C`, `+0x290`, `+0x294` |
| `DriverName "<name>" a b` | name `+0x7A + i x 25`, `+0x144 + i x 0x18` = a, `+0x14C + i x 0x18` = b |
| `ShortDriverName "<name>"` | `+0xDE + i x 25` |
| `WillAttendAt <word>` | `+0x264 + 4 x index(word)` = 1 (`0x004A92E0`) |
| `RespectFactor f` | `+0x04 + 4 i` = f x 0.01 |
| `Circuit <track> <route> km difficulty` | `+0x1C4 + 4 i` = the track (`0x00426490`), `+0x1E4 + 4 i` = km, `+0x204 + 4 i` = difficulty; `+0x228` + 1 |
| `P2P`, `Cruise`, `Illegal`, `Sprint <track> <route> km` | `+0x22C`/`+0x234` and count `+0x23C`; the others likewise |
| `ProbMix`, `ProbCircuit`, `ProbSprint`, `ProbP2P`, `ProbCruising`, `ProbTerritory`, `ProbMake`, `ProbModel f` | `+0x298`, `+0x29C`, `+0x2A0`, `+0x2A4`, `+0x2A8`, `+0x2AC`, `+0x2B0`, `+0x2B4` = f x 0.01 |
| `CarPref <word>` | `+0x2B8` (`0x004A9CA0`), `+0x2BC` (`0x004A9C60`) |

### `CMSCrewmates.txt` (map `0x00768A1C`)

`[BeginCrewmate] <n>` (`0x0075EF60`) selects crewmate `n`, `[EndCrewmate]` none. `Name` `+0x08` (64 bytes),
`ShortName` `+0x48`, `JoinThreshold` `+0x88`, `JoinCrewIdx` `+0x8C`, `DrivingAbility` `+0x94`, `Recover` `+0x9C`,
`LearnRate` `+0xA4`, `ForgetRate` `+0xA8` (floats); `DrivingLimit` and `RecoverLimit` set `+0x98` / `+0xA0` to 100.0
whatever the value. `PhoneNumber` sets `+0x90` (atol) without the check of the number, so every crewmate takes the
last one of the file.

### `CMSCrewChallenges.txt` (map `0x007689EC`)

`[BeginCrew] <n>` selects crew `n` and restarts the count; `Challenge <type> a <car> b c` fills challenge `i`
(`+0x2DC + i x 0x1C`): `+0x14` = 0, `+0x00` = the type (`LapTime` 0, ... the table `0x007187A8`, 4 for an unknown
one), `+0x04` = a, `+0x08` = the car (`0x0042A8F0`), `+0x0C` = b (the winnings), `+0x10` = c (the slot of the crew's
circuit routes raced); `+0x18` takes the player's best lap of the challenge (-1.0 for another result).

A crew's counters: `+0x224` the player's wins at the crew's events; `+0x2C0` the player agreed to host the crew's
challenge, `+0x2C4` won it, `+0x2C8` lost it; `+0x2CC` the player's races since the crew's last call; `+0x2D0` the
crew's cars the player won in pink slip races; `+0x2D4` and `+0x2D8` (the respect meter blinks) are only cleared;
`+0x338` the last bet race against the player: -1 none, 0 the player won, 1 the crew won. A crewmate's `+0xB0` races,
`+0xB4` wins, `+0xBC` days in the crew since its last race, `+0xC0` days of wanting to join.

### The profile (`0x004B7660`)

`+0x0C` (money) = 40000; `+0x10`, `+0x14`, `+0x500 .. +0x510` = 0; `+0x4E0` = 1; `+0x620` = 7; `+0x624` = -1;
`+0x18 .. +0x25`, `+0x26 .. +0x37`, `+0x38 .. +0x49`, `+0x4A`, `+0x4C` = 0; then the crews' start (`0x004B7F40`); per
car model `c` (0 .. 52): the unlock step `+0x4A9 + c` = 0, its parts tables `+0x50 + 10 c` and `+0x262 + 10 c` = 0xFF
x 10, its level `+0x474 + c` = 0xFF, then 9 unlock steps (`0x004B8230`); `+0x4E8 .. +0x4FC`, `+0x08`, `+0x4E` = 0;
`+0x628` = -0.4.

The fields: `+0x00` the introduction (the first event and its car, set by `0x004A32B0`, cleared when the dealership
opens), `+0x04` its repair bill paid, `+0x08` the career complete message shown; 16-bit counters: the races won by
the round's type (`+0x18`) and by the event's location (`+0x26`), the crews the player's crew finished ahead of
(`+0x38`), the races placed but not won (`+0x4C`), the races placed (`+0x4E`); `+0x618` the meeting's race entered,
`+0x61C` watched; `+0x620` the type of the race the player hosted last, `+0x624` compared with an event's location;
`+0x628` the opponents' skill after the introduction (+0.04 a win up to 0; the introduction itself -0.2); `+0x4E0` 0
after a pink slip race lost (the results then go to `CMSMainMenu` once); `+0x4E4` the money when the garage or the
dealership was entered, leaving with other money autosaves (-1 before the test track forces it); `+0x630` out of the
race (quit, or the player's crew not among the first five), `+0x634` the showoff tutorial shown. The money counters:
`+0x4E8` spent on the cars (with each car's `+0xA8`, `0x0048F270`), `+0x4EC` the winnings of races and bets, `+0x4F0`
the garage's sale worth when `CMSCarCollection` shows it and `+0x4F4` the greatest of those (the respect by the
garage's value writes its own sum of capped values there too), `+0x4F8` only cleared, `+0x4FC` the best car's value
(the respect by the best car); the advice compares `+0x4F4` with `+0x4F0` and `+0x4FC` with `+0x4F8`.

An unlock step (`0x004B8230`, a count of changes): the table at `0x00718B50` holds records {index, value, kind}
ending with value -1, read from the car's step on; each record moves the step on; kind 0 raises the byte `index` of
the car's table at `+0x50` to the value, kind 1 that of the table at `+0x262` (for index 0 also its bytes 1 and 4,
which do not count), kind 2 the car's level; a record that raises a value counts. The values are signed bytes.

### The dealer (`0x004B4BE0`)

The count `+0x4FAC` = 0; for each car model 1 .. 52 whose value in the table at `0x00718A78` times 3 is at most the
profile's `+0x4E` (16-bit): when the value is above `+0x27E0`, `+0x27E0` takes it (and a message follows); a car is
offered (`0x004B4DD0`), and twice more each on `rand() & 1`. The offers are sorted by rating, largest first, and
numbered (`0x004B4D30`). When `+0x27E0` rose: a phone message from "Mr. Jeffs Motors" with one of four texts
(`rand() & 3`; "The phone").

An offer (0x40 bytes at `+0x27EC + 0x40 i`, `0x004B4DD0`):

| offset | value |
|---|---|
| +0x04 | the car model |
| +0x08 | used: the CarDef's `STATE`; a new one becomes used when `rand() % 100 > 50` |
| +0x1C | price: the CarDef's value + `(rand() % 5) x 100` |
| +0x14 | mileage: new `rand() % 15`, used `rand() % 50000 + 5000`; a used car of a new model costs `trunc(trunc(price x 0.8) - mileage x 0.1)` |
| +0x0C | 0, 1 above 20000 miles |
| +0x10 | the model's class (the jump table of `0x0048CE20`: 0, 1 or 2) |
| +0x18 | the rating of a car of the model (`0x0048E8A0`), cut to an integer |
| +0x20, +0x24, +0x28 | 0; a used car: when `rand() % 100 > 60`, `+0x0C` = 2, `+0x24` = `rand() % 3 + 1`, price = `trunc(price x 0.9)` |
| +0x2C .. +0x3C | a random paint of `cardefs.txt` (`rand() %` the paints), its metallic and pearl colour = the base, the amounts 0 |

`+0x20` automatically modified (listed " (M)"; only cleared), `+0x24` the damage (" (D)"; a damaged garage car for
sale has 1), `+0x28` with prototype parts (" (P)"; a heavily tuned garage car for sale).

The dealer's constructor (`0x004B4280`) gives the paint of every offer of both lists (`+0x14 + 0x40 i` and `+0x27EC +
0x40 i`, 159 each) base -1, metallic 0xFF000000, pearl -1, amounts 0.5 and 0.1; the other bytes of offers past the
count are what the memory held.

### Crewmate 0 joins (`0x004B80D0`)

With fewer than 4 members (`profile +0x510`): the member slot `profile +0x500 + 4 n` = the crewmate; its `+0xAC` = 1,
`+0xB0`, `+0xB4`, `+0xBC` = 0, `+0x04` = n, `+0xB8` = the calendar's `+0x04`; for crewmate 0 the crew's member names
`+0x7A + 25 n`, `+0xDE + 25 n` and the crewmate's `+0x48`, `+0x08` become the driver's name; the count goes up.

### The history (`0x004BD8A0`)

At the position `+0x708` (0 .. 49, wrapping): `+0x000 + 4 p` = the profile's money, `+0x0C8 + 4 p` .. `+0x640 + 4 p`
= the `+0x70` of crews 1 .. 8 (as floats).

## The calendar

The calendar (`career +0x2C`, 0xF94 bytes): today's day of the month `+0x00` and month `+0x04`, the days played
`+0x08`, `+0x0C` the month again (written with `+0x04`), 30 events of 0x84 bytes at `+0x10`, `+0xF88` (set when a
career starts), the day of the month `+0xF8C` and the month `+0xF90` chosen in the calendar window. An event:

| offset | meaning |
|---|---|
| +0x00 | used |
| +0x01 | raced |
| +0x02 | the player hosts it (the phone's "Host challenge?" answered yes) |
| +0x03 | a pink slip race |
| +0x04 | extreme: asks for a car with prototype parts; prizes and entrance fee x 3 |
| +0x05 | a crew's challenge |
| +0x06 | a challenge of the crew (-1: none) |
| +0x07 | the day of the month |
| +0x08 | the month (counted from the start; the months have 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 days) |
| +0x0C | the crew that holds it |
| +0x0D | the location: the crew whose turf (its first circuit route's location; `WillAttendAt` by it) |
| +0x10 | the type: 0 challenge, 2 cruise, 3 point to point, 4 circuit, 5 sprint |
| +0x14 | entrance fee |
| +0x18 | the prizes of its rounds together |
| +0x1C | the round (0x38 bytes, below) |
| +0x54 | the number of rounds |
| +0x55 | the round raced |
| +0x56 .. +0x5E | per crew: it comes |
| +0x60, +0x64, +0x68 | the cars it asks for: region, make, model (0: any) |
| +0x6C | the car class (`ratingClass`, 8: any) |
| +0x70 | it asks for a car with prototype parts (+0x74 the part kind, +0x78 the supplier) |
| +0x7C | the time of day |
| +0x80 | the weather |

A round: `+0x00` = 1, `+0x01` set when its results are left, the event quit or a pink slip lost (`0x004A3A60`), `+0x02`
the event's prototype part goes to the winner, `+0x04` the type, `+0x08` team of more than one, `+0x0C` the most crews,
`+0x0D .. +0x15` per crew: it races, `+0x16` the number of crews, `+0x17` the team size, `+0x18` the drivers, `+0x1C`
the route (an index of the routes table), `+0x20` the crew's route slot, `+0x24` laps (0x7F for a cruise), `+0x28` km,
`+0x2C .. +0x34` the three prizes.

The class of a rating (`0x0048E9E0` for a car's `+0x88`, `0x00447480`): the rating cut toward zero, 800
and more 0, 700 .. 799 1, 600 2, 500 3, 400 4, 300 5, 200 6, less 7.

### The first event (`0x004A32B0`)

The profile's `+0x04` = 0 and `+0x00` = 1; `career +0x58` is cleared. A circuit race (type 4) of crew 1 on day 0 of
month 0 that crews 0 and 1 attend, without fee, its class that of the best rating in the garage; the round is set up
(below), then has 2 laps on route 0x31, prizes 5000, 0, 0; `+0x7C` = 4, `+0x80` = 1; the total prize is 5000 plus
what the set up added.

### Filling the calendar (`0x004A25D0`)

While fewer than 20 events wait (used and not raced): after the latest event (the largest month, then day) by `1 +
(rand() & 3)` days, an event is made (`0x004A27B0`, with its flag 0).

### Making an event (`0x004A27B0`)

None when an event is on that day. Otherwise (fractions are `rand() x 1/32767`):

1. The crew `1 + (rand() & 7)` holds it.
2. One time in eight, with cars in the garage and the profile's `+0x4E` at least 6, it may ask for a car with
   prototype parts (`0x004A2C70`; it draws `rand()` once in any case when the garage has cars).
3. Not asking for such a car, with a car of the crew's that has more than three prototype parts: extreme when a
   fraction is above 0.8.
4. Type 4; 3 when a fraction is below the crew's `ProbP2P`; 2 when another is below `ProbCruising` (not extreme, not
   asking for a car); 5 when a third is below `ProbSprint`. A cruise (when not made a sprint) has class 8; otherwise
   the class is chosen (`0x004A9920`) from the player's cars when `rand() & 3` is not 0 and the crew is 1 .. 3, else
   from the crew's: no cars: `rand() % 7`; when the profile's `+0x00` is set: 7 if a fraction is above 0.25, else 6;
   otherwise a random class among the cars', moved one down when `rand() % 5` is 1, until within the crew's limit
   (the table at `0x00718780`).
5. Each crew comes (`0x004A93C0`): not crew 0; crew 3 to a cruise; else it needs cars of the region, make, model and
   class asked for (and an extreme event a car with prototype parts) and the event's crew in its `WillAttendAt`, and
   (when no car is asked for) a fraction above 0.25. With fewer than 3 crews coming there is no event; the event's
   crew comes.
6. The fee: the crew's `EntranceFee`, x 2 when asking for a car, x 3 when extreme, 0 for a cruise. Not a cruise, not
   extreme, not asking for a car: when a fraction is below `ProbTerritory` it asks for the region of one of the
   crew's cars (and that car's class), else when another is below `ProbMake` for the make; a class of -1 becomes 7.
7. The round is set up (`0x004A33E0`, below); asking for a car marks the round (`+0x02`); the time of day and the
   weather are set (`0x004A4410`).
8. With an empty garage the event is kept. Otherwise not a third waiting cruise; when the player cannot enter it
   (`0x004B7D80`) it is kept only with the profile's `+0x4E` at least 6, the flag 0 and no other such event waiting;
   when the profile's `+0x00` is 0 an event the player lacks cars, members or the crew's step for is dropped unless
   (flag 0, `+0x4E` at least 4) fewer than 2 such events wait.

The player can enter an event (`0x004B7D80`) unless it is raced, or it is a cruise and the garage is empty, or its
crew has not reached its first step (`+0x54`) and it is not a cruise of crews 1 .. 4.

### A round (`0x004A33E0`)

`+0x00` = 1, the type copied; by type:

- circuit: team size `rand() % 3 + 1` (1 when the profile's `+0x00` is set), up to 6 crews, a random `Circuit` of
  the crew (`rand() %` the count); laps = the nearest whole number of (16 - class) / km (extreme: 15 or 10 / km by
  `rand() & 1`), at least 2; km = laps x the route's km.
- point to point: team size `rand() % 3 + 1`, up to 6 crews, a random `P2P`, 1 lap.
- sprint: team size 1, up to 4 crews, the `Sprint`, 1 lap.
- cruise: team size 1, 1 crew, the `Cruise`, laps 0x7F, km 1.0.

Then: the team size at most 3; above the player's members, the members when a fraction is below 0.5; 1 when a
fraction is below 0.25. The crews racing: up to 1000 draws of `rand() % 9`, a coming crew not yet in joins while
fewer than 6 drivers and fewer crews than the most. The prizes by the number of crews: 2 or 3 crews the crew's first
`PrizeMoney`; 4 the first and the third; 5 or 6 all three; each x the factor of the class (the table at
`0x0071875C`), cut to a whole number, x 2 when asking for a car, x 3 when extreme; the event's total takes them.

### Time of day and weather (`0x004A4410`)

The time of day `+0x7C` (1 morning, 2 noon, 3 evening, 4 night) and the weather `+0x80` (2 wet). A pink slip
race: `+0x7C` = 4 or 3 by `rand() & 1`, `+0x80` = 2 when `rand() % 5` is 2, else 1. Circuit, point to point and sprint
events: `+0x80` = 1 or 2 by `rand() & 1` and `+0x7C` = 1 .. 4 by `rand() & 3`; the others `+0x80` = 1 and `+0x7C` =
`rand() % 3 + 1`.

### Days

The calendar's `+0x00` is today's day of the month, `+0x04` its month, `+0x08` the days passed. The event of a day
(`0x004A64A0`) is the first used event of that day and month. Days are added (`0x004A6650`) a month at a time by the
month's days (the table `0x00718EDC` by the month % 12).

The next day (`0x004A6530`): `+0x08` one up; today's event raced (`+0x01` set, `+0x00` cleared); the date a day on
(`+0x0C` the month); in a new month the dealer's offers are made again (`0x004B4BE0`); the crewmates' day
(`0x004AA4C0`); the calendar filled (`0x004A25D0`).

The crewmates' day (`0x004AA4C0`): a crewmate who joined counts a day (`+0xBC`); one who did not, with fewer than four
members and the respect of the crew it comes from (`+0x8C`) over its threshold (`+0x88`), counts a day of wanting
(`+0xC0`); past 30 + 5 x its number it calls (`0x004B63B0`, a phone message of kind 1) and every crewmate's `+0xC0`
is 0.

A special event is one with `+0x05`, `+0x03` or `+0x02` that waits (used, not raced): `0x004A6DD0` tells whether there
is one, `0x004A6800` whether there is one before a date. The first free day (`0x004A6A10`) is the first day after
today without a used event.

The open events (`0x004A6B30`) are the waiting events, not cruises, the player may enter (`0x004B7D80`) with a reason
other than 0, 1 and 7. Topping up (`0x004A2670`): with cars and fewer than six open events, an event is made
(`0x004A27B0`, its flag 1) on today + 2 + `rand() % 20` days, a day with an event tried again (after 100 tries any
day), until six are open or more than 101 were made.

The career's completion (`0x004BCA70`): the respect of crews 1 .. 8 (each at most 1000) and 40 for each challenge
done, x 0.00011160714 (1 / 8960), within 0 .. 1; 1 when every crew has 1000 and every challenge is done.

What a crew respects most (`0x004BC110`): the first of its factors 0, 1, 2, 4 .. 8 with the greatest value above 0
(0 when none), then factor 9 when it is greater.

## The race of an event

The career's `+0x30` (0x37C bytes, made by `0x004B8450`) holds the race of the current event; the bets on the rival
crew are the career's `+0x6C` (0x84 bytes), the AI drivers its `+0x74` (0x68 bytes). The current event
(`0x004A64E0`) is the used event of the calendar's slot `+0x00` and day `+0x04`; its round is the one of its round
index `+0x55` (`+0x1C + index x 0x38`).

The session's constructor works out the 33 fractions of the bookmaker (`0x00718D40`: against, stake) as floats and
sorts them by value, smallest first (neighbours swap when the first is larger and the pass starts again).

### The value of a car (`0x0048CFE0`)

The CarDef's VALUE, plus (level + 3) x 500 for each body part chosen (levels 1 .. 126), plus the COST of each
catalogue part fitted; at least 199; rounded to a whole number.

### The bets (`0x004B1870`)

The rival's money (`+0x1C`) is the crew's money (`+0x334`), 2000 in the first event (the profile's `+0x00`). Seven
amounts from the money less 3 x 0.2 of it up in steps of 0.2 of it, each cut to a whole number and then to hundreds,
with the weights `0x007189F8` (0.3, 0.6, 0.9, 1.0, 0.9, 0.6, 0.3); `+0x58` counts them. After a bet too low the
three highest are left (`0x004B1A00`).

Paying (`0x004B1A40` and the same lines in the betting window): a bet sets `+0x00` = 1 and `+0x0C` = the amount; an
amount of 0 or more adds to the career's `+0x58` object's `+0x00`, a negative one its opposite to `+0x04`; the
profile's money loses the amount, not below 0.

### The racers (`0x004B8570`)

With the session's `+0x264` set and an event of the current day: the event and its round (`+0x04`, `+0x08`), the
location of the event crew's first circuit route (`+0x378`), the racers `+0x00` = the round's opponents x its
`+0x17` (at most 6), each racer's crew, member and car -1. For each crew of the round (in crew order, while racers are
left) its race cars `+0x33C` are -1 and its `+0x17` racers are made: crew 0 takes the profile's member slots
(`+0x5F4`) and garage cars (`+0x600`), the member slot 0 being the player (`+0x30` = 1); another crew picks a car of
the opponents' list (`0x004A9540`; in the first event then the first car of model 50 instead), the leader for its
first racer and a member no racer has yet for the others (`rand() & 3` until free, `0x004A98C0`). Each racer keeps
the member's short name. The racers are sorted by the car's value + 1000 x the crew number, largest first
(neighbours swap when the second is larger and the pass starts again), and numbered in that order.

Then `+0x364` = 0, the 9 odds (`+0x268`, 0x14 bytes) and `+0x31C` cleared, `+0x340 .. +0x354` = -1, `+0x368`,
`+0x36C` = -1, the odds and the drivers (below). Last, also without a setup: `+0x370` = the bets' `+0x00` and, when
set, `+0x374` = the racer of the crew bet with (its leader, `0x004B1780`); `+0x264` = 0.

In the race: `+0x31C` counts per crew its racers finished; a crew whose count reaches its team size (1 without a
team race) takes the next place of the order `+0x340` (their count `+0x364`); `+0x368` and `+0x36C` keep the racer
and the crew that finished first. An odds record: `+0x00` the crew's race time, `+0x04` against, `+0x08` the stake,
`+0x0C` the player's money on the crew, `+0x10` what it won. The bets: `+0x04` the player won the bet race, `+0x5C`
per crew: the betting window talked with it, `+0x80` a rival crew came for a money bet. The money counters'
(`career +0x58`) `+0x08` and `+0x0C` the money put on odds and won by them, `+0x10` the races entered.

A car of the opponents' list for a crew (`0x004A9540`): the least rating is the bottom of the event's class + 50
(class 1 700, 2 600, 3 500, 4 400, 5 300, 6 200, 7 and 8 0, else 800; `0x0047FCA0`), or, when the player's cars of
the race are chosen (`+0x614`), the best rating of those cars less 50 with its drive. Three passes of up to 1000
tries: a try takes `rand() % the list's size` (from the 701st try the try's number instead); the third pass first
asks for the crew's preferred region and make; all passes ask for the event's region, make and model (when set), the
event's class (8 any), at least the least rating, for an extreme event (`+0x04`) a car with more than three prototype
parts, otherwise in the first two passes not such a car, and for type 5 not model 40 and the same drive. In the last
pass the least rating falls by 0.1 a try after the 200th. The car's index goes into the crew's first free race car
slot. None found: 0.

### The odds (`0x004B9920`)

Only with 2 racers or more and neither the player's cars chosen (`+0x614`) nor `+0x618`. A crew leader's odds
against: by the round's laps (below 2: 6, 4: 10, 6: 12, 8: 15, 11: 20, 26: 30, else none) less a share by its place
(10, 9, 4, 3, 0.1, 0.05, 0 for places 0 .. 6), over a stake of 1; then the first fraction of the table at least as
long. While the leaders' chances (stake / (against + stake)) add up to less than 1.05 (at most 1000 times) the next
leader in turn takes the fraction before its own.

### The drivers (`0x004BDCC0`)

The career's `+0x74`: `+0x18`, `+0x1C`, `+0x24`, `+0x2C`, `+0x64` = 0, `+0x00` = the session. Each racer: a member of
the player's crew (with members) takes its crewmate's driving ability x 0.01 (`+0x3C`), recover x 0.01 (`+0x40`),
1.5 / recover (`+0x44`, 0 for 0) and recover x 0.001 (`+0x48`); another racer the crew member's ability x 0.01,
concentration x 0.01, 1.4 / concentration and concentration x 0.0005. Then `+0x50 .. +0x5C` = 0, the side `+0x4C`
(1 the player's crew, 2 another) and `+0x60` = `rand() % 5 + 1`. `+0x60`, `+0x0C`, `+0x10` = 0; a racer of the
player's crew other than the player sets `+0x0C`, `+0x10` = 1 and `+0x14` = 0.

### Into the career (`0x00435434`)

DemoFlyby's way in: the music off, race mode 2, sound CLICK, the career head value cleared, a new career
(`0x004A2300`) for the driver, then the bets: crew 1 (`+0x08`); a money race (`+0x18` 0) sets `+0x80` = 1, the kind
`+0x10` = 0 and the first step `+0x14` = 3, a pink slip race kind 1 and step 13; the bets on crew 1 (`0x004B1870`);
`+0x60` (the `+0x5C` value of crew 1) = 1, `+0x0C` = 0. The profile: the first racer's member slot and garage car 0
(`+0x5F4`, `+0x600`), one car (`+0x60C`), the cars chosen (`+0x614`). The session needs a setup (`+0x264`) and is set
up (`0x004B8570`). Then `CareerLoad` (mode 1) and the career manager's `+0xA4` = 2 (the betting window).

### The start of a race (`0x004A3CC0`)

The crews' `+0x330` = 0; the career's `+0x34` cleared with `+0x00` = -1; the car of the car statistics (`0x00728C60`)
none; with the profile's `+0x618` the career's `+0x58` object's `+0x10` + 1. The race (the game object's `+0x6C`):
for an event without a challenge (`+0x06` -1) the type by the round's type (2 → 4, 3 → 7, 4 → 5, 5 → 6 with `+0x08`
= 0 and `+0x0C` = 3, or 1 with `[0x0075EF8C]` set); for a challenge of the event's crew that challenge's `+0x18` =
-1.0 and by its type: LapTime and PerfectLap the type 0 / 1 with `+0x74` = the challenge's value and `+0x7C` = 1,
Speed type 2 with `+0x78` = the value and `+0x7C` = 1, Cruise type 4. `+0x50` = the round's laps, `+0x10` / `+0x30`
the track and route names of the round's route (31 characters each), `+0x58` = 1. While the round's `+0x18` is above
4 and the event's `+0x7C` is 0 or 4 it is drawn again (`rand() % 5`); with `+0x18` above 2, `+0x7C` 4 and `+0x80` 2,
or `+0x18` above 4 and `+0x80` 2, `+0x80` = 1; the race's `+0x6C` (time of day: 1 morning, 2 noon, 3 evening, 4
night) and `+0x70` (weather: 2 wet) take them. `+0x54` = the save system's
`+0x84` (0 for type 6). Sound CLICK, `loading` (mode 1); a round of type 2 also calls `0x004AA730`; the career's
`+0x60` object's `+0x10` = 1.

The bet share (`0x004BC2E0`): the profile's `+0x62C` = 0; the sum of the nine odds' `+0x0C` (whole numbers) and, with
a bet made, its amount (cut); when above 0 the share is the sum / the profile's money x 100.

## The race's respect (`0x004BCD90`)

`0x004BCD90(object, last, collection)` (the career's `+0x34` object; the HUD and the challenge's restart ask with 0, 0,
the pause with 1, 0, the sprint's end with 0, 1) sets the object's `+0x30` to 0; outside a career race, in one of type
9, or with neither the race cars chosen (profile `+0x614`) nor `+0x618` it gives 0. The calendar's event (`0x004A64E0`)
and its round: n = the round's crews (`+0x16`) less one; the player's crew's place: in a round of type 5 by the points
(`0x004A5670`: the racers sorted by their points `+0x138` with 6, 4, 2 for the places 1 .. 3, the most first, the sort
starting again after each swap; the first racer of crew 0), else `0x004A4F90` with crew 0 (the crews in the order their
last driver, the round's `+0x17` drivers each, is reached along the places; -1 when a racer has no car or a car has not
crossed the start line; when a place is missing the last place found, `[0x007255B4]`; the crews counted from the
session's event); -1 or `last` give n. An event with `+0x05` gives 0. For each rival crew 1 .. 8 (from the
career's `+0x0C`):

- an event with `+0x03`: 20 × (the event's crew `+0x0C` + 1) for the first place, -30 for the second, else 0, × the
  crew's factor 8 (`+0x24`), cut;
- else a round of type 2 (`0x004BBB00`, the money won `[career+0x60]+0x30` cut): below the crew's mark `+0x6C` the
  shortfall × factor 7 × -0.001; else the excess × factor 7 × 0.001 held to 30 × the event's crew + 20, doubled;
  type 5 (`0x004BB740`): by n and the place 80 / 50 / -10 / -5 / -20 (n 1: 80, -20 second; 2: 80, -20 third; 3: 80,
  50, -20 fourth; 4: 80, 50, -10, -20; 5: 80, 50, -5, -10, -20) plus the lead (`[0x0079E83C]`, taken while nobody has
  finished: trunc(-lead / 20) × 20), × factor 5; else (`0x004BB520`): the same tables with 80, 50 (60 for n 5), -8,
  -8 / -15, -20, plus the lead (`[0x0079E840]`: trunc(-lead / 100) × 20), halved when above 0 in an event of type 3,
  and for a crew other than number 1 none at a respect from 350, × 0.8 below 0; × factor 4. The lead (`0x004A5110`):
  with crew 0 first the rivals' farthest race distance less the crew's nearest, × 0.01, cut; with it last the rivals'
  nearest less its nearest × 0.01, at most 1000; else 0;
- without the profile's `+0x618` the standing (`0x004BB9A0`): 80 / 50 / -10 / -8, -12 / -20 by the place, × factor 2;
- the rams `+0x330` × factor 3 × -0.3, cut.

Tables of types 2, 5 and the others, and the standing, are lowered by the crew's respect (`+0x70`): × 0.6 over 700,
× 0.8 over 400 (type 2 only in its excess). With a bet placed on the race (bets `+0x00`, not a pink slip `+0x18`):
crew 0's place before the bet crew's gives 50 / 25 / 10 for the profile's bet share `+0x62C` over 7.5 / over 3 /
else, after it 10 / 5 / -20, lowered by the respect of crew 7 (the career's `+0x24`, a stock problem) and × its
factor 6. `last` or `collection` adds the garage's respect as worked out after the race but not kept: the best car
(`0x004BB3D0` with crew 5) and the garage's worth (`0x004BB2F0` with crew 6). Over 10 in size the total is cut to a
multiple of 5; it goes to the object's `+0x30`.

## After the race

The front end made after a career race (`22-race-hud.md`, leaving the race) first ends the session (`0x004A3A40`):

1. `0x004BA8A0`: the profile's `+0x630` set when the sixth place of the crews' order is not 0 while none of the first
   five is the player's crew (0); with no crew finished (`+0x340` -1) the nine crews' odds have `+0x04`, `+0x0C`,
   `+0x10` 0 and `+0x08` 1. Outside a cruise the racers not finished (`+0x28` 0) are listed by their numbers (`+0x24`);
   with more than one, two draws (r1, r2) give (r2 % 6) + 2 x (r1 % the round's laps) swaps of two neighbours at
   `rand() % (n - 1)` (a round of no laps divides by 0 there: a stock problem). The time t is the route's record
   (`[G+0xA8]`, `0x00426490`) cut toward zero, 180 below 10, else plus `rand() % 15`, raised to a finished crew's time
   (its odds' `+0x00`, cut toward zero) when that is greater. Each listed racer of a rival crew, then each of the
   player's crew, finishes (`0x004B9CF0`, the challenge's time -1) at (`rand() % 20`) x 0.1 + its place in the list x
   3.2 + t. The draws are the job thread's.
2. `0x004B9E20` (nothing for a race of type 9; the cheat "win all races", `[career + 0x64]` `+0x04`, makes the
   order's first 0, the player's crew): the crews' order goes into the stats (`+0x78` + 0x18 x the event's round `+0x55`). A money round (type 2) gives
   each crew's respect of the money won (`0x004BBB00`); then the best car's respect (`0x004BB3D0`, crew 5) and the
   garage's (`0x004BB2F0`, crew 6). For each place of the round's crews: once the player's crew is placed, each crew
   after it counts in the profile's `+0x38` word of the crew; the player's crew counts in the profile's `+0x4E` and
   every crew's `+0x2CC`; first, in the round type's word (`+0x18`), the event's `+0x0D` word (`+0x26`) and `+0x628`
   by 0.04 (none above 0), else in `+0x4C`; with the event's `+0x02` the host crew's `+0x2C4` (first) or `+0x2C8`;
   then each of the nine crews gains the respect of the player's place: with the event's `+0x03` (first (crew + 1) x
   20, second -30) x its factor 8 to its gain 8, else the standing (`0x004BB9A0`, with the round's `+0x08`) and the
   place (`0x004BB520`, round types 3 and 4) or the points' place (`0x004BB740`, type 5). With a bet placed, the first
   of the player's crew and the crew bet with in the order: the player's crew sets the bets' `+0x04` and the rival's
   money x 0.8 (its `+0x338` 0), the rival x 1.2 (`+0x338` 1, `+0x04` 0); the money at least 2500. The first crew's
   odds with a stake (`+0x0C`) win stake x (against / for + 1) into `+0x10`. A win counts in the stats' `+0x14` and
   the host crew's `+0x224`. Each racer of the player's crew (`0x004AA440`): its crewmate's `+0xB0` counts, `+0xBC`
   0, `+0xB4` counts a win, and it learns (`0x004AA3B0`: the driving ability and the recovery grow by the learn rate
   up to their limits, a growth below the limit added to the stats' `+0x18` / `+0x20` + 0x18 x its number). Crews 1
   .. 8 take gain 3 = rams x factor 3 x -0.3. An event with a prototype part (`+0x70`, the round's `+0x02`, won):
   each car of the player's racers takes the first part of the kind (`+0x74`) of the supplier (`+0x78`) at the
   prototype level (`0x004990F0`), is rated again, is noted in the `+0x38` list (0x1C: the supplier, the kind, the
   model) and the profile's part state of the model and kind becomes 3. Each such car counts in its `+0x98` ..
   `+0xA6` words by the event's type, its `+0x03` and a win. The unlock steps of each car's model (`0x004B8230`, 10
   changes when the profile's `+0x4E` is over 10, else 4; 8 for a race lost) are taken, and noted in the `+0x38` list
   (0x1D: the model) unless the steps ran out: in a challenge when it is met (`0x004BACB0`: of types 0 .. 2 the race's
   `+0x7C` at 1, of type 3 the money over its value), in a cruise when the money is over crew 3's mark (`+0x6C`),
   else without the profile's `+0x00` and `+0x630`. A bet placed (not a pink slip) gives each crew the bet's respect
   (`0x004BB8E0`); with the profile's `+0x630` the two cars' respects are taken again and added to the career's `+0x34`
   object's `+0x30`.

The crews' respect tables (each returns its value x the crew's factor, cut toward zero; applying adds it to the
crew's gain of that factor; "lowered" is x 0.6 for a crew's respect over 700, x 0.8 over 400):

| Function | Gain | Value |
| --- | --- | --- |
| `0x004BB520` | 4 | the place of n + 1 crews (first 80; second 50, 60 of six; third -8 of five or six; fourth -15 of six; last -20) plus the lead (`[0x0079E840]`: the lead x -1/100 cut, x 20; worked out again only while measuring with nobody finished), halved when positive in an event of type 3, lowered; a crew other than crew 1 with respect from 350 gives 0 (and takes nothing), below it a loss x 0.8 |
| `0x004BB740` | 5 | the place (80; 50; third -10 of five, -5 of six; fourth -10 of six; last -20) plus the lead (`[0x0079E83C]`, x -1/20 cut, x 20), lowered |
| `0x004BB9A0` | 2 | the place (80; 50; third -10 of five, -8 of six; fourth -12 of six; last -20), lowered |
| `0x004BBB00` | 7 | below the crew's mark (`+0x6C`) the shortfall x factor x -0.001; else the excess x factor x 0.001 held to 30 x the event's crew + 20, doubled, lowered; applying above the mark doubles the mark (at most 250000) |
| `0x004BB8E0` | 6 | the profile's bet share over 7.5: 50 won, 10 lost; over 3: 25, 5; else 10, -20; lowered |
| `0x004BB2F0` | 0 | none with the profile's `+0x00`; the garage's worth (`0x0048D430` summed) over the mark (`+0x64`) x factor x 0.001; applying keeps the worth as the mark and in the profile's `+0x4F4` |
| `0x004BB3D0` | 1 | none with the profile's `+0x00`; the garage's best (8 - class) x worth over the mark (`+0x68`), x factor, x 1/450 when it rose, else x 1/2250; applying keeps the product as the mark and the car's worth in the profile's `+0x4FC` |

A car's worth (`0x0048D430`) is its model's VALUE plus each body part's (level + 3) x 500 (none for -1, 0 and over
0x7E) and each catalogue part's COST (the list's first past its end), each held to 15000; at least 199, cut toward
zero.

The event's end (`0x004A6870`, from `CMSRaceResults`' SELECT and after a cruise) sets the event's `+0x01`; with more
won on bets than lost (the career's money counters `+0x0C` over `+0x08`) each crew gains the bet's respect of a win
(`0x004BC390`, as `0x004BB8E0`); with the profile's `+0x00` both message lists are emptied, else crews 1 .. 8's gains
go into their respect (`0x004BBD80`) and the crewmates' learning is noted (`0x004B5DE0`: with the profile's `+0x630`
what crewmates 1 .. 3 learnt is cleared, else each of them with a value learnt not 0 is noted in the `+0x38` list,
0x1F for a gain and 0x20 for a loss, the first such value deciding).

The respect gains go into the respect at the event's end (`0x004BBD80`): each gain not 0 is noted in the career's
`+0x3C` list when it is at least 1 either way (`0x004BBBF0`: of kind k for a rise, 10 + k for a fall, with the gain,
the crew's number and its respect), and is added to the respect, cut toward zero. A respect step reached or lost
(`0x004BBFC0` / `0x004BC070`) clears the leader's `+0x2D4` and `+0x2D8`, sets or clears the step's flag and notes kind
20 + the step or 24 + the step with the crew's number, 0 and 0 (the privileges gained or lost of
`14-career-windows.md`). The two lists hold 50 entries each
(`0x004B5C70`: a kind and three numbers); emptying one (`0x004B5CC0`) also clears the gains of crews 1 .. 8
(`0x004BC760`) and every crew's `+0x330`.

`CMSRaceResults` (`0x004DA310`, `0x004DAF80`): for the rows 1 .. 3 pos, time, driver1 .. 3 and bet emptied, crew,
timetitle, drivers, winnings and box0 hidden; for each of the round's crews up to three in the crews' order the row
shown: pos the place and its ending (lines 37 ..), crew the crew's logo (the career's head object, `+0x58` + 0x24 x
crew), time the crew's time for the first and "+" the time behind the first for the others (`0x0044FBD0`), the crew's
drivers in the session's order (their members' names), bet the round's prize of the place (`+0x2C`, as money). The
player's crew takes the prize (the record's `+0x0C`, the profile's `+0x14`); a crew with a bet (its odds' `+0x0C`)
adds the bet's winnings (`+0x10`) to it and to the career's money counter `+0x0C`, and its prize to the profile's
`+0x10`. The money won (cut toward zero) goes to the counters (`+0x04` a gain, `+0x00` less a loss) and the profile's
money (none below 0); with the race cars chosen each race car's `+0xAC` and the profile's `+0x4EC` take it; Cash and
Date (`0x004A4130`). With the session's first finisher and without the cheat "win all races" (`[career + 0x64]`
`+0x04`) the winning car (`[0x00728C60]`: of the garage for the player's crew, else the opponents'; with the cheat
the car before stays); with a car the SEL2 elements are shown, else those are hidden. Each frame totalWinnings
("TOTAL WINNINGS" and the money left) and cash (the money shown); after 3 s 0.05 of the money moves across each frame
(SWOOSH) until none is left. SELECT: the event's end (`0x004A6870`), CLICK, the round's `+0x01` and the event's
`+0x01` set; with a bet placed `careerload` (the career's `+0xA4` 3), else `0x004BAEE0`: `CMSMeetingSummary` while
the `+0x38` list has entries, `CMSRespectChanges` while the `+0x3C` list has; else the history's step (`0x004BD8A0`);
with the profile's `+0x00` the profile's `+0x4E4` the money, the dealer's `+0x00` 1, `+0x08`, `+0x27D4`, `+0x27DC` 0,
its offers made (`0x004B4620`), `+0x27D8` its `+0x27E8`, `CMSCarDealership`; the first time (the profile's `+0x4E0`
0, then set) `CMSMainMenu`; with the race cars chosen the dealer's `+0x00`, `+0x08`, `+0x27D4`, `+0x27DC` cleared, its
lists made (`0x004B4440`, `+0x27D8` the garage, `0x004BB150`, `0x004B52F0`), its `+0x0C` 0, the profile's `+0x4E4` the
money, `garageload` (the career's `+0xA4` 0); else `CMSMainMenu`. PGDN (event 6) with a winning car asks for
`car_stats2purple` over the window.

## The advisor (the career's `+0x78` object)

Its fields: `+0x00` an advice chosen; `+0x04`, `+0x08`, `+0x0C`, `+0x10` ask `CMSMainMenu` for the phone, the calendar
(two flags) and the dealership; `+0x14` a challenge's advice due; `+0x18` the advice given last and `+0x1C` the advice
to give (13 none) with its event (`+0x20`); `+0x24` a u16 per crew and challenge (`+0x24` + 6 x crew + 2 x
challenge): the car rating told; `+0x5C`, `+0x68`, `+0x6C` the rating, crew and challenge found; `+0x60` no advice
to host events; `+0x70` the crew `0x004A6100` found. `+0x24` .. `+0x59` and `+0x60` are saved.

An event search (`0x004A6A70`: crew, round type, any reason, by crew) takes the first waiting event (used, not raced),
of the crew when asked, with a round of the type (unless -1; past the event's first round the type is read at the
round's place after it), the player may enter (`0x004B7D80`) for any reason when asked, else with a reason other than
0, 1 and 7.

A challenge's car (`0x004BC1D0`): the crew's challenge not done, the crew in the phone's contacts and a garage car of
the challenge's model; its rating cut toward zero. The challenges' advice (`0x004A6190`): the first challenge of
crews 1 .. 8 with such a car whose rating has not been told (5) or is above the one told (4), kept in `+0x5C`,
`+0x68`, `+0x6C`. A challenge's advice is due (`0x004A57B0`, `+0x14`) when there is one.

The advice (`0x004A57E0`): `+0x04` cleared; then 100 rounds of 13 candidates, each worth a score (and an event); a
candidate takes the advice when `rand() % 10` is 5 (one `rand()` per candidate, always drawn), its score is not 0,
it is not the advice given last and its score is above the best so far (then worked out again and kept with its
event):

| Advice | Score |
| --- | --- |
| 8 | crew 1's event with a round of type 3, else 4: 6 with less than 10000 |
| 12 | without `+0x60`: over 100000 6, over 50000 5 |
| 9 | the challenges' advice (no event) |
| 0 | an event with a round of type 4, else 3: crew 1's steps (4 without its second, 4 without its third, 1 with its fourth, else 3) |
| 1 | `0x004A5DF0`: with crew 2's respect under 350 an event with a round of type 4, else (or none) of type 5: crew 2's steps (4, 3, 1, 2) |
| 2 | an event with a round of type 2: crew 3's steps (4, 3, 0, 2) |
| 10 | `0x004A6240`: a prototype part event (`+0x70`) the player may enter (reason not 0, 1, 7) with a garage car of its class whose part of the event's kind is none or not a prototype part: 5 |
| 3 | `0x004A5E60`: an event with a round of type 4 and more than one crew member: crew 4's steps (4, 3, 0, 2) |
| 4 | `0x004A5ED0`: the best car's worth (the profile's `+0x4FC`) at least 5000 over the one told (`+0x4F8`) and an open event: crew 5's steps (4, 3, 0, 2) |
| 5 | `0x004A5FA0`: the garage's worth (`+0x4F4`) at least 5000 over the one told (`+0x4F0`) and an open event: crew 6's steps |
| 6 | `0x004A6070`: an event with a round of type 4, else 5, else 3 for any reason, and at least 15000: crew 7's steps (4, 3, 1, 2) |
| 7 | `0x004A6100`: three cars or more and a crew of 1 .. 8 at its third step with its `+0x2D0` under 3 (`+0x70`): crew 8's steps (4, 3, 1, 2) |
| 11 | an empty garage: 6 |

After the rounds a due challenge's advice wins (its rating told in `+0x24`, the advice 9, `+0x14` cleared, no
event). The advice goes to `+0x1C` with its event and `+0x00` is set. `CMSAtMeeting` (`0x004C01E0`) keeps the advice
as the last one (`+0x18`) when its event is the meeting's, else 13, and clears `+0x1C` and `+0x20`.


## The dealer's list

The dealer (career `+0x70`) shows a list of up to 159 entries of 0x40 bytes at `+0x14` (count `+0x27D4`), the same
layout as its offers: `+0x00` an index, `+0x04` the model, `+0x08` used, `+0x0C` the condition, `+0x10` the model's
class table value (`0x0048CE20`), `+0x14` the mileage, `+0x18` the rating, `+0x1C` the price, `+0x24` damaged, `+0x28`
prototype parts. Its `+0x00` is the mode, `+0x04` the entry chosen, `+0x08` a car bought or sold (done when the
window is left), `+0x0C` the garage opened from `CMSStatsCarCollection`, `+0x10` the garage opened after a race (its
BACK goes to the career menu), `+0x27D8` the cars of the list shown, `+0x27DC` the chosen entry's index, `+0x27E0` the
level, `+0x27E4` the class listed (-1 every class), `+0x27E8` the showroom's cars, `+0x4FB0` a car for each offer.

The mode (`0x004B43C0`): `+0x00` the mode, `+0x27D4`, `+0x08`, `+0x27DC` 0; mode 1 the offers, `+0x27D8` its
`+0x27E8`; modes 0 and 2 the garage, `+0x27D8` the garage.

The offers (`0x004B4620`): each of the dealer's offers (`+0x27EC`, count `+0x4FAC`) copied to the list.

The garage (`0x004B4440`): an entry for each garage car: its garage index, its model, used 1, condition 1 (2 and
`+0x24` 1 when its grid is damaged, `0x00557E80` over 0), the model's class value, mileage 0, its rating (cut), its
sale worth (`0x0048DA40`), `+0x20` 0 and `+0x28` more than three prototype parts (`0x004A8F50`). The garage's first
entries are then sorted by the rating and then by `+0x24`, each the greatest first, a swap starting the pass again.

Choosing (`0x004B52F0`): the first entry with that index: `+0x27DC` its index, `+0x04` the entry, `+0x08` 0.

The dealer's cars (`0x004B5330`): its `+0x4FB0` list of cars freed (`0x00489C30`), then a car for each offer: a new car
(`0x0048DB40`) of the CarDef of the model's name (`0x0048CC30`), the offer's paint (`+0x2C`, `+0x30`, `+0x34` to the
car's `+0x18`, `+0x1C`, `+0x20`), modified automatically (`0x0048F2C0` with `+0x28` as the prototype flag, level -1)
when the offer's `+0x20` is set, rated (`0x0048E8A0`) and given its grid (`0x005562A0`). The dealership's 3d view
shows them in mode 1.

Taking an entry to the garage (`0x004B5070`): in mode 1 a car of that offer as above, the showroom (`+0x27E8`) freed
and the car added to it, `+0x27DC` 0 and `+0x04` the offer; then twice the next other offer (`0x004B5C20`: the
(n + 1)th offer with `+0x08` set and `+0x24` 0 whose place is not the chosen offer's `+0x00`), when there is one, a car
of it added the same way but without a grid. In the other modes `+0x04` the list's entry and `+0x27DC` its `+0x00`.
`+0x08` 0, and it returns 1. The garage (`garageload`) sets `+0x08` when a car is bought or sold.

Done (`0x004B5490`): with `+0x08` set (then cleared) mode 1 adds the showroom's car at `+0x27DC` to the garage (its
grid made) and takes it out of the showroom (`0x00489D30`); the other modes take the garage car at `+0x27DC` out
(`0x00489CD0`). The showroom is then freed. The garage car of a list's place (`0x004B5470`) is the garage's car of
the entry's `+0x00`.

The player's race car (`0x004BB150`): the car of the session's first racer of crew 0 and member 0, else of the first
racer of crew 0, else 0.

## The phone (the career's `+0x5C` object)

Its fields: `+0x00` the state (0 idle, 1 opened by the player, 2 calling, 3 ringing, 4 talking), `+0x04` the time
before the next call, `+0x08` the phone model, `+0x0C` an event to go to and its month and day (`+0x10`, `+0x14`),
`+0x18` a respect change to show with the change (`+0x1C`), the respect after it (`+0x20`) and the crew (`+0x24`), ten
messages (`+0x28`, 0xBC bytes), the call (`+0x780`, the same layout), the contacts (`+0x83C`, 0x48 bytes) and their
number (`+0xF8C`), the contact called (`+0xF90`), its ringing (`+0xF94`) and `+0xF98` (answered at once), the crew and
the crewmate the call shows (`+0xF9C`, `+0xFA0`, pointers) and the host rights note shown (`+0xFA4`). Only the
contacts, their number, `+0x08` and `+0xFA4` are saved.

A message: `+0x00` used, `+0x04` the caller's kind (0 a crew's leader, 1 a crewmate, 2 Mr. Jeff), `+0x08` the name,
`+0x88` the type, `+0x8C` the speech sample (played below 0x21, -1 none), `+0x90` the sample's crew, `+0x94` the text
(a wide string), `+0xA4 ..` four answers (pointers to text lines), `+0xB4` the crew (the crewmate for kind 1). Queuing
(`0x004B63B0`) skips a message of the same type, kind and name (case aside) waiting already, else takes the first free
slot. The messages:

| type | from | kind, sample, text, answers |
|---|---|---|
| 0 | the crewmates' day (`0x004AA4C0`) | 1, 0x24, "Let driver join crew?" (315), YES, NO |
| 2 | a crew reaching its last respect step (`0x004BBD80`, its leader's `+0x2C0` 0) | 0, 1, "Host challenge?" (321), YES, NO |
| 5 | `0x004B6A10` each phone frame | 0, 4, "Pink slip race?" (324), YES, NO |
| 10 | the dealer's new stock (`0x004B64C0`) | 2, -1, the player's leader's name and line 487 + `rand() & 3`, "DELETE MESSAGE" (335) |

`0x004B6A10`: a crew (0 .. 8) past its second step (`+0x5C`) with the player's money over 40000, two to 54 garage
cars, more days since its last call (`+0x2CC`) than 17 less its leader's id and fewer than 3 pink slip races
(`+0x2D0`): `+0x2CC` 0 and the message. A message's name is "<short name> (<crew's name>)" for crews (the names
`0x00718738`).

The phone's frame (`0x004B60F0`): a noted respect change is shown in `CMSRespectMsg` (the crew's respect before, the
change, x 0.02 the step) and the crew's respect becomes `+0x20` (not below 0, cut); `0x004B6A10`; a window new to the
phone (`[0x0079E824]`) waits 2 s (`[0x0079E820]`, the front end's frame time); then `+0x04` runs down; at 0 with the
phone idle the first message becomes the call (`0x004B6580`: type, sample, its crew, text, answers; its sample not
played), for types 0 and 1 `+0xFA0` its crewmate, the call's kind, crew and name, its caller a contact unless it is a
text message (`0x004B6640`), the phone ringing (3) and `CMSMobilePhone` over the window. Idling (`0x004B69B0`): the
state 0, `+0x04` 5 + `rand()` x 0.00030518509, the sample stopped, `+0xF9C`, `+0xFA0` 0.

Calling a contact (`0x004B6710`): `+0xF90` the contact, `+0xF94` 4 + `rand()` x 9.155553e-05, `+0xF98` 0. A crewmate
(kind 1) who joined: `+0xF98` 1 and a call of type 11 with END CALL (313) only; else "Join my crew?" (type 1, sample
0x21, line 318; answers 319, END CALL) and `+0xFA0` the crewmate. A crew's leader (kind 0): type 8, sample 0, line 330,
by the advice (`+0x1C`): 9 "TAKE A CHALLENGE." (331) and END CALL, the advised challenge's rating written to the
advisor's table (`+0x24 + 2 x (crew x 3 + challenge)`); 7 "RACE FOR PINKS." (327) and END CALL; with cars both and
END CALL; else TAKE A CHALLENGE and END CALL. The call takes the contact's kind, crew and name; the phone calls (2).
Another kind does nothing more.

The answers (`0x004B6B20`, by the call's type; the replies are of type 11 with END CALL, their crew the call's):

| type | answer | what follows |
|---|---|---|
| 0 | any | `+0xFA0` 0 |
| 0 | YES | the crewmate joins (`0x004B80D0`), sample 0x25, line 316 |
| 0 | NO | sample 0x26, line 317 |
| 1 | first | the crewmate joins, sample 0x22, line 320 |
| 1 | END CALL | idle |
| 2 | YES | the next free day (`0x004A6A10` into `+0x14`, `+0x10`), the host rights race (`0x004A2DC0`), sample 2, line 322, `+0x0C` 1 |
| 2 | NO | the respect within 0 .. the last step less 100 noted (`+0x18`), sample 3, line 323 |
| 5 | YES | the next free day, the pink slip race (`0x004A2F80`), sample 6, line 325, `+0x0C` 1 |
| 5 | NO | the respect less 25 noted, sample 5, line 326 |
| 8 | first | the challenges: with the advice 9 the advised one's kind (372 + type), else each of the three with " (c)" when done, as the answers with END CALL (type 9, sample 0x15, line 332, the crew's id), `+0xF9C` the crew |
| 8 | second | a crew past its second step with fewer than 3 pink slip races and under 55 cars: the next free day, the pink slip race, sample 8, line 328, `+0x0C` 1; else sample 7, line 329 |
| 8 | third | idle |
| 9 | END CALL | idle |
| 9 | a challenge | the next free day, `+0xF9C` 0; a garage car of the challenge's model (`0x004A8C80`, any class; 100 for model 0): its event (`0x004A3130`), sample 9, line 333, `+0x0C` 1; else sample 10, line 334 |
| 10 .. 15 | any | idle |
| 16 .. 19 | any | the crew's respect a step less 20 (100, 300, 1000, 600; `0x004A6E10`), its gains (`0x004BBD80`), idle |

For type 8 the advice changes the answer: with 7 the first is the second, with 9 the second the third; without cars
the second is the third. Type 18 goes on into 19.

The phone's events (`0x004A2DC0` host rights, `0x004A2F80` pink slip): none when a used event is on the day; the crew's
race attended by it and the player (`+0x02` or `+0x03`; the host rights set the crew's `+0x2C0`), type 4, 3 when
`rand()` / 32767 is under the crew's P2P, 5 under its sprint, the class of the best rating of the player's garage
(`0x004A9A70` on the career's first crew, the player's: its list is the garage, another crew's every opponent's car),
`+0x06` -1, its round (`0x004A33E0`), time of day and weather (`0x004A4410`), the calendar's first free slot. The challenge's event (`0x004A3130`): `+0x05` 1, `+0x06` the
challenge, `+0x68` its car, class 8, type 0, the first place's prize the challenge's prize unless done, its round and
teams.

## Cheats (`0x004A6E50`)

The cheat of CMSCheats (`02-frontend.md`) by its number:

0. The arcade's progress unlocked (`0x0042B6B0` on `[G+0x8C]`).
1. The money 10000000.
2. For crews 1 .. 8: the respect level up twice (`0x004B8320`: the profile's `+0x4E` and every crew's `+0x2CC` up one),
   the crew's respect 2000 (`0x004A6E10`: none below 0, cut), `+0x2D4` and `+0x2D8` 0 of the crew of its number, its
   `+0x60` (the fourth step) 1, a note in the respect changes (`0x004B5C70`: kind 23, the crew's number, 0, 0) and its
   leader a phone contact (`0x004B6640`: kind 0, "<short name> (<the crew's name>)", the crew). Then the rivals' gains
   (`0x004BC2B0`: `0x004BBD80` for crews 1 .. 8) and both note lists emptied (`0x004B5CC0`).
3. For each of the 53 models: the respect level up, its unlock steps (`0x004B8230`, 100 changes), the profile's
   `+0x50` and `+0x262` values of the model 10, its `+0x474` value 10. Then every `+0x50` value 3 (the whole table),
   both note lists emptied, and for the models 1 .. 52, while the garage does not have 55 cars, a car of the model
   (`0x0048DB40`, `0x0048CC30`) with a paint at random (`rand()` % the paints: base, metallic and pearl), rated
   (`0x0048E8A0`) and with its grid (`0x005562A0`) joins the garage.
4. Each crewmate not in the crew (`+0xAC` 0) joins it (`0x004B80D0`).
5. `[career + 0x64]` `+0x00` 1 ("character test mode").
6. `[career + 0x64]` `+0x04` 1 ("win all races").
7. Cheats 0 .. 4, then the profile's `+0x628` 0.

A phone contact (`0x004B6640`): kind 1 is never added; a name already among the contacts (without regard to case) is
not added again; else the contact (kind `+0x00`, name `+0x04`, crew `+0x44`) goes at the end (`+0xF8C` the count).

## The crew's scheme on its cars

`0x004A9080` (a crew in `edi`): every car of the crew's list (the garage for the crew of id `+0x78` 0, else every
opponents' car) takes the paint (`+0x24` 0.5 and `+0x28` 0.1 first, then `+0x1A4` .. `+0x1B4` to `+0x18` .. `+0x28`),
the wheels (`+0x1BC` to `+0x12`), the neon on (`+0x0D` 1) of the colour `+0x1C0` of the CarDefs' neon colours (the
first past the list) to `+0x2C`, `+0x40` 1, the wheels' colour `+0x1B8` of the wheel colours to `+0x3C`; a
prototype car (`0x004A8F50`) the decal (`+0x0C`) none and its colour (`+0x38`) 0, another the CarDef's decal list's
size less 8, 7, 1, 4, 3, 5, 6 for the crews of id 2 .. 8 (2 for the others) and its colour 0.
