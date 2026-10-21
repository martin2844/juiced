# Windows of the career

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## The way in: DemoFlyby and careerload

`DemoFlyby` (`0x00435090` enter, `0x004352B0` update, `02-frontend.md`) ends in the career entry (`0x00435434`,
`08-career.md`, "The race of an event"), which asks for `CareerLoad` (switch mode 1) and sets the career manager's
`+0xA4` to 2.

`careerload` (`0x004358A0` enter, `0x00435960` update, `0x00435930` exit; `garageload` is the same state) counts 3
frames, then asks (mode 3) for the window that `+0xA4` names:

| `+0xA4` | window |
|---|---|
| 0 | `carbuy` while the dealer's `+0x00` is set, else `carsel` |
| 1 | `CMSGridWalk` |
| 2 | `character` |
| 3 | `characterPostRace` |
| 4 | `cmsCutScene` |
| 5 | `car_rolling` |
| 6, 7 | `carsel` |
| 8 | `CMSOnlineGridWalk` |

Its enter stops the music (`[[game + 0x38] + 0x38]` = 0) and, with `+0xA4` 0 and the dealer's `+0x00` 1, hides the
elements whose name holds `GR_`; else shows them (at the end of their transitions). Its exit sets the front end's
black (`+0x10C`) to 1.0 and turns the music flag on again.

## The betting window `character`

State record `0x0071FA78` (vtable `0x006EFAD8`): enter `0x0042E610`, update `0x004325D0`, exit `0x0042F030`. The
crew bet with (the bets' `+0x08`) has a leader who talks in movies: `movies\<leader>\<leader>_<CLIP>.bik`, the leader
by crew (`0x004B1650`): 1 TK, 2 Biggi, 3 Sue, 4 Maria, 5 Jack, 6 Melody, 7 Poppa, 8 Carlos. The talk goes in steps
(`+0xD0`); a step picks its clip when the clip of the step before has finished (`+0xD4` cleared).

### Enter

- `Pref`, `PrefVal`, `YourCash`, `YourCashVal` shown for a money bet (the bets' `+0x18`, pink slip, 0), else hidden.
- The music stops. `+0xCC` = the bets' kind (`+0x10`), `+0xD0` = the bets' first step (`+0x14`), `+0xD4` = 0,
  SKIPFADE `+0x6C` = 1.0, the leader's folder, `+0xC0` = the pink slip flag, `+0xBC` = 0, `+0xC8` = 1 (one amount may
  be turned down before the leader leaves). `+0x20`, `+0x24`, `+0x28`, `+0x78`, `+0xC4` = 0.
- `Menu` hidden; the elements holding `ARROW` and `BET` hidden; `carbox` hidden for a money bet.
- With `CAR_CHARACTER` the bet crew's scheme goes on its cars (`0x004A9080`, `08-career.md`); `CAR_CHARACTER` and
  `CAR_PLAYER` get a car view ("The car view": zoom 0.82, the shadow on) showing the crew leader's car (`0x004B1730`,
  the opponents' cars or the garage for crew 0) and the player's (`0x004B17C0`), turned 3.74 (`[0x0072530C]`,
  `[0x00725308]`), not turning, at once; the camera at (0, 110, -560) (`0x0045B050`); both hidden for a money bet; the
  enter waits for each car (frames of the view, the loading thread pumped, 5 ms sleeps). Each frame after the answers
  both views take their frame (`0x00459EE0`).
- Texts: `PrefVal` the rival's money (cut) as money; `CharCrew` the crew's name (text line 108 + crew); `CharName` the
  leader's name (8-bit, widened by sign); `SkillMeter` the leader's ability x 0.01, `AggMeter` its concentration x
  0.01; `BossName`, `BossCar`, `YourName`, `YourCar` (several lines): the leader's and the player's short names, the
  display names of their cars; these four hidden for a money bet.
- `+0x2C` = (crew + 2) x 500, the price of a look under the bonnet; `CASH` the player's money, `DATE` the calendar's
  day (`0x004A4130`, `0x004A68F0`).
- The question flags `+0xE4`, `+0xE8`, `+0xEC`, `+0xF0` = 0, the answer `+0xF4` = -1; input on (`+0x1C`).
- With the career's movie test (`[career + 0x64]` set, nothing sets it) the talk would start at step 0 and SELECT /
  BACK would step through a list of 81 clips (`0x00715AE0`), event 6 through the crews.

Money as shown (`0x004A3A90`): the amount with its thousands marked (`0x004A3AF0`: "." in French and Italian, else
","; "-" before a negative amount), then "`<amount> $`" in French, else "`$<amount>`". The day (`0x004A68F0`): the
day number + 1, then in Italian line 40 for the first day; in French "er" for the first day; in Spanish " de "; else
line 37 ("st"), 38 ("nd"), 39 ("rd") for days ending in 1, 2, 3 except 11 .. 14, line 40 ("th") for the others;
then " " and the month (lines 128 .. 139).

### Update

In this order each frame (the window's state does not run while a message window that waits for an answer is open,
nor while another window lies over this one; the movie plays on in the MOVIE element's draw):

1. With BET_CASH and `+0x68` set: `+0x64` = `+0x68`, BET_CASH the amount as money.
2. `+0x18` (a pink slip's stake after the autosave): a copy of the stake (`+0x14`, `0x00434D90`: every field but
   the grid `+0xB4`, which is made anew, so the kept damage is not copied) joins the garage's end, rated
   (`0x0048E8A0`); the current event's `+0x01` cleared; the profile's `+0x600` the new car and each racer of crew 0
   of the session races it (`+0x0C`); the stake freed, `+0x18` cleared.
3. The answers: `+0xE4` (quit the career): yes → `menu` (mode 1), else the movie goes on. `+0xE8` (no bet): yes →
   step 18, input off, ARROW and Menu hidden; else ARROW hidden, Menu shown. `+0xEC` (pink slip) and `+0xF0` (bet):
   yes → step 17; else ARROW hidden, Menu shown. Each clears its flag, the answer and `+0xD4`.
4. The movie of the MOVIE element finished (`0x0055A530`) → `+0xD4` = 0.
5. SKIPFADE: `+0x6C` falls by the frame time to 0; the box's colour is black with alpha = `+0x6C` x 255 (cut).
6. Step 19: the race starts (below); the update ends.
7. With `+0xD4` 0 the step picks its clip into "`<leader>\<leader>`" + the clip's name; the MOVIE element gets it
   (volume: the save system's `+0x5C`), `+0xD4` = 1.
8. `+0x78` = 1 (an amount confirmed): `+0x78` = -1, step 12, `+0xBC` = the selected item's value (cut), `+0x68` = that
   amount; ARROW goes out, Menu hidden, BET comes in; `+0xD4` = 0, SKIPFADE 1.0.
9. SELECT (event 5) with input on and a Menu: `+0x64` = `+0x68`, then by the step:
   - 9 (the bet menu): sound CLICK, ARROW goes out, Menu hidden; by the item's name: `yes` → (money) "Place Bet?"
     (line 1998) + "\n" + "ARE YOU SURE?" (86), `+0xF0`; (pink slip) line 669 + "\n" + line 1942, `+0xEC`; `no` →
     line 86, `+0xE8`; questions: message window type 3 starting on NO, answer to `+0xF4`, the movie paused.
     `hood` → step 14, `raise` → 26, `lower` → 28.
   - 11 (the amounts): CLICK, `+0xBC` = the item's value (cut), BET_CASH = the item's text; -1 ("Back out") →
     line 86, `+0xE8`; else "Place Bet?" + "\n" + line 86 with the answer to `+0x78`; the movie paused.
   - else: a clip playing is skipped (CLICK, `+0xD4` = 0, SKIPFADE 1.0).
10. BACK (event 12) with input on: in the first event (the profile's `+0x00`) "QUIT CAREER" (line 1) + "\n" + line
    1823 + "\n" + line 86, `+0xE4`; otherwise line 86, `+0xE8`; message type 3 starting on NO, the movie paused.

A movie that is not there still gets a player (`0x0055A9B0`); without a Bink it counts as finished, so its clip ends
at once.

A clip is given to the MOVIE element as `0x005AA300` inlined (the interface's movie given back, the name, played
once, the volume 1, then the save system's `+0x5C`; the hold taken off when it is already the interface's movie).
"Paused" and "stopped" are the same (`0x00435030` and its copies inlined): when the interface's movie is the
element's, it is held (`+0x24` 1) and stepped twice; "goes on" takes the hold off. The leave (`0x0042F0C0`) stops the
movie and sets MOVIE's volume 0.

### The steps (the jump table `0x00433750`)

Clips named "1 / 2 / 3" take `rand() % 3`: 1 the second, 2 the third, else the first.

| step | function | clip and what it does |
|---|---|---|
| 1 | | nothing (the talk is over) |
| 2, 5, 6 | `0x0042F140` | the movie test's clip (`+0x0C`); BOSSCAR shown |
| 3 | `0x0042F1D0` | by the crew's last race (`+0x338`): none (-1): `GREETING1`, or in the first event `CHALLENGE_INTRO1` and the bet's racer set (session `+0x374`, `+0x370` = 1); won (1): `GREETING_WON_LAST1 / 2 / 3`; lost: `GREETING_LOST_LAST1 / 2 / 3`. Step 4 |
| 4 | `0x0042F490` | `sel_but`, `sel_text`, `back_but`, `back_text` shown, input on; the rating of the player's leader's car less the crew leader's: above 20 `COMMENT_CAR_NEG1 / 2 / 3`, below -20 `COMMENT_CAR_POS…`, else `COMMENT_CAR_NEUTRAL…`; the leader's car becomes the car of the car statistics window (`0x00728C60`). Step 7; the car views, `carbox`, the names and cars shown |
| 7 | `0x0042F910` | kind 1: `SUGGEST_PINKSLIP1 / 2 / 3`, step 8; kind 2: `HOW_MUCH1 / 2 / 3`, step 10; else `SUGGEST_BET2` when `rand() & 1` is 1, else `SUGGEST_BET1`, step 8 |
| 8 | `0x0042FBE0` | Menu shown with its transition again, emptied: ACCEPT BET (530, `yes`), DECLINE BET (531, `no`, not in the first event), LOWER BET (529, `lower`), RAISE BET (528, `raise`), and while the player has more money than `+0x2C` "CHECK CAR (`<price>`)" (107, `hood`); ARROW and BET come in; `WAITCYCLE1`, step 9; kind 0: `+0x68` = the rival's money (cut); kind 1: BET_CASH "Pink slip" (104) |
| 9, 11 | `0x00431F90` | `+0xC4` + 1; below 10: `rand() % 100`: below 20 `WAITCYCLE1`, 40 `WAITCYCLE2`, 60 `WAITCYCLE3`, 75 `COUGH1`, 85 `YAWN1`, else `SIGH1` (the first event: `COUGH1`); the tenth: `+0xC4` = 0 and by the counter `0x00715AB4` (1 at start, kept while the game runs; then + 1, 4 → 0): 2 `IMPATIENT1`, 3 `IMPATIENT3`, else `IMPATIENT2` |
| 10 | `0x0042FDE0` | Menu shown again, ARROW in, BET out; items: "Back out" (422, -1), then each amount of the bets the player has more money than (value: its index); `WAITCYCLE1`, step 11 |
| 12 | `0x0042FFB0` | the leader takes the amount when `rand()` / 32767 < its weight: `ACCEPT_BET1 / 2 / 3`, step 20, the bet made (`0x004B1A40`), the bet's racer set, the phone contact (below). Else an amount of the lower half: the three highest kept (`0x004B1A00`), respect kind 1; with `+0xC8` 0 `DECLINE_BET1 / 2 / 3` and step 21, else `DECLINE_BET_LOW1 / 2 / 3`, `+0xC8` - 1, step 10. Else (the upper half): the amounts cut to 4; with `+0xC8` 0 respect kind 3, `DECLINE_BET…`, step 21; else kind 0, `DECLINE_BET_HIGH1 / 2 / 3`, `+0xC8` - 1, step 10 |
| 13 | `0x00432440` | Menu shown again, emptied: YES (105, `yes`), "Back out" (422, `no`); ARROW in, input on; `SUGGEST_PINKSLIP1 / 2 / 3`, step 9 |
| 14 | `0x00430650` | `RESPONCE_SEE_BONNET1 / 2 / 3`, step 15; the `hood` item taken out, the first item selected |
| 15 | `0x00430770` | the leader's car to the car statistics; `car_stats2purple` over this window (mode 5); `WAITCYCLE1`, step 16 |
| 16 | `0x00430830` | `OKAY1 / 2 / 3`; Menu shown again, ARROW in, step 9; the look paid (`+0x2C`), CASH and DATE, YourCashVal the money |
| 17 | `0x00431760` | `PLAYER_ACCEPT_CHALLENGE1 / 2 / 3`. Money: unless a lower bet was taken (`+0x20`) respect kind 4; the bet made: the rival's money (cut), halved (`+0x20`) or doubled (`+0x24`); the phone contact. Pink slip: the car views freed; a copy of the player's car (the profile's `+0x600` of the garage, `0x00434D90`) kept as the stake (`+0x14`) and the car taken out of the garage (`0x00489CD0`), so that the autosave keeps the garage without it; the event's `+0x01` set; the race set up again (`0x004B8570`). Step 20, the bet's racer set |
| 18 | `0x00431CC0` | `PLAYER_DECLINE_BET1 / 2 / 3`; unless a higher bet was turned down (`+0x28`) respect kind 2. Step 21 |
| 20 | | the window `autosave` (mode 5) when the save system's method `+0x24` answers non-zero; `+0x18` = 1 for a pink slip; step 19, `+0xD4` = 0, SKIPFADE 1.0, `+0x64` = `+0x68` and BET_CASH; the MOVIE element gets "`<leader>\<leader>`" (no such movie) |
| 21 | | pink slip: the event lost (`0x004A3A60`: the byte `+0x1D` + the event's `+0x55` x 0x38, its round's `+0x01`, and the event's `+0x01` set; the round, signed, other than 0 writes past the event's one round into the calendar, a stock problem), the movie stopped, the event's end (`0x004A6870`), CLICK, `0x004BAEE0`; money: the movie stopped, `CMSGridWalk` (mode 1). Step 1 |
| 22 .. 25 | `0x004321B0`, `0x00432360`, `0x00432280`, `0x004323B0` | after the race (`characterPostRace`): `WINS_CASH1 / 2 / 3`, `WIN_PINKSLIP1`, `LOSES_CASH1 / 2 / 3`, `LOSES_PINKSLIP1 / 2` (`rand() & 1`) |
| 26, 28 | `0x004309F0`, `0x00431190` | `THINKING1 / 2 / 3`, step 27 / 29 |
| 27 | `0x00430AD0` | `rand() % 18` below crew x 2 + 4: `ACCEPT_BET1 / 2 / 3`, respect kind 5, `+0x24` = 1; else `DECLINE_BET_HIGH1 / 2 / 3`, kind 6, `+0x28` = 1 |
| 29 | `0x00431270` | `rand() % 18` above crew x 2 + 4: `YES1 / 2 / 3`, `+0x20` = 1; else `DECLINE_BET_LOW1 / 2 / 3`, kind 7 |

After 27 and 29: step 9, the `raise` and `lower` items taken out, the first selected, Menu shown again, ARROW in;
BET_CASH and `+0x68`: the rival's money doubled (27, taken) or halved (29, taken), from the money cut; otherwise
BET_CASH shows the money cut and `+0x68` keeps it rounded.

Step 19 (before the clips): money bet in the first event: the movie stopped, the bet share (`0x004BC2E0`), the race
started (`0x004A3CC0`, which plays CLICK and asks for `loading`); money otherwise: the race started, the bet share,
CLICK, `loading`; pink slip: CLICK, `loading`. Step 1.

The phone contact (`0x004B6640`): "`<short name> (<crew's name>)`" (8-bit, the names at `0x00718738`) is looked for
among the phone's contacts (without regard to case); a new one is added (kind 0, the crew's number) and told in a
message window of type 1: the leader's name, " (", the crew's name (line 108 + crew), ") ", line 471; the movie is
paused (the player of the clip then playing, which the next clip replaces).

### Respect messages (`0x004BC7B0` and the same lines in steps 17, 18, 27, 29)

| kind | text | change |
|---|---|---|
| 0 | 437 " is impressed by your high bet." | +20 |
| 1 | 438 " laughs at your low bet." | -25 |
| 2 | 439 " is annoyed at you backing out." | -35 |
| 3 | 440 " has waited too long." | -30 |
| 4 | 441 " is pleased you accepted the bet." | +20 |
| 5 | 442 " is impressed by your high bet." | +10 |
| 6 | 443 " is impressed by your high bet." | +30 |
| 7 | 444 " laughs at your low bet." | -25 |

The comment: the leader's name (not in Spanish), the text, " ", "Respect" (172), " ", "+" for a gain, the change.
The record `0x007227E8` takes the crew (`+0x0C`), its respect (`+0x10`), the change (`+0x14`), the change x 0.02
(`+0x18`), the respect after it (`+0x1C`) and the comment (`+0x20`); `CMSRespectMsg` is asked for over the window
(mode 5); the crew's respect becomes respect + change, cut, at least 0.

## The window after a bet: `characterPostRace`

Record `0x0071FB70` (vtable `0x006EFAC8`): enter `0x004337D0`, update `0x004344D0`, leave `0x00434380`. `CMSRaceResults`'
SELECT with a bet placed asks for `careerload` with the career's `+0xA4` 3, which opens it. Its fields: `+0x0C` and
`+0x10` the car views of the player and the leader, `+0x14` the MOVIE element, `+0x1C` the SKIPFADE alpha, `+0x20` ..
`+0x34` the elements PotVal, cash, YourName, YourCar, BossName, BossCar, `+0x38` the outro played, `+0x3C` the pot
left, `+0x40` the step, `+0x44` the money shown, `+0x48` .. `+0x50` three delays, `+0x54` the leader's movie folder,
`+0x94` a clip plays, `+0x98` a sound, `+0x9C` the bets (career `+0x6C`), `+0xA0` the career's `+0x34`.

Enter: the music's volume 0 (`[game + 0x38]`); `+0x94` 0, `+0x1C` 1.0; the leader's folder (`0x004B1650`); BossName,
BossCar, YourName, YourCar over several lines; the car views of the leader's car (`0x004B1730`, the opponents' cars or
the garage for crew 0) and the player's (`0x004B17C0`) as in the betting window (the crew's scheme first), each
waited for until loaded; the update gives them no frames. BossName the crew's short name (`+0xDE`), BossCar its car's name (def `+0x5C`), YourName and
YourCar the player's. cash the money (`0x004A3A90`), `+0x44` the money; PotVal twice the bet (cut), `+0x3C` that,
`+0x40` that x 0.05. `Pot` and `YourCash` shown; `Outcome` over several lines and by the bet:

| bet | Outcome | money | clip |
|---|---|---|---|
| pink slip, won (bets `+0x04`) | "PINK SLIP WON" (427) | — | `_LOSES_PINKSLIP1` |
| pink slip, lost | "PINK SLIP LOST" (428) | — | `_WIN_PINKSLIP1` |
| money, won | "BET WON" (425) | the pot paid to the player (as `0x004B8200`); each race car's (profile `+0x600`, count `+0x60C`) `+0xAC` and the profile's `+0x4EC` take the bet (cut) | `_LOSES_CASH1 / 2 / 3` |
| money, lost | "BET LOST" (426) | — | `_WINS_CASH1 / 2 / 3` |

The money clips by `rand() % 3` (1 the second, 2 the third, else the first); the clip is "`<leader>\<leader><clip>`".
For a pink slip the elements `Pot` and `YourCash` are hidden (`+0x58`) and cash and PotVal emptied. `+0x94` 1,
`+0x48` .. `+0x50` 3.0, `+0x38` 0.

Update: the clip finished (the Bink player's method `+0x08`): `+0x94` 0. `+0x1C` less the frame time, within 0 .. 1.
With no clip playing: in the first event (the profile's `+0x00`) `_CHALLENGE_OUTRO1` once (`+0x38`), else by
`rand() % 100` below 20 `_WAITCYCLE1`, 40 `_WAITCYCLE2`, 60 `_WAITCYCLE3`, 75 `_COUGH1`, 85 `_YAWN1`, else `_SIGH1`
(the first event `_COUGH1`); `+0x94` 1. SKIPFADE black with the alpha `+0x1C` x 255 (`0x00434D20`).

SELECT in the first event: the repair bill of the garage's first car (`0x0048EBC0`, cut, at most 5000); with the
profile's `+0x04` 0 and a bill of 10 or more it is paid (`0x004B8200` with minus the bill), the profile's `+0x04` set,
cash the money, `+0x44` less the bill, a note "Repair bill for T.K.'s car " (553) and the bill (type 1), the movie
paused (`0x00435030`); otherwise CLICK, the movie stopped, the current event's `+0x01` set, the profile's `+0x4E4` the
money, the dealer's `+0x00` 1, `+0x08`, `+0x27D4`, `+0x27DC` 0, its offers made (`0x004B4620`), `+0x27D8` its
`+0x27E8` and `CMSCarDealership`, and the frame ends. SELECT otherwise: CLICK, the movie stopped, `0x004BAEE0`.

Then while `+0x48` is over 0 it counts down. A pink slip counts down `+0x4C`, then `+0x50`. Money: while `+0x3C` is
over 0 the step moves from it (SWOOSH), to `+0x44` when the bet was won; cash `+0x44` + 0.5 and PotVal `+0x3C`, cut.

Leave: a `Menu` element's highlighted item kept, the sound `+0x98` stopped, the music's volume back (`[game + 0x64]`
`+0x54`), the movie stopped, the car views freed; a pink slip's car changes hands (`0x004B1A80`); the bets cleared
(`+0x00`, `+0x04`, `+0x0C`, `+0x18`, `+0x5C` .. `+0x80`).

The pink slip's car (`0x004B1A80`): won (the bets' `+0x04`), while the garage has fewer than 55 cars, a copy of the
car of the leader of the crew bet with (`0x004B1730`; the garage for crew 0, else the opponents' cars) joins the
garage with a new grid, the crew's `+0x2D0` grows, and the profile learns the car's parts: for each kind 0 .. 9 the
catalogue part of the car's `+0x0E` + kind (the list's first past its end) gives its level (`+0x2B`) to the model's
`+0x50` levels, and each body part of `+0x04` + category to the model's `+0x262` levels (category 3 then 100), each
only when higher. Lost: the garage's last car (the stake, put back at the end before the race) goes (`0x00489CD0`),
the crew's `+0x2D0` and the profile's `+0x4E0` cleared.

## CMSRespectMsg

State record `0x007227E8` (vtable `0x006F5EF8`): enter `0x004DC9E0`, update `0x004DCBD0`, exit `0x004DCB90`.

Enter: `comment` takes several lines; `logo` the crew's logo (the flyers, career `+0x00`), `leader` the leader's
picture (`0x007187B8`: `ui_crewmember_00`, `ui_leader_tk`, `_biggi`, `_shystie`, `_maria`, `_jack`, `_melody`,
`_poppa`, `_carlos` by the crew's number); `title` "RESPECT DECREASE" (534) for a change of 0 or less, else
"RESPECT INCREASE" (533); `comment` the record's; `+0x30` = 1.5, `+0x34` = 2.5, `+0x38` = 0; `meter` shows the respect
x 0.000667, at most 1, at once.

Update: SELECT or BACK once the change is done, or `+0x34` below 0: sound CLICK, the window taken off (mode 6).
While `+0x30` is at least 0 it falls by the frame time. Then, with the change done: below 0.75 of `+0x34` the
window's transitions go out (once), the respect = the target, `+0x34` falls by the frame time. Otherwise each frame
(sound SWOOSH) the change moves to 0 by 0.5 while it is within 50, else by 10, the respect by as much the other way;
within 0.6 of 0 it is 0. Each frame: `meter` the respect x 0.000667 (at most 1), `change` the change cut ("+" before
a gain), `value` the respect (at least 0) cut, with its thousands marked.

## CMSRespectChanges

State record `0x00722308` (vtable `0x006F5A28`): enter `0x004CF5A0`, update `0x004D00F0`, exit `0x004D0000`. It
shows the career's `+0x3C` list (`08-career.md`): the crews' respect changes of the event's end, and the privileges
gained and lost (kinds 20 .. 27). The crews' respect (`+0x70`) changes here, entry by entry; leaving makes it the
respect now (`+0x74`) for crews 1 .. 8.

Enter: `fade1` .. `8` hidden; each crew's components `comp<crew>1` and `comp<crew>2` hidden, their places kept, their
changes 0 and their start -(column x 0.1 + crew x 0.2) s; the point they fly from is the centre of `Respect_Icon` less
half of `comp11`'s size; the icon blinks for 2 s (`+0x270`); `counter` the race's respect (the career's `+0x34`
object's `+0x30`: "+n" green 0xFF32FF32 above 0, else "n" red 0xFFFF3232). Each entry of kinds 0 .. 19 gives its
crew's component (the second for kind 13, the rams', else the first) its change ("+n" green, a loss red; a later entry
of the same component replaces it). Cash and Date (`0x004A4130`); the music's volume x 0.25. `meter1` .. `8` go to the
crews' respect / 1500 (at most 1) and `val1` .. `8` show it with its thousands marked. The first entry is taken (its
crew's respect shown from the respect, which becomes the respect plus the change, none below 0); `comment` and `type`
hidden, `counter` shown; the count waits 2 s (`+0x28`), the text 1 s (`+0x30`); PGDN allowed; `SEL2_` shown.

Update (by the game's frame time; `[0x007A2D48]` runs over 0 .. 0.3 s): PGDN while allowed: CLICK, `SEL2_` hidden,
every entry passed (`0x004D0D40`), every component hidden and arrived. While the icon blinks (shown after 0.2 s of
each 0.3) SELECT ends it with every component arrived and the first entry shown at once. Then each component, once
its start has passed, flies from the icon's point to its place over a second (shown until the window is done); as
one starts its change is taken off `counter` (SWOOSH; after crew 8's second component `counter` is 0). Once all have
arrived `counter` and the icon are hidden; a finished window empties both lists and goes on (`0x004BAEE0`); else after
the text's second the entry is shown (`comment`, `type` and `counter` shown, `0x004D1050`); then SELECT (CLICK) takes
the next entry, or after the last finishes the window (with the save system's autosave over it when it is on); and
the entry is counted (`0x004D06D0`) until every entry has passed.

An entry shown (`0x004D1050`): kinds 0 .. 9 "RESPECT INCREASE" (533) green, 10 .. 19 "RESPECT DECREASE" (534) red,
with the crew's fade hidden and the others shown, `comment` the crew's name (108 + crew) and what it thinks (277 +
kind, 287 + kind - 10), the crew's component highlighted (the second for kind 13); kinds 20 .. 23 "GAINED PRIVILEGE"
(535) green and 24 .. 27 "LOST PRIVILEGE" (536) red by the crew of the entry's first number, with lines 269 + kind -
20 and 273 + kind - 24. The crew leader speaks (the speech object's line 0x1E for a rise, 0x1F for a fall, 0x20 for
kind 13, kind + 2 for a privilege; none for kinds 4 and 14 of a crew past 1).

The next entry (`0x004D0D40`): `counter` and the highlighted component emptied; an entry of kinds 0 .. 19 leaves its
crew's `val` and `meter` (at once) at the respect. After the last entry: the speech stopped, the fades hidden, the
privileges' titles (`attendtitle`, `racetitle`, `illegaltitle`, `hosttitle`) and numbers (`num1` .. `4`) shown,
`comment`, `Respect_Icon` and `type` hidden, the window done. Else the entry is taken, shown after 0.75 s (`+0x2C`)
and counted after 0.5 s (1.5 s for kinds 20 .. 27).

Counting (`0x004D06D0`; `[0x0079E85C]` runs over 0 .. 0.25 s): `type` shown, blinking (hidden after 0.1 s) while the
entry waits to be shown, the privileges shown. Then, of kinds 0 .. 19, once the count's delay is over 0.06 of the
change left moves into the respect shown each frame (SWOOSH while more than 1 is left); before, the target is the
respect plus the change; with less than 1 left the respect is the target (none below 0; a fall's respect none below
0). `counter` and the highlighted component show the change left; the crew's `val` the respect (cut) and its `meter`
(jumping on the entry's first frame). Of kinds 20 .. 27 the privilege's title and number (by kind - 20 mod 4) blink,
the others hidden; else all are shown.

Exit (`0x004D0000`): both lists emptied, the music's volume back, crews 1 .. 8's respect their respect now, the
history's step (`0x004BD8A0`).

## CMSMeetingSummary

State record `0x007222B0` (vtable `0x006F5A38`): enter `0x004CEAE0`, update `0x004CECD0`, exit `0x004CEC30`. It shows
the career's `+0x38` list (`08-career.md`) entry by entry.

Enter: `type` and `comment` take several lines; `+0x20` 2; the first entry shown (`0x004CEE50`); Cash and Date
(`0x004A4130`); the music's volume x 0.25; not finished (`+0x50`).

Update: finished, the window goes on (`0x004BAEE0`). SELECT (CLICK): the next entry (`+0x20` 1.5) is shown; after the
last the list is emptied (`0x004B5CC0`), the speech stopped, and `CMSRespectChanges` follows while the `+0x3C` list has
entries; else the window is finished (with the save system's autosave over it when it is on).

An entry shown (`0x004CEE50`): `logo`, `leader`, `leaderBorder`, `carLogo`, `crewPic`, `crewBorder`, `sponsorName` and
`sponsorType` hidden, then by the kind:

| Kind | Shown | type | comment |
| --- | --- | --- | --- |
| 0x1C | `sponsorName` `UI_<supplier>_ON` (the supplier table `0x00717970` by the entry's first number), `sponsorType` the part kind's icon (1 `ui_backbox_on`, 2 nitro, 3 turbo, 4 wheel, 5 brake, 6 tyre, 7 ice, 8 airfilter, 9 gears, else suspension) | "WON PROTOTYPE PART" (537) | the make (193 + make) and model (211 + id) of the entry's third number |
| 0x1D | `carLogo` the make's logo (`0x0048CF20`: `ui_Honda` .. `ui_Acura`, `ui_None`) | "UNLOCKED NEW MODS" (1788) | "Unlocked mods for " (1789), the make and model of the first number |
| 0x1E | `logo` the crew's logo (the career's head object), `leader` the leader's picture, `leaderBorder` | "WON CREW CHALLENGE" (538) | "MINIMUM RESPECT VALUE +50" (539) |
| 0x1F | `crewPic` `ui_crewmember_<number>` of the crewmate of the first number, `crewBorder` | "DRIVER SKILLS IMPROVED" (540) | its short name and "'s driving skills have increased." (307) |
| 0x20 | the same for the crewmate of the member slot of the first number | "DRIVER SKILLS WORSE" (541) | its short name and "'s driving skills have decreased." (310) |

The make of a car model id comes from the executable's table (`0x0048CCD0`).

Exit: the list emptied, the music's volume back, the history's step (`0x004BD8A0`).

## CMSMainMenu

State record `0x0072229C` (vtable `0x006F59C4`): enter `0x004CE160`, update `0x004CE1F0`. The career's hub.

Enter: a career without cars and with less than 30000 gets the rest of 30000 (`0x004CE870`: the money counter
`+0x04`, the money none below 0) and one of four notes (lines 1955 + `rand() & 3`); when every garage car needs a
repair (its grid damaged, its `+0x74` or `+0x7C` over 0.1, or its nitro not full with a nitro part) costing at least
the money (`0x004CE910`, a car needing none ends it), one of four notes (lines 1960 + `rand() & 3`) and every car is
repaired (`0x0048EB20`); Cash and Date; `+0x0C` 0, `+0x10` -1; the dealer's `+0x10` 0; the item `Workshop` of
`menu` enabled (the item's `+0x00`) with cars in the garage.

Update: `BackMenu` follows `menu`; Cash and Date; the phone's frame (`0x004B60F0`). The career's `+0x78` object asks
for windows: `+0x04` `CMSMobilePhone` over this one; `+0x10` the dealership (the profile's `+0x4E4` the money, the
dealer's `+0x00` 1, `+0x08`, `+0x27D4` and `+0x27DC` 0, its offers `0x004B4620`, `+0x27D8` its `+0x27E8`,
`CMSCarDealership`); `+0x0C` or `+0x08` `CMSCalendar` (`+0x0C` cleared). Then advice when it is due (`0x004A57B0`)
with the profile's `+0x4E` over 3: `0x004A57E0` and `CMSAdvice` over this one. Leaving (`+0x0C`) goes back to `menu`
(mode 2); a "yes" (`+0x10` 1) asks for `autosave` over the window when the save system saves (its `+0x08`, method
`+0x24`: set once a driver is made or loaded) and leaves. BACK (CLICK) asks "Save and exit career mode?" (1544, with
saving) or "Leave career mode?" (1607). SELECT (CLICK) by the item: `GoRace` `CMSCalendar`, `Advice` (`0x004A57E0`)
`CMSAdvice` over this one, `Dealership` the dealership as above, `Workshop` `CMSCarCollection2` (the profile's `+0x4E4`
the money, `0x004B43C0` on the dealer), `Respect` `CMSRespectOverview`, `Crew` `CMSCrew`, `Phone` `CMSMobilePhone`
over this one, `Stats` `CMSStatsMenu`. The phone's frame (`0x004B60F0`) each frame.

A car's repair (`0x0048EBC0`) costs its worth (`0x0048D430`) x its damage level (`0x00557E80`: the mean of the
damaged cells' kept and race damage as sizes x 0.05, within 0 .. 1) x 0.1, plus (each cut toward zero) with `+0x7C`
over 0.1 the brake part's COST (at most 5000) x 0.3 (100 when 0), with `+0x74` over 0.1 the tyre part's (200 when 0),
and with the nitro not 15 the nitro part's COST (at most 15000) x 0.1 (0 without a nitro part); a cost over 0 at
least 500, at most 25000. The repair (`0x0048EB20`) clears the grid, fills
the nitro (15) and clears `+0x7C`, `+0x74`, `+0x64`, `+0x70` and `+0x6C` (with `+0x6C` set also the neon part).

## CMSAdvice

State record (vtable in the windows' table): enter `0x004BFB60`, update `0x004C0110`; opened over `CMSMainMenu`.

Enter: by the advisor's advice (the career's `+0x78` object's `+0x1C`; none past 12) `Advice`, `Topic` and `Tip` take
the advice's lines (1679, 1694 and 1709 + the advice; for 12 the lines 1692, 1707 and 1722), each over several
lines. The advice's crew (advices 0 .. 7 crews 1 .. 8, 9 the advisor's `+0x68` unless -1, 12 the player's crew; none
for 8, 10 and 11) shows `Logo` (its logo of the flyers), `Leader` (its leader's picture) and `LEADER_BORDER`, else
they are hidden. The `No` button is highlighted.

Update: SELECT (CLICK) on `No` drops the advice (`+0x1C` 13, its event none); on the other button the advice asks
`CMSMainMenu` for its window: advices 7 and 9 the phone (`+0x04`), 12 the calendar (`+0x08`), 11 the dealership
(`+0x10`), any other the calendar (`+0x0C`). The window then goes (mode 6).

## CMSCalendar

State record `0x00721CF8`: enter `0x004C11D0`, update `0x004C1320`. Its fields: `+0x0C` the first day shown (a day
count, kept while the game runs), `+0x10` the highlight may scroll, `+0x14` a week on, `+0x18` the answer to the
question, `+0x1C` the advice's button, `+0x20` `SELECT`, `+0x24`, `+0x28` its place, `+0x2C`, `+0x30` the place it
goes to. Globals: the selected day and month (`0x00728C5C`, `0x00728C58`), hosting (`0x00728C54`), the event chosen
(`0x00728C50`, also the profile's `+0x610`), the last highlighted button (`0x0079E850`), `SELECT`'s pulse
(`0x00725600`, 1 at start) and the advice's blink (`0x0079E84C`).

Enter: the calendar topped up (`0x004A2670`); `+0x18` -1, `+0x1C` and `+0x14` 0; the profile's `+0x4E0` 1; the first
day the week of today (`+0x08` - `+0x08` % 7) when `+0x0C` is 0 or the career is new (the calendar's `+0xF88`, then
cleared); `+0x10` 1; the weeks built with today highlighted. When the career is complete (`0x004BCA70`) and the
profile's `+0x08` is 0: `+0x08` 1, the note "Congratulations! ..." (1939), every car model's parts state (the profile's
`+0x50 .. +0x261`) 3 and the arcade championships unlocked (`0x0042B650`). `SELECT` shown at the highlighted button.

The weeks (`0x004C1A20`, with or without today highlighted): the selection and hosting cleared; `FLYER` and `CD_`
(by part of the name) and `CrewInfoBG` hidden; with a day highlighted `EnterEventText` hidden; `HostEventText` hidden;
`Sel_but` hidden when a day is highlighted (a name whose fourth character is 0 .. 7); Cash and Date; `EventFee` and
`EventPlayerFee` emptied. The buttons `D<column>-<row>` (rows 0 .. 4) take the days from `+0x0C`:

| Day | Font | Box |
| --- | --- | --- |
| any | `0xFAFFFFFF` (odd months) or `0xFA9BD0E2` | `0x329BD0E2` |
| past | `0xFA373737` | `0xFA000000` (past the first row: `+0x14` set) |
| an event attended (`+0x56`) | | `0x9BFFFFFF` |
| an event the player may not enter | | `0x9B7D0000` |
| an event with reason 0, 1, 7 or 4 | | `0x9BD2D200` |
| another event | | `0x9B32FF32` |
| today | | `0x9B00B4FF` |

The advice's event's button is kept in `+0x1C`; with the advice just given (the advisor's `+0x00`) it is highlighted;
`+0x00` is cleared. The highlighted day: with an event, the selection, `FLYER` shown, the flyer (`0x004B2E20`), the
crew's details (`0x004C2570`) and, when the player may enter, `EnterEventText` and `Sel_but`; without one, when no
event of the player's crew waits, no special event waits and the day is after today, hosting: `HostEventText`,
`Sel_but` and `FLYERNOENTRYTEXT2` ("HOST YOUR OWN CUSTOM EVENT ON THIS DAY.", 1918). `Month_Year`: the highlighted
day's month (128 + month % 12), "YEAR" (613) and month / 12 + 1; emptied when no day is highlighted.

Update, by the game's frame time: `SELECT`'s alpha |p| (p - 4 a second, from -1 back to 1), its place eased by 0.21
a frame to the highlighted button's; the phone's frame (`0x004B60F0`). The advisor: `+0x04` `CMSMobilePhone` over
this one; `+0x08` (cleared, `+0x00` cleared) the advice's day (`0x004C2490`), the calendar's `+0xF8C`, `+0xF90` the
selected day and `CMSHostMeetingSetup` (its `+0x38` 1). A challenge's advice (`0x004A6190`) sets `+0x14` and, with the
profile's `+0x4E` over 3, the advice (`0x004A57E0`) and `CMSAdvice` over this one; none clears `+0x14`. BACK:
`CMSMainMenu`. A new highlighted button builds the weeks again; `SELECT` goes to it, at once when the highlight
wrapped between columns 0 and 6. Scrolling (`+0x10` cleared on rows 1 and 3, by UP before today's week, by DOWN 25
days after it): DOWN on row 4 a week on, UP on row 0 a week back (not before 0), each with `+0x10` set (CLICK), then
`+0x10` set. PGDN: CLICK and the window's help (`0x004850E0`). `+0x14`: a week on (CLICK) with today highlighted.
SELECT3 (CLICK): today's waiting special event stops it ("You are trying to skip past a special event in the
calendar.", 603); else the next day (`0x004A6530`), the advice dropped (13, no event) and the weeks built again.
SELECT on a selected event the player may enter: a special event before it gives the same note, else "GO TO
EVENT?" (1921) into `+0x18`; hosting: the calendar's `+0xF8C`, `+0xF90` and `CMSHostMeetingSetup`. A "yes": the
event chosen, the meeting (`0x004C27A0`), CLICK and `CMSAtMeeting`. The advice just given brings its day in
(`0x004C2490`) and the weeks are built again. The advice's button blinks: `0xFF0064FF` for 0.25 s, white for 0.25 s.

The advice's day (`0x004C2490`): for advice 12 the first free day (`0x004A6A10`), else the advice's event's day; the
first day moves on a week at a time until that day is within its first 14 days; for advice 12 it is selected.

The meeting (`0x004C27A0`): both message lists emptied (`0x004B5CC0`); days pass (`0x004A6530`) until the event's
day; the garage cars the player may race in it go to the profile's `+0x514` (count `+0x5F0`): every car when the
reason is 0, 1 or 7, else those of the event's region, make and model (when asked), with more than three prototype
parts for a prototype cars' event (`0x004A8F50`) and of its class (or any for class 8); sorted by rating, the greatest
first (a swap starts the pass again); `cmsAtMeeting`.

The crew's details (`0x004C2570`, none for crew 0): `CD_` and `CrewInfoBG` shown; `CD_Leader` the leader's picture,
`CD_Name` the leader's short name, `CD_Crew` the crew's name (108 + crew), `CD_Location` the location (575 + it) of the
route of its first circuit, `CD_Respects` what it respects most (297 + `0x004BC110`), `CD_respect_meter` respect /
1500 (at most 1).

The flyer (`0x004B2E20`): the flyers' `+0x50` takes the chosen logo (the settings' `+0x20`); `EventFee`, `FlyerClass`,
`FlyerBHP` shown and `EventPlayerFee`, `FlyerPlayerClass`, `FlyerPlayerLogo` hidden. `FlyerMain` by the crew:
`ui_flyer_UrbanMaulerz`, `AWB`, `Vixens`, `WildCats`, `OmegaTau`, `UrbanChaos`, `Lords`, `Legion`; else the player's
(`0x004B3F10`: `ui_flyer_player1 .. 3` by the chosen logo, the table 2 3 3 2 2 1 2 1 3 3 3 3 2 1 2 1 1 1 2 for logos
1 .. 19, else 1; `FlyerPlayerLogo` the player's crew logo; the fee, class and bhp swapped for the player's). Then:

| Event | Flyer |
| --- | --- |
| a prototype part (`+0x70`) | `ui_flyer_PartSponsor`; `FlyerMod` the part (`+0x74`: 1 backbox, 2 nitro, 3 turbo, 4, 7, 9 `ui_part_`, 5 brakes, 6 tyres, 8 airfilter, else suspension); `FlyerSponsor`; `FlyerSpecial` "SPONSORED EVENT" (1797), `FlyerSpecial2` "Win Prototype Mods!" (1794) |
| a pink slip (`+0x03`) | `ui_flyer_PinkSlip`; `FlyerProt` "PINK SLIP RACE" (1814) |
| a region (`+0x60`, a crew's) | `ui_flyer_CarSponsor`; `FlyerCarTerritory` the flag: 2 usa, 3 euro, 4 oz, else japan |
| a make (`+0x64`, a crew's) | `ui_flyer_CarSponsor`; `FlyerCarMake` the make's logo |
| a model (`+0x68`, a crew's) | `ui_flyer_CarSponsor`; `FlyerSpecial2` the make and model; `FlyerCarMake` the logo |
| prototype cars (`+0x04`) | `ui_flyer_Prototype`; `FlyerProt` "PROTOTYPE RACE" (1796), `FlyerSpecial2` "Prototype Cars Only" (1795) |

`FlyerNoEntry`, `FlyerNoEntryText` and `FlyerNoEntryText2` are hidden, then by the player's entry (`0x004B7D80`): one
that may not enter shows `FlyerNoEntry` and the reason's line (1880 + reason, `0xFFFF3232`), for reason 3 also the
crew's line (1519 + crew); one that may with reason 0, 1, 7 or 4 shows the reason's line in `0xFFECEC00` (reason 4
followed by the line of reason 0), for reason 1 also what car is needed (1966 a region, 1965 a make, else 1935 and the
class + 1). `FlyerRaceType` the type (147 + type); `FlyerBHP` the class's ratings (`<low>-<high> bhp`: class 1 700
.. 799 down to 6 200 .. 299, 7 0 .. 199, else 800 .. 1200; hidden for class 8); `FlyerClass` the class (604 + class;
hidden for a cruise or `+0x05`, shown for a crew's event) in the class's colour; `FlyerPlayerClass` the class in the
player's colour (white when black); `FlyerTeam` the team's line (491 + size, hidden for 1); `FlyerLaps` the laps and
length (174 and ` %d (%.1fkm)`, or miles x 0.625 with the settings' `+0x40`), only the length for types 3 and 5,
"Perform flashy moves and tricks!" (1819) for a cruise; `EventFee` and `EventPlayerFee` the fee or "Free" (165).

The texts' colour (`0x004B4060`): `0xFFD4D4D4` for `+0x03` or `+0x05`, white for `+0x70`, `0xFFFF3232` for `+0x04`,
white for a crew's region or make, the player's colour (the flyers' colour at the chosen logo) for crew 0, else the
crew's flyer colour. The class's colour (`0x004B4130`) has the same first cases, then by the crew: 0 the player's,
2 `0xFFEDA725`, 4 `0xFFCA2A2A`, 5 `0xFFD9D9D9`, 6 `0xFFF11F2A`, 7 `0xFFDFC070`, others white; none past 8.

Not followed: the arcade championships' unlock.

## CMSAtMeeting

State record (vtable `0x006F4F38`): enter `0x004C01E0`, update `0x004C06A0`, leave `0x004C0650`. Its fields: `+0x0C`
the route's map object, `+0x10` the event, `+0x14` the answer to leaving a special event, `+0x18` leaving, `+0x1C`
the answer to leaving, `+0x20` the answer to the fee.

The player's crew's text lines (108 "Player's crew" and 117 "PLAYER'S CREW") take the crew's name: when the settings
are read (the end of `0x00409D20`, the name made wide with signed characters), when a new driver's crew is named
(`0x00409F30`, as typed) and when this window opens (from the settings).

Enter: the crew's text lines; the career's money counters (`+0x58`) cleared (`0x004B5D60`); the profile's `+0x634` 0;
the advisor's last advice (`+0x18`) its advice when its event (`+0x20`) is this one, else 13, then no advice (13, no
event); Cash and Date; `+0x10` the event chosen (the profile's `+0x610`); `RaceMenu` emptied; `CrewLogo` the logo of
the flyers and `CD_Leader` the leader's picture, both by the event's crew's id; `Crew` the crew's name in capitals
(117 + crew); `Location` the location (575 + it) of the first circuit of the crew of the event's `+0x0D`; the details
(`0x004C0B60`); `+0x14 .. +0x20` 0; the crew's `+0x2D4` and `+0x2D8` 0; the map (below). `RaceMenu` takes, when the
player may race the event (`0x004B8060`) and has the fee or it is the player's own event, "ENTER CHALLENGE" (1923,
type 0), "ENTER SHOWOFF" (1922, type 2), "ENTER SPRINT" (1924, type 5) or "ENTER RACE" (1925), value 2; and for a
race or a sprint without `+0x02` and `+0x03` "WATCH SPRINT" (1927, type 5) or "WATCH RACE" (1928), value 3.
`RaceMenu` shown; the profile's `+0x630` 0; the flyer (`0x004B2E20`).

The details (`0x004C0B60`): every element named `extra1`, `laps`, `type`, `prize`, `sponsored`, `route`, `routetxt`,
`lapstitle`, `typetitle` or `prizetitle` hidden; `lapstitle`, `typetitle`, `prizetitle` shown; `Prize` " " and the
round's first prize (or "NA", 432); `route` "ROUTE:" (126); `routetxt` " " and by the round's type: circuit or
challenge the route slot + 1, or past 3 the slot - 3 and "Reversed" (127); a cruise "Showoff" (599); point to point
"Point-to-point" (601) and " Reversed" past slot 0; a sprint "Sprint" (600).

Update: the map turned (`0x0045BC40`). `+0x14` answered "yes": the crew's respect message of kind 2 with the event's
`+0x02` (`0x004BC7B0`), `+0x14` 0 and `+0x18` set. `+0x18`, or `+0x1C` answered "yes": the event's end
(`0x004A6870`), the history's step (`0x004BD8A0`), CLICK and `CMSCalendar`. `+0x20` answered "yes": the race entered
(below) and its set up (`0x004C1060`). BACK (CLICK): "Are you sure you want to leave the race event?" (1512; for an
event with `+0x05`, `+0x03` or `+0x02` "Leave event and lose respect?", 614) and "The event will be closed and you
will not be able to return." (1997) into `+0x1C` (`+0x14`). SELECT (CLICK) on the highlighted item: value 2, when the
player may race the event: with today's event's fee not 0 and another crew's event "Enter race event? Fee:" (1802)
and the fee into `+0x20`; else the race entered, `RaceMenu` hidden and its set up. Value 3: the session's `+0x264`
1, the profile's `+0x614` 0, `+0x618` 0, `+0x61C` 1, `CMSGridWalk`.

The race entered: the session's `+0x264` 1, the profile's `+0x614` 0, `+0x618` 1, `+0x61C` 0, the bets cleared
(`0x004B1620`: `+0x00`, `+0x04`, `+0x0C`, `+0x18`, `+0x5C .. +0x80`), the event attended (`0x004B7F00`: its `+0x56`
set; another crew's event the player may enter for a reason other than 0, 1 and 7 takes the fee, `0x004B8200`).

The set up (`0x004C1060`): today's event's crew with its `+0x2D0` over 0 lowers it by one (not below 0) when
`rand()` x 1/32767 is above 0.75; with more than one member, a round neither a cruise nor a challenge and no pink
slip `CMSCrewRaceSetup`; else the profile's `+0x5F4` 0, `+0x614` 1 and `CMSRaceSetup`.

The player may race an event (`0x004B8060`): the round's team size (`+0x17`) within the cars listed for the meeting
(the profile's `+0x5F0`) and the members (`+0x510`); the event's crew past its second step (`+0x58`), a cruise or
the event's `+0x05`; and a reason (`0x004B7D80`) other than 0, 1 and 7.

The crew's respect message (`0x004BC7B0`; crew, kind and a flag): the change by kind: 0 and 4 +20, 1 and 7 -25, 2
-35 (with the flag 980 less the crew's respect: the value before the months' days, `0x00718ED8`, less 20), 3 -30, 5
+10, 6 +30, 8 +50, past 8 none. The comment: kind 8 "CHALLENGE COMPLETED" (381); else the leader's name (not in
language 4) and the kind's line (437 + kind); then " Respect " (172), a "+" for a gain and the change cut. The crew,
its respect, the change, the change x 0.02, the sum and the comment go to `CMSRespectMsg`'s record (`0x007227F4` ..)
and `CMSRespectMsg` opens over the window; the crew's respect is then the sum, not below 0, cut.

Leave: a `Menu` element's highlighted item (its `+0x13C`) is kept in the state's `+0x04`; the map is freed.

The route's map (`0x0045B6B0`, the city map of `02-frontend.md`): the `MAP3D` view on place 0 at the pitch -0.4
(`0x0045BE00`), the first round's route (`+0x38`) added (`0x0045B720` with 1: its `UI_<route>_GUI` model and its
`centre`); a cruise then looks at the location's text (the language's) by the nine English names (`0x0045BEC0`; in
another language none matches: place 0, a stock problem). Each frame the map's frame (`0x0045BC40`).

## CMSRaceSetup

State record `0x00722640` (vtable `0x006F5DBC`): enter `0x004D69F0`, update `0x004D7840`, leave `0x004D77E0`. Its
fields: `+0x0C` the answer to leaving a special event, `+0x10` leaving, `+0x14` the answer to leaving, `+0x18` the
answer to the repair, `+0x1C` the car shown, `+0x20` the answer to the tutorial, `+0x24` `carmenu`, `+0x28` `bhp`,
`+0x2C` `drive`, `+0x30` the car's view, `+0x34` `FADEBOX`, `+0x38 .. +0x40` the team's cars (garage indices, -1
free), `+0x44` how many are taken. The round is the current event's current round.

Enter: `drive` and `bhp` emptied; `+0x0C .. +0x14` 0; the car menu (`0x004D7470`); `Driver1 .. 3` emptied, `Car1 .. 3`
hidden, emptied and over several lines; `back_text` "BACK" (1953) for a crew's race (more than one member, a round
neither a cruise nor a challenge, no pink slip), else "LEAVE EVENT" (1724); `Crewmate1 .. 3`, `Border1 .. 3`,
`CrewBG1 .. 3` hidden; for each driver of the team (the round's `+0x17`): `Driver<n>` the short name of the crewmate
of its member slot (the profile's `+0x5F4`), `Crewmate<n>` shown with `ui_crewmember_<number>`, `Border<n>` and
`CrewBG<n>` shown; `+0x44` 0, the cars -1 and `Car1Icon .. 3` hidden; `+0x1C` -1; the markers (`0x004D8990`); Cash and
Date; `+0x18`, `+0x20` -1; the car's view (`+0x30`, 0x50 bytes: `ORIGIN`, `LOGO`, `CAR3D`); `FADEBOX`. A cruise asks
once (the profile's `+0x634`, then set) "VIEW SHOWOFF TUTORIAL?" (1911) into `+0x20`.

The car menu (`0x004D7470`): `carmenu` emptied (its `+0x100` set); an item for each car listed for the meeting (the
profile's `+0x514`, count `+0x5F0`) with the garage index as its value: the model's display name, " (D)" when its grid
is damaged (`0x0048EF50`) or (`0x0048EF10`) its `+0x74` or `+0x7C` is over 0.1 or its nitro time is not 15 with a
nitro part, " (P)" with more than three prototype parts (`0x004A8F50`); then "GO" (29, value 999) disabled and the
cars' items enabled.

Update: the view turned (`0x00459EE0`), `FADEBOX`'s alpha 1 - the view's `+0x4C`; Cash and Date. `+0x0C` answered
"yes": the crew's respect message of kind 2 with the event's `+0x02` (`0x004BC7B0`), `+0x10` set. `+0x10`, or `+0x14`
answered "yes": the event's end (`0x004A6870`), the history's step, CLICK and `CMSCalendar`. BACK: for a crew's race
CLICK and `CMSCrewRaceSetup`; else CLICK and the question of `CMSAtMeeting` (1512 or 614, and 1997) into `+0x14` or
`+0x0C`. The highlighted car (`CarMenu`) other than `+0x1C` becomes `+0x1C` (to the nearest) and, when not GO, goes
into the view (`0x0045A080` with 2.64, 1, 1). With the view not busy (its `+0x40` and `+0x3C` 0) the stats
(`0x004D7660`) and `bhp` and `drive` shown, else both hidden. `+0x20` answered "yes": `CMSRaceSetup` to come back to
(`0x0072158C`) and `vid_tutorial`. `+0x18` answered "yes": the repair's cost (`0x0048EBC0`) paid (`0x004B8200`) and
noted (`0x0048F270`: the car's `+0xA8` and the profile's `+0x4E8`, cut), Cash and Date, the car repaired
(`0x0048EB20`), `+0x18` -1, its item the display name (and " (P)"), the car taken (`0x004D8410`) and, with the team
complete or every listed car taken, GO highlighted (keeping its place) and enabled.

SELECT on GO: `0x004D8810`. On a car (CLICK): one in the team is given back (`0x004D8270`) and GO disabled; a damaged
car (as above) asks "Repair this car for" (462), the cost and "ARE YOU SURE?" (86) into `+0x18` (then `+0x18` -1), or
with the cost over the money notes it and "INSUFFICIENT CASH" (1884); else the car is taken and, when it fills the
team or the listed cars, GO is highlighted and enabled. PGDN on a car with the view not busy (`0x0045A880`): CLICK,
`0x00721F30` 1, the car in `0x00728C60` and `car_stats2purple` over the window.

The stats (`0x004D7660`, not for GO): `bhp` and `drive` shown, `STATS` (by part of the name) shown, `bhp` the car's
rating (cut, thousands marked, `0x004A3AF0`), `drive` its drive (1830 + `0x0048CE20`).

Taking a car (`0x004D8410`): its items disabled; the first free slot of the team takes it (`+0x44` up), `Car<n>icon`
and `Car<n>` shown with its display name; with no free slot the last slot's car is given back (its items enabled) and
the car takes that slot. Giving a car back (`0x004D8270`): its slot -1 (`+0x44` down), `Car<n>icon` and `Car<n>`
hidden and emptied, its items enabled. Both end with the markers (`0x004D8990`): `selection1 .. 3` hidden, the first
free slot's shown.

GO (`0x004D8810`): the profile's `+0x60C` 0; without the team complete the sound "no". Else the team's cars go to the
profile's `+0x600` (count `+0x60C`); a round the player's crew is not in yet (`+0x0D`): a race or a sprint with
room (team size x crews under 6, a sprint 4) one crew more (`+0x16`) and the player's drivers added (`+0x18`), else
the first crew of 1 .. 8 racing gives its place; then `+0x0D` 1. One driver in the round (`+0x18` 1): the race set up
(`0x004B8570`), started (`0x004A3CC0`, `loading`) and the bet's share (`0x004BC2E0`); else `CMSGridWalk`.

Leave: a `Menu` element's highlighted item kept in the state's `+0x04`; the view freed (`0x0045AE90`, `0x0045A340`).

The car view (`+0x30`) is made in place as `0x0045A2C0` does (CAR3D, LOGO, ORIGIN, the zoom 0.7, the mirror image,
the folder kept and the shadow on; "The car view"). Not followed: `carmenu`'s `+0x100`, `vid_tutorial` and `car_stats2purple`.

## CMSGridWalk

State record `0x00722148` (vtable `0x006F55A8`): enter `0x004C7FA0`, update `0x004C91B0`, leave `0x004C9130`. Its
fields: `+0x0C .. +0x20` the racers' car views, `+0x24` the cars' list (`0x00489B80`), `+0x28 .. +0x3C` `SELECT1 ..
6`, `+0x40 .. +0x54` the bet elements blinking and `+0x58` their count, `+0x5C` the lines asked again, `+0x60` the
answer to quitting, `+0x64` the race to start. Globals: the loader taken away (`0x00728C68`), the blinking
(`0x0079E858`), the selection's pulse (`0x00725610`, 1 at start, 4 a second), `go`'s place as the window has it
(`0x007A2ED8`, kept once), the place `CMSBet` bets on (`0x007221BC`).

Enter: `0x00728C68`, `+0x58`, `+0x60` 0; the racers of the round (`0x004B8570`); the cars' list; `back_text` "LEAVE
EVENT" (1724). With one racer the race starts (`0x004A3CC0`) and the bet's share (`0x004BC2E0`) follows. `RaceInfo`
the round's type (140 + type), "   ", for a circuit "Laps" (174) and its laps and "   ", the team size's line (491 +
size) (`0x005B52C0` on it). `%dLogo` (1 .. 6) shown with the crew's logo of the flyers, past the racers every such
element hidden. For each place: `ps2_car%d` hidden; `%dInfo` over several lines, the member's short name, " - " and
the car's display name (emptied past the racers); `%dInfo2` as in the lines below; the racer's car in `Car3d%d` (a
car view per racer, the crew's cars made ready `0x004A9080`, the car `0x00434D90`, the view `0x0045A080` with 2.355,
0.5, 1, its place 110, -560); `Recovery%d`, `Ability%dt`, `Recover%d`, `Ability%d`, `BOX0%d`, `%d`: for the player
(crew id 0, member 0) the first four hidden; for a crewmate or another crew's member all six shown and the meters
`Recover%d` (a crewmate's recover, else the member's concentration, x 0.01) and `Ability%d` (driving ability or
ability x 0.01); past the racers all six hidden; `bph%d` the car's rating (cut, thousands marked) and "bhp" (484),
hidden past the racers. Cash and Date; the button `go` highlighted; `CMSLoader` over the window (mode 8); the lines
(`0x004C9DC0`); `SELECT1 .. 6` hidden; `bet` "BET WITH DRIVER" (1937) when betting with drivers (the profile's
`+0x614`), else "BET ON CAR" (1936); `Go` and `Bet` shown; with nothing blinking `go` at x 0.3125, `bet` hidden and
`go` highlighted, else `bet` highlighted and `go` at its place; `+0x64` 0.

The lines (`0x004C9DC0`): `+0x58` 0; for each place `%dInfo2`: for a crew's leader when betting on cars (the profile's
`+0x61C`, not `+0x614`) the crew's odds (`0x004B9C50`: against and stake cut, "%d:%d") with " ($<money>)" when money
is on them (`+0x0C`), else emptied; the leader of the crew bet with (the bets placed) shows the bet's amount.
`bet%d` hidden, then shown and added to the blinking: betting with drivers, no `+0x02` or `+0x03`, no bet asked (the
bets' `+0x80`), the leader of another crew not yet bet with (`+0x5C`) and no amount, when its bets (`0x004B1870`, made
here) have their middle amount (`+0x38`) under the money; and betting on cars for a leader with no money on its odds.

Update: with `+0x64` the race starts. The blinking: +the frame time, past 1 less 1; over 0.75 the blinking elements
hidden, else shown. The views turned (`0x00459EE0`); until every view has loaded (their `+0x40` and `+0x3C` 0)
nothing more; the first time the loader over the window goes (`0x00728C68` 1). Cash and Date. `+0x60` answered
"yes": in the first event (the profile's `+0x00`) `menu`; else the profile's `+0x630` 1, the racers again
(`0x004B8570`), the session's end (`0x004BA8A0`, `0x004B9E20`), the round's and the event's `+0x01` set, the bets
cleared, CLICK and `CMSCalendar`.

BACK: on another button (or none) `Go` and `Bet` shown, `back_text` "LEAVE EVENT", CLICK and the button `Bet`; on `Go`
or `Bet`: outside the first event (CLICK) "QUIT RACE AND FINISH LAST?" (89) betting with drivers, "QUIT RACE AND LOSE
BET?" (90) betting on cars with money on some crew's odds (`0x004B9CC0`), else 1512 and 1997; in the first event
"QUIT CAREER" (1) and "ARE YOU SURE?" (86); the answer into `+0x60`. PGDN: CLICK and the help (`0x004850E0`).

SELECT (CLICK): `Bet` hides every `Go` and `Bet`, `back_text` "BACK" and highlights the button `1`. `Go`: betting with
drivers in a pink slip race the first racer of another crew is the rival (the bets' `+0x18` 1, `+0x00` 1, `+0x08` the
crew, kind 1, step 13, its bets, `+0x5C`, no amount), `careerload` and the career's `+0xA4` 2 (the betting window).
Then betting with drivers with no bet placed or asked, `rand()` / 32767 under 0.333 and no `+0x02`: up to ten tries
of a racer (`rand() %` the racers) of another crew not yet bet with, past its second step, whose money twice is under
the player's: a bet asked by it (`0x004B1810` with 0: the bets' crew, step 3, kind 0 and `+0x80` set, or kind 1 and
step 13 for a pink slip; its bets, `+0x5C`, no amount), `careerload` and `+0xA4` 2; after ten tries the race starts
and the bet's share follows. Otherwise the bet's share (`0x004BC2E0`) and with the profile's `+0x62C` over 0 `+0x64`
set and `autosave` over the window when saving, else the race starts. A racer's button (its name the place; not in
the first event): betting on cars on a leader with no money on its odds `CMSBet` over the window (the place in
`0x007221BC`), else the sound "No"; betting with drivers, no `+0x02` or `+0x03`, no bet asked, the leader of another
crew not yet bet with and no amount: a bet with it (`0x004B1810` with 1: kind 2) and, its middle amount under the
money, `careerload` and `+0xA4` 2.

After the events: the `SELECT<n>` of the highlighted button's place shown with the pulse's |p|, the others hidden; the
pulse less 4 a second, below -1 back to 1; `+0x5C` set: cleared and the lines again.

Leave: a `Menu` element's highlighted item kept; the views and the cars' list freed.

The racers' views are made in place as `0x0045A2C0` does (`Car3d%d`, the zoom 0.9; "The car view"); the crew's
scheme (`0x004A9080`) goes on for every racer of a crew other than 0 before its car is copied. Not followed:
`0x005B52C0` and `autosave`.

## CMSBet

State record `0x007221B0` (vtable `0x006F55C8`): enter `0x004CA1A0`, update `0x004CA210`; opened over `CMSGridWalk`.
Its fields: `+0x0C` the racer's place (`0x007221BC`), `+0x10` the amount, `+0x14` the money left, `+0x18` the answer.

Enter: the amount the money already on the racer's crew's odds (`+0x0C`), the money left the player's, `+0x18` 0; the
texts (`0x004CA5F0`): `odds` "ODDS" (166) and the odds (`0x004B9C50`), `cash` "CASH" (155) and the money, `amount`
"BET" (1929) and the amount, `winnings` (when there is one) "POTENTIAL WIN" (1558) and $ the amount / the stake x
against (cut, thousands marked).

Update: while RIGHT or UP is held (the input's `+0x90`) the amount goes up, while LEFT or DOWN down, a step on a
repeat (`0x007A2EA4` the time left, `0x007A2EA0` the delay: 0.2 s while nothing is held, x 0.9 at each step). The
step by the amount: 1, from 10 on 10, from 100 on 100, from 1000 on 1000, from 25000 on 5000 (down: past each
instead of from). Up (CLICK) below 500000 with more money left than the step, else "NO"; down (CLICK) above 0, else
"NO". SELECT: "Place Bet?" (1998) and "ARE YOU SURE?" (86) into `+0x18`. A "yes": the amount on the crew's odds
(`+0x0C`), the money left minus the money paid or received as by `0x004B8200`, the career's money counter `+0x08` up
by the amount, CLICK, the window off (mode 6) and the grid walk's lines asked again (`0x007221A4`). BACK: CLICK, the
window off and the lines asked again. Then the texts.

## The help pages

The help (`0x004850E0`; a window and whether to show a page shown before) takes the table at `0x00715D48` (0x4C bytes
an entry: `+0x00` shown, `+0x04` the window, `+0x24` the note's text, `+0x28` the window to open instead, `+0x48` a
condition) in order: the first entry of the window (by name, case aside) not shown yet, or any when asked, whose
condition holds (with `+0x48` some crew of 1 .. 8 past its fourth step, `+0x60`) gives its note (type 1) or opens its
window over this one (mode 5) and is shown. The shown flags are UIHelp.sav (`0x00485210` writes, `0x00485240`
reads). `CMSCalendar` and `CMSGridWalk` ask for it on PGDN (shown or not).

| Entry | Window | Page |
| --- | --- | --- |
| 0 | `CMSCalendar` | the window `CMSCalendarHelp` |
| 1 | `CMSCarMenu` | 628 "GARAGE: .." |
| 2 | `CMSRespectOverview` | 629 "RESPECT STATUS: .." |
| 3 | `CMSCrew` | 630 "CREW SCREEN: .." |
| 4 | `CMSMobilePhone` | 631 "MOBILE PHONE: .." |
| 5 | `CMSGridwalk` | 632 "RACE GRID SCREEN: .." |
| 6 | `CMSRaceSetup` | 633 "CAR SELECTION: .." |
| 7 | `CMSCrewRaceSetup` | 634 "DRIVER SELECTION: .." |
| 8 | `CMSCalendar` (a crew past its fourth step) | 635 "HOST EVENTS: .." |
| 9 | `CMSAtMeeting` | 637 "RACE EVENT: .." |
| 10 | `instant_selcar` | 638 "SELECT CAR: .." |
| 11, 12 | `tuningmenu`, `stylingmenu` | 1549 "Remember to save some cash to enter races!" |

The arcade's car windows (`0x0043A8B0`, `0x0043B060`) look the pages up by the same lines themselves (not shown before)
while `[[0x0075ECF8] + 4] + 0x0C` is under 10000.

## CMSCrewRaceSetup

State record `0x00722688` (vtable `0x006F5DCC`): enter `0x004D8AC0`, update `0x004D90F0`. Its fields: `+0x0C`,
`+0x10`, `+0x14` the leaving answers as in `CMSRaceSetup`, `+0x18 .. +0x20` the member slots chosen (-1 free), `+0x24`
how many.

Enter: `+0x0C .. +0x14` 0; Cash and Date. With one member, a cruise or a challenge: the profile's `+0x5F4` 0, `+0x614`
1 and `CMSRaceSetup`. Else `back_text` "LEAVE EVENT" (1724); `+0x24` 0; the buttons `driver1value .. 4` and the
elements `selection`, `border`, `crewpic`, `driver` 1 .. 4 hidden; for each member slot of the crew (the profile's
`+0x500`) `driver<n>value` its crewmate's short name and shown, `border<n>` shown, `crewpic<n>` its
`ui_crewmember_<number>` and shown; the slots -1; `Type` the team size's line (491 + size); `Choose` "SELECT
DRIVERS: " (1790) and the drivers left; every `0`, `2`, `3`, `4` hidden and `1` shown; the profile's `+0x614` 0; the
button `driver1value` highlighted.

Update: every `0 .. 4` hidden, then the one of the highlighted button's seventh character (1 .. 4, else `0`) shown.
`+0x0C` answered "yes": the crew's respect message of kind 2 with the event's `+0x02`, `+0x10` set; `+0x10`, or
`+0x14` answered "yes": the event's end, the history's step, CLICK and `CMSCalendar`. BACK (CLICK): the question of
`CMSAtMeeting` into `+0x14` (or `+0x0C` for a special event). The button `finished` hidden unless the team is
complete. SELECT on `finished` (`0x004D9640`): the team complete (CLICK): the slots into the profile's `+0x5F4` (a
member 0 sets `+0x614`) and `CMSRaceSetup`; else "NO". SELECT on a driver (its button's seventh character - 1): a
member chosen is given back (`0x004D9790`: CLICK, its slot -1, `+0x24` down, `selection<n>` hidden); else with the
team complete "NO", otherwise it takes the first free slot (`0x004D9860`: CLICK, `+0x24` up, `selection<n>` shown).
A complete team then shows and highlights `Finished`; `Choose` the drivers left.

## CMSCalendarHelp

State record `0x00721D2C` (vtable `0x006F5008`): enter `0x004C2980`, update `0x004C2A60`; opened over `CMSCalendar`
by its help page. Enter: `Back_Text` "OK" (161); `OPENEVENTS`, `ATTENDEVENTS`, `CLOSEDEVENTS`, `SPECIALEVENTS`,
`PAYDAY` and `FLYERTEXT` over several lines. Update: SELECT (CLICK) takes the window off (mode 6); BACK does nothing.

## CMSChallengeResults

State record `0x007226D0` (vtable `0x006F5E24`): enter `0x004D9960`, update `0x004D9FB0`; after a challenge's race.
Its fields: `+0x0C` the prize left to count, `+0x10` the step, `+0x14` the money after, `+0x18` the money shown,
`+0x1C` the delay, `+0x20` finished, `+0x24` the autosave to ask for.

Enter: the current event's crew's challenge (`+0x2DC` + 0x1C x the event's `+0x06`). `+0x0C` 0, `+0x18` the money;
`raceName` the crew's name (108 + crew), ": " and the challenge's type (372 + type), over several lines. By the type:
lap time and perfect lap `criteria` "Time" (177), `criteria1` the result (`+0x18` as a time, `0x0044FBD0`; "-" for 0
or less) and `required1` the value as a time; speed `criteria` "Speed" (178), `criteria1` empty, `required1` the value
and " MPH" (1494) with the settings' `+0x40`, else the value x 1.6093 (cut) and " KPH" (1495); cruise `criteria`
"Score" (176), `criteria1` the race's money (the career's `+0x60` object's `+0x30`) and `required1` the value, both
cut with thousands marked. Met (`0x004BACB0`) and not done: `winnings1` the prize (`+0x0C`), `+0x0C` the prize and the
challenge done (`+0x14`); else `winnings1` $0. `outcome` "CHALLENGE COMPLETED" (381) or "CHALLENGE FAILED" (382). The
prize paid in (`0x004B8200`'s lines); the step 0.05 of it, 3 s before the count; each race car's `+0xAC` and (for
each car) the profile's `+0x4EC` (cut) take the prize; `+0x14` the money shown plus the prize; `RACENAME` over
several lines; `+0x20`, `+0x24` 0.

Update: `+0x24`: the autosave over the window when saving, `+0x24` 0, `+0x20` set. `totalWinnings` "TOTAL WINNINGS"
and the prize left, `cash` the money shown; after the delay (the front end's frame time) each frame the step moves
(SWOOSH) until none is left, then the money after. `+0x20`: the event's end (`0x004A6870`), CLICK, the current
round's and the event's `+0x01` and the next window (`0x004BAEE0`). SELECT: a challenge met gives the crew's respect
message of kind 8 (`0x004BC7B0`, +50) and sets `+0x24`; else `+0x20`.

## CMSCrew

State record `0x00722094` (vtable `0x006F545C`): enter `0x004C5F60`, update `0x004C63D0`. Its fields: `+0x0C` the
highlighted member's crewmate, `+0x10` -1. The last highlighted button is `0x007A2D4C`.

Enter: the button `1` highlighted; `+0x0C` the crewmate of the button's place (the profile's `+0x500` + 4 x (place -
1)), none past the members; the stats (`0x004C64C0`); `+0x10` -1; the list (`0x004C6070`: Cash and Date; the buttons
`1 .. 4` "-" and `crewpic1 .. 4` hidden; for each member its button the crewmate's short name over several lines and
`crewpic<n>` shown with `ui_crewmember_<number>`; every `Selection2`, `Selection3`, `Selection4` hidden). With more
than one member every element with `MSG_` in its name is hidden, else `MSG_TEXT1 .. 3` go over several lines.

The stats (`0x004C64C0`): every element with `SELECTION` in its name hidden and `selection<n>` of the highlighted
button shown; `Skill1 .. 4Meter` and `skill1 .. 4` hidden. Without a crewmate `skill1 .. 3Meter` 0 and `RacesValue`,
`WinsValue`, `TimeValue`, `Per_WinsValue` "-". Else `Skill1Meter` the driving ability (`+0x94`) and `Skill3Meter`
the recover (`+0x9C`) x 0.01, with `skill1` / `skill3` shown (crewmate 0: the meters 0 and hidden); `RacesValue` the
races (`+0xB0`), `WinsValue` the wins (`+0xB4`), `Per_WinsValue` wins / races x 100 as "%.1f%" (0 without races),
`TimeValue` the calendar's month less the month it joined (`+0xB8`) and "Months" (1821).

Update: the phone's frame (`0x004B60F0`); BACK (CLICK) `CMSMainMenu`; a new highlighted button takes its crewmate and
the stats again.

## CMSStatsMenu

State record `0x0072298C` (vtable `0x006F6308`): enter `0x004E0700`, update `0x004E0760`. Enter: Cash and Date;
`menu`'s first item of value 1 (the car collection) enabled with cars in the garage. Update: `MenuBG` takes `menu`'s
highlighted place; BACK (CLICK) `CMSMainMenu`; SELECT (CLICK) by the highlighted item's value: 0 `CMSStats`, 1
`CMSStatsCarCollection`, 2 `CMSRespect`.

## CMSRespectOverview

State record `0x00722770` (vtable `0x006F5F0C`): enter `0x004DC260`, update `0x004DC7A0`, leave `0x004DC760`. Its
`+0x18` keeps, per crew 1 .. 8 and challenge, whether the player has a car for it (`0x004BC1D0`).

Enter: `help_text` "HELP" (1623); for each crew 1 .. 8 `meter<n>` its respect x 1/1500 (at most 1) and `val<n>` the
respect (thousands marked); for each challenge the car's test kept and `chall<n>-<k>` `ui_yes` when done, else
`ui_no`; `progress` the career's completion (`0x004BCA70`) and `progressval` its percent ("%.0f%% ") and "Completed"
(338).

Update: the blinking (`0x0079E864`: the game's frame time, past 1 back to 0); the phone's frame (`0x004B60F0`); BACK
(CLICK) `CMSMainMenu`; PGDN (CLICK) `CMSRespectHelp`. A crew with its `+0x2D8` blinks `meter<n>` and each challenge
the player has a car for blinks `chall<n>-<k>` (hidden over 0.75, shown otherwise). Leave: a `Menu` element's
highlighted item kept; `0x00722768` 0.

## CMSRespectHelp

State record `0x00722830` (vtable `0x006F5F2C`): enter `0x004DCEF0`, update `0x004DD290`. Enter: for crews 1 .. 8
`RespectTxt<n>` what the crew respects most (297 + `0x004BC110`); `RESPECTTXT1 .. 8` over several lines; `HELP` over
several lines with "Crew Challenges. .." (1969); `LEADERTXT3` "SUE YEN" (a constant). Update: BACK (CLICK)
`CMSRespectOverview`.

## The phone's windows

The phone (`08-career.md`, "The phone") runs its frame (`0x004B60F0`) in `CMSMainMenu` (after Cash and Date),
`CMSCalendar` (after SELECT's pulse), `CMSCrew` (first) and `CMSRespectOverview` (after the blinking). The globals
`[0x00728C8C]` (Mobile), `[0x00728C94]` (PIC_PICTURE) and `[0x00728C90]` (PIC_BORDER) are the elements of
`CMSMobilePhone`, which the windows over it change.

`CMSMobilePhone` (record `0x00722588`, enter `0x004D17A0`, update `0x004D1C10`, leave `0x004D1AD0`). Enter: the
sample stopped, Mobile the phone's model (`+0x08`: 1 `ui_moto_02`, 2 `ui_sharp_01`, 3 `ui_sharp_02`, 4 `ui_sony_02`,
5 `ui_sony`, else `ui_moto`), PIC_PICTURE the caller (`0x004D23B0`: kind 0 the crew's leader's picture by the crew's
id, `0x007187B8`; kind 1 `ui_crewmember_<nn>`; kind 2 `ui_MrJeff`), the phone's `+0x0C` 0, Message (over several
lines) and Caller emptied. An idle phone (the player opened it) becomes 1, Caller "CONTACTS" (314),
`CMSMobilePhoneContacts` over it and SEL2 shown; else SEL2 hidden. Reject hidden, Answer shown (hidden for the opened
phone). The music at a quarter of its volume; the ring's time (`[0x0075EF78]`) 10.

Update: BACK_ hidden, then by the phone's state: 0 the window goes (mode 6); 1 Answer and Reject hidden, BACK_ shown,
BACK: the window goes and the phone idles (`0x004B69B0`); 2 SEL2 hidden, the same, then the call (`0x004D2140`); 3 SEL2
hidden and the ringing (`0x004D1F10`); 4 SEL2 hidden, message the call's text (`0x004B65E0`) and
`CMSMobilePhoneMenu` over it.

The ringing (`0x004D1F10`): RingIn each 2 s; caller "NEW MESSAGE" (336) for a text message (type 10), else the
caller's name. SELECT on `answer`: the ring stopped, Answer and Reject hidden, the phone talking (4), message the
call's text, caller the name for a text message, `CMSMobilePhoneMenu` over it.

The call (`0x004D2140`): caller "RINGING" (1638) while the blink (`[0x0079E860]`, the frame time, past 1 less 1) is
over 0.2, else empty; RingOut each 2 s. With the phone's `+0xF94` below 0: caller emptied, talking (4), message the
call's text, BACK_ hidden, `CMSMobilePhoneMenu` over it, the ring stopped. Else, unless `+0xF98`, `+0xF94` runs down
by the frame time and SELECT sets it to -1.

Leave: the sample stopped, a `Menu` element's highlighted item kept, the ring stopped, the music back; the phone's
event (`0x004B75D0`: with `+0x0C` the event on the day `+0x14` of the month `+0x10` becomes the event chosen
(`[0x00728C50]`, the profile's `+0x610`; `[0x00728C58]`, `[0x00728C5C]` its month and day), both note lists emptied,
the days passed to it and `0x004C27A0`); an advice to call (`+0x1C` 9 or 7): the advisor's `+0x18` takes it, `+0x1C`
13, `+0x20` 0.

`CMSMobilePhoneMenu` (record `0x00722598`, enter `0x004D2460`, update `0x004D27C0`, leave `0x004D2740`). Enter: Menu
emptied, the call's answers (`+0x824 ..`) as items, their place the value; data1 .. 4 (over several lines) and meter1
.. 4 hidden. Update: a host challenge call (type 2) the first time (`+0xFA4`) notes the host rights (1636) and ends
the frame. SELECT on an item: the answer (`0x004B6B20`), CLICK, the window goes. Else the highlighted item: with the
crew of the challenges (`+0xF9C`) and the item below 3 (below 1 with the advice to take a challenge, 9; the item then
is the advised challenge `+0x6C`): PIC_PICTURE and PIC_BORDER hidden, data1 the challenge's kind (372 + type) and "
(Completed)" (338) when done, data2 its car's name, data3 its target: a cruise "Score <n>", speed "<n> MPH" or, by the
settings' units (`+0x94`), "<n x 1.6093> KPH", else "Time <time>" (`0x0044FBD0`). With the crewmate (`+0xFA0`): the
pictures hidden, data1 "Skill" (1767) with meter1 its driving x 0.01, data3 "Composure" (1769) with meter3 its
recovery x 0.01. Else all eight hidden (`0x004D2700`). Leave: the highlighted item kept, the sample stopped.

`CMSMobilePhoneContacts` (record `0x007225C8`, enter `0x004D2CB0`, update `0x004D2D90`). Enter: Menu emptied, the
contacts' names as items, their place the value. Update: a new phone model (`[0x00725618]`) shown on Mobile. An advice
to take a challenge (9) or to race for pinks (7) calls the contact of the advised crew (`+0x68`, `+0x70`; past the
contacts when there is none) at once (`0x004B6710`), the window goes, Dial, the ring's time -1. PIC_PICTURE the
highlighted (or called) contact's crew's leader (crews 0 .. 8). SELECT calls the highlighted contact the same way
(CLICK). With the phone opened (1), PGDN (CLICK) opens `CMSMobileChoose` over it; else BACK idles the phone (5 .. 15
s), CLICK, the window goes.

`CMSMobileChoose` (record `0x00721DC4`, enter `0x004C3DB0`, update `0x004C3DD0`). Enter: `+0x0C`, `+0x10` -1. Update:
a new model highlighted in MOBILENAMES (`+0x0C`, the value cut) shows the pictures `0` .. `3` and hides the model's. A
"yes" (`+0x10`) pays the model's price (`0x00718F70`: 100, 100, 150, 150, 200, 200), the model becomes the phone's
(`+0x08`) and the window goes. BACK (CLICK): an idle phone (a new career) `cmschooselogo`, else the window goes.
SELECT: a new career keeps the model and asks the crew's name (`02-frontend.md`); else CLICK, the model in use closes
the window, with the price in money "Buy this phone for " (1824) and the price (type 3, on NO) into `+0x10`, else
"Cannot buy this phone. Insufficient cash." (1825).

## CMSStats

Record `0x00722954` (vtable `0x006F6318`): enter `0x004DF750`, update `0x004DFC10`, draw `0x004DFD50`, leave
`0x004DFBC0`. Its fields: `+0x0C` the highlighted button, `+0x10` a graph object, `+0x14 .. +0x34` the series shown
(0 the money, 1 .. 8 the crews' respect), kept from one visit to the next.

Enter: the first time (`[0x0079E868]`) the series cleared and series 1 (crew 1) shown; the elements whose name holds
SELECTION or TICK hidden; `+0x0C` 0; a graph object (0x78 bytes, `0x004F8610` "TEST" of 50 points, nine series "CASH"
with `0x004F8780`) filled with the history's values (`0x004F8C30`; it is not drawn); the texts (`0x004E03A0`); Cash
and Date.

The texts (`0x004E03A0`): Tick1 .. 9 shown for the series shown, else hidden (`+0x58`); with the money shown CashTop
the scale (`0x004E0680`) x 0.001 and CashMid x 0.0005 (cut, thousands marked) as "$<n>k" (in French "<n> $k"), both
shown; else hidden. The scale: the history's greatest money (`0x004BD970`, cut) under 10 000 gives 10 000, under
100 000 100 000, under 1 000 000 1 000 000, else 10 000 000.

Update: a new highlighted button (`+0x0C`) hides every SELECTION and shows Selection<its name>. SELECT (CLICK): the
series of the button's number (`atoi`; 1 the money, 2 .. 9 crews 1 .. 8) turned over, the texts again. BACK (CLICK):
`CMSStatsMenu`.

Draw (each frame, after the window): the series' colours (`[0x007A270C]`, made the first time): white, 0xFFFF3737,
0xFF37FF37, 0xFF9B9BFF, 0xFFFFFF37, 0xFFFF37FF, 0xFF37FFFF, 0xFFFF9B9B, 0xFF9BFFFF. The frame in white; with a
crew's series shown the respect steps (1/15, 0.2, 0.4, 2/3: 100, 300, 600, 1000 of 1500) across in 0xFF00B4FF and the
elements of RESPTXT shown, else hidden. Each series shown: 49 lines, x from 0 by 0.02, from the history's place
(`+0x708`) on (modulo 50), the money over the scale, a crew's respect x 1/1500. A line (`0x004E0110`): each value held
to 0 .. 1, at (56 + 530 x) of 640 and (480 - (130 + 260 y)) of 480 of the screen, drawn (`0x00517A60`, a line list) at
y, y + 1 and y + 2 in its colour and at x + 1, y + 3 in black.

Leave: the graph object freed.

## CMSRespect

Record `0x007226F8` (vtable `0x006F5F1C`): enter `0x004DB2F0`, update `0x004DB740`, leave `0x004DB4B0`. Its fields:
`+0x0C` the value of Menu's highlighted item, `+0x10` the crew the map went to, `+0x38` the MAP3D element, `+0x3C` the
time since the crew was chosen, `+0x44` the narration's volume, `+0x48 ..` the map's nine territories, `+0x6C` Menu,
`+0x70` the DATA panel shown.

Enter: `[game + 4]`'s method `+0x34` with 1 (the application's methods `+0x34` and `+0x38` are empty on PC: vtable
`0x006F77A0`, `0x005A10E0`); MAP3D's camera from 4000 to 5 000 000 (`0x005B4660`); Menu emptied, then
"OVERVIEW" (532, value 0) and the crews 1 .. 8 (118 .., values 1 .. 8); `+0x0C`, `+0x10` -1, `+0x70` 0; the elements
whose name holds DATA hidden; the territories (`0x004DBE60`: for crews 0 .. 8 the model of `0x00719110 + 0x14 x crew`
(ui_westanderson, ui_sanricardo, ui_campbellhills, ui_downtown, ui_angelnorth, ui_westanderson again, ui_southside,
ui_eastangel, ui_engstrom) added to MAP3D's list at (0, 0, 0) and hidden; the list's entry k + 1 placed at the height
2000, the seventh 4000 (another model's entry when one before was not added, a stock problem); its first material, of
Coloured Spec ABlend, takes the colour 0x33FF0000 - n × 0xFF00 with n = 255 × the crew's respect (at most 1000) × 0.001
cut, from red to green, Additive 1, Specular Highlight and Shininess 0.5); Cash and Date; the music at a quarter of its
volume; `+0x3C`, `+0x40` 0.

Update: while `+0x3C` passes 4 s the narration of `+0x0C` plays (`0x00719080`: narrAC, narrSR, narrCH, narrDT, narrANC,
narrAWS, narrSB, narrEAI, narrWA) and the frame ends; `+0x3C` takes the frame time. BACK with the panel hidden (CLICK):
`CMSStatsMenu` (mode 3). For a crew, SELECT with the panel hidden or BACK with it shown (CLICK) turns it over: shown,
the DATA elements come in (`0x005A9C20`) and sel_but, sel_text are hidden; hidden, DATA goes out (`0x005A9AD0`) and
sel_but, sel_text come in. With the map, a new value of Menu's highlighted item becomes `+0x0C` (`0x004DB610`: for
another crew than `+0x10` `+0x3C` 0, the narration stopped, the view out of and back into the map mode (`0x005B4490`
with 0, then 4) on an arm of (0, 0, -400 000), -300 000 for a crew, the pitch within -1.5 .. -0.4; the camera to
`0x00722840 + 16 x crew`; the texts; the regions lit, `0x004DC050`: all for the overview, else only the crew's); the
overview hides the panel (`+0x70` 0) with sel_but and sel_text; a crew with the panel hidden shows sel_but and sel_text;
SELECT is played.

The texts (`0x004DBA80`, a crew only): data_logo its flyer's logo, data_location_val the location of its first
circuit's route (575 + it), data_leader_val the leader's name, data_respect_val its respect (thousands marked),
data_respect_meter its respect / 1500 (at most 1), data_bio_val1 .. 3 over several lines (724, 732 and 740 + the
crew), data_respects_val what it respects most (297 + `0x004BC110`), data_chall1 .. 3 ui_yes for a challenge done, else
ui_no.

Leave: a `Menu` element's highlighted item kept, the map's objects freed, the narration stopped, the music and the
narration's volume back, `[game + 4]`'s method `+0x34` with 0 (empty on the PC).

## CMSStatsCarCollection

State record `0x00722998` (vtable `0x006F6328`): enter `0x004E08D0`, update `0x004E0C70`. Its fields: `+0x0C` the
filter, `+0x10 ..` the garage indices in order, `+0xEC` the car kept highlighted.

Enter: `filtermenu` the ten filters (1834 .. 1843, values 0 .. 9); `SpentValue` the money spent on cars (the
profile's `+0x4E8`) and `WinningValue` the money they won (`+0x4EC`), "$%s" (language 1 "%s $"), thousands marked;
the garage's worth (the sale worths, `0x0048DA40`: the value (`0x0048CFE0`) x 0.7 less the repair's cost x 3, at least
the value x 0.2, cut) into the profile's `+0x4F0` and the greatest so far into `+0x4F4`; `ValueValue` the worth;
`CarsValue` the cars; `+0xEC` -1; the filter 0; the order and the lists.

The order (`0x004E1460`): the garage's, then sorted by the filter, the greatest first, a swap starting the pass again:
0 the rating, 1 the sale worth, 2 spent (`+0xA8`), 3 won (`+0xAC`), 4 won less spent, 5 races (`+0xA6`), 6 circuit
races (`+0x9C`), 7 sprints (`+0x98`), 8 showoffs (`+0xA0`), 9 pink slips (`+0xA2`).

The lists (`0x004E0E70`, none while `CarMenu` has no item): `+0xEC` the first of the order at first, else
`CarMenu`'s highlighted car; an empty garage empties `CarMenu` and `DataMenu`. `DataTitle` the filter's line;
`CarMenu` each car in order (the display name and " (<rating>)", its value the garage index), the kept car
highlighted; `DataMenu` the filter's figure: 0 "<class + 1> (<rating> bhp)", 1 the sale worth, 2 spent, 3 won, 4 won
less spent (money), 5 races (wins `+0xA4`), 6 circuit races (wins `+0x9E`), 7 sprints (wins `+0x9A`), 8 showoffs, 9
pink slips (thousands marked); the kept car's place highlighted (its text into the menu's `+0x184` element).

Update: BACK (CLICK) `CMSStatsMenu`; SELECT (CLICK) the order and the lists again; PGDN on a car (CLICK): the
dealer's `+0x00`, `+0x27D4`, `+0x08`, `+0x27DC` 0, its lists (`0x004B4440`) with `+0x27D8` the garage and the car
(`0x004B52F0`), its `+0x0C` 1, `garageload` and the career's `+0xA4` 0; a filter newly highlighted becomes `+0x0C`
and the lists are built again (unsorted).

## CMSNewCont

State record `0x00721DA0` (vtable `0x006F51A4`): enter `0x004C37E0`, update `0x004C3910`, leave `0x004C3830`. The main
menu's career item (`CMSHub`, race mode 2) opens it when the career has started (`[[game + 0x7C]]` not 0). Its
fields: `+0x0C` leaving to the main menu, `+0x10` the answer, `+0x14` asked.

Enter: the button `Continue` highlighted, `Menu` at its first item, `+0x0C` 0, `+0x14` 0, `+0x10` -1.

Update: `backmenu` follows `menu`. Asked: a "yes" asks for `careerload` with the career's `+0xA4` 2 (the betting
window), a "no" clears `+0x14` (and ends the frame). SELECT (CLICK) by `Menu`'s highlighted item: `NewGame`, not asked,
asks (CLICK; type 4) "CURRENT GAME WILL BE LOST" (42) and "ARE YOU SURE?" (86) into `+0x10` (then -1) and sets
`+0x14`; `Continue` `CMSMainMenu`. BACK, not asked: `+0x0C` set, CLICK and `menu`.

Leave: a `Menu` element's highlighted item kept; after a "yes" (`+0x0C` 0, `+0x14` set) the career's head 0, a new
career (`0x004A2300`) and the first event's race set up as on the way in from `DemoFlyby` (the bets with crew 1, the
first racer and car, `0x004B8570`).

`garageload` (record `0x0071FC60`, vtable `0x006F0444`) is `careerload`'s window: enter `0x004358A0`, update
`0x00435960`.

## CMSCarDealership and CMSCarCollection2

Both windows have one class (vtable `0x006F5154`; records `0x00721D6C` and `0x00721D38`): enter `0x004C2AD0`, update
`0x004C2F10`, leave `0x004C2E70`. The hub's Dealership opens the dealership with the dealer in mode 1, its Workshop
`CMSCarCollection2` in mode 0. The record: `+0x0C` the list's place the view shows, `+0x10` `Repair_Text`, `+0x14`
`Repair_But`, `+0x18` the car shown is damaged, `+0x1C` the repair's answer, `+0x20` the car's 3d view (0x50 bytes,
`0x0045A2C0` on `CAR3D`, `LOGO` and `ORIGIN`), `+0x24` `FADEBOX`, `+0x2C`, `+0x30` leaving.

Enter: `+0x0C` -1; the dealer in mode 1 lists its offers (`+0x27D8` its `+0x27E8`), else it goes to mode 0 and lists
the garage (`+0x27D8` the garage), `+0x27D4`, `+0x08` and `+0x27DC` 0 (`08-career.md`, the dealer's list). Cash and
Date (`0x004A4130`). With `Repair_Text` and `Repair_But` both there: `Repair_Text` "REPAIR CAR" (1593), both hidden.
`+0x18` 0, `+0x1C` -1, the view made and `FADEBOX` found. A first visit (the profile's `+0x00` set): the career's head
1, the profile's `+0x00` 0, the garage freed (`0x00489C30`) and the history's step (`0x004BD8A0`); the list made
before stays. `+0x2C` and the dealer's `+0x27E4` (the class listed) -1; `carmenu` filled (`0x004B4680`), the dealer's
cars made (`0x004B5330`), `+0x30` 0. `carmenu`'s first item is highlighted, then the item left highlighted
(`[0x0075EF68]` in mode 1, `[0x0075EF6C]` else) when the menu has more items than its place. The elements with
`SPEC_` and `SEL_` in their names are shown (without their transitions) when there is a car (mode 0: a garage car;
mode 1: an item in `carmenu`), else hidden.

`carmenu` (`0x004B4680`): the six details' texts emptied, then an item for each entry of the list (with `+0x27E4` not
-1 only those of that class, `0x00447480`): the model's display name, " (M)" for `+0x20`, " (D)" for `+0x24`, " (P)"
for `+0x28`; the value is the entry's place.

The details (`0x004B48D0`) of the highlighted entry: `state` New or Used (445 + `+0x08`), `condition` Excellent, Good
or Poor (447 + `+0x0C`), `bhp` the rating with its thousands marked, `drive` FWD, RWD or 4WD (1830 + `+0x10`), `class`
(when there is one) the class of the rating (604 + `0x00447480`), `value` the price.

Update: the view's frame (`0x00459EE0`) and `FADEBOX`'s alpha 1 - the view's `+0x4C` (`0x0042E4F0`). An empty
`carmenu` empties the view (`0x0045AE90`). With an item highlighted: when its value is not `+0x0C` and the item was
the highlighted one the frame before too (`[0x0079E854]`), `+0x0C` takes the value (to the nearest) and the view shows
the car (`0x0045A080`, yaw 2.64, rate 1, fading: mode 1 the dealer's car of the place, else the garage car of the
entry); in mode 0
with the repair elements the entry's `+0x24` shows them and sets `+0x18`, else hides them and clears it. With the view
not busy (its `+0x40` and `+0x3C` 0) the details are written and the six elements shown, else hidden. Then:

- `+0x30` set: `CMSMainMenu`.
- BACK: CLICK, the dealer done (`0x004B5490`); the money other than the profile's `+0x4E4`: that set and, when the
  save system saves, `autosave` over the window; `+0x30` set.
- SELECT: with an item CLICK, the entry taken to the garage (`0x004B5070`), the dealer's `+0x0C` 0, `garageload` and
  the career's `+0xA4` 0; without one NO.
- The repair answered "yes" (`+0x1C` 1): `+0x18` 0, `+0x0C` -1; the cost (`0x0048EBC0`) paid as `0x004B8200` does and
  noted (`0x0048F270`), the car repaired (`0x0048EB20`), the dealer back in mode 0 with the garage listed, `carmenu`
  filled, Cash and Date, `+0x1C` -1.
- Event 7 with `+0x18` set: CLICK; a cost over the money shows "INSUFFICIENT CASH" (type 1), else "Repair this car
  for", the cost and "ARE YOU SURE?" (type 3, on NO) into `+0x1C` (then -1).
- PGDN (event 6) in mode 0: with an item and the view not busy (`0x0045A880`) CLICK, `[0x00721F30]` 1, the car
  (`0x004B5470`) into `[0x00728C60]` and `car_stats2purple` over the window; else NO.

Leave: a `Menu` element's highlighted item kept in `+0x04`; the view freed; `carmenu`'s highlighted item kept
(`[0x0075EF68]` in mode 1, else `[0x0075EF6C]`); the dealer's cars freed (`0x00489C30`).

## The car view

An object of 0x50 bytes (`0x0045A2C0`) shows a car in a Begin3d element: `+0x00` the car's model object, `+0x04` the
element, `+0x08` a text for the model's name, `+0x0C` LOGO, `+0x10` ORIGIN (each may be none), `+0x14` the zoom,
`+0x18 .. +0x20` the camera's place (0, 180, -760), `+0x28` the car's folder, `+0x2C` the mirror image's model object,
`+0x30` the mirror image on, `+0x34` the car's folder kept (the load's `+0x48`: without it the load lets go of
`cars\<car>_ui`, its `+0x44`, at its end, `0x00459B24`; with it the load holds the folder until freed, `0x004596AD`,
and the view takes it on as `+0x28`, `0x00459C06`), `+0x38` the shadow and the neon on, `+0x3C` the load, `+0x40` a second load (the
parts changed, `0x0045A8A0`), `+0x4C` how much the car is shown (0.6 at first). The dealership makes it on CAR3D,
LOGO and ORIGIN with the zoom 0.7 and the mirror image, the folder kept and the shadow on.

Showing a car (`0x0045A080`: the view, a car list, its place, the yaw, the turn rate and the fade): a load not done is
dropped; with the element, the list's `+0x10` takes the place and a load (`0x0045FC80`, `0x004593A0`; LOGO and ORIGIN
hidden) goes to the loading thread (`0x00459730`, its own `rand()`): `cars\<car>_ui\models\<model>.boh` into the
element's world, `gen_neon`, a copy of the car (`0x00459070`, `0x00458EB0`: every mesh node made again and mirrored,
`+0xC0` | 1, the top node's Y axis and height negated), the car's model objects (`0x0048A0A0`, the copy's with 1) and
`arcade_shadow`; LOGO takes "ui_<manufacturer>" (the names at `0x007162F8`) for a make over 0, ORIGIN the flag of the
region (1 `ui_flag_japan`, 3 `ui_flag_euro`, 4 `ui_flag_oz`, else `ui_flag_usa`). The name's text the model's display
name. Without the fade `+0x4C` is 0 and the view emptied at once.

Each frame (`0x00459EE0`): with a load, `+0x4C` goes down by twice the frame time to 0; at 0 the view is emptied
(`0x0045ACE0`: the model objects freed, every listed node out of the element's world and list, LOGO and ORIGIN
hidden) and, the load done (its `+0x08` 3), the car put in (`0x00459BB0`) and its chassis (`_CHASS`, the model
object's `+0x0C`) lowered by the CarDef's MAXDROPHEIGHT x the car's `+0x58` (the mirror image's too). Without a load
`+0x4C` goes up by twice the frame time to 1.

Putting the car in (`0x00459BB0`): `rand() % 3` (the main thread's) picks the pose; with the mirror image its top node
is listed (`0x005B4150`) first, then the car's, each turned about its own Y by the yaw (brought into (-pi, pi],
`0x00407910`), turning by the rate a second about Y, posed (`0x004590E0`: `_BRKLIGHT`, `_REVLIGHT`, `LIGHTSON` and
`DRIVER` hidden; the front wheels steered by 0, 0.35 or -0.35 for the draw 0, 1, 2; `WHEEL_FR` and `WHEEL_RR` turned by
3.14 first). With the shadow on the shadow is listed and turned the same, and the neon (`0x0045A560`) is listed to
turn about its parent's axes, sized to the chassis (x its width x 1.25, z its length x 1.1, each over the neon's),
turned about the parent's Y by the yaw, turning by the rate, shown for a car with a neon (`+0x0D` over -1; the
`gen_neon` material, the model object's `+0xF0` found at its making `0x0048A3AC`, takes the car's neon colour and its
parameter 4 1, `0x0048BEB0`) else hidden. Painting the car again (`0x0045A6F0`) colours that material too. LOGO and ORIGIN shown; the camera placed (`0x0045AEF0`: in the map mode it looks at
(0, 20, 0) from the arm (0, 0, -725) turning by -0.25, the zoom from 0 to 0.7; else at its place looking at (0, 20, 0),
the zoom and its target `+0x14`).

The view is busy (`0x0045A880`) while either load is there; `0x0045AE90` drops the load and empties the view;
`0x0045A340` frees it (waiting for its loads).

## The garage's scenes

A damaged offer's damage (`0x004B5700`, the dealer in mode 1, the entry's `+0x24`): on the showroom's first car's
grid 25 times (`0x007255E0`) damage kept (the grid's `+0x18`, `0x00557AA0`) at a point drawn with `rand()`: kind 1
(the front) x = `rand()` x 0.5 / 32767 x 400 + 70, negated when `rand()` / 32767 is over 0.5, y = `rand()` % 30 + 40,
z 400, 8 each; kind 2 (a side: `rand()` / 32767 over 0.5 x 400, else -400) y = `rand()` % 100 + 40, z = (`rand()` /
32767 - 0.5) x 400, 24 each; kind 3 (the rear) as kind 1 with z -400.

carbuy (`0x00438150`, on `Frame_Car3D`): buying, a used car's (the entry's `+0x08`) damaged (`+0x24`) offer first
takes its damage and the scene is `scrapyard` (its atmosphere and sky, `0x005B4610`, `0x005B4660`), another used
car's `showroom3` (the atmosphere `dealer`), a new car's `showroom2`; the scene's copy mirrored (`0x00459070`) is
listed first with `show_plinth`, `resessft`, `show_plinth_bot`, `show_floor`, `scrapfloor1` and `ssb_tree_1`,
`ssb_tree_2`, `ssb_tree_03` .. `07` hidden. Selling the scene is `don_garage1`. `show_plinth` (`+0x18`) and
`SHOW3_SIGN` (`+0x1C`, at (0, 0, 300)) found. The car (`0x00437BB0`: `cars\<car>_ui\models\<model>.boh`, the lights
and the driver hidden, `WHEEL_FR` and `WHEEL_RR` turned by 3.14, its model object `0x0048A0A0`); buying, its mirrored
copy listed (`0x00438080`); the scene listed; the car listed (`0x00437FF0`); `gen_shadow` at the middle. A new car,
its copy and the shadow turn 0.35 a second about Y. `[0x0075ED00]` the car's model object, `[0x0075ED0C]` whether it
is a prototype. The camera hangs on its arm (0, 0, -425) looking at (0, 50, 0) (`0x005B4490`, the map mode); for a new
car the rig's yaw within -0.5 .. 0.5, its own turn 0 and its angles (-0.2, 0, 0).

carsel (`0x00436380`, on `FRAME_CAR3D`): the damage as above (the dealer is in mode 0: none); `don_garage1` and the
car (`0x00435AE0`, as `0x00437BB0`) listed; `[0x0075ED00]`; the camera (`0x00436200`) at once at the car's place
+ 50 each way + (-233.37, 115.84, 493.664) looking at the place + 50, and the garage list's `+0x10` 0; `gen_shadow`
(`0x00436080`) sized to the chassis (x its width x 1.25, z its length, each over the shadow's) and, for a car with a
neon, `gen_neon` (`0x00436160`) sized so, its material the car's model object's neon material with the car's neon
colour (`+0x2C`, `0x00435FB0`).

## The garage's texts

`carbuy` and `carsel` share a record of texts at `0x00767D58`: the price kept and the elements of the price, the
money, the price's box, the class, the class's meter and the rating. The car shown is the dealer's list (`+0x27D8`:
mode 1 the showroom, else the garage) at `+0x27DC`; `[0x0075ED0C]` keeps whether it is a prototype (`0x004A8F50`).

- `0x00447090`: the price kept and its element the price as money (thousands marked).
- `0x00447100`: the money's element the player's money.
- `0x004473B0` (`eax` 0 show, 1 hide): the price and its box shown (their transitions start again) or hidden.
- `0x00447170`: with the class's element there, a car that stopped being a prototype notes "This car is no longer a
  prototype." (1543, type 1; its `+0x0C` 0xFF when it was 0), one that became one "Congratulations! This is now a
  prototype car." (1542); the class's element the class's name (604 + `0x0048E9E0`) and " (P)" for a prototype. With
  the rating's element there: the rating (cut) as "%d", and on the class's meter the rating (999 at most) less 100
  while over 100, x 0.01. Each text set starts its effect (the element's `+0x108`: `+0x18` 1, `+0x24` 1.5; not
  followed).

The dealer's deal (`0x004B5550` may it be made, `0x004B5580` make it): not once the dealer's `+0x08` is set; buying
(mode 1) only with the entry's price (`+0x1C`) within the money. Buying: a price over the money notes "INSUFFICIENT
CASH" (1884, type 1) and fails; else the showroom's car notes the spending (`0x0048F270`: the car's `+0xA8` and the
profile's `+0x4E8` take the price), the price is paid, the offer leaves the offers (`0x004B5690`: the offers after
its place move down one, all 159 numbered again, the count one less) and `+0x08` is set. Selling: the garage car's
sale worth (`0x0048DA40`) is paid to the player and `+0x08` is set. The car changes hands when the dealer is done
(`0x004B5490`, `08-career.md`).

## carbuy

Record `0x0071FCB0` (vtable `0x006F0374`): enter `0x00438150`, update `0x00438BB0`, leave `0x00438A70`. Its fields:
`+0x28` leaving, `+0x2C` the answer, `+0x30` asked, `+0x34` the note's turn, `+0x38` the car bought or sold.

Enter: `[0x0075ECF8]` the career; the texts `PriceVal` (emptied), `CashVal` and `CashBox`; the elements with
`message` in their names hidden; the money shown; with `Frame_Car3D` the scene ("The garage's scenes"); the price
the entry's (`+0x1C`); `[0x00728C60]` the car; `sel_text` "BUY" (459) in mode 1, else "Sell" (463); `+0x2C` -1, `+0x30`
0, `+0x34` 1, `+0x38` 0, `+0x28` 0; Cash and Date.

Update: `[0x0075F108]` 2; with the scene's `show_plinth` (`+0x18`) and a new car (`+0x20`, the offer's `+0x08`, 0) the
plinth turned about Y by the front end's frame time (`+0x104`) × 0.35 (`0x00534960`). `+0x28` set: `CMSMainMenu`.
SELECT:

- The deal made (`+0x38`): with the save system saving the dealer done (`0x004B5490`), its list again (`0x004B43C0`,
  its mode) and `autosave` over the window; `+0x28` set.
- Else, not asked (`+0x30` 0): when the deal may be made, buying with 55 cars in the garage gives CLICK and "Cannot
  purchase car. You have reached the maximum number of cars allowed." (1613, type 1); else CLICK and "Buy this car
  for" (460) or "Sell this car for" (461), the price and "ARE YOU SURE?" (86, type 4, on NO) into `+0x2C`, `+0x30`
  set. When it may not: with `+0x34` NO and "INSUFFICIENT CASH" (type 2), `+0x34` 0; else `+0x34` 1.

BACK, not asked: CLICK, the message window closed (the front end's `+0x48`), `CMSCarDealership`. Asked: a "yes"
makes the deal: done, CLICK, the money shown, "Car purchased" (458) or "Car sold" (457, type 2), `+0x38` set, `+0x30`
0; failed, `+0x2C` -1 and `+0x30` 0. A "no": `+0x30` 0.

The update ends (not after `+0x28`'s switch or the full garage's note) with the chassis of the record's car object
(`+0x0C`) and of the garage's (`[0x0075ED00]`) lowered (`0x00438FDF`): the `_CHASS` node at (0, −the CarDef's
MAXDROPHEIGHT × the car's `+0x58`, 0).

Leave: a `Menu` element's highlighted item kept; the views freed; the dealer done and its list again (its mode).

After a purchase the car's model object takes its sponsor decals again (`0x0048B440`, `0x0048B690`).

## carsel

Record `0x0071FC70` (vtable `0x006F0414`): enter `0x00436380`, update `0x004371C0`, leave `0x00436B90`. Its fields:
`+0x14` `menu`, `+0x18` the menu hidden for a window over it, `+0x1C` the repair's price shown, `+0x20` leaving after
the autosave, `+0x24` the repair's answer, `+0x28`, `+0x2C` the sale's answer, `+0x30`, `+0x34` the repair asked.

Enter: `[game + 4]`'s method `+0x38` with 1; `+0x20` 0; `BACK_TEXT` "CAREER MENU" (1970) for the career menu's garage
(the dealer in mode 0 with its `+0x10`), else "BACK" (1953); the race's type 5 and laps 1; the texts `Price`,
`frame_Cash`, `PriceBox`, `frame_Class`, `frame_ClassMeter` and `frame_Bhp`; `Price` emptied; `+0x1C` 0; the money
shown; with `FRAME_CAR3D` the scene ("The garage's scenes"); `[0x00728C60]` the car; its class and rating
(`0x00447170`). `menu` and `backmenu` back to their items (`0x005AECF0`). A car without damage (its grid's,
`0x00557E80`, and its worn parts, `0x0048EF10`) loses `menu`'s `repair` item (`0x005AFAD0`) and `backmenu`'s first
(`0x005AFAA0`); else both highlight their first. Every item of `menu` is enabled; for a damaged car those named
`cust`, `race`, `testtrack`, `shop`, `rolling` and `scheme` (case aside) are disabled. The view in the map mode
(`0x00436010`, then again at `0x004369B6`: looking at (0, 50, 0) at once, `0x005B4490` with mode 4 and no arm, so the
arm (0, 0, -500) and the rig (-0.12, -1.57), and its own turn `+0x190` -0.1): the camera goes from the car's side
(`0x00436200`) to the arm and turns about the car. `+0x34` 0, `+0x24` .. `+0x30` -1; Cash and Date. `menu`'s first
item highlighted (`+0x13C` and `+0x148` 0) and its help in the ticker (as `0x005B5240`, at `0x00436A24` and again for
the highlighted item): without the repair item the ticker would still hold its help.

The menu hidden or shown (`0x00436D40`, `+0x18` takes it): hidden, `menu` goes out (`0x005A9B40`), `backmenu`,
`aniline`, `animline2`, `menufade`, `frame_metalline2` and `infobox` are hidden (by name, case aside) and `BACK_TEXT`
is "BACK"; shown, they are shown again (`menu` with its transition) and `BACK_TEXT` as at the enter.

The leave of the garage (`0x00437000`): the money other than the profile's `+0x4E4`: that set, `autosave` over the
window when the save system saves, `+0x20` set and nothing more. Else CLICK and, in the career (the race's mode 2),
`CMSStatsCarCollection` (the dealer's `+0x0C`), `CMSCarDealership` (mode 1), `CMSMainMenu` (`+0x10`) or
`CMSCarCollection2`; outside it `menu`.

Update: `[0x0075F108]` (the clouds' mode, `12-rendering.md`) 2; Cash and Date; `+0x20`: cleared and the leave.
`backmenu` highlights `menu`'s item; with `+0x18` the menu is shown again. The `repair` item highlighted on a damaged
car shows the price and its box, the price the repair's cost (`0x0048EBC0`, cut), once (`+0x1C`); another item hides
them and clears `+0x1C`. SELECT on an item (CLICK) by its name:

- `cust`: a damaged car notes "Car is damaged" (475, type 1); else `custmenu` over the window, the menu hidden.
- `shop` `modshop`, `details` `car_stats2` (`[0x00721DE8]` 1), `maintenance` `car_maintain`, `scheme` `schememenu`:
  over the window, the menu hidden.
- `leave`: the leave of the garage.
- `testtrack` (a damaged car: the note): the session's test track (`0x004B9670`), the career manager's `+0xA8` 1,
  `+0xA4` 4, the profile's `+0x4E4` -1, `garageload`.
- `rolling` (a damaged car: the note): `+0xA4` 5, `garageload`.
- `sell`: "Sell this car for", the sale worth and "ARE YOU SURE?" (type 3, on NO) into `+0x2C`.
- `repair` on a damaged car not yet asked (`+0x34`): a cost over the money notes "INSUFFICIENT CASH" (type 1) and NO;
  else `+0x34` 1, CLICK and "Repair this car for" (462), the cost and "ARE YOU SURE?" (type 3, on NO) into `+0x24`
  (then -1).

The garage's car (`[0x0075ED00]`) has its chassis lowered (`0x00437967`: the `_CHASS` node at (0, −the CarDef's
MAXDROPHEIGHT × the car's `+0x58`, 0)). BACK without the repair asked: the leave of the garage. `+0x28` 1 (never set):
CLICK, `+0xA4` 5, `garageload`. A "yes" to the sale: CLICK, the deal (`0x004B5580`), `CMSCarCollection2`. With the
repair asked: a "yes" pays the cost and notes it (`0x0048F270`), repairs the car (`0x0048EB20`), `+0xA4` 7 and
`garageload`; a "no" clears `+0x34`. Every frame but the leave's (`+0x20`) ends with the record's car object's (`+0x0C`)
chassis lowered again (`0x00437B69`).

Leave: a `Menu` element's highlighted item kept; `[game + 4]`'s method `+0x38` with 2; the views freed; the dealer
done; the garage's sounds (`0x00436CF0`).

`car_maintain` is no window of the game: no record takes the name and there
is no `car_maintain.ui` (`carsel.ui` has no `maintenance` item either).

## CMSCutScene (the test track's drive out)

Record `0x007220A8` (vtable `0x006F5560`): enter `0x004C6B80`, update `0x004C7BF0`, leave `0x004C7030`. The garage's
`testtrack` (the session of the test track, `0x004B9670`: the profile's list `+0x514` holds the garage's car, its count
`+0x5F0` 1) sets the career's `+0xA8` 1 and `+0xA4` 4; `careerload` then asks for `cmsCutScene` (the windows' names
compare without case). Fields: `+0x0C` the world, `+0x10` CAR3D, `+0x14` the camera's place, `+0x24` its stage,
`+0x28` its time, `+0x2C` LOCATION, `+0x30` the cars' shadows, `+0x40` the cars (0x514 bytes each, `0x00485530`),
`+0x50` their stages, `+0x60` their times, `+0x70` their push, `+0x80` the count.

Enter: the race's type 9; the front end's black (`+0x10C`) 1; the camera's stage and time 0; the music at a quarter
and the scene's sounds; CAR3D with `outsidegarage` listed at (0, 52, 0), its atmosphere and sky; a world without a
route (`0x0053DEA0`) with one plane at (0, 0, 0) facing up (`0x004C7DF0`). For each car of the profile's list a car of
the race (not of the AI, the record's wear and nitro kept) at x 825 cm for the second and third cars (else 0), z 400 cm
for the first two (else -200), 300 cm less with fewer than three cars, y 1 cm, its heading a word of the stack never
set; at rest (`0x00486E90`), its sounds, `gen_shadow` listed at its place and scaled to the chassis (`0x004C71F0`), in
the world; stage 0, time its number + 7 s. The record has room for four cars; more write over its next lists. The view
takes move mode 2 (`CAMERA_AUTO`, `0x005B4490`); the camera at (1800, 75, 2000).

Update: with a car the script (`0x004C72D0`) and the camera (`0x004C7870`); the view's zoom and its aim 1. SELECT
(CLICK): before the camera's stage 3 the camera's time and every car's -1 (the next stage at once); else the test
track's race when `+0xA8` is set, otherwise `CMSAtMeeting`, and `+0xA8` 0. BACK (CLICK): `+0xA4` 6 and `garageload`.

The script, for each car: the shadow's matrix the identity at the body's centre of gravity (centimetres, 36 lower),
turned about y by minus the yaw (the shadow's scale goes with it before the first drawn frame); by the car's stage:

| Stage | Brake | Throttle | Gearbox | Other | Next stage's time |
|---|---|---|---|---|---|
| 0 | 1 | 0 | by hand | | 1 s |
| 1 | 1 | 1 | by hand, neutral | | 1 s |
| 2 | 1 | 0 | by hand | push 0 | 0.4 s |
| 3 | 0, handbrake 0 | 0 | by hand, neutral | velocity (0, 0, push) in the car's frame; push + step within 0 .. 1 | 0.2 s |
| 4 | 0, handbrake 0 | the stage's time squared (1 from 1 s) | by hand, neutral | velocity (0, 0, push) | 1 s |
| 5 | 0 | 0.25 | automatic | steering 0 | 0.5 s |
| 6 | 0 | 0.16 | automatic | steering 0 | 10 s |
| 7 and on | 0 | 1 | automatic | steering 0 | |

The stage's time falls by the window's step. The pools are emptied, the world steps the game's frame time within
0.016 .. 0.02 s x 0.75, then the car's frame (`0x00488EE0`: the wheels, the tilt, the car's frame, the slipstream which
returns without a race, the chassis's drop, the lights, the exhausts, the sounds, the damage's effects `0x00488B50`
and the wheels' `0x00487FE0` with the car's effects object made with it, `0x00486164`, but without emitters: its tyres
wear off the stand, its flames draw `rand()`; such a car has no competitor, so a turn-over of 4 s or an impact ahead
would read address 0, Stock problem); the body's definition loses its
traction control (`+0x174` 0).

The camera, by its stage: 1 looks at (0, 0, 0) and moves 450 cm a second along x; 2 looks at (0, 50, 100) and moves
25 cm a second back along x; 3 looks at (0, 0, 0) and moves 150 cm a second along z; in each the black is 1 - the
stage's time under 1 s; 4 the black 1. Its time falls by the window's step; past 0 the next stage: 1 at
(-3800, 150, 3000) for 5 s, 2 at (600, 70, 250) for 5 s, 3 at (250, 75, 700) for 8 s with the black 1, 4 the test
track's race when `+0xA8` is set (else `CMSAtMeeting`, mode 3) and `+0xA8` 0.

The test track's race (`0x004A4060`): type 9, 99999 laps, time of day 1, weather 1, `Angel SuperSpeedway`'s
`Speedway_R1`, one player, the settings' `+0x84` for `+0x54`; `loading` (mode 1) and the cruise's `+0x10` 1.

Leave: the sounds stopped, the world and the cars freed, CAR3D's list emptied.

## car_rolling (the dyno)

Record `0x007228D0` (vtable `0x006F60EC`): enter `0x004DD490`, update `0x004DF260`, draw `0x004DF660`, leave
`0x004DDF90`. The garage's `rolling` sets the career's `+0xA4` 5; `careerload` asks for `car_rolling`. Fields: `+0x0C`
the world, `+0x10` FRAME_CAR3D, `+0x14` the car (0x514 bytes), `+0x18` DATA_REVS, `+0x1C` DATA_SPEED, `+0x20`
DATA_GEAR, `+0x24` DATA_TORQUE, `+0x28` DATA_BHP, `+0x2C` DATA_MAXSPEED, `+0x30` DATA_MAXTORQUE, `+0x34` DATA_MAXBHP,
`+0x38` DATA_MAXREVS, `+0x3C` MSG, `+0x40` DATA_TCLIGHT, `+0x44` DATA_TCLIGHTTEXT, `+0x48` the best speed, `+0x4C`
torque, `+0x50` power, `+0x54` revs, `+0x58` the revs of the best power, `+0x5C` the wait between gears, `+0x60 ..
+0x6C` the rollers, `+0x70` the graph, `+0x74` the graph's revs, `+0x78` the state, `+0x7C` the countdown, `+0x80` the
test's gear (4).

Enter: `[game + 4]`'s method `+0x38` with 1; the music at a quarter; FRAME_CAR3D with `rollingarage` listed at
(0, -3, 0). The garage's car (the dealer's list `+0x27D8` at `+0x27DC`) becomes a car (`0x00485530`, not of the AI, the
record's wear and nitro kept) at (0, 1, 0) with an unset heading, at rest, by hand, on the stand (`+0x19C` 1);
`gen_shadow` at (0, 0, 0) sized to its `_CHASS` (x 1.25, z 1; `0x004DE250`). The view looks at (0, 50, 0) in move mode
4; a world without a route with the ground plane holds the car. MSG "START DYNO TEST" (97), its transition started
again and highlighted. The body's DragCoefficient 0. `ROLLING_ROAD` listed under the front wheels (z the first wheel
pose's less the centre of gravity's z x 100, y -3) turned about y by 3.14, and under the rear wheels (the third pose's);
their `ROLLINGFRONT` and `ROLLINGREAR` nodes (`0x00534E30`: the first node of the name, case aside, depth first).
Every `DATA_` and `RESULT_` element hidden. The graph `RESULT` of (the last TorqueCurveX point in rpm / 74.62, cut) +
10 points with the series TORQUE and BHP, running; its revs the first TorqueCurveX point in rpm, its points below them
written 0 every 74.62 rpm. The units: DATA_SPEED_TEXT "MPH" or "KPH" by the settings' miles (`+0x94`),
RESULT_SPEEDTEXT " (MPH)" or " (KPH)" more, DATA_TORQUE_TEXT and RESULT_TORQUETEXT " (lbft)" or " (NM)" more,
DATA_BHP_TEXT and RESULT_BHPTEXT " (bhp)" (484) more. Every `SEL_` element shown.

Update: with a car the script (`0x004DE350`), the readings (`0x004DEBA0`) and the rollers (`0x004DF180`); the view's
zoom 1. BACK, or SELECT in state 4 (CLICK): `+0xA4` 6 and `garageload`. SELECT in state 0 (CLICK): state 1, the
`DATA_` elements coming in, the bests 0, the countdown 4 s, MSG "INITIALIZING" (98), the `SEL_` elements hidden. In
state 1 the countdown runs down; past 0 the body gets (0, -10000, 0) on its external force, the test's gear (third),
state 2, WARNING and MSG "ANALYZING" (99).

The script by the state:

| State | Brake | Throttle | Else |
|---|---|---|---|
| 0 | 1 | 0 | by hand |
| 1 | 0 | 1 while the countdown is over 3 s, else 0 | by hand, neutral; the traction light and its text hidden |
| 2 | 0 | 0.49 in neutral, else 1 | by hand; past 0.6 (neutral) or 0.97 of the last TorqueCurveX point the next gear after a wait of 2 s, (0, -10000, 0) on the external force; from the last gear state 3, WARNING, MSG "RUN COMPLETE" (100) |
| 3 | 0.512 | 0 | the lights hidden; under 10 m/s forward state 4 (below) |
| 4 | | | |

State 4: the `SEL_` elements shown, WARNING, MSG "EXIT DYNO" (101) highlighted, the `DATA` elements going out and the
`RESULT_` ones coming in; RESULT_SPEED, RESULT_TORQUE and RESULT_BHP the texts of DATA_MAXSPEED, DATA_MAXTORQUE and
DATA_MAXBHP; RESULT_SPEED_OLD, RESULT_BHP_OLD and RESULT_TORQUE_OLD the car's last run (`+0x94`, `+0x90`, `+0x8C`;
"%03d", "---" for -1 and below); RESULT_KEY5 "LAST RUN" (1600); the run's bests go to the car; RESULT_BHPREV the
revs of the best power ("%04d"); the graph's drawn points and next point 0, its scales 136 / (the best x 1.2).

Then, in every state, the pools are emptied and the world steps the game's frame time within 0.016 .. 0.02 s; the car's
frame (`0x00488EE0`); the body stays at x and z 0 (its node and state too), unturned (`0x004F3080`, `0x004F3150`); the
wheel poses of the wheels not driven do not turn; the definition's traction control is on (`+0x174` 1). The speed
read is the body's speed forward while its place is held.

The readings: DATA_REVS the revs and DATA_MAXREVS their best; DATA_GEAR "R", "N", "1" .. "6"; DATA_SPEED the speed
forward (none under 0) x 2.235 (mph) or x 3.6 (km/h) and DATA_MAXSPEED its best. In state 2 DATA_TORQUE the engine's
torque out x 0.29 (x 1.335818 in newton metres) with `rand() & 3 - 2` and DATA_MAXTORQUE its best; DATA_BHP the torque
out x the engine's speed x 0.000481 and DATA_MAXBHP its best, a new best keeping its revs with `rand() % 10 - 5`; in
fourth gear past the graph's revs without the traction control at work the graph's next point the torque and the power
each with `rand() % 6 - 3`, the graph's revs 74.62 more. All numbers "%03d", cut.

The rollers: a driven front wheel turns the first road's rollers about x by -0.0175 x its spin each frame, a driven
rear wheel the second road's by 0.0175 x its spin.

Draw: in state 4 once the window's transitions are done the graph at (130, 342) of 640 x 480, drawn without and with its
shadows, one point more each frame. The graph (`0x004F8610` .. `0x004F8C30`, 0x78 bytes): its name, its points (`+0x40`)
and those drawn (`+0x44`), running (`+0x48`), the next point (`+0x4C`), the series (`+0x50`, 0x4C bytes: name, values,
shown, scale), the order (`+0x60`), its place (`+0x70`, `+0x74`). It draws (`0x004F8940`) a line of `0x80FFFFFF` from
its place to (2 x the points, its y) in pixels, then each series (`0x004F8410`): from the next point on, lines between
the drawn points, each at (2 i + x, y - value x scale) of 640 x 480, in the series' colour of `[0x007191E0]` (0xFF
shown, 0x30 not); with shadows a value not 0 first gets a black line 3 right and 2 down.

Leave: `[game + 4]`'s method `+0x38` with 2; the graph, the world and the car freed; FRAME_CAR3D's list emptied.

## modshop (the tuning shop)

Record `0x0071FFE8` (vtable `0x006F0484`): enter `0x00442A80`, update `0x00444690`, no leave. It opens over `carsel`
(mode 5) on the garage's car (`[0x0075ED00] + 0x8C`) and closes itself (mode 6). Its fields: `+0x0C` the number of
upgrades, `+0x10` ten upgrades of 0x4C bytes, `+0x308` `menu`, `+0x30C` `backmenu`, `+0x310` the item last shown,
`+0x314 ..` the elements PRICE, BHP, CLASS, CLASSMETER, Speed, Accel, Braking, Handling, `+0x334`, `+0x35C`, `+0x384`
the LOGO_, LEVEL_ and ICON_ elements of each kind, `+0x3AC` the analysis's seconds, `+0x3B0` its beeps, `+0x3B4` the
answer. An upgrade: `+0x04` the class + 1, `+0x08` 1, `+0x0C` its rating, `+0x10` its cost, `+0x14` speed, `+0x18`
accel, `+0x1C` braking, `+0x20` handling, `+0x24` the catalogue part of each of the ten kinds (-1 none).

The catalogue's draw of a part of a kind and level: the suppliers (`+0x2A`) of the kind's parts of the level, each
once in the list's order (`0x00498F40`, the list at the catalogue's `+0xF0`, the level its `+0x100`), one of them by
`rand() %` their number, that supplier's parts of the level (`0x004990F0`, kept at the catalogue's `+0x104`) and the
first one's place (`+0x24`).

Enter: the upgrades cleared; the car's induction (kind 8), exhaust (1) and turbo (3) kept, their levels (`+0x2B`,
-1 none) and the nitro's; the car's rating cut. For each level (-1 none .. 3) of the turbo, inside it the exhaust's,
inside that the induction's, all three none left out, none past the levels the profile unlocked for the model
(`+0x50 + model x 10 + kind`) nor below the car's own: the car takes its three back, then each kind of a level a part
drawn (put on unless the car's own is of that level or more), none for -1; the car is rated (`0x0048E8A0`); the
debug line "ITER %d - Class %d - BHP %d - Induc %d - Exhaust %d - Turbo %d" is printed (`0x0059AF00`). A rating whose
whole number (to the nearest, `0x005CA370`) passes the car's and passes the best found for its class (7 under 200,
6 under 300 .. 0 from 800) takes that class: the nitro, the suspension, the brakes and the tyres, each at the
profile's level but at most the highest of the three levels, are drawn when above the car's own; the class's
induction, exhaust and turbo the car's; its cost the parts' COST (the three only when not of the car's levels), its
meters from the parts' PHYSICS (/ 255): the three's a into accel and b into speed, the nitro sets speed to 0.35, 0.25
or 0.1 by its level 2, 1 or 0, the suspension's d into handling, the brakes' c into braking, the tyres' c into
braking and d into handling; accel x 0.3, handling and braking x 0.4, each held within 0 .. 1; the three of the
car's levels none again; a class without a part is not found. Then the car's three back and rated again. With the
nitro's level unlocked above the car's, the class of the car's rating, not found yet, is found with the nitro of that
level (drawn when above the car's), its cost and speed. Every class found, from 7 to 0, makes an upgrade.

`menu` and `backmenu` emptied; for each upgrade from the last `menu` "UPGRADE CLASS <n> " (1792, 1791) with its place
and `backmenu` a bar `ui_pan5_green` drawn adding (`+0x54`); both highlight their last item. `cash` the money (as
money); BHP, PRICE, CLASS, CLASSMETER, Speed, Accel, Braking and Handling hidden; CARNAME the model's display name;
the ICON_, LEVEL_ and LOGO_ elements of the suspension (0), exhaust (1), nitro (NOS, 2), turbo (3), brakes (5), tyres
(6) and induction (8) hidden (the other kinds have none); `+0x310` none, `+0x3B4` -1, the analysis 3 seconds and 3
beeps; ANALYSIS over several lines.

Update: `[0x0075F108]` (the clouds' mode) 2. While the analysis lasts: BACK gives CLICK and closes the window; each
time its whole seconds (cut) fall below `+0x3B0` that is one less and WARNING sounds; ANALYSIS is shown with the
seconds' fraction above whole ones as its alpha; `menu` and `backmenu` hidden; the frame time is taken off; the
elements holding "carname" (strstr, so not CARNAME) hidden. Once it is over: 0, the message window closed (the front
end's `+0x48`), `menu` and `backmenu` shown, ANALYSIS hidden, "carname" shown, BHP .. Handling shown. With the
analysis still running nothing more.

An empty `menu`: "No upgrades available!" (1799), a line feed and "This car's performance is maxed" (1801, every
kind unlocked to 3) or "Race this car to unlock or win more mods" (1800) as a note (type 1); the window closed.

Else the upgrade of the highlighted item (its value):

- Bought (`+0x3B4` 1): its parts put on the car (an exhaust also on the car's model, `0x0048BE30`; the model's parts
  and lights again, `0x0048B440`, `0x0048B690`), the car rated and made ready (`0x0048EA80`: the grid's race damage,
  the nitro 15, `+0x7C`, `+0x74`, `+0x64`, `+0x6C`, `+0x70` cleared), its cost paid and noted (the car's `+0xA8`, the
  profile's `+0x4E8`), the money and the class shown (`0x00447100`, `0x00447170`); the window closed.
- Another item than `+0x310`: BHP its rating (cut), CLASS its class number, both shown; with `[0x0075ED08]` (written
  by other windows) a place of the catalogue's `+0x104` list the meters take that part's PHYSICS b, a, c and d / 255;
  CLASSMETER the rating (999 at most) less 100 while over 100, x 0.01 (1 for a rating over 900), shown; PRICE its cost
  (cut) as money; the meters Braking, Handling, Accel and Speed its own. For each kind with its elements: a part shows
  ICON_ white, LEVEL_ its level + 1 ("P" for 4) and LOGO_ "ui_<supplier>_on" (the names at `0x00717970`) with its
  transition from the start; none shows ICON_ in 0x80808080, LEVEL_ "-" and sends LOGO_ out (hidden without a
  transition).
- SELECT: with the money (as a float) over the cost CLICK and "BUY UPGRADE CLASS <n> " (459 and the item's text; in
  German the item's text then 459), "\n ", the cost as money, "\n " and "ARE YOU SURE?" (type 3, on NO) into
  `+0x3B4`; else "INSUFFICIENT CASH" (type 1) and NO.
- BACK: CLICK, the window closed.

A purchase gives the car's model its exhaust part (`0x0048BE30`) and its sponsor decals again (`0x0048B440`,
`0x0048B690`).

## The customising windows

`carsel`'s CUSTOMIZE opens `custmenu`; its windows replace one another (mode 7) over `carsel`, and the garage's
camera moves around the car: `0x004362C0` sets the camera's goal to the car's place plus an offset (its height less
15) looking at the car's place at height 50, `0x00436010` lets the camera turn around the car (`+0x2C` -0.1, out of
the map mode: looking at (0, 50, 0) at once, mode 4). The offsets: the garage's place (-233.37, 115.84, 493.664,
`0x007203E8`), the side (-500, 100, 0, `0x00720408`), the engine (130, 100, 350), the wheel (-200, 30, 230), the
exhaust (40, 50, -460). Each window's update sets `[0x0075F108]` (the clouds' mode) 2 and lowers the chassis by
MAXDROPHEIGHT x the car's `+0x58`.

The windows' common enter (`0x005A5F20`) gives a Menu element its items back (`0x005AECF0`) and highlights the item the window
kept (its `+0x04`); the common leave (`0x004C7F70`) keeps the highlighted item.

`custmenu` (`0x00439050`, update `0x004390A0`): the camera to the side and turning. Back from a window (`+0x14`) the
menu shows again with its transition and the camera goes to the side. SELECT on Menu's item (CLICK): tuning opens
`tuningmenu`, styling `stylingmenu`, paint keeps the car's paint (`0x00767D90 .. 0x00767D98`), clears
`0x00728AF8 .. 0x00728B00`, sets `[0x00715C24]` 1 and opens `colourmenu`. BACK (CLICK) closes the window and sends
the camera to the garage's place.

`tuningmenu` (`0x0043A780`, update `0x0043A8B0`, leave `0x0043A860`): enter rates the car (`0x0048E8A0`), puts its
model's parts on again (`0x0048B440`, `0x0048B690`), shows its class (`0x00447170`), lets the camera turn, takes the
ride item out when the car has no suspension part, and highlights the first item when the car is another than the
one before (`+0x10`). Leave keeps the highlighted item and sets `+0x0C` 1. Update: with the money under 10000 the
window's help page (once); back from a window (`+0x0C`) the menu shows again. SELECT on Menu's item: suspension
(kind 0, the camera to the side), induction (8), nitro (2) and turbo (3, the camera to the engine), exhaust (1, to
the exhaust), brake (5) and tyre (6, to the wheel) give CLICK and open `performtype` for their kind (`[0x0071FF9C] +
0x14`); ride (kind 0, the side) `suspensionmenu`; gears (9) `gearboxmenu`; chip (the engine) `enginechip`. PGDN:
`car_stats2` over the window (its garage flag 1), every menu hidden. BACK (CLICK): `custmenu`.

`performtype` (`0x00441560`, update `0x00441800`): enter empties the menu, clears the refund (the garage's `+0x1C`),
adds "STOCK" (102, value 1, `stock`) when the car has a part of the kind, highlights the first item, hides LOCKED,
then adds "LEVEL 1" .. "LEVEL 3", "PROTOTYPE" (the texts at `0x00761D10`, lines 547 ..; `street`, `sports`,
`competition`, `prototype`): a level past the one the profile unlocked for the model and kind is valued -1 (locked),
the level of the car's part shows the picture `ui_yes`; the answer (`+0x0C`) 1 and the question (`+0x10`) none.
Update: after the question a "yes" takes the part off (an exhaust also off the model, `0x0048BE30`; the suspension
also clears the ride, the car's `+0x58`), gives the refund back (cut), shows the money, clears the garage's price,
rating, refund and bought, and opens `tuningmenu`; a "no" clears the question. A highlighted item valued -1 shows
LOCKED, another hides it. SELECT on an item not locked (CLICK): `stock` asks "REMOVE MOD" (724), with the rating
without the part when the kind shows one ("NEW BHP" 675 and the rating, rated twice; not for the suspension, nitro,
brakes, tyres and gearbox) and "ARE YOU SURE?" (type 3, on NO); a level opens `suppliers` for the exhaust, else
`performmod`, and sets the catalogue's level (`+0x100`: 0 street, 1 sports, 2 competition, else 3). BACK (CLICK):
`tuningmenu`.

`performmod` (`0x00441D20`, update `0x00442020`, leave `0x00441F90`): enter brings the garage's price and its box in
(their transitions from the start), empties Menu and highlights the first item; the suppliers of the kind's parts of
the catalogue's level (`0x00498F40`) each make an item: no text, its place as value, the pictures
`UI_<supplier>_OFF` and `_ON`, the supplier's name; `[0x0075ED08]` 0. None: "NONE AVAILABLE" (96) and every
element holding PRICE hidden; else the money shown and Help's ticker by the kind (turbo 92, nitro 93, suspension 94,
brakes 95, tyres 91). `+0x10` the car's part of the kind. Update: with a part bought (the garage's `+0x2C`) and one
chosen (`+0x14`) the part is put on (`0x0048BD70`: the kind's part; the nitro 15, the tyres' wear and the brakes'
wear 0) and `tuningmenu` opens. Without a window over the windows `performance` comes over them (mode 8). The
highlighted item's supplier (`[0x00715C30]`), its parts of the level (`0x004990F0`), the first chosen and the garage's
price its COST. SELECT: the car's own part notes "You already have this mod" (103, type 1); else the rating with the
part (0 for the kinds without one), the refund 0, CLICK and `buymod` over the window. BACK (CLICK): `performtype`; the
garage's price, rating, bought and refund cleared. Leave keeps the highlighted item, sends the price and its box out
and forgets the chosen part.

`buymod` (`0x00446750`, update `0x00446B80`): with the money under the garage's price "INSUFFICIENT CASH" (type 1),
NO, and the window closes. Else the question "NEW MOD PRICE" (1829) and the price, with a refund "CURRENT MOD
REFUND" (1828) and it (cut); with a rating: for another class "NEW CLASS" (676) and its name, then "NEW BHP" (675)
and the rating (cut); "ARE YOU SURE?" (type 3, on NO); for another class the message's banner "CLASS CHANGE" (1787,
`0x005A8330`). The rating is cleared. Update: a "yes" pays the price less the refund (cut), notes it on the car and
the profile (`0x0048F270`: the car's `+0xA8`, the profile's `+0x4E8`), shows the money (`0x00447100`), clears the
garage's price, rating and refund and sets bought 1; the window closes either way.

`performance` (`0x004423C0`, update `0x00442420`): the meters Speed, Accel, Braking and Handling. For the gearbox
(kind 9) the car's gearing (`+0x5C`) under 0.5 gives Speed (0.5 - it) x 2 and Accel 0, else Speed 0 and Accel
(it - 0.5) x 2; Braking and Handling 0. Else the part at `[0x0075ED08]` of the supplier's parts of the level gives
Speed, Accel, Braking and Handling its PHYSICS b, a, c and d / 255; another part or supplier than before (`+0x1C`,
`+0x20`) shows the meters from 0 (their shown value `+0xEC`).

The garage's price (`0x00447090`) also clears the bought flag, so a part bought here is fitted once: the next
window that shows a price (or the next frame of `performmod`) clears it. `0x00447050` clears the price, its text, the
rating, the bought flag and the refund. `0x004473B0` sends the price and its box out (`eax` not 0: their transitions
out, hidden without one) or brings them in. `0x0048BE30` puts an exhaust on the car: the car's exhaust (`+0x0F`) the
part and its SHAPE in the slots the garage's car has (EXHAUST_S, EXHAUST_C, EXHAUST_O); -1 the CarDef's
STANDARDEXHAUST (nothing at all without one).

`suppliers` (`0x00440B80`, update `0x00440CC0`): enter empties Menu, adds "STOCK" (102, value -1) for the wheels (kind
4) when the car has a wheel, and an item for each supplier of the kind's parts of the catalogue's level (as
`performmod`), then the common enter. SELECT on an item: its supplier (`[0x00715C30]`, -1 for -1), CLICK, and by the
kind `suspensionmenu` (0), `exhaustmenu` (1) or `wheelsmenu` (4). BACK (CLICK): `stylingmenu` for the wheels, else
`performtype`.

`exhaustmenu` (`0x00440EF0`, update `0x004411C0`, leave `0x00440320`): enter holds the garage's rig (its pitch and
yaw 0 from `[0x0079E798]` and `[0x0079E79C]`, which nothing writes; no turn; input after 0.25 s), keeps the car's
exhaust (`+0x18`), fills Menu with the supplier's exhausts of the level (no text, the part's place, its UIOFFTEX and
UIONTEX, its SHAPE), puts the highlighted one on the model (`+0x14`; past the list the first), shows SuppLogo
`UI_<supplier>_ON` and the money, then the common enter, the camera to the exhaust, the price and its box in and the
garage's texts cleared. Update: another highlighted item puts its part on the model and becomes `+0x14`
(`[0x0075ED08]`); SELECT: the car's own exhaust notes "You already have this mod" (type 1), else the rating with the
part (the kind's part the item's, rated, the exhaust back to `+0x18`, rated again), CLICK and `buymod`. Bought: the
car's exhaust the item's and `tuningmenu`. Else Name shows the part's TEXT and the price its COST. Without a window
over the windows `performance` comes. BACK (CLICK): `suppliers`, the car's exhaust back on the model, the garage's
texts cleared. The leave keeps the highlighted item and sends the price and its box out.

`suspensionmenu` (`0x00440560`, update `0x00440620`): enter sends the price and its box out, sets the price 0 and
HEIGHTSLIDER to the car's ride (`+0x58`, kept). Update: bought (CLICK) opens `tuningmenu`; the chassis is lowered,
then the ride takes the slider's value. SELECT (CLICK): `buymod` (the price 0). BACK (CLICK): `tuningmenu` and the ride
back.

`enginechip` (`0x004407B0`, update `0x004408E0`): enter brings the price and its box in, the price 250, CHIPSLIDER at
(the chip (`+0x60`) - 1) x 10 + 0.5 (the chip kept), the rating with the chip at 1 and the car's rating (cut). Update:
bought (CLICK): `tuningmenu`, the chip 1 + the chosen part, the price and its box out. NEWBHP: the chosen part is
(the slider - 0.5) x 0.1 and the text the rating x (1 + it) to the nearest. SELECT (CLICK): the car's own rating
opens `tuningmenu`, another goes to the garage's rating and `buymod`. BACK (CLICK): `tuningmenu`, the chip back, the
price and its box out. Its menu item `chip` is reached in `tuningmenu` like the others.

`gearboxmenu` (`0x004425B0`, update `0x004426B0`, leave `0x00440320`): enter brings `performance` (mode 8), sets
GEARSLIDER to the car's gearing (`+0x5C`, kept), brings the price and its box in, the price 500, no question.
Update: the slider takes the direction held; the chassis is lowered, then the gearing takes the slider's value. After
the question a "yes" pays the price, notes it on the car and the profile, CLICK, `tuningmenu`, the car's gearbox
(`+0x17`) 1, the garage's texts cleared and the money shown; anything else clears the question. SELECT: the gearing
as it was gives CLICK, `tuningmenu` and the texts cleared; with the money under the price "INSUFFICIENT CASH" (type
1) and NO; else "NEW MOD PRICE" (1829), the price, a line feed and "ARE YOU SURE?" (CLICK, type 3, on NO). BACK: the
gearing back, CLICK, `tuningmenu`, the texts cleared.

## schememenu

Record `0x0071FDE0` (vtable `0x006F03A4`): enter `0x0043C600`, update `0x0043CAB0`, no leave; over `carsel` (mode 5).
The crew's scheme is kept in the settings (`[game + 0x64] + 0xF4`, 17 values): the base, metallic and pearl colours
and amounts, the dial colours, the tint, the sponsor decals (`+0x48`), the decal colour (`+0x4C`), the decal, its
colour, the wheel colour and its flag, the wheel, the neon and its colour. Enter keeps the car's look the same way
(`+0x1C .. +0x5C`), not applied (`+0x60`), the answer -1.

Update: a "yes" to saving puts the kept look in the settings' scheme, to applying pays its price (`+0x64`) and shows
the money; CLICK and the window closes. With menu's apply (1) highlighted the scheme goes on the car once (a decal 0 in
the scheme keeps the car's, a prototype (`0x004A8F50`) taking 0); set (0) after it puts the kept look back
(`0x0043C750`). Either brings the decal, the wheels, the decal colour, the wheel colour, the paint, the tint and the
neon (shown or hidden) onto the model. SELECT first makes the scheme's tint and amounts the kept ones when within
0.05. Set: the scheme as the look "THIS SCHEME IS ALREADY SAVED" (1919), else "SAVE THIS SCHEME. YOUR EXISTING CREW
SCHEME WILL BE OVERWRITTEN!" (1916) and "ARE YOU SURE?" (type 3, on NO). Apply: alike (`0x0043C9F0`, the decal not
compared when the look has decal 0) "THIS SCHEME IS ALREADY APPLIED" (1920); else the price: 1000 for another base,
3000 for another metallic none of whose red, green and blue is 0, 5000 for another pearl not the base, 500 for
another wheel colour, 299 for another tint, the wheel's COST for another wheel, 2000 for another decal; with the money
over it "APPLY YOUR CREW'S SCHEME TO THIS CAR." (1917), the price and "ARE YOU SURE?" (type 3, on NO), else the price
and "INSUFFICIENT CASH" (type 1) and NO. BACK: the kept look back, CLICK, the window closed.

## The paint windows

`custmenu`'s paint keeps the car's base, metallic and pearl colours (`0x00767D90 .. 0x00767D98`), clears the paints
charged (`0x00728B00` base, `0x00728AFC` metallic, `0x00728AF8` pearl) and sets `[0x00715C24]` (the next base change
also clears the metallic colour). The paint chosen is `[0x00728B0C]` (0 base, 1 metallic, 2 pearl, 3 the decal), the
price of the changes so far `[0x0075ECFC]`. The paint is put on the model by `0x0048BF90` (Car Colour, Metallic
Colour, Metallic Shininess 30 - the amount x 25, Pearlescent Colour, Pearlescent Shininess the amount x 6 + 0.01).

`colourmenu` (`0x0043D270`, update `0x0043D390`): enter sends the camera to the garage's place, takes the wheel item
out when the car has no wheel (the fourth highlighted then the first), keeps the metallic and pearl colours and
amounts (`0x00767D88`, `0x00767D8C`, `0x00728B08`, `0x00728B04`), highlights the first item for another car than
`+0x24`, hides every BUY_ element. Update: with changes to pay, once, the price theirs, the price and its box in, the
BUY_ elements in and BUY_TEST "BUY PAINT" (1930). LEFT, RIGHT or the first frame hide LOCKED and price the highlighted
item: base the changes + 1000 (uncharged), metal + 3000, pearl + 5000 (LOCKED under the car's level 0 or 1, the
profile's `+0x474 + model`), wheel 1000. SELECT: the money under the price "INSUFFICIENT CASH" (type 1) and NO; else
CLICK and base, metal or pearl (not locked; else NO) `colourchoose`, wheel `wheelcolourmenu`. PGDN with changes: CLICK
and "CONFIRM NEW CAR COLOR" (1786), the price, "ARE YOU SURE?" (type 3, on NO); a "yes" pays them, notes them on the car
and the profile, clears the garage's texts and goes to `custmenu` (a metallic paint sets the car's `+0x50`, a pearl its
`+0x54`). BACK (CLICK) with changes asks "LOSE PAINT CHANGES" (1931); a "yes" puts the kept paint back and goes to
`custmenu`; without changes `custmenu`.

`colourchoose` (`0x0043DB90`, update `0x0043DD60`, leave `0x00440320`): enter shows HELP and makes SELCOLOUR look for
the paint's colour (the decal: the pearl's), keeps it (the decal: the car's `+0x4C`), brings the price and its box in.
Update: the garage's zoom goal 1; the chart's colour goes to the paint each frame (base: the base and the pearl, and
the metallic cleared on the first change; metallic or pearl: theirs; the decal: `+0x4C`); GRIDX and GRIDY "X<column>",
"Y<row>"; the paint on the model. BACK puts the paint and both amounts back (CLICK, `colourmenu`; the decal
`sponsordecalmenu`). SELECT (CLICK): the decal `stylingmenu`; metallic or pearl with the money over the price hide
HELP and `colourfocus`; else the base is charged 1000 (once) and `colourmenu` follows.

`colourfocus` (`0x0043E2D0`, update `0x0043E410`, leave `0x00440320`): FOCUSSLIDER at the metallic amount or the
pearl amount x 5; each frame the amount the slider's (the pearl's x 0.2), GRIDX "Z<slider x 100>", the paint on the
model. SELECT: the metallic charged 3000 or the pearl 5000 (once), CLICK, `colourmenu`. BACK (CLICK): `colourchoose`.

`wheelcolourmenu` (`0x0043F3F0`, update `0x0043F600`, leave `0x00440320`): the price 500 with its box; COLOURMENU an
item `ui_wheel_off` / `_on` in each [WheelColour] of the CarDefs; the car's own colour (`+0x40` set) is `+0x10` (else
2) and highlighted. Each frame the highlighted colour goes on the car (`+0x3C`, `+0x40` 1) and its wheels. SELECT
(CLICK): the own colour "You already have this mod", another `buymod`; bought goes to `colourmenu`. BACK (CLICK):
`colourmenu` and colour `+0x10` back on the car.

## The styling windows

`stylingmenu` (`0x0043B060`, update `0x0043B4A0`): enter sends the camera to the garage's place, shows the window's
help page with the money under 10000, takes out the items of the body part categories 0 .. 8 (fbump, rbump, bonnet,
spoil, skirt, headlight, -, wmirrors, decal) whose list is empty or holds one part and a brand (the list's `+0x14`),
and the sponsor item when the car has no performance part (`+0x0E .. +0x16` all none); another car than `+0x18`
highlights the first item. A question (`+0x10`, `+0x14`) that nothing asks would switch the car's `+0x48`. Each frame
the highlighted item sets the garage's rig (`0x00436340`: pitch, yaw, no turn, input after 0.25 s); the pitches come
from `[0x0079E7A0 ..]`, which nothing writes (0), but spoil -0.35, bonnet -0.2, neon -0.2 and tints -0.25; the yaws
fbump 3.14, rbump 0, wmirrors 2.14, spoil 4.5, skirt 4.1, bonnet 3.28, headlight 3.14, wheels 3.75, decal 5, neon -2,
hud -2, ice, horn, tints and sponsor -1.57. SELECT (CLICK): a body part `modmenu` with its category
(`[0x00715C28]`); wheels the catalogue's level 0 and `suppliers` for the wheels; decal `decalmenu`, neon `neonmenu`,
hud `hudcolourmenu`, ice `icemenu`, horn `hornmenu`, tints `tintsmenu`, sponsor `sponsordecalmenu`. BACK (CLICK):
`custmenu`.

`modmenu` (`0x0043BDE0`, update `0x0043C1B0`, leave `0x00440320`): enter keeps the car's part of the category
(`+0x10`), hides LOCKED and fills Menu: "None" (164, value 127, the price 500) for ALLOW_NONE, then each part of the
CarDef's list: the first one or one without a brand "STOCK" (102, its place; only when the car has a part),
another `ui_<brand>_off` and `_on` at its place; ARROWLEFT and ARROWRIGHT shown; after the common enter the car's
part highlighted (one later after "None"; past the list the first); the price and its box come in. Update: the arrows
hidden at the ends; another highlighted item puts its part on the car and the model (`0x0048BC30`) and prices it:
500 for "None", 0 for the car's own, else 500 x (place + 3) + 500 (`0x00447660`); LOCKED shows for a part whose
place - 1 passes the profile's level for the model and category (`+0x262 + model x 10 + category`). SELECT: a locked
part NO; else CLICK and the car's own "You already have this mod", another `buymod`. BACK: the car's part back, CLICK,
`stylingmenu`, the garage's texts cleared. Bought: `stylingmenu`.

`wheelsmenu` (`0x00439300`, update `0x00439730`, leave `0x00439630`): enter sets the price 0 and keeps the car's wheel
(`+0x20`). For "STOCK" (the supplier -1) the DATA_ elements are hidden, the price and its box come in, `buymod` comes
over the window and the standard wheels go on the model. Else the DATA_ elements show, Menu holds the supplier's
wheels of the level but the car's own (their UIONTEX, their place in the list, their SHAPE), SuppLogo
`UI_<supplier>_ON`, the money shown, the price and its box in. Update: bought: `stylingmenu`, the car's wheel colour
cleared (`+0x40`); "STOCK" otherwise `suppliers`. The highlighted wheel: data_name its TEXT, the price its COST,
data_ize its DATAVAL; another item puts it on the car and the model (`0x0048ABC0`, which sets the car's `+0x12`).
SELECT: the car's own "You already have this mod", else CLICK and `buymod`. BACK (CLICK): `suppliers`, the car's
wheel back with its colour, the garage's texts cleared. The leave keeps the item and sends the price and its box out;
"STOCK" not bought puts the car's wheel back, a wheel bought clears the wheel colour.

`tintsmenu` (`0x00440250`, update `0x004403B0`, leave `0x00440320`): the price 299 with its box; TINTSLIDER at the
car's tint (`+0x44`, kept). Each frame the tint is the slider's, on the model (`0x0048C040`). BACK (CLICK):
`stylingmenu` and the tint back. SELECT (CLICK): another tint `buymod`, the same `stylingmenu`. Bought:
`stylingmenu`.

`neonmenu` (`0x0043E690`, update `0x0043E9E0`, leave `0x00440320`): LOCKED hidden. Without a neon (`+0x0D` none)
the neon shows under the car (`0x00436160`: gen_neon the size of the chassis, the car model's neon material), MENU is
hidden and COLOURMENU shown, the price 1000 with its box; with one MENU (remove, change) shows. COLOURMENU holds each
[NeonColour] of the CarDefs in its colour (`ui_neon_off`, `ui_neon_choose`; valued -1 under the profile's neon level
0, `+0x26B + model x 10`), the car's colour (`+0x2C`, kept) highlighted; the price 0. Update: bought (a removal with
MENU shown: the neon none and hidden) goes to `stylingmenu`. With COLOURMENU shown the highlighted colour is the
car's on the neon (parameter 0, parameter 4 1), the car's neon 1, the price 1000, LOCKED for a locked colour. BACK
(CLICK): `stylingmenu`, a car without a neon before loses it again, the colour back. SELECT (CLICK): remove `buymod`,
change COLOURMENU with the price 1000; a colour: the car's own "You already have this mod", a locked one NO, another
`buymod`.

`hudcolourmenu` (`0x0043ED70`, update `0x0043F120`, leave `0x00440320`): the price 100 with its box; the car's dial
colours (`+0x30`, `+0x34`) kept; COLOURMENU each [DialColour] of the CarDefs in its first colour (`ui_dials_off`,
`ui_dial_choose`: not in the game's files, so the highlighted dial is drawn with `ui_dials_off`, 04-interface.md),
the car's highlighted; MPH "MPH" or "KPH" (1494, 1495) by the settings' units. Update: the
highlighted pair goes on REVSBG and TIME_BG (the first, made opaque) and the fonts of TIME, SPEED and GEAR (the
second); bought puts them on the car and goes to `stylingmenu`. BACK (CLICK): `stylingmenu`, the colours back.
SELECT (CLICK): the car's own "You already have this mod", another `buymod`.

`icemenu` (`0x0043F7F0`, update `0x0043FB40`, leave `0x00440320`): the price 199 with its box; the car's ICE
(`+0x15`, kept); the catalogue's level 0 and MENU the ICE parts of the suppliers 40 and 41 (their UIONTEX, their
index, their SHAPE), the car's highlighted. Update: another highlighted item shows HEAD_LOGO `ui_<supplier>_on`, puts
the part on the car, HEAD_NAME its TEXT and the price its COST. BACK (CLICK): `stylingmenu` and the ICE back. SELECT:
the car's own "You already have this mod", another CLICK and `buymod`. Bought: `stylingmenu`.

`hornmenu` (`0x0043FEC0`, update `0x00440040`, leave `0x0043FF90`): the price 399 with its box; the car's horn
(`+0xB0`, kept; MENU's item horn - 1 highlighted). Update: another highlighted item stops the horn playing and plays
`carhorn<item + 1>`; the car's horn becomes the item + 1 (`0x00440124`), the sample's number. BACK (CLICK):
`stylingmenu` and the horn back. SELECT: the car's own "You already have this mod", another CLICK and `buymod`. The
leave also stops the horn.

`decalmenu` (`0x00439AD0`, update `0x00439E50`, leave `0x00440320`): enter keeps the car's decal (`+0x0C`) and the
place of its decal colour (`+0x38`) in [DecalColour] (twice; nothing changes either), fills Menu with the CarDef's
decals (the picture `UI_` and the mesh name from "DECAL" on; the first one "LOCKED" (775) unless the car is a
prototype, `[0x0075ED0C]`; the others valued their place) and "None" (164, 127) for ALLOW_NONE; after the common
enter the price and its box come in, the garage's texts are cleared, the DECAL_COLOUR elements hidden and
DECAL_COLOUR_CHART looks for the car's decal colour (kept). Update: bought goes to `stylingmenu`. With the chart
shown its colour is the car's decal colour on the model (`0x0048BEF0`: the material named from "DECAL" on in the
decal's mesh name, of Coloured Spec ABlend, takes it as its colour) and GRIDX, GRIDY show the cursor. The highlighted
decal is priced (none 0, the first 12000, up to 14 1000, under 23 5000, else 9000) and put on the car and the model.
SELECT: the first decal on a car not a prototype notes "Logo for prototype cars only" (778); a colourable decal (1 ..
14) with the list shown shows the chart (Menu hidden); the car's own decal: another colour `buymod`, else "You already
have this mod"; another decal `buymod`. BACK: from the chart back to the list with the colour back; else
`stylingmenu` with the colour and the decal back.

`sponsordecalmenu` (`0x0043A520`, update `0x0043A590`): MENU back to its items (`0x005AECF0`; the window is kept, so the
items taken out at the last enter come back), then without show when the car's sponsor decals are shown (`+0x48`), else
without remove and colour; the highlighted item's help again (`0x005AF860`). SELECT (CLICK): remove hides them and goes
to `stylingmenu`; show (shows them) and colour clear the garage's texts and open `colourchoose` for the decal colour
(paint 3, `+0x4C`) with no price. BACK (CLICK): `stylingmenu`.

## CMSHostMeetingSetup

Record `0x007221E0` (vtable `0x006F56C0`, also `[0x00719070]`): enter `0x004CA9A0`, update `0x004CAAE0`, leave
`0x004CAA90`. The calendar sets its `+0x38` before asking for it. Its fields: `+0x0C` the answer, `+0x10` the meeting's
map (0xC bytes, `0x0045B6B0`), `+0x14` the type (3 .. 5), `+0x18` the location (the crew 0 .. 8 whose territory),
`+0x1C` the criteria (0 none, 1 a region, 2 a make), `+0x20` the region (1 .. 4), `+0x24` the make (1 .. 17),
`+0x2C` the class (0 .. 7), `+0x30`, `+0x34` the cost, `+0x40 ..` crews 1 .. 8 coming.

Enter: with `+0x38` the defaults (type 3, location 0, region 1, make 1, class 7, `+0x30` 1, cost 0,
CMSHostMeetingSetupRace's `+0x20` 0 (`[0x00719074]`), no rounds `[0x00728C70]`); the map made (`0x0045BE00` -0.4); the
texts (`0x004CAE60`); `+0x0C` -1; Cash and Date; the advisor's `+0x60` set (no advice to host).

The texts (`0x004CAE60`): the reason `[0x0075EF70]` 0. TypeText the type's line (140 + type). LocationText the
location crew's name (108 + it) and " (<the track of its first circuit route>)" (575 + the route's location; the map
shows the route, `0x0045BEC0`), white when the crew has its fourth step (`+0x60`) and a respect (`+0x70`) of 1000 or
more, else 0xFFBC381D with reason 1. CriteriaText "None" (164) in 0xFF646432, or the region (188 + it) or the make
(193 + it) in white, and when the opponents' cars (`0x004A9820` on crew 1: of the region or the make, the class, for
type 5 not the model 0x28) are none reason 5. With reason 5 CriteriaText, CriteriaTypeText (184 + the criteria) and
BHPText are 0xFFBA381D, else white. BHPText the class (604 + it) and " (<low> - <high>" (`0x0047FCA0`) "bhp" ")".
CostValue the location's cost (`0x00718FB4`: 1000, 2000, 4000, 8000, 10000, 12000, 15000, 20000, 30000) into `+0x34` and
`[0x00728C6C]`; CashValue the money, under the cost 0xFFBC381D with reason 2; Attendance the crews coming
(`0x004CD580`), 0xFFBC381D under 2.

The crews coming (`0x004CD580`): an event made of the setup (`+0x0D` the location, `+0x10` the type, `+0x14` 5000,
`+0x6C` the class, `+0x60` the region or `+0x64` the make by the criteria, the prize of the rounds and of
CMSHostMeetingSetupRace's `+0x20` from `0x00718FD8`); for crews 1 .. 8 `0x004A9480` into `+0x40 ..`, the count
returned. `0x004A9480`: never the player's crew, always crews of id 1 and 2; else the crew's second step (`+0x58`),
its `+0x264` of the location, not for a crew past id 4 when the type is the profile's `+0x620` or the location its
`+0x624`, and cars for the region, make, model and class (and with `+0x04` prototype cars).

Update: the map's frame (`0x0045BC40`). A "yes" (`+0x0C` 1): CLICK and `CMSMainMenu`. BACK (CLICK): "Cancel event
setup?" (1827) and "ARE YOU SURE?" (type 3, on NO) into `+0x0C`. SELECT (CLICK): without a reason
`CMSHostMeetingSetupRace`, else a note (type 1) of line 1996 for reason 5, else 476 + the reason. LEFT or RIGHT (CLICK)
on the highlighted button: `Type` (3 .. 5), `Location` (0 .. 8), `CriteriaType` (0 .. 2), `Criteria` (the region 1 ..
4 or the make 1 .. 17 by the criteria; nothing with none), `carclass` (the other way, 0 .. 7), each with the texts
again.

Leave: a `Menu` element's highlighted item kept, the map freed.

## CMSHostMeetingSetupRace

Record `0x00722240` (vtable `0x006F56B0`, also `[0x00719074]`): enter `0x004CB550`, update `0x004CB690`, leave
`0x004CB640`. Its fields: `+0x0C` the answer, `+0x10` the type, `+0x14` the route, `+0x18` the laps (the place in
`0x00719044`: 2 .. 12), `+0x1C` the team (1 .. 3), `+0x20` the prize (the place in `0x00718FD8`: 3000/2000/1000,
7000/3000/2000, 12000/5000/3000, 20000/8000/5000, 25000/10000/5000, 30000/12000/6000, 40000/15000/7000), `+0x28` the
map. The rounds set up are kept at `0x00728C74` (0x18 bytes: type, route, laps, team, prize, `+0x14`), their count
`[0x00728C70]`.

Enter: the type the setup's, route 0, laps 0, team 1, the prize the setup's location; the map (`0x0045B6B0`, its route
`0x0045BEC0`); the texts (`0x004CBA80`); `+0x0C` -1; Cash and Date.

The texts (`0x004CBA80`): the reason 0. TypeText 140 + the type. RouteText: a point to point "1", or past route 0 "1
(Reversed)" (127); other types the route + 1, past 3 "<route - 3> (Reversed)"; white for types 3 and 4, else
0xFF646432. The route's distance by the type from the location's crew: circuits `+0x1C4` / `+0x1E4` by the route,
sprints `+0x24C` / `+0x250 + 4 x route`, point to points `+0x22C` / `+0x234` (the map shows it, `0x0045B720`). LapsText:
point to points and sprints "1 (<km>km)" in 0xFF646432, else "<laps> (<laps x km>km)" in white; with the settings'
miles (`+0x94`) "m" and the distance x 0.625. NumCrewText: a sprint sets the team 1 in 0xFF646432, else white; the
team's line (491 + it). prizeMoneyText1 .. 3: "1" "st" " $" and the first place's prize (thousands marked), "2" "nd",
"3" "rd" (lines 37 ..). CostValue: the setup's cost and every round's `+0x14` (`[0x00728C6C]`); CashValue the money,
under the cost 0xFFBC381D with reason 2; Attendance the crews coming (`0x004CD580`), red under 2.

Update: the map's frame. A "yes" (`+0x0C` 1): `CMSMainMenu`. BACK (CLICK): "Cancel event setup?" and "ARE YOU SURE?"
(type 3, on NO) into `+0x0C`. SELECT (CLICK): without a reason the round kept (its `+0x14` 0) and
`CMSHostMeetingSummary`, else a note (type 1, 476 + the reason). LEFT or RIGHT (CLICK) on the highlighted button: `Type`
(for a setup of type 5 only: 3 .. 5, route 0), `Route` (0 up to the location crew's routes of the type less one: point
to points `+0x23C`, circuits `+0x228`, sprints `+0x254`), `Laps` (0 .. 10), `Crewnum` (1 .. 3), `prize` (0 .. 6), each
with the texts again.

Leave: a `Menu` element's highlighted item kept, the map freed.

## CMSHostMeetingSummary

Record `0x0072226C` (vtable `0x006F56D0`): enter `0x004CC3A0`, update `0x004CC500`, leave `0x004CC4B0`. Its fields:
`+0x0C` the cancel answer, `+0x10` the create answer, `+0x14` the map.

Enter: the map (`0x0045B6B0`, `0x0045BE00`; the route of CMSHostMeetingSetupRace's type and route, `[0x00722250]`,
`[0x00722254]`, by `0x0045B720`); the texts (`0x004CC820`); `+0x0C` and `+0x10` -1; Cash and Date.

The texts (`0x004CC820`): the reason 0. Race1Type, Race1Laps, Race1Route, Race1NumCrew, Race11st, Race12nd, Race13rd
"-"; location the track of the location crew's first circuit route (575 + its location; the map's route
`0x0045BEC0`); event the setup's type (140 + it). For each round n kept (`Race<n>..`): its type's line; the laps "-"
for point to points and sprints, else the laps of `0x00719044`; the route "1" or "1 R" for a point to point (past
route 0), else "<route - 3> R" past 3, else "<route + 1>"; the team; the prize's places in thousands "$<n>k" (marked).
CostValue the setup's cost (`[0x00728C6C]`); CashValue the money, under the cost 0xFFBC381D with reason 2; Attendance
the crews coming (`0x004CD580`), red under 2.

Update: the map's frame. A "yes" to cancel (`+0x0C` 1): `CMSMainMenu`. BACK (CLICK): "Cancel event setup?" and "ARE
YOU SURE?" (type 3, on NO) into `+0x0C`. SELECT on `menu`'s item of value 1 while no round is kept (CLICK):
`CMSHostMeetingSetupRace`. A "yes" to create (`+0x10` 1): the event made (`0x004CD6F0`). SELECT on the item of value 2
(CLICK): with a reason its note (type 1, 476 + it); with no round reason 3 and "Cannot Host Event - You need at least
one race" (479); else the setup's `+0x38` set and "Create event. Cost: " (1826), the cost and "ARE YOU SURE?" (type 3,
on NO) into `+0x10`.

Leave: a `Menu` element's highlighted item kept, the map freed.

The event's making (`0x004CD6F0`): an event with the calendar's `+0xF8C` as its slot (`+0x07`) and `+0xF90` as its day,
the cost paid (as `0x004B8200`), the type, the location (`+0x0D`), crew 0, the fee 5000, the class and the region or
make; for each round kept (at `+0x1C`, 0x38 bytes each; one fits before the event's `+0x54`) used (`+0x00` 1, `+0x01`
0), its type, the prize's places (`+0x2C ..`, their sum into the event's `+0x18`) and by the type: a point to point the
location crew's route and km of the round's route (`+0x1C`, `+0x20` the route, `+0x28`), the team (`+0x17`) the
round's, laps 1, `+0x0C` 6; a circuit the same with the laps of `0x00719044` and the km times them; a sprint the
crew's `+0x24C` and `+0x250` (+ 4 x the route), team 1, laps 1, `+0x0C` 3; `+0x54` the rounds. For each round: `+0x16`
and `+0x18` 0, `+0x08` 1 for a team over one, the crews' flags (`+0x0D`) cleared; the crews 0 .. 8 shuffled by 50
swaps of `rand() % 9` and `rand() % 9`; then in that order each crew's coming (the setup's `+0x3C + 4 x crew`) goes to
the event's `+0x56`, and while the round has under 6 racers (`+0x18`) and fewer crews than `+0x0C`, a crew coming takes
a place (`+0x18` by the team, `+0x16` one more, its `+0x0D` flag). Then the teams (`0x004A4410`), the profile's
`+0x620` the type, the event into the calendar's first free slot (marked used), the event of that slot and day chosen
(`[0x00728C50]`, the profile's `+0x610`; `[0x00728C5C]` and `[0x00728C58]` its slot and day), both note lists emptied
(`0x004B5CC0`), the days passed (`0x004A6530`) until the calendar is at its slot and day, and the meeting
(`0x004C27A0`).

## Car_Stats2 and Car_Stats2purple

One class (vtable `0x006F5258`; records `0x00721DD8` and `0x00721F20`, registered by `0x004C5470` with
`Car_StatsLogos`, `0x00722064`): enter `0x004C42D0`, update `0x004C4770`, leave `0x004C4710`. `car_stats2purple`
opens over CMSRaceResults (the winning car, PGDN), the betting window (the car of the leader of the crew bet with),
CMSRaceSetup and CMSCarCollection2 (PGDN on a garage car); `car_stats2` over the garage and the arcade's car choice.
The car is `[0x00728C60]`. The record: `+0x0C` the car's model id, `+0x10` a car of the player's garage (`[0x00721DE8]`,
`[0x00721F30]`: set by CMSRaceSetup and CMSCarCollection2, cleared by CMSRaceResults and the betting window),
`+0x14 ..` the blinking elements (their count `+0x140`).

Enter: `BHPVAL` the rating (cut), `CARVAL` the model's display name, `CLASSVAL` the class + 1 (`0x0048E9E0`), `Spent`
(`+0xA8`) and `Winnings` (`+0xAC`) as money, each "NA" (432) when not over 0, `Value` the sale worth (`0x0048DA40`).
Then the parts' icons (`0x004C4920`), each the first element of its name:

- `KeyIcon3` blinks.
- The catalogue's kinds 0 .. 6 and 8 (the table `0x004C520C`): icons 2, 3, 7, 6, 6, 4, 5 and 1, named `P<n>-<l>` (kind
  4 `V<n>-<l>`), levels 1 .. 4. Each is shown: `ui_mod_got` for the level (`+0x2B`) of the car's part of the kind
  (`+0x0E`; the kind's first part past its list, none for 0xFF), else `ui_mod_locked`, and, unless the level is past
  the profile's `+0x50` value of the model and kind while the car is the player's, `ui_mod_available`, which blinks
  from the car's part's level on.
- The body categories 0 .. 4 and 9 (the table `0x004C5234`): icons 1, 4, 2, 5, 3 and 8. A category with parts (more
  than one, or one with the list's `+0x14` 0) colours its box `VB<n>` white (each corner, the alpha floats 1) and
  shows `V<n>-<l>` (levels 1 .. 3): `ui_mod_got` below the car's part (`+0x04`), else `ui_mod_locked` and, unless past
  the profile's `+0x262` value while the car is the player's, `ui_mod_available`, blinking. Without parts `VB<n>` is
  dark grey (0xFF282828) and every `V<n>-<l>` hidden.
- The car's `+0x50` is set when its metallic colour (`+0x1C`) is not its base colour (`+0x18`, RGB), `+0x54` the same
  for the pearl (`+0x20`).
- `V7-1` and `V7-2` (the paints) are shown: `ui_mod_locked` past the profile's `+0x474` value of the model while the
  car is the player's, else `ui_mod_available`, then `ui_mod_got` for `V7-1` with `+0x50` and `V7-2` with `+0x54`, else
  blinking.

A garage car shows `Logo_But` and `Logo_Text` (`[0x00728C64]` 0); another car asks for `car_StatsLogos` (mode 8, the
front end's `+0x30`), hides every `Logo_But` and `Logo_Text` and sets `[0x00728C64]`.

Update: the phase `[0x007A2D40]` grows by the frame time x 8 and goes back to 0 past 6.282. With a garage car the
blinking elements take the alpha (sin(phase) + 1) x 0.5 (`0x0042E4F0`), and PGDN (event 6, CLICK) takes
`car_StatsLogos` off (the front end's `+0x30` view and object freed) or asks for it again, flipping `[0x00728C64]`.
BACK (CLICK) closes the window (mode 6).

Leave: a `Menu` element's highlighted item kept, the blinking elements' alpha back to 1.

`Car_StatsLogos` (enter `0x004C5260`: for the kinds 1 .. 9 `Logo<n>` and `Stock<n>`, a car's part of the kind showing
`ui_<the part's supplier (+0x2A)>_on` in `Logo<n>` and hides `Stock<n>`, else hides `Logo<n>` and shows `Stock<n>`)
is the window over the windows.

## Windows the game never registers

Two window classes are in the executable without their factory entries ever being handed to the window registry
(`0x005A8E30`), so no name opens them.

An earlier career menu (vtable `0x006F58C8`, entry `0x00722284`): enter `0x004CDBF0` writes Date and Cash
(`0x004CE040`, then `0x004A4130`) and keeps SELECTIONIMAGE; each frame (`0x004CDC50`) a new highlighted item of Menu
gives SELECTIONIMAGE the picture of its name (calendar, cars, respect, crew, mobile, career: `ui_hubbg_calendar`,
`_cars`, `_respect`, `_crew`, `_mobile`, `_summary`) and starts its transition again, then the phone's frame;
SELECT (CLICK) goes by the item: Cars to a window named "" (none), Calendar CMSCalendar, Respect CMSRespect, Crew
CMSCrew, Mobile CMSMobilePhone over it, Career CMSCareerSummary; BACK (CLICK) asks the autosave with saving on and
leaves for menu. Neither SELECTIONIMAGE nor the `ui_hubbg_*` pictures are among the game's files, and the menu of
`cmsmainmenu.ui` names its items otherwise.

A repair window (vtable `0x006F0404`, entry `0x007203C4`), its `.ui` not among the game's files: newmenu's items
brakes, tyres, nos and all (`0x00445660`: on with the brakes' wear `+0x7C`, the tyres' `+0x74`, the nitro not full
with a nitro part, any of them); Condition1 .. 3 `ui_condition_bad` / `_mid` / `_good` (a wear over 0.6 / over 0.3 /
else; the nitro left / 15 over 0.9 good, over 0.75 middling, else bad; Condition3 hidden without a nitro part);
CostText1 .. 3 the costs as money or "NA" (432). The costs: brakes (`0x00446030`) the brake part's COST x 0.3 (100
for 0), tyres (`0x00445FA0`) the tyre part's x 0.3 (200), the nitro (`0x00445F10`) the nitro part's x 0.1 (0 full or
without a part), none capped. SELECT asks "<item's line> <cost>\nARE YOU SURE?" (1500 .. 1502, all 1499; 431 "No
maintenance required" for a cost of 0, 1884 "INSUFFICIENT CASH" and NO when short); a yes repairs (`0x004463D0`,
`0x00446270`, `0x004460C0`, `0x00446530`: the cost paid cut, noted on the car `0x0048F270`, the wear 0 or the tank 15
and for the nitro alone the leak `+0x70` cleared; "Maintenance complete" 430), writes the texts again and closes the
window (mode 6); BACK closes it.
