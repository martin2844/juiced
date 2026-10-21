# The race's HUD

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## The window

The race front end (`[G+0x3C]`, made by `0x004563E0`: its window manager "race", the messages of each player, the HUD
state `+0x54`) takes the window the race camera's views choose ("Choosing the window", below). So the HUD comes
when the sweep before the start gives the player's view (the count below 4 s or the sweep done) and stays through the
race.

All hud_* windows share one state (0x24C bytes, `0x0044D9F0`, vtable `0x006F1438`): enter `0x004476D0`, frame
`0x0044B1C0`, leave `0x0044A450` (each player's messages emptied, the crew orders' fields cleared). It keeps its
elements by name; an element the window lacks is left out (with two exceptions noted).

The race front end is made at the end of the race's load (`0x00461390`, on the loading thread, after the cruise
manager's zones `0x004B0330`): it loads its windows (`0x005A8E30` registers, `0x005A8430` loads) by the race's type (the
table `0x00456A24`): 0 .. 2 hud_challenge_results and hud_solo, 3 and 9 (the test track) hud_solo, 4 hud_cruise_results
and hud_cruise, 5 and 7 hud_race, 6 hud_sprint, 8 none of them; then blank, blank_split, hud_pause, hud_race_complete,
hud_race_results, hud_sprint_results, hud_replay, restart_race, start_replay, restart_replay, closedown, hud_spectator.
After hud_solo, hud_race or hud_sprint loads, the map is made (`0x0044A100`, below), which draws the loading thread's
`rand()` (four values for each mesh node of each model).

## Enter (`0x004476D0`)

The route's index (`0x00426490`) and its lap record (`+0x68`, the records object `[G+0xA8]`: per route 40 bytes, the
record at `+0x2C`; `0x0040A110` gives 300 s, 0 for a route of type 3). The world record (`+0x6C`) comes from the
online records only (`[0x0075F030]`, `0x004ED490`); without them it is -1, and an online record of 0 becomes 300 but
for type 4.

For each player i (`[G+0x6C]+0x58` of them; names with i + 1): the car's HUD colours, the car record's `+0x30` (c30)
and `+0x34` (c34), each with its alpha set to 255. Then:

| element | at | done |
|---|---|---|
| `REVS%d`, `SPEEDO%d` | `+0x84`, `+0x8C` | dials |
| `SPEED%d` | `+0x94` | the font colour c34 (written without a check: a window without it would fail) |
| `MPH%d` | `+0x9C` | the text MPH (line 1494) with the settings' `+0x40` set (the English game's default), else KPH (1495) |
| `GEAR%d` | `+0xA4` | colour c34 (no check) |
| `GEARLIGHT%d`, `POS%d`, `POSTH%d`, `POSNUM%d`, `LAP%d`, `LAPNUM%d` | `+0xDC`, `+0xAC`, `+0xB4`, `+0xBC`, `+0xC4`, `+0xCC` | |
| `TIME%d` | `+0xD4` | a fixed advance (below), colour c34 |
| `ABSLIGHT%d`, `TCLIGHT%d` | `+0xE4`, `+0xEC` | |
| `WINNER%d`, `WINNERBOX%d` | `+0xFC`, `+0xF4` | hidden |
| `MSG%d` | `+0x104` | its text emptied; colour the larger of c34 and c30 as numbers, also the first player's messages' colour (`+0x18`); not highlighted (`0x005B2930`) |
| `MAPGLOW` | | colour c30 (the four corners, `0x00434D20`) |
| (REVS) | | the needle's colour the larger of c34 and c30 |
| `REVSBG%d`, `SPEEDOBG%d`, `REVSBG_ADDITIVE%d`, `SPEEDOBG_ADDITIVE%d`, `REVSOPAQUE%d`, `SPEEDOOPAQUE%d`, `NOSOPAQUE%d` | `+0x16C` .. `+0x19C` | colour c30 |
| `COUNTDOWN%d` | `+0x1B4` | hidden |
| `DRIVERAID%d` | `+0x1C4` | |
| the curve warning | `+0x1BC` | made once: a Box (`0x005B2A80`) at (0.45, 0.185) of 0.1 x 0.1333 (race mode 3: (0.445, 0.148) or for the second player (0.445, 0.604), 0.1 x 0.077), colour 0xFFFFFF00, hidden, put at the end of the window's list |
| | | `DRIVERAID`, then `MSG`, go to the end of the window's list (`0x005A9CA0`) |
| `WRONGWAY%d` | `+0x11C` | hidden; its fade `+0x124` 0 |
| `SPLIT%d` | `+0x12C` | to the end of the list; its timer `+0x134` 0 |
| `LR_TIME%d` | `+0x10C` | alpha 1, shown, a fixed advance, colour c34 |
| `DIGITAL_BG_COMBO%d`, `_POINTS`, `_BEST`, `_SCORE%d`, `_BONUS`, `_TIME%d` | `+0x13C` | each found: colour c30; they share the field, the last search stays |
| `DIGITAL_BG_RECORD%d`, `DIGITAL_BG_WORLD%d` | `+0x144`, `+0x14C` | colour c30 |
| `TIME_BACK%d`, `LR_TIME_BACK%d`, `WR_TIME_BACK%d` | `+0x154` .. `+0x164` | colour c34, hidden |
| | `+0x1AC` | the panels' fade timer: 10000000 for the types 1, 2, 6, 4, 3, 0 (`0x00448E01`), else 3 |
| `WR_TIME%d` | `+0x114` | alpha 1, a fixed advance, colour c34; it and `DIGITAL_BG_WORLD` hidden without a world record (below 0) |
| | | the player's best lap of the race (`R+0xF8`) above 0 takes the world record and the lap record where it is below them |
| `LR_LAPTIME%d` | `+0x220` | alpha 1, shown |
| `WR_LAPTIME%d` | `+0x228` | hidden without a world record, else alpha 1, shown |
| | `+0x1A4` | -1: the speedometer's end, found at the first frame |
| `NOSMETER%d` | `+0x218` | with a nitro part (`V+0x184`); without, every element named `NOSBG` and `NOSMETER1` (the second player: `NOSBG2`, `NOSMETER2`) is hidden |

The fixed advance (`0x005B1BF0`): the element's fonts (Font and HighFont) step every character but the space by
the screen's width (`[0x0071E5B8]`) x 12 x 0.0015625 pixels, cut toward zero (19 at 1024, 24 at 1280); the font
objects are the window's, so every text of those fonts takes it.

Then once: five Boxes `RIVAL0` .. `RIVAL4` (`+0x1CC`; a found `RIVAL%d` is replaced): (0, 0.881) of 0.094 x 0.135,
the texture `ui_behindyou`, colour 0xFFFFFF00, hidden, at the end of the list. The count `+0x14` = the race state's
count (`+0x38`) cut toward zero, done `+0x10` = it is 0. By name, case aside: `score` `+0x1C`, `combo` `+0x20`,
`regionmessage` `+0x24` (colour c34), `Menu` `+0x28`, `MenuPoints` `+0x2C`, `Points` `+0x30` (colour c34), `best`
`+0x34`, `dist` `+0x38`, `REV_METER` `+0x23C`, `CARAHEAD` `+0x3C`, `CARBEHIND` `+0x40` (their timers `+0x60`,
`+0x64` 0), `RESPECT` `+0x54`, `RESPECT_ICON` `+0x58`, `RESPECT_TEXT` `+0x5C` (c34 and c30 here are those of the last
player's car). Type 2: `SPEEDCHALLENGEBG` colour c30, `SPEEDCHALLENGE` `+0x230` colour c34; else every element with
`SPEEDCHALLENGE` in its name is hidden. `TOPSCORE` `+0x44` and `WR_TOPSCORE` `+0x4C` colour c34 and alpha 1,
`TOPSCORETITLE` `+0x48`, `WR_TOPSCORETITLE` `+0x50`; without a world record the WR ones are hidden. `best`, `score`,
`combo`: a fixed advance and colour c34.

The challenges' targets: type 2 the first `LR_TIME` " %d " of the challenge's speed (`[G+0x6C]+0x78`, x 3.6 in KPH,
cut toward zero) and MPH or KPH; types 0 and 1 the challenge's lap time (`+0x74`); both set `LR_LAPTIME1` to TARGET
(line 43).

The turbo's dial `TURBO1` (`+0x234`) with a turbo (`V+0x180`) or the car record's `+0x68` (the turbo lost to damage):
its needle 0xFFFF5050; else every element with `TURBO1` in its name is hidden. `RESPECT`, `RESPECT_ICON`,
`RESPECT_TEXT` hidden, and every element with `DONTDRAW` in its name. The crew's orders (`0x004BDA30`): in the career
(mode 2) with the profile's `+0x614` 0 the elements with `RP_` in their names show with their transitions, else they
are hidden; `DpadBg1` .. `3`, `Dpad1` .. `3`, `DpadControl01` .. `03` are hidden outside the career and in a career
race without crew mates. Type 6: "HEAT" (554) + " n / heats" (the race's config `+0x08` + 1, `+0x0C`) a message of
4 s to the first player (`0x004561A0`), its LR_TIME and LR_LAPTIME shown.

## Each frame (`0x0044B1C0`)

The front end's frame first steps each player's messages (`0x00456300`, below). For each player, with V the player's
car (`P+4`, else the car watched) and R its competitor:

- **The records' panels.** While R's race time (`+0xF0`) is above 0 the timer `+0x1AC` loses the frame's time; the
  alpha of `DIGITAL_BG_RECORD` and `DIGITAL_BG_WORLD` (each corner's alpha, `0x0042E4F0`) and of the texts `LR_TIME`,
  `WR_TIME`, `LR_LAPTIME`, `WR_LAPTIME`, `WR_TOPSCORE`, `TOPSCORE` (`0x00447530`) is the timer below 1, 4 - the timer
  between 3 and 4, else 1.
- **The winner.** R finished (`+0x118`): `WINNERBOX` and `WINNER`, when hidden, show with their transitions anew.
- **Wrong way.** The fade `+0x124` gains the frame's time while R goes the wrong way (`+0x60`), else loses it, held
  within 0 .. 1; it is `WRONGWAY`'s alpha and the text is hidden at 0.
- **The rev counter** (`REVS`): with r the engine's speed (rad/s) and the torque curve's last point L (rad/s): while
  L x 9.310564 is not below r x 9.549296 the share is r / PowerRange's top x 9.549296 within 0 .. 1.2 and the needle
  turns to min + (max - min) x share; past it the needle jumps (the shown angle too) to 1.1 or 1.15 of the range, by
  a flag that flips each time (`[0x00725370]`, kept for the whole game, 1 at its start). `GEARLIGHT` is hidden;
  `REV_METER`'s value is 0.725 (`[0x0072536C]`) x the share.
- **The speed.** v = |the car's speed along itself| (`V+0x168` `+0x1A0`); the speedometer (`SPEEDO`) turns to
  v / its end x the range (the end, `0x004F7460`: the torque curve's last point / the top gear's ratio / the first
  wheel's FinalDrive x its radius); v x 2.235 (MPH) or x 3.6, cut toward zero, as "%03d" into `SPEED` and
  `SPEEDCHALLENGE`. `ABSLIGHT` shows while the ABS works (`+0x384`) above 5 of those units (without `SPEED` the
  comparison takes the signed speed in m/s); `TCLIGHT` while the traction control works (`+0x388`).
- **The rivals and the driver aid** (`0x0044AA80`, not type 4), below.
- **The gear**: R, N, 1 .. 6 for the gears 0 .. 7; another leaves the text.
- **The place** (with `POS`, `POSNUM` and `POSTH`): once R's lap (`+0x100`) is above 0, R's place (`+0x104`, 1 for 0)
  as "%d" and its ending st, nd, rd, th (lines 37 .. 40; th for 4 .. 6, none past 6); before, "-" and no ending.
  `POSNUM` "/%d" of the competitors.
- **The lap** (with `LAP` and `LAPNUM`): R's lap, no more than the race's laps (`+0x34`), as "%d" above 0, else "-";
  `LAPNUM` "/%d" of the laps.
- **The times** (with `TIME`; not type 4): `TIME` the lap's time (`+0xF4`); except for the types 1, 0 and 2, `LR_TIME`
  the lap record, "--:--:--" while it is 300; with a world record `WR_TIME` it, shown, with `DIGITAL_BG_WORLD` and
  `WR_LAPTIME`.
- **The count** (while not done): when the count number `+0x14` is above the race state's count, the picture of it
  (`ui_start_01` .. `03`) on `COUNTDOWN`, its place (0.5, 0.25) and size 0, shown; at the last player the number goes
  down. Once the number is 0 and the count is 0: `ui_start_go` the same way, and done at the last player. While
  `COUNTDOWN` shows, each frame its size grows by half the frame's time and its place moves back by a quarter (it
  grows about the point); white, with an alpha of 1 - (size - 0.125) x 2 within 0 .. 1 once the size is past 0.125
  (cut toward zero); hidden past 0.375.
- **The message**: when the player's messages have a new first (`+0x10`, then cleared), `MSG` takes its text and
  colour; with none the text is emptied; the text's alpha is the messages' `+0x14`, the box's alpha 0.
- **The nitro**: `NOSMETER`'s value the nitro left (`+0x37C`) / 15.
- **A new lap** (R `+0x11C`; not type 4): the first player's messages are emptied; the panels' timer becomes 4
  (10000000 for the types 1, 2, 6, 4, 3, 0); with R's best lap (`+0xF8`) b: below the world record it takes it (and
  the lap record when below) with "!!NEW WORLD RECORD!!" (760); else below the lap record it takes it with "NEW LAP
  RECORD" (32); else R's lap equal to the laps "FINAL LAP" (1771); else R's `+0x124` (with the player in their own car)
  "FASTEST LAP" (31); else R's `+0x120` "NEW BEST LAP" (30); else "LAP COMPLETE" (33); each for 2 s.
- **The car ahead** (no new lap, R's `+0x128` set and the gap ahead `+0xC8` above 0, with `CARAHEAD`): "CAR AHEAD"
  (35), a space and the gap's time, white, alpha 1, its timer 3, the gap made 0.
- **The split** (R `+0x128` or a new lap, not type 6, with `SPLIT`, the sector's difference `+0x17C` not 0): "SPLIT"
  (1773), a space and "+" with the time of the difference, or "-" with that of its negation; red 0xFFFF5050 above 0,
  else green 0xFF50FF50; alpha 1, its timer 2. Each frame the timer loses the window's frame time within 0 .. 2, below
  1 it is the alpha.
- **The nitro's warnings** (the player in their own car, with `NOSMETER`, not type 4): under 2 s left with `V+0x198` below
  1, "NITROUS LOW" (87) and `V+0x198` 1; else under 0.01 s with it below 2, "NITROUS EMPTY" (88) and 2; each with the
  sound WARNING (`0x005A8860`).
- **The car behind** (the gap behind `+0xCC` not 0, with `CARBEHIND`): "CAR BEHIND" (36) and its time the same way,
  the gap made 0. Both timers: above 0 they lose the frame's time within 0 .. 10, below 1 they are the alpha; at 0 the
  text is emptied.
- **The damage** (the player in their own car): the steering's fault, the turbo lost and (with nitro left) the leak of the
  frame (`V+0x4F4`, `+0x4FC`, `+0x500`, set by `0x00488B50`) give "STEERING DAMAGED", "TURBO DAMAGED", "NITROUS LEAK"
  (761 .. 763) with the sound WARNING.

After the players: `best` the first player's best lap (not for the types 1, 0, 2); with one player the race's news
(`0x0044A4C0`, below); the turbo's needle to its range x
the turbo's boost (`0x004F7550`) with a turbo and a gear other than neutral, else to its start; the map's setting (the
settings' `+0x2C`, kept in `+0x244`, which the constructor leaves as it was): when it changes the elements with `MAP`
in their names are shown (without their transitions) or hidden.

The texts are those of the game's text table (`scripts\<language>.txt`); the numbers above are its lines from 0.

In a speed challenge (type 2), after the turbo's needle and before the map's setting, the first player's messages
(each with the WARNING sound): the trap's speed reached this frame (R `+0x130`) "SPEED ACHIEVED" (line 45) for 2 s;
before the trap's speed is reached (`+0x12C`), with no message waiting, "ACHIEVE TARGET SPEED" (46) for 1.5 s while
the race time is within 5 .. 6, 7 .. 8 or 9 .. 10 s; after it, with no message waiting, below the challenge's speed
(`[G+0x6C]+0x78`) + 15 for the car's speed (its local velocity's length × 2.235 whatever the units) "DRIVE FASTER"
(47) for 1.5 s. The race's news (`0x0044A4C0`) comes last, after the map (`0x0044A7A0`).

Then (`0x0044D827`) the frame asks its window manager not to draw the windows this frame (the manager's `+0x50`, cleared
as its frame starts, `0x005A6B41`; read before the windows are drawn, `0x005A6BB5`): with one player while the first
player's camera looks back (`+0xA8`) or, in view 5 or 6 (the bumper, the bonnet), pans (`+0xCC` not 0); with any
number of players while it plays the fly-by or the sweep before the start (modes 9, 10). So a HUD the pause's CONTINUE
asks for (`0x004584C0` with 1) during the fly-by comes in but shows nothing until the race's view takes over.

In a sprint (type 6), after the count is done (`+0x10`), before the turbo's needle: with the first player in its own
car and a gear up by hand this frame (the player's `+0x30`) the first player's messages are emptied and the change is
rated by the engine's revs (body `+0x358` × 9.549296) less the car's top of the power range (dynamics `+0x160`):
"EARLY CHANGE" (line 79) below -150, "LATE CHANGE" (80) above 150, else "OPTIMUM CHANGE" (78), for 1 s.

Not yet followed here: the cruise's parts (type 4), the sprint's records at its finish and the sounds.

## The respect (`0x004BCB60`)

At the frame's start with RESPECT found (`+0x54`; its icon `+0x58`, RESPECT_TEXT `+0x5C`), in a career race not of
type 9 with the race cars chosen (profile `+0x614`) or the profile's `+0x618`, and the profile's `+0x00` at 0 (else
only the messages `0x004BD130`): once the first racer's race time (`+0xF0`) is 10 s (5 s in a sprint, type 6) the
race's respect so far (`0x004BCD90`, `08-career.md`) cut toward zero shows in RESPECT ("+n" from 1, else "n"), RESPECT
and its icon shown, the colour 0xFF28FF28 from 0, else 0xFFFF2828. A value other than the last shown
(`[0x0079E830]`) flashes for 2 s (`[0x0079E838]`): while the flash lasts it runs down by the frame's time and a blink
(`[0x0079E834]`, by 0.2 s rounds) over 0.1 hides RESPECT and the icon; RESPECT_TEXT shows while the flash is above 0
with the flash as its alpha (at most 1), else it is hidden.

The respect's news (`0x004BD130`, after the respect, or alone with the profile's `+0x00` set; not in a round of
type 2, from the first racer's race time of 10 s, with every crew of the session's round having its session's
`+0x340` set) to the first player's messages, each for 1 s in green 0xFF32FF32 or red 0xFFFF3232: the crew's place
(`0x004A4F90`, by the points in a round of type 5) against the last kept (the career's `+0x34` object `+0x00`, -1 at
the event's start): the messages emptied, "Gained a place!" (line 1777) or "Lost a place!" (1778), then "Player's
crew <place><ending>"; at most every 5 s ([0x0079E828]) the lead's step (`0x004A5110` / 100, / 20 in a round of type
5) against `+0x04`: with either below 0 "Improving lead!" (1779) when it fell, else "Pack gaining!" (1780); with
either above 0 "Catching up!" (1781) when it fell, else "Dropping back!" (1782); with a bet placed its state (the
crew before the bet crew) against `+0x2C`: "Winning the bet!" (1783) or "Losing the bet!" (1784); a rival crew's rams
(`+0x330`, kept cut at `+0x0C` ..) changed: "Damage caused!" (1785, red) at most every 3 s ([0x0079E82C]).

## The crew's orders (the race drivers, career `+0x74`)

Enter (`0x004BDA30`, from the HUD's enter): RP_ elements shown in the career before its race cars are chosen, else
hidden; the window's DpadBg1 .. 3, Dpad1 .. 3 and DpadControl01 .. 03 found (hud_race has DPAD1 .. 3 and
DPADCONTROL01 .. 03); once per race the members' cars taken (`0x004BDE90`: the session's racers of crew 0 other than
its leader, in order, each with its car, which the race's setup keeps at the racer's `+0x2C`, `0x00496B28`; their
count `+0x18`, at most three lines); in the career with the player's crew racing with members (`+0x0C`) the state is
1 and each line's elements are shown while it has a member, else they are hidden (`0x004BDBC0`); the lines emptied
(`0x004BDC50`); without `+0x10` the window is dropped (`+0x38`).

In the drivers' levels' frame (`0x004BDF00`, not in a cruise, a replay or a pause): a flash flag
(`[0x0075EF64]`) toggles; with `+0x10` in state 1 the lines (`0x004BE720`) and the keys (`0x004BE340`). A line:
"<short name> <place><ending> <level>": the member's crewmate's short name (`+0x48`), the competitor's place
(`+0x104`) with the ending of its last digit (th, st, nd, rd, th ..., `[0x00760290 + 16 n]`), the racer's level as
Low, Medium or High (lines 464 .. 466). A blink (`[0x0079E848]`) runs down by the game's frame time, back to 0.25 below
0; while a line's flash time (`+0x20`, set to 1 by `0x004BEAB0`) is above 0 and the blink below 0.1 the line is empty
and its flash runs down by the window's step. Its colour (`0x004BEF20`) by the racer's nerves: red 236, green
(2 − nerves) × 0.5 × 236 rounded to nearest within 0 .. 236 (236 on the flash frames past 1.9), blue 1. The keys
(DRIVER1, one-shot): DRIVER_TACTICS_UP (W) and _DOWN (Q) for the first line, _RIGHT (S) and _LEFT (A) for the second,
DRIVER_TACTICS3_UP (X) and _DOWN (Z) for the third: the racer's order and its time 0, its level one up or down
within low .. high, and when it changed its time −1.

The D-pad's state (`+0x04`) is made 1 in the career when the drivers race with members (`+0x0C`), else 0 with every
picture hidden (`0x004BDA30`, `0x004BDBC0`). In state 0 (`0x004BE290`) any of the six tactics keys opens it: state 1,
the members' cars taken if not yet (`0x004BDE90`), ordered by their competitors' places (`0x004BE9F0`: two in a row
swapped while the second is ahead, from the first again after each swap), their pictures shown, the lines, the time
`+0x08` 5 s (read by nothing in state 1). State 2 is set by nothing: its lines (`0x004BE960`) are emptied, then Dpad1 ..
Dpad3 take "High", "Medium", "Low" (`[0x007617E0]`, `[0x007617D0]`, `[0x007617C0]`, their effects started again) in
0xFFECEC01; its keys (`0x004BE5C0`) give the member of `+0x34` the level 2, 1 or 0 by DRIVER_TACTICS_UP, _RIGHT, _DOWN
(its order and the order's time 0; a changed level's time −1); one of them or _LEFT sets the time to −1; the time runs
down by the window's step and below 0 the state is 1 again.

The same frame, in the career with the race's cars not chosen (the profile's `+0x614` 0: a race the player watches)
and the camera not in the fly-by or the sweep (modes 9, 10): NEXTCAR (Page Down, `0x004BE04B`) follows the car of the
place before the followed car's (the last after the first), SELECT (RETURN, `0x004BE13D`) the car of the place after
it (the first after the last); a car without a place (-1) gives the race's next car (`0x00497530` with 1). The camera
takes the car (`0x00401DB0`: another car resets the view, `+0xA4` and `+0xB0`, and tells the cars' engine sounds which
one is heard). These keys, DRIVER_TACTICS3_UP / _DOWN (offline, the cars not chosen) and the four tactics keys set the
drivers' `+0x64` (the racers' names' time, a float; 0 from their set up `0x004BDCC0`) to 2 s; it runs down by the
game's frame time (outside the attract replay, mode 4, and a replay's race, the race not paused, not only watching a
network game), and `[0x0075EF64]` turns over each frame. The network's overlay (`0x00456BA0`) shows the cars' names
while it is above 0.

## The labels over the racers

Off the network the race front end's frame (`0x004574C1`) draws `0x004BF320` in goodtime11 (the front end's `+0x4C`),
outside the attract replay (mode 4), mode 3 and a replay's race, for each racer of the career's session (`+0x30`,
whatever race it last held) whose car (its `+0x2C`) is not the player's (`0x004BAE70`: the car of the first racer of
crew 0, member 0). The colour: white for the player's crew; else 0xFFE6gg00, the green (1 - (nerves - 1)) x 236 held
within 0 .. 236, 236 above 1.9 nerves on the frames `[0x0075EF64]` is set (they blink), rounded. The alpha 0, then
1 - depth / d in front of the camera within d.

- The names' time below 0 (`0x004BF8B0`): a racer with nerves of 1.9 or more gets `!` (`[0x00769A58]`) centred 1.2 m
  above its car's centre, d 100 m (`[0x0079E844]` counts the racers 0 .. 7 round, read by nothing).
- Else 1.35 m above the car's centre, d 150 m: the car's crew's name (108 + the car's crew, `+0x47C`) centred there;
  15 pixels lower the racer's member's short name (the car's crew's `+0xDE` + 25 x the racer's `+0x08`); with the
  player's money on the car's crew (the session's odds `+0x0C`) "BET" (1929), a space and the money 30 pixels above
  that; for a crew leader (member 0) of the crew bet with (the bets' `+0x08`, a bet placed) with an amount over 0
  (rounded) the amount 30 pixels above that again.

## The map

Made with the window (`0x0044A100`): `MAP3D` (`+0x240`); unless made already (`+0x248`), its model list (`+0x118`)
is emptied and `UI_<route>_MAP` (the race's route, `[G+0x6C]+0x30`) added at the origin (`0x005B3E60`: a holder of
0x80 bytes, `0x005B3420`, with the model of `models\<name>.boh` in the element's scene, its place `+0x40` and the
node's translation set). Without that model every element with `MAP` in its name is hidden. Else for each vehicle of
the race manager, from the last to the first, `ui_map_car` is added at (0, 1000000, 0) and the material named
`ui_map_car` (`[G+0x10]` vtable `+0x18`) takes the vehicle's car paint (`+0x4E4` `+0x18`) as its parameter 0 (the
Colour of Coloured Spec ABlend); the markers share that material, so the first vehicle's paint stays. `+0x248` is
set; `+0x244` takes the settings' map flag and the `MAP` elements are shown (without their transitions) or hidden by
it.

Each frame with the map shown (`0x0044A7A0`), with the car the first player watches (`P+0xC`, `+8`): each vehicle
from the last, with the listed models from the second on: the holder's scale `+0x78` 3 (`[0x00725398]`), its place
(x + 10, 10, z + 10) of the car's model origin in centimetres (`0x00407B20`: the body's origin x 100), and
(`0x005B3520`) its node the scale on the diagonal, the place, then turned about y, x and z by the body's angles
(`+0x218`) negated. The camera: the height `[0x0072539C]` (12000, held within 8000 .. 200000 and kept), back by
`[0x00725390]` (1.75) x the height along the watched car's heading (its yaw, the angles' y): the element's `+0x130`
and the camera node's place are (x - d sin yaw, height, z - d cos yaw) with d = 1.75 x height, its angles (`+0x140`
and at once `+0x150`) the pitch `[0x00725394]` (-0.5), -yaw, 0.

The 3D element's view clears only the depth (`12-rendering.md`): the map draws over the window's frame.

The 3d elements of a window whose name (in capitals) holds `HUD_` (`0x005AB3DC`) get the view flag 0x10
(`0x005B36D0`): the view's begin (`0x00516880`) masks the corners off (`0x00516450`): no culling, no colour written,
the stencil set to 1 where drawn (ALWAYS, REPLACE, reference 1) by a fan for each corner from 2 pixels past it along
the quarter of the ellipse within the view (nine points pi / 16 apart); then culling back (3), red, green and blue
written, and the view drawn where the stencil is 0 (EQUAL, KEEP, reference 0); the view's end (`0x00516D20`) turns the
stencil off first. The map of `hud_race`, `hud_solo` and `hud_sprint` is round.

## The rivals and the driver aid (`0x0044AA80`)

Not in race mode 3: for each neighbour k of R (`+0x150`, sorted each frame) the box `RIVAL<k>` is hidden, then shown
when the neighbour is within 40 m aside (|its x in the car's frame| below 4000 cm), between 2 m and 40 m behind (its z
between -4000 and -200, both open) and behind on the lap (its distance, `0x0049B230`, below R's, `0x0049EFE0`). The
box's x (`+0xA4`) becomes 0.5 ± (|x| held to 600) / 600 x 0.4 (+ for x above 0) less half its width, its y stays;
its colour the neighbour's car's paint (`+0x4E4` `+0x18`, when it has a vehicle, `0x0049B270`), its alpha 0.8 +
z / 4000. (Constants `[0x00725374]` 600, `[0x00725378]` 4000, `[0x0072537C]` 200.)

The driver aid (the settings' `+0x28`; options `+0x7C`) off: `DRIVERAID`'s text is emptied and the curve warning
hidden. On (not type 6): the curve warning's timer `+0x1F8`, while above 0, loses the frame's time; below 0.5 it is the
alpha `+0x200`, below 0 the box is hidden and both are 0; the alpha, held within 0 .. 0.5, is the box's. R going the
wrong way: `DRIVERAID`'s alpha 0, and nothing more. Then with `DRIVERAID` (type 2: its text emptied): with v the
car's speed along itself and the excess v - 1 x R's corner speed (`+0x140`; `[0x00725388]` 1): above 10 m/s with an
excess above 0 the text's alpha `+0x1F0` gains 0.7 x the frame's time; an excess (held within 0 .. 30) above 0.95 x
30 gives "BRAKE NOW!" (1845) and sets `+0x208`, else without `+0x208` "SLOW DOWN" (1844); the colour is white with green
and blue 255 - (excess / 30 x 255, to the nearest); with `+0x208` the flag `+0x210` flips and while it is set the
colour is white. Otherwise the alpha loses 2.5 x the frame's time. The alpha is held within 0 .. 1 (at 0 `+0x208`
clears) and is the text's, with the colour as its font colour.

Then R's corner warning (`+0x148`), when not 0: the curve warning's texture, colour and texture coordinates by it
(the table `0x0044B1A0`): 2 `ui_arrow02` 0xFFE8E003, 3 `ui_arrow02` 0xFFB90505, 4 `ui_arrow01` 0xFF159B02 mirrored, 5
`ui_arrow02` 0xFFE8E003 mirrored, 6 `ui_arrow02` 0xFFB90505 mirrored, any other `ui_arrow01` 0xFF159B02 (mirrored: u
from 1 to 0); its timer and alpha 0.5, the box's alpha 0.5, shown.

## The race's news (`0x0044A4C0`)

Not networked, types 5 and 7, the player's race time at least 5 s; the race state's finishers (`+0x20`) against the
count told (`+0x64`, 0 at each reset of the race state, `0x0049FF02`): a new finisher gives "<its member's short name>
<its crew's name> HAS FINISHED" (1774) and the count is kept; else a new leader (`+0x54`): the player's car "1ST
PLACE" (1776), else "NEW LEADER: <name> - <crew>" (1633); else a car spun (`+0x5C`, `+0x4C`): "<crew> <name>: HAS
WIPED OUT" (1775). The short name is the crew's (career `+8` + 4 x crew) `+0xDE` + 25 x the member of the racer
(the session's racer of the car's `+0x480`); the crew's name the text line 117 + the crew. The message goes to the
first player's list for 2 s, with the sound CLICK.

## The player's finish

The player's frame (`0x0041FE50`: the camera `0x00406300`, its controls `0x00406AB0`, the driver's control
`0x0041E200`, then `0x0041FB60`). The driver's control reads the controls only while the player's car is the
player's (`V+8` 0, `0x0041E294`) and the count is below 10. `0x0041FB60` (not networked): once R has finished
(`+0x118`) with the player's `+0x18` clear: `+0x18` and `+0x1C` are set; outside a replay with one player the
controller is let go (`0x00407FB0`: force feedback stopped); R's progress timer `+0xA8` 5, progress `+0xAC` 1000,
respawn asked `+0xB8` 0; the car goes to the AI (`V+8` = 1); then the camera director (`P+0x10`): on a circuit not of
type 6 it starts (`0x00408670`: active, the camera's mode 0 taken with a reset, its time 5 s) with `+0x20` set, else
`+0x20` is set and the camera takes mode 0 (`0x00406990`); online the race's time goes to the records. A player
without a car (a spectator) gets both flags once every driver of the session has finished, with the director in mode
0xC. `+0x1C` makes the window `hud_race_complete` (once), `+0x18` then `hud_race_results` (`0x004584C0`).

The director's views and its cut's transition: `17-race-camera.md`, `12-rendering.md`. Not followed yet: the
results' windows other than `hud_race_results` (they show nothing).

## A race the player watches

In the career the first player is made (`0x0041DE30`) with the controller (DRIVER1 registered) whether or not the
player races; its car (`+0x04`) is the racer of crew 0, member 0 (`0x004BAE70` through `0x00425750`), none when the
player only watches (WATCH at a meeting, the profile's `+0x614` 0). Without a car (`0x0041E0B0`) the camera takes the
car of `0x004BB1A0`: the session's first racer of crew 0, else the first whose crew has the player's money on its odds
(the session's odds `+0x0C`), else, with a bet made, the first of the bets' crew, else the first racer; a restart
(`0x0041E110`) puts the camera on that car again whenever the race's cars are not chosen. The player's frame
(`0x0041FE50`) runs as with a car: the director, the camera and its controls, the driver's control (`0x0041E200`
without a car goes straight to the pause and SELECT, `0x0041F620`), and the finish (`0x0041FDD9`): while the director
is not active, once every racer of the session has finished, both race flags (`+0x18`, `+0x1C`) and the director on
view 12 for 5 s (`0x00408670`). The race front end is made as for any race. The HUD's frame takes for each player
its own car, else its camera's car (`P+0x0C`, `+0x08`; `0x0044B41C`), the parts kept for the own car (the records'
messages, the nitro's meter) left out; the map always follows the camera's car (`0x0044A7A0`).

## Choosing the window (`0x004584C0`)

The camera's views ask as they reset: the views behind the car (`0x00402040`, with 1), on the car (`0x004033E0`, 1),
the sweep before the start (`0x00405300`, 0), the TV views 11, 12 and 13 (0), the fly-by (`0x00405BE0`, 0). With one
player off the network, outside a replay and not paused (`[G+0x34]+0x20`; paused only the camera's mode 14 goes on),
asked with 0 before the player's finish (`+0x18`) gives `blank`; else by the race's type (`0x00458A44`), the
player's `+0x1C` read and cleared first:

| Type | `+0x1C` set | else after the finish | else |
|---|---|---|---|
| 0 .. 3, 9 | `hud_race_complete` | `hud_race_results` (3), `hud_challenge_results` | `hud_solo` |
| 4 | `hud_race_complete` (`hud_cruise_results` online) | `hud_cruise_results` | `hud_cruise` |
| 5, 7, 8 | `hud_race_complete` | `hud_race_results` | `hud_race`, any waiting window dropped |
| 6 | `hud_sprint_results` | `hud_sprint_results` | `hud_sprint`, any waiting window dropped |

A window is asked for by the race front end's manager (`+0xBC` = 2, the name at `+0xC0`) only while none waits. The
manager (`0x005A8580`) takes the window on its next frame (the delay `+0x100` is 0) unless it is the one shown (the
names compared without case): every open window leaves (the HUD's leave `0x0044A450`) and the new one opens and
enters (its transitions starting). The shown window's transitions step each frame (`0x005B56B0`) as it is
drawn: in hud_race NOSMETER1 fades in 0.75 s after the window opens.

## The race complete (`hud_race_complete`, `0x0044FD20`, `0x004504A0`)

Elements found without case. Enter: `[G+0x4C]+0x24` set (the speech leaves the 3D world alone); each player's messages emptied
(`0x00456140`); the time (`+0x0C`) 5 s; MSG (`+0x10`) by the race's type:

- 0 .. 2: CHALLENGE FAILED (line 382) for the challenge's outcome (`[G+0x6C]+0x7C`) 2 .. 5, else CHALLENGE COMPLETED
  (381) and POS the competitor's best lap (`+0xF8`, `0x0044FBD0`);
- 3, 4: TIME UP (83); in a cruise POS takes the score (`[G+0x80]+0x60`);
- 9: Completed (338); POS becomes `+0x10` and is emptied;
- else, off the network with the first player's car (and in the career not in a round whose `+0x17` is over 1): WINNER
  (81) for the finish place (`+0x110`) 1, else RACE OVER (82); POS becomes `+0x10` and shows the place: "2", "3",
  "4", "5", "6" with "nd", "rd", "th", "th", "th" (lines 38 .. 40), any other place "1" with "st" (37), then " place"
  (41); without the car RACE OVER.

Every text set restarts the element's effect (`+0x108`, when it has one). The music's volume is read
(`[G+0x50]`). Counter (`+0x18`), RespMsg (`+0x1C`) and RespCounter (`+0x20`) are found; `+0x28`,
`+0x2C`, `+0x38` cleared. In the career with `[[0x00728C40]+4]` 0: `+0x24` the race's respect (the career's `+0x34`
object's `+0x30`, cut), Counter's text emptied; `+0x30` the best car's respect and `+0x3C` the garage's worth's
(`0x004BB3D0` with crew 5, `0x004BB2F0` with crew 6, neither applied, `08-career.md`); each not 0 starts its phase
(`+0x2C`, `+0x38`) with a wait of 1 s (`+0x34`, `+0x40`) and holds the window 1.5 s (`+0x28`; the garage's worth also
plays CLICK); the race's respect becomes `+0x24` + `+0x30` + `+0x3C`; RespMsg's and RespCounter's texts emptied. Else
Counter, RespMsg and RespCounter and every Respect_Icon are hidden.

Each frame: a blink timer (`[0x0079E7D4]`) grows by the frame's time, back to 0 over 0.3; Counter shows the respect
(`+0x24`) as "+n" in 0xFF32FF32 over 0, else "n" in 0xFFFF3232 (n cut toward zero). While `+0x28` is above 0 it runs
down and nothing more. The best car's phase (`+0x2C`), then the garage's (`+0x38`): the first player's camera director's
time (`+0x10`'s `+0x0C`) set to 2.5 s; RespCounter shows what is left of the phase's respect the same way, RespMsg
"Best Car" (line 298) or "Car Collection" (297), shown; after the phase's wait, while more than 0.5 is left in size,
3 × the step × what is left goes from it into `+0x24` each frame (the sound swoosh), else the phase ends; during the
wait RespMsg is hidden from 0.15 of each 0.3 of the blink timer. With neither phase: `+0x24` the career's respect (`[[0x00728C40]+0x34]+0x30`); RespMsg and
RespCounter hidden; the time runs down by the window manager's step; under 2 s the text `+0x10`'s alpha is time - 1
held in 0 .. 1 (`0x00447530`); under 4 s the 3D world's volume falls to a quarter of its master; below 0 the window asks for the results:
`hud_challenge_results` for 0 .. 2, `hud_cruise_results` for 4, else `hud_race_results`.

## The pause (`hud_pause`, `0x0044E920`, `0x0044EEA0`)

The driver's control (`0x0041E200`) of a paused race (the race state's `+0x20`) clears the car's `+0x10C` and `+0x110`
and does nothing more. Else, after the car's controls (a player's own car only), while some player has not finished
(`0x00420570`), PAUSE (DRIVER1, a held action: Escape) pauses the race outside a replay with a window shown that is
not `hud_pause`: the manager's waiting window dropped and `hud_pause` asked for, CLICK, `0x004263E0` with 1 (the
effects' `+0x98`, the sounds `[G+0x48]`, `[G+0x4C]` and the music `[G+0x50]` paused, the race's timer `[G+0x34]+8`
noting the time, `+0x20` set), which player paused kept (`[G+0x34]+0xC`'s `+0x10`), the controller let go
(`0x00407FB0`).

The race front end's frame (`0x00457180`) also follows the first player's device: its event's `+0x18` is the device's
`+0x34`, 0 when a controller's poll failed (`0x00594CFE`) and whenever the game's window is inactive (`0x00594C38`).
Lost while `+0x14` is set (1 from the front end's making, `0x004568D1`): a message (`0x005A8030`, `msgbox_critical`
with no text) and, once it is up, `+0x14` 0, the waiting window dropped and, with some player not finished off a
replay and not paused, the race paused with `hud_pause` (`0x004263E0`); back while `+0x14` is 0: DRIVER1 registered
(`0x00457B06`), the message closed (`0x005A7F70`), `+0x14` and `+0x18` 1, the UI map (`0x00457B45`) unless the player
who paused (`[[G+0x34]+0x0C]+0x10`) has its `+0x00` 1, and in a replay `hud_replay` with the UI map (`0x00457B9C`).
So leaving the game's window pauses the race (`02-frontend.md`, "Input").

`blank` and `blank_split` (records `0x00720578`, `0x00720584`, vtable `0x006F14F8`) are not the pause: their window
has no elements, the enter (`0x0044F990`) only makes the driver tactics' D-pad (`0x004BDA30`) and the update does
nothing. The fly-by shows `blank`: no text over it.

Paused, the race's frame (`0x00425D40`) leaves out the physics' steps, the scene's frame, the cars' frame with the
count (`0x00497C50`) and the scene objects (`0x00422BB0`); the drivers' levels wait (`0x004BE238`); the players'
frame goes on: the camera's time is 0 (`0x00406322`), its controls wait (`0x00406ACD`), the director's step is 0.

Enter (`0x0044E920`): the pause made sure of; the race's respect worked out as for the last place (`0x004BCD90` with
`last`); Menu (and BackMenu, not in the file) back to its first item; FRIENDS taken out off line, PLAYERS without a
split screen, RESTART online and in the career (except a challenge, types 0 .. 2, with an outcome other than 1, and a
cruise of a crew's challenge, the current event's `+0x05` by `0x004A64E0`); PLAYER hidden with one player, else its text
the player's number; QUIT's text online; `+0x0C` .. `+0x2C` cleared, `+0x1C` set; `[G+4]`'s method `+0x10` with 0
(empty on the PC); every COUNTDOWNBOX hidden.

Each frame (`0x0044EEA0`, offline with one player): the cruise's quiet time (`[[G+0x80]+0x60]+0x14`) 0.1; online, with
`+0x24` once `[0x0075EF7C]` is 0, FRIENDS and BackMenu's first item are taken out (`+0x24` is set by the enter only
online). Quitting (`+0x20`) asks for `closedown` (while no window waits) and ends the frame; a restart (`+0x18`) with
the lap's records looked at (`+0x1C`) asks for `restart_race` (a sprint's racers' `+0x138` and the race's `+0x08` -1,
which player paused, `[G+0x34]+0x0C`'s `+0x10`, back to 0) and ends it; the records not yet looked at (`+0x1C` 0): the
best lap of the race's cars (the career only with the race cars chosen) against the route's record (`0x0040A1C0`; a new
one kept by `0x0040A220` with the driver's name and the car's model, and `newhighscore_cms` over the window, also while
a record is not written, `02-frontend.md`; the pause's best lap to the online statistics first), then quitting set unless restarting, `+0x1C`
set, the end. A quit asked (`+0x0C`) answered 1 (`+0x2C`): in an arcade championship `0x0042B290` with 100, the replay
stopped, the network told; in the career the current event's crew's `+0x2C8` set when the event has `+0x02`; `+0x1C`
cleared; the end. Answered otherwise `+0x0C` and `+0x2C` cleared. A restart asked (`+0x10`) answered 1 clears `+0x1C`,
sets `+0x18` (`0x00426F60` on the replay), else `+0x10` is cleared; `+0x2C` cleared. Then each player's event: BACK goes
on; SELECT (CLICK) on CONTINUE goes on; on QUIT a question (`0x005A80E0`, type 3, the answer to `+0x2C`, `+0x0C` set):
of type 9 "QUIT RACE?" / "ARE YOU SURE?" (lines 84, 86); in the career with the race cars chosen "QUIT RACE AND FINISH
LAST?" (89), with the profile's `+0x61C` "QUIT RACE AND LOSE BET?" (90), each followed by "ALL GAME PROGRESS WILL BE
LOST." (1823) and "ARE YOU SURE?", else "QUIT RACE?" / "ARE YOU SURE?"; off the career "QUIT RACE?", "ALL GAME PROGRESS
WILL BE LOST.", "ARE YOU SURE?"; on RESTART "RESTART RACE?" / "ARE YOU SURE?" (85, 86, the answer to `+0x2C`, `+0x10`
set); FRIENDS and PLAYERS open the online windows, OPTIONS `opts_ingame_bg` in place of the windows (mode 2; the options
from a race's pause, `02-frontend.md`).
Going on (`0x0044ED90`): the pause ends (the race's timer adds the time paused), the
game's timer told 1, the director's period cut to 2 s from its time (0 past 2 s) with its cut due, the window of the
views asked for (`0x004584C0` with 1: `hud_race` again before the finish, even over the fly-by), and each player's
controller made again (`0x004080D0`; here a key held through it gives its action only after it is let go). The window's
events reach it from the frame after it came in: the stock game reads the window events after the race's frame and the
driver's actions before it, so the Escape that paused does not also give the pause window its BACK.

## The race results (`hud_race_results`, `0x00452E60`, `0x00453450`)

Enter: `[G+0x4C]+0x24` set; in the career with `[[0x00728C40]+4]` 0 `counter` shows the race's respect (the career's
`+0x34` object's `+0x30`, as Counter above), else every Respect_Icon and Counter is hidden; each player's messages
emptied; for the rows 0 .. 5 the elements Pos%d, Name%d, Team%d, Car%d, Time%d, PosBar%d (found without case, else
none); `+0xBC` .. `+0xC8` cleared; the table filled (`0x00451C90`); the controller let go (`0x00407FB0`); `+0x14` 1 when
the options' `+0x24` call gives 0 (read by nothing); `+0x10` set in the career before its race cars are chosen; the menu
(Menu, `0x005AECF0`) loses RESTART online and in the career and REPLAY online (online QUIT takes the host's or the
guest's text); `+0xA8` .. `+0xB4` cleared; QUIT selected (`0x005AF920`); the time `+0x0C` 1 s.

The table (`0x00451C90`): a split race (`[0x0075EF84]`, `[0x0075EF88]` 1, `[0x00728CEC]` over 1) has its own
(`0x00452330`), the network its own (`0x00452030`), a career round (not of type 9) whose `+0x17` is over 1 its own
(`0x004529E0`). The crews' table (`0x004529E0`): `+0xB8` = 0; a row for each crew of the session's round in the order
the crews finished (the session's `+0x340`; -1 a crew not finished: Team "----------", Time "--:--:--", Car
"----------"): Team the crew's name (line 108 + the crew of its leader's car, else of its first driver's), Time the
latest race time of its drivers (the latest is 0 once before the rows, `0x00452A2C`, and only raised after: a row
shows the latest of its crew and of the crews above it, a stock problem), Name and Car emptied; Pos, Name, Team, Time
and PosBar shown and Car hidden; the first player's row has Team and Time highlighted; rows from the round's crews
on are hidden. With `+0xB8` left at 0 the frame fills the table again each frame once anyone has finished. Else `+0xB8` = 0 and for each row i the i-th finisher of the race (`[G+0x34]`'s race state, its
finishers): Name the competitor's name (`+0x14`: in the career the racer's short name, `0x00496B00`; the network's
entries `[0x0076A1E0]`, 0x208 bytes, `+0x14`; in an arcade race the drivers' `+0xDE`, `0x004966C8`, not
followed); Team the crew's name (line 108 + the car's `+0x47C`) off
the network and outside mode 3, else emptied; Time the race time (`+0xF0`, `0x0044FBD0`); Car online emptied, else
line 211 + the id of the car's model (`[[V+0x4E4]]+0`); the player's row (the first player's competitor) has Name,
Team, Car and Time highlighted (`0x005B2930`); `+0xB8` grows. Past the finishers Name "----------", Team
"--:--:--", Time "--:--:--", Car "----------". The rows from the number of the race's vehicles on are hidden (Pos,
Name, Team, Car, Time, PosBar). Pos keeps its caption.

Each frame (`0x00453450`): the time runs down by the window manager's step; below 0 the controller is let go and
the time is 4 s. The first frame (`+0x10` 0) looks at the lap's records (the best lap of the race's cars, outside a
sprint, against the route's record by the route's table `0x00711AC0`; a new one kept by `0x0040A220` and
`newhighscore_cms` asked over the window), sets `+0x10` and ends. A quit (`+0xA8`) answered 1 (`+0xB4`)
stops the replay, tells the network (`0x004EA850`, `0x004EA4E0`, `0x004E2740`), clears `+0x10` and asks for
`closedown`; a restart (`+0xAC`) answered 1 asks for `restart_race` (`0x00426F60` on the replay first); each clears
its flag and the answer. A timer of the game (`[0x007A2EB0]`) runs down by the game's frame time; when more have
finished than the table shows or it is below 0 it becomes 1 and the table is filled again. Then the event: SELECT
(CLICK) on QUIT sets `+0xA8`: in a career's team round (`0x004BAEB0`: not type 9, the session's round's `+0x17` over
1) with the race cars chosen, when no crew in the round's order (`+0x340`, as many as the round's crews) is the
player's (0), a question "QUIT RACE AND FINISH LAST?" / "ALL GAME PROGRESS WILL BE LOST." / "ARE YOU SURE?" answers
to `+0xB4`, else `+0xB4` is 1 at once; REPLAY asks for `start_replay`; RESTART asks "RESTART RACE?" / "ARE YOU SURE?"
(the answer to `+0xB4`, `+0xAC` set). The window manager gives the window the UI map's event: any event ends its
transitions (`0x005A9E80`) and the horizontal menu takes LEFT and RIGHT (`0x005B11A0`).

## The challenge results (`hud_challenge_results`, `0x00450B60`, `0x00450F80`)

Enter: the sounds stopped (`[G+0x4C]`'s `+0x08` told, `+0x0C` set), the music's `+0x1C` and `[G+0x44]` told; each
player's messages emptied; the controller let go; the challenge's outcome (`[G+0x6C]+0x7C`) 0 becomes 1 and in an
arcade race (mode 0) wins it (`0x0042B350`). MSG1 (found without case): CHALLENGE FAILED (382) for the outcomes 2 .. 5,
else CHALLENGE COMPLETED (381). MSG2 by the outcome: 2 OUT OF TIME (378), 3 CAR DAMAGED (377), 4 YOU ARE TOO SLOW
(379), 5 TARGET SPEED NOT REACHED (380), else WELL DONE (376) for a speed challenge (type 2) or without the first
player's car, else its best lap (`+0xF8`, `0x0044FBD0`). The menu (Menu) back to its items (`0x005AECF0`), its
first item selected (`+0x13C`, `+0x148` 0; its text element `+0x184`, none in this window, would take the item's text
and measure it, `0x005B52C0`); RESTART taken out online, and in the career unless the race is a challenge (types 0 ..
2) not met (an outcome other than 1); `+0x0C` .. `+0x14` cleared; with the outcome 1 QUIT selected.

Each frame (`0x00450F80`): a quit (`+0x0C`) answered 1 (`+0x14`) stops the replay's recording (`[G+0x9C]`'s `+0x828`
and its method `+0x2C`), tells the network and asks for `closedown`; a restart (`+0x10`) answered 1 stops the replay
(`0x00426F60`), works out the race's respect (`0x004BCD90`) and asks for `restart_race`; each clears its flag and the
answer. SELECT (CLICK): QUIT sets `+0x0C` and the answer 1 at once (the text "QUIT RACE?" / "ARE YOU SURE?" is made
and dropped: a stock problem); REPLAY stops the replay and asks for `start_replay`; RESTART asks "RESTART RACE?" /
"ARE YOU SURE?" (`+0x10`, the answer to `+0x14`).

## The sprint results (`hud_sprint_results`, `0x00454760`, `0x00454DB0`, `0x00454D80`)

A sprint (type 6) is run in heats: the race's config `+0x08` the heat (from 0) and `+0x0C` the heats, 3 off line and
1 online (the arcade `0x0042D8E7`, the custom race `0x0045B5FF`, the career `0x004A3E9E`). Each finish gives the
competitor 6, 4 or 2 points for the places 1 .. 3 (`+0x138`); its heats' time (`+0x134`) grows with its race time.
Both are kept from heat to heat; `restart_race` counts the heat and starts a later heat with the sweep before the
start and the count at 8. A restart from the pause in a sprint (`0x0044EF7D`) clears the points of the session's
racers' cars and makes the heat -1 (the heats' time is kept: a stock problem).

Enter: in the career `counter` shows the career's respect (its `+0x34` object's `+0x30`: "+n" in 0xFF32FF32 over 0,
else "n" in 0xFFFF3232, both as its colour `+0x11C` and `+0x150`), else every Respect_Icon and Counter hidden; the
music's volume taken to `[0x00715C38]` times its own (`[G+0x50]`, its `+0x0C` .. `+0x14`); `[G+0x4C]+0x24` set; each
player's messages emptied; for the rows 0 .. 3 the elements Pos%d (`+0x30`), Name%d (`+0x48`), Time%d (`+0x60`),
Team%d (`+0x78`), Car%d (`+0x90`), PosBar%d (`+0xA8`); Large "HEAT" (554) + " n / heats"; SelectText and SelectIcon
hidden; `+0x20`, `+0x24` 0; the rows filled (`0x00453BA0`; online `0x00453FB0`); the controller let go; `+0x1C` 1,
`+0x10` 0, `+0x14` 1, `+0x18` 0, `+0x28` -1; nextheat LEADERBOARD (1534; online "Leave Session" for the host, else
"End Race"); the time `+0x0C` 1. The exit (`0x00454D80`) gives the music back.

The rows (`0x00453BA0`): `+0xC0` 0; the controller let go; for each row i the i-th finisher of the heat gives Name its
name (`+0x14`), Team its crew (line 108 + the car's `+0x47C`), Time "<points> Pts  (<race time>)" with the points 6,
4, 2, 0 of the row (1820, `0x0044FBD0`), Car line 211 + its model's id (online emptied); the first player's
competitor's row has Name, Team, Time and Car highlighted; `+0xC0` counts the finishers' rows. Past the finishers Name,
Team and Car "----------", Time "--:--:--". The rows from the number of the race's vehicles on are hidden. Every
competitor finished: `+0x20` 0.1.

The leaderboard (`0x004542D0`): SelectText and SelectIcon shown, Large "sprint leaderboard" (555); the heat's first
four finishers bubble-sorted by their points (more first), then their heats' time (less first); each row gives Name,
Team, Time the points ("%d") and Car; the first player's row has Name, Team and Time highlighted and, in the last
heat, its place goes to the arcade's result (`0x0042B290`); past the finishers Name and Team "----------", Time
"--:--:--"; rows from the vehicles' count on hidden but for Car; `+0xC0` counts on.

Each frame (`0x00454DB0`): the music's volume back up to its share; `+0x18` works out the race's respect (`0x004BCD90`
with its collection flag) and asks for `closedown`. The time runs down by the window manager's step, below 0 the
controller let go and 4 s. `+0x14` 0 (the end taken): outside the career, or with its race cars chosen, the online
tables (`0x004EC6D0`) and, while the records were not written (`[G+0xA8]+0x08`), `newhighscore_cms` over the window
(mode 5); `+0x14` 1, and with `+0x1C` set `+0x18` set. Else `+0x1C` 0 sets `+0x18`. The first frame (`+0x10` 0) looks
at the lap's records (outside the career or with its race cars chosen: the best lap of the players' cars against the
route's record; a better one kept by `0x0040A220`, nothing asked) and ends. Off the network the rows are filled again
while `+0xC0` is below the finishers. SELECT (CLICK) once every competitor finished (`+0x20`): first nextheat takes
NEXT HEAT (1533) or, before the last heat's end, CONTINUE (1913), `+0x24` is set and the leaderboard shown; then the
sounds told (`0x0040B600`) and, before the last heat, CLICK, the replay stopped and `restart_race`; after the last
heat `+0x28` 1. With `+0x28` 1: the race's vehicles (four indices kept on the stack: with more the sort reads past
them, a stock problem) sorted as the leaderboard; the career's race session's first place `+0x368` -1; in the career
each racer in that order finished (`0x004B9CF0` with its heats' time); the replay's recording stopped; the network
told; `+0x1C` 1 when the save system does not save (its method `+0x24`), `+0x14` 0, `+0x28` -1.
Leaving (`0x00454D80`): the 3D world's voices paused (`[G+0x50]+0x1C`, vt+0x20 with 1).

## The message window (`0x005A80E0`)

The race front end's message window is the front end's (`msgbox`, `04-interface.md`): while one is open (the window
manager's `+0x44`, counted in `+0x108`) it takes the events and the windows under it wait and draw only their
elements whose name holds `FRAME_` (`0x005A9D20`); a question (type 3) starts on "no" (the message's `+0x28`, set by
the pause, the results, the challenge's and the showoff's questions; the replay's eject passes a value of its state)
and, once its transitions are over, SELECT gives its answer (1 for "yes", else 0) to the asker's flag and closes it. The
message's `+0x24` is the player whose events it reads (`[front end + 0x88 + 4 n]`, `0x005A923F`): the pause loops over
the local players (`[G+0x6C]` `+0x58`) and its questions pass the one who asked (`0x0044F6A5`, `0x0044F912`), the
other askers 0; with one player it is always 0.

## Starting the race again (`restart_race`)

`restart_race` (vtable `0x006F14C8`, the file's LOADING text and its animated boxes). Enter (`0x00455940`): the time
(`+0x0C`) 3 (`+0x10` 1, 0 with the statistics outside a replay; read by nothing), the game's sounds held. Each frame (`0x00455A90`): the time less 1; once the window
manager's windows have come in (`0x005AA120`) and the time is below 0, with the race there: the career's `+0x60`
`+0x3C0` cleared; a sprint counts a heat (the race's config `+0x08`); then `0x004261F0`:

- `0x00426160`: the scene objects' replay cameras reset (`0x004204C0`); each car made ready again (`0x004894C0`):
  with a neon that flickered (the car's `+0x6C`) its neon part 0 (`+0x0D`); the car repaired (`0x0048EB20` in an
  arcade or custom race off line: every cell's damage of the race and the damage kept with the car cleared,
  `0x00557C00`; else `0x0048EA80`: only the race's, `0x00557C90`): the nitro 15 (`+0x84`, `0x0048F210`), `+0x7C`,
  `+0x74`, `+0x64`, `+0x6C`, `+0x70` cleared; the model's grid drops its cells' vertex lists (`0x00556130`) and the
  meshes are made again (`0x0048A700`, `0x00489440`); the zones keep their damage; a lost turbo (`+0x68`) comes back
  (the vehicle's `+0x180` set, the body's Turbo 1.3: a stock problem, whatever the car's own was); the lights' flags
  (`+0x44C`, `+0x450`, `+0x454`) cleared and with the lights on (`+0x458`) the headlights' flares and projector on.
  Then the cars settle on the grid again (`0x004975A0`, as before the start: the race's state reset, the game's
  rand()) and the camera goes on the first car.
- Each player (`0x0041E110`): the director stopped (`0x00408750`: inactive, its time and period 0, its transition
  and sounds ended); the camera's mode by the race's type (`0x00401E80`: 3 and 4 the player's view (`+0x8C`), a
  sprint's later heat the sweep before the start with the count at 8, else the fly-by, 9) with a reset; the camera on
  the player's car (in the career before the race cars are chosen the session's car, `0x004BB1A0`); the car the
  player's again (`+8` = 0); a cruise's money (`0x004AA730`); the player's `+0x18` .. `+0x20` cleared; the controller
  made again.
- The sounds and the music again; `[G+0x4C]+0x24` and the config's `+0x7C` cleared; each car's effects reset
  (`0x00492710`); the race's `+0x78` cleared.

Then the effects' `+0x98` cleared, the race going on (its timer, `+0x20` cleared; the sounds) and the window
manager's `+0x10C` 1.
Leaving (`0x00455F50`, also `start_replay`'s and `restart_replay`'s leave): with the sound system the game's sounds
given back whatever the race: the speech (`+0x0C` 0, its slots resumed), the 3D world's voices (vt+0x20 with 0), the
music's song (`[G+0x48]+0x58` 0, resumed where it was: a sprint's next heat goes on with the last heat's song) and
the front end's player (`[G+0x38]`, else `[G+0x3C]`: its `+0x08`'s `+0xAC`), the sound system updated between; the
race front end watches the controllers again (`[G+0x3C]+0x38` 1).

## Leaving the race (`closedown`)

`closedown` (vtable `0x006F14E8`; the file's LOADING text). Enter (`0x0044F9C0`): three frames to wait (`+0x0C`), the
replay stopped (`0x00426B80`, `[G+0x9C]+0x828` 0), `[G+4]`'s `+0x20`, `+0x24` and the effects' `+0x98` set, the race
paused as by the pause (unless it is), the race's timer stopped, the HUD's `+0x38` cleared (leaving, `0x0044FAA0`,
sets it). Each frame (`0x0044FAC0`): the frames run down; then, with no message window open, the game phase "load"
(`0x00462C30`) with 1 takes the game's `+0x40`: its first frame (`0x004634B0`) lets the race's objects go and shows
the loading front end of the race's mode (as the load: `arcade_loading`, `custom_loader` or `loader` and its
animation) while a job (vtable `0x006F1E1C`, run `0x00461A10`) makes the front end with its first window; once the
job is done (and the loading front end's last window has left) the front end's music starts (`0x0046BA90`; with the
sound on `0x0046BAC0` draws one rand()) and the front end takes the game's `+0x38` (`0x0040D140`).

The first window (`0x00461A10`): a window named by `[0x0075ED18]` when set, which it empties (the network's quits set
it, `0x00457180`, and a saved replay's read, `0x00429550`: `menu`), else by the race's mode: arcade `Arcade_Race`
(`Arcade_Results` while the arcade championship's `[G+0x8C]` `+0x53C8` .. `+0x53D4` are set; online the lobby's
windows), custom `instant_manu` (online the lobby's), split screen `multi`, mode 4 `start`; the career: the dealer's
`+0x10` set; type 9 `garageload` (the dealer's `+0x0C` and the career's `+0xA4` cleared); else the session's end
(`0x004A3A40`: `0x004BA8A0`, `0x004B9E20`) and by the current event: a cruise (type 2) the event's end (`0x004A6870`)
and `CMSMeetingSummary` when the career's `+0x38` list has entries (`+0x320`), `CMSRespectChanges` when the `+0x3C` list
has, else `0x004BD8A0` on the history and `CMSMainMenu`; a challenge (`+0x05`) `CMSChallengeResults`; else
`CMSRaceResults`.

## Times

`0x0044FBD0`: below 0 the time is 0; s = the time cut toward zero, the minutes s / 60, the rest the time - minutes x
60 (not below 0), the seconds the rest cut toward zero, and the fraction of the rest (`modf`) x 99 cut toward zero;
"%02d:%02d:%02d".

## Messages

Each player's messages (0x1C bytes, `0x004560B0`): a list of at most 10 (each 0x18 bytes: the text, its time, its
colour), new `+0x10`, alpha `+0x14`, colour `+0x18` (white). Adding (`0x004561A0`) takes the object's colour and sets
new; emptying (`0x00456140`) clears new. Each frame (`0x00456300`): with none the alpha is 0; else the first's time
loses the frame's time held within 0 .. 0.1, the alpha is twice the time below 0.5 s, else 1, and below 0 it goes;
when one is left after it, new is set.
