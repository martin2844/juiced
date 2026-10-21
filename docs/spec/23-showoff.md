# The showoff

A showoff is a cruise (race type 4). The cruise manager (the career's object `+0x60`, 0x460 bytes, made by
`0x004AA5F0`; `[[G+0x80]+0x60]`) watches the first player's car do its tricks: each trick builds a line of the trick
list worth points, the points go into the combo, the combo goes into the bank when it is held no longer, and the bank
pays itself into the score bit by bit. The time runs down from the cruise's minutes; at its end the car is finished.

## The object

| offset | what |
|---|---|
| `+0x00` | the crowd zones (one for each `Move*` helper of the scene, below) |
| `+0x10` | running |
| `+0x14` | a crash's quiet time: 0.25 s while the race is paused |
| `+0x18` | the time left |
| `+0x1C` | the next whole second to warn at (10) |
| `+0x20` | the frame's time |
| `+0x24` | the bank |
| `+0x28` | the combo |
| `+0x2C`, `+0x34` | the combo and the score last sounded |
| `+0x30` | the score |
| `+0x38` | the sounds' time |
| `+0x3C` | the combo's hold: when it runs out the combo goes into the bank |
| `+0x40` | the car's factor |
| `+0x44`, `+0x48` | the time of donuts and of high speed |
| `+0x4C` | the multiplier's blink |
| `+0x50` | eight lines of 0x38 bytes, the newest last: `+0x00` a bonus or crash line, `+0x04` the trick, `+0x08` the special move, `+0x0C` the name, `+0x1C` the points' text, `+0x2C` the points, `+0x30` going on, `+0x34` its time |
| `+0x210` | eighteen trick slots of 0x18 bytes by the trick: `+0x00` its number, `+0x04` its time, `+0x08`, `+0x0C` how long its condition holds (a turn's own time), `+0x10` a turn's state, `+0x14` its line (the list's last place) |
| `+0x3C0`, `+0x3C4` | the car and the player |
| `+0x3C8` | the time left of the tyres' last smoke (0.5 s) |
| `+0x3CC`, `+0x3DC` | the last four tricks (the newest last) and their ages |
| `+0x3EC`, `+0x3F0` | the multiplier (1 .. 25) and the one shown |
| `+0x3F4`, `+0x3F8` | the time since the last trick and the multiplier's fall |
| `+0x3FC`, `+0x400` | the music's volume aimed at and heard |
| `+0x404` | the special moves done, by the move |
| `+0x42C`, `+0x430` | the list changed; a crash |
| `+0x434`, `+0x438` | the zone the car was in the frame before |
| `+0x454` .. `+0x45C` | the network's mplay lines |

The setup (`0x004AA730`): the time 60 x the minutes in a cruise (the career's 3), else 60000; the warning at 10; the
combo, bank, score and the rest cleared; the frame time 0.0167; the car's factor and the volumes 1; the slots numbered,
the lines and the special moves cleared (the moves' flags `0x007188F4` too); the zones calm (`0x004B1510`). A career
round of type 2 sets it up for 3 minutes (`0x004A3CC0`) and every career race sets `+0x10`; an arcade showoff the same
(`0x0042D9DF`); the player's start and restart (`0x0041E110`) set it up in a cruise for the race's `+0x60` (cut).

## The frame (`0x004AAAB0`)

From the first player's frame (`0x0041E200`, off the network): while the race is paused the quiet time 0.25 and no more.
Each zone's crowd (`0x004B04A0`, below) and the zone (`+0x434`, `+0x438`): the last one active and not yet done holding
the car's origin of the frame before (`[0x00768A34]`, kept below from race to race). Outside a cruise the time is held
at 10. With no time left nothing more. The first frame takes the player's car (else the race's first vehicle) and the
car's factor: its value (`0x0048CFE0`) x 0.00005 within 1 .. 3, x (1 - the damage of its grid, `0x00557E80`), at least
0.3 (1 outside a cruise); in a cruise of the arcade the player is told "TARGET SCORE" (777) and the arcade race's target
(its `+0x10`, `0x0042B240`) for 8 s, in the career the event crew's challenge target (`+0x2E0` of the challenge `+0x06`)
or crew 3's `+0x6C` for 5 s, then "CAR MULTIPLIER" (776) " x<factor>" for 5 s. Before the start (the car's race count
over 0) the time stands (a networked race's lines go on); else the frame's time, the time less it, and run out the bank
and the combo go into the score. The car's origin is kept; the tyres' smoke time and the tricks' ages run; the music's
volume follows the crowd of the zone the car is in (`0x004B0440`: the first zone holding its origin; 0.9 while it cheers
at 3 .. 5 within 4 s); the tricks below; the lines' texts (`0x004ABA30`); the multiplier's fall (`0x004AB740`: after 20
s without a trick one less each 2 s, at least 1). A combo going on pays the bank into the score at once; the bank pays
5% of itself into the score each frame. In a cruise: warnings each second of the last 10, the count and swoosh sounds;
the combo's hold runs out into the bank; at the multiplier's 25, once, "MAX COMBO MULTIPLIER" (1915) for 2.5 s.

The tyres' smoke time (`0x004907D0`, with the tyre tracks' effect): each wheel down (no replay playing) smokes its
skid and smoke within 0 .. 1; a wheel down whose axle's other wheel is up or smoking takes the mean of the two (its own
0.5 more when the other is up); both over 0.5 keep `+0x3C8` at 0.5 s.

## The tricks

Each trick has a line in the list (`0x004ABD10`: the lines move up one; the last the trick's, worth 0, going on, shown
2.5 s). A trick ended (`0x004AC240`) outside a cruise brings the multiplier back to 1; in a cruise, worth more than 9
points: the combo held 3 s, the trick the newest of the last four, the special moves looked at, the crowd's bonus
unless the trick's crowd kind (`0x00718870`) is 1, 4 or 15, the multiplier up one (to 25) unless the trick was the
newest already (its blink 2 s), the idle time 0.

| trick | line | detector | what |
|---|---|---|---|
| 1 | High Speed | `0x004AC780` | a cruise only: top gear with the engine past 80% of the torque curve's end over 10 m/s, or over 55.6 m/s; held 0.2 s; while it holds, points x the speed x 0.01 a frame; past 2.5 s the crowd's bonus |
| 3, 4 | Left / Right Donut | `0x004AC9F0`, `0x004ACCA0` | the tyres smoking, turning past 0.5 rad/s that way, under 10 m/s, in a gear; held 2 s; points x the frame x its time x 10; past 3 s the crowd's bonus |
| 5, 6 | Left / Right Donut (Rev) | `0x004ACF50`, `0x004AD200` | the same in reverse under 20 m/s |
| 7 | Bootleg Turn | `0x004AD4B0` | from straight with the wheel turned: the handbrake and 2.8 rad/s within 3 s, going the other way (the world's direction x 5 and x -5, cut, equal) |
| 8, 9 | Right / Left 360 | `0x004AD960`, `0x004ADE70` | from straight over 20 m/s: backed up with the handbrake, within 1 .. 4 s going the same way again (x 2, cut) without having turned the other way past 1 rad/s |
| 10, 11 | Right / Left 180 | `0x004AE380`, `0x004AE830` | from straight over 10 m/s: the handbrake and 1.8 rad/s within 2.5 s, in reverse gear backing over 10 m/s the other way (the car's frame, x 10 and x -10) |
| 12 | Boomerang | `0x004AECE0` | as the 360 in a gear, having turned past 2 rad/s both ways |
| 13, 14 | Right / Left J-Turn | `0x004AF220`, `0x004AF690` | from backing straight over 10 m/s (no boomerang going on): 1 rad/s that way within 4.5 s, ahead over 10 m/s the other way |
| 15 | Drift | `0x004AFB00` | a cruise only, not during high speed: over 20 m/s, under 2 rad/s, no handbrake, ahead with the side speed over 0.1 of it and below it, no nitro; held 0.5 s starts it worth 10; points x the slide each frame; 2 s without it ends it |
| 16 | Nitrous Drift | `0x004AFE30` | the same with the nitro asked |
| 17 | (a crash) | `0x004AB7C0` | see below |

The points of a frame are the multiplier x the car's factor x the trick's factor (`0x007187E0` + 4 x the trick: 1,
0.25, 0.5 .. 0.5, 75, 500, 500, 200, 200, 300, 150, 150, 5, 25) times the trick's measure; a turn is worth the
multiplier x the car's factor x its factor once. After 30 s of donuts, or of high speed, their points fall instead and
their lines read "Boring!!" (371). The turns' zones cheer (state 1 under way, 2 when given up, 3 when done).

The special moves (`0x004AC0A0`, the table `0x007188D8`): a move whose four tricks' groups (`0x00718828`; 0 any; the
oldest within its window x 4 s .. the newest within the window) match the last four, and that was not the last move
done, is done: a line of the move (359 + move) worth its points x the multiplier x the car's factor, divided by the
times it was done (from the ninth a hundredth), the combo held 3 s and gaining it, the multiplier up by the move's.

| move | name | groups | points | window | multiplier |
|---|---|---|---|---|---|
| 1 | !! Super Donut !! | any, 7, 4, 7 | 250 | 30 | 2 |
| 2 | !! Reverse Super Donut !! | any, 10, 6, 13 | 250 | 30 | 2 |
| 3 | !! Quad Bootleg Turn !! | 7, 7, 7, 7 | 1000 | 20 | 6 |
| 4 | !! Double 360 !! | any, any, 8, 8 | 1000 | 8 | 4 |
| 5 | !! Flip Flop !! | any, any, 10, 13 | 250 | 10 | 2 |
| 6 | !! Ding Dong !! | 7, 10, 13, 7 | 1500 | 10 | 4 |
| 7 | !! Chain Drift !! | 15, 15, 15, 15 | 500 | 10 | 2 |
| 8 | !! Max Speed 360 !! | any, any, 1, 8 | 2500 | 4 | 4 |
| 9 | !! Max Speed 180 !! | any, any, 1, 10 | 1500 | 3 | 4 |

The crowd's bonus (`0x004AC560`): the zone the car was in, of the trick's crowd kind and not yet pleased, gives a
"BONUS MOVE!" (1912) line worth the multiplier x 5000 x 0.2 and the multiplier 5 more; a zone that pleases once
(`+0x118`) pleased at last with every zone pleased gives "!!! Crowd Pleaser Super Bonus !!!" (370) worth 100000 and
the multiplier 25 more (at most 25).

A crash (`0x004AB7C0`): a contact's change over 750 on a car with the cruise running and its race time over 1 s
(`0x004925F0`), after the quiet time and of 1000 or more on the cruise's car: over 3000 "Crash 'n Burn!" and the
multiplier 15 less, over 2000 "Mega Wipeout!" and 10, else "Wipeout!" and 5 (at least 1); the combo, the lines' kinds,
the last tricks and the slots cleared; the last line the crash (17) shown 2.5 s; the HUD says WIPEOUT.

## The crowd zones

As the track is made each helper named `Move*` becomes a zone of the cruise manager (`0x004B01D0`, 0x148 bytes made by
`0x004B0F40`): its node (`+0x08`, a box or a sphere) and its properties:

| key | field | what |
|---|---|---|
| `CRUISE_MOVE` | `+0x10` | the crowd kind that pleases it (`0x004B1440`): MaxSpeed 1, Doughnut 4, UTurn 7, m360 8, m180 10, Boomerang 12, JTurn 13, Drift 15, else 0 |
| `CRUISE_NAME` | `+0x18` | a text (empty without one) |
| `CRUISE_DIFFICULTY` | `+0x14` | 5 without it, at least 5 |
| `CRUISE_CROWD` | `+0x118` | pleasing it counts towards the crowd pleaser bonus |
| `CRUISE_MESSAGE` | `+0x134` | its name, read only when `CRUISE_NAME` has a text: MaxSpeed "HIGH SPEED" (1562), Doughnut "DONUT", UTurn "BOOTLEG TURN", m360 "360", m180 "180", Boomerang "BOOMERANG", JTurn "J-TURN", Drift "DRIFT" (1569); another message none |

A point is in a box zone when, brought into the node's frame by the inverse of its world matrix (`0x004F36C0`), it is
within the box's corners on each axis (`0x00554570`); in a sphere zone when its distance from the centre (moved by the
world matrix) is not over the radius (`0x00554360`). Points are the cars' origins in centimetres (`0x00407B20`).

The zone's constructor leaves its active flag (`+0x0C`) and its done flag (`+0x144`) as the heap gives them. In a
cruise the end of the track's making (`0x00422AC0`) and every setup make each zone ready (`0x004B1510`): active, not
done nor pleased (`+0x11C`), calm (state, its hold and its time 0); outside a cruise they stay as made. The race's end
(`0x00420B30`) lets the zones go (`0x004B0110`). After the race track each child of a zone's helper gets a crowd sound
(`0x004B0330` → `0x0041A430`: its node and zone, its first sound after 1 + `rand()` x 0.0009155553 s) once the crowd's
sounds are loaded (`0x0041A300`).

A zone's crowd each frame (`0x004B04A0`, the cars of the cars' manager, the count of the first one's race):

1. the count over 3.5: calm (state 0); over 2: state 1; over 0.1: state 3 (each held 0.1 s); over 0: state 4 for 4 s;
2. in a cruise with its time run out: state 4 for 4 s; in another race the first car in the zone that finished: state
   4 when it won, else 3, for 5 s;
3. else the state's time runs and its hold runs down; while it holds nothing more. Then each car in the zone raises the
   state (a lower one is left; a new state starts its time at 0 and holds 0.5 s): outside a cruise to 3 for a car in
   its top gear past 95% of its torque curve's end or over 200 km/h (its speed ahead x 3.6), with the nitro asked over
   100 km/h, or first; in any race to 3 for a car whose nearest competitor ahead is within 8000 along the lap and to 4
   within 3000. With none, calm for 0.5 s.

A state changed sets the state's time to 0 (`+0x130`); the tricks make the zone the car is in cheer (`+0x128` up to
their level, `+0x12C` their time) and a crash makes it 5 for 6 s.

## On the HUD

`hud_cruise` (`0x0044B1C0`): TIME the time left; the score over the record shown as TOPSCORE; WR_TOPSCORE "-"
without the online records. Then (`0x0044D0BD`) the list (`0x004ABE70`): Menu the lines' names, MenuPoints their
points, Points the combo and the bank; a crash's line goes from both when its time is out; a line no longer going on and
out of time scrolls the lists up by an empty line. At the end of the time (`0x004AB640`) the lists are emptied,
MenuPoints shows "Score <score>" and the car's competitor is finished (`+0x118`, `+0xA8` 10) with its damage kept as it
is (`0x00556FB0`, as a car finishing a race, `0x0049BE50`). With the cruise running: score " <score>", combo
" x<multiplier>" (blinking each 0.25 s while the multiplier changed), regionmessage the zone's name (blinking).

The race front end then shows `hud_race_complete` (TIME UP, POS "Score <score>") and, the player finished,
`hud_cruise_results` (`0x00451270`, `0x00451830`): score the score with its thousands marked; the career's respect;
RESTART out in the career unless the event is a crew's challenge whose target the score did not reach; the first
frame keeps a better record of the route (`0x0040A1C0`: the greater score) and asks for `newhighscore_cms`; QUIT,
REPLAY and RESTART as the challenge's results. An arcade showoff is won by a score over the race's target
(`0x0042B290`, from the window chooser when `hud_cruise_results` is asked).
