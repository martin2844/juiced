# The racers

Every car of a race is a vehicle of the race manager (`0x00497C50` steps them) and has a competitor (`0x184`
bytes, made by `0x0049AFF0` through `0x0049FCE0`) in the race's state (the manager's `+0x1C`, stepped by
`0x004A0220`). The competitor follows the route's racing lines, counts the race's time, laps, sectors and places,
and for the AI's cars decides the steering, the throttle, the brake and the handbrake. A player's car has a
competitor too: everything below runs for it, and its controls are taken from it while the count is above 5 s.

## The route (`0x004A0C20`, `0x100` bytes)

The `.spl` of the route (`15-tracks.md`) gives three lines, FAST (0), ATTACK (1) and CENTRE (2), of the same number
of points; a line without points is read as the fast line. Every index into a line wraps by the fast line's count.
After the file is read (`0x004A0620`) each point gets, for each line:

- its direction from the point before (point 0 from the last): the difference (z first, then x and y), its length
  √((x² + y²) + z²), and the difference times 1 / (length + 1.1920929e-7);
- its length (`+0x20`) and its distance along the line (`+0x28`): 0 for point 0, else the previous point's distance
  plus its length.

A line's length (`+0x30` .. `+0x38`) is the first non-zero length of its points plus the last point's distance; the
fast line's is the lap's. `+0x3C` is 100, MINCORNERSPEED (`+0x48`) 13.41 unless given, `+0xB0` the lowest height of
all points.

The five sectors (`0x004A0AF0`, `+0xB4`, 12 bytes each) start at points 0, n/5, 2n/5, 3n/5 and 4n/5 of the fast line
(an unsigned division), with no time and no racer (-1).

The corners of the fast line (`0x004A0840`, `+0xF0`): walking its points, the turn at a point is acos of the dot of
its direction with the previous point's ((y y' + z z') + x x', within -1 .. 1; the runtime's acos), its side the sign
of z' x - z x'. A corner is a run of points turning by more than 0.1 rad to the same side while the run is shorter
than 150 m; it ends at a point turning less, or to the other side, or once the run is 150 m long. A run that turned
by more than 0.4 rad in all is kept: its first point, the middle ((end - first) / 2 + first, the end being the point
after the run), its kind (`0x004A07A0`: by the sum of its turns, 1 over 0.4, 2 over 1.1, 3 over 2.25; 4, 5, 6 for the
other side) and its first point's distance.

### The curve

`0x004A1370` (route, line, i, t) gives the point at t between points i and i + 1 of a line and the direction
there. Through the points p0 = i - 1, p1 = i, p2 = i + 1 and p3 = i + 2, in x and z (`0x004A0FD0`):

    c3 = (((p3 0.5 - p2 1.5) + p1 1.5) - p0 0.5) t
    c2 = (c3 + ((((p2 + p2) - p3 0.5) - p1 2.5) + p0)) t
    c1 = (c2 + (((p2 0.5 + p1 0) + p3 0) - p0 0.5)) t
    value = c1 + (((p2 + p0) 0 + p3 0) + p1)

and y = (p2.y - p1.y) t + p1.y. The direction (`0x004A11D0`) is the slope of the same curve in x and z, y 0,
normalised (`0x00407C00`):

    a = ((((p3 0.5 - p2 1.5) + p1 1.5) - p0 0.5) t) 3
    b = (((p2 + p2) - p3 0.5) - p1 2.5) + p0;  b = b + b
    slope = (a + b) t + (((p3 + p1) 0 + p2 0.5) - p0 0.5)

## The race's state (`0x004A0220`)

Made by `0x0049FA40` with the route, the laps (1 on a point-to-point route) and the count of 14; reset by `0x0049FDF0`
(also at the start of `0x004975A0`, the cars to the grid): the sectors as above, every competitor reset, the best
lap 0, the count by the race's type (4.005 for a replay's race or type 4, 4 for type 3 and past 9, else 14) and its next
whole second 3; the leader, the spinner and their flags and timers 0, the HUD's finishers told (`+0x64`) 0, the
events' hold 5 (`0x0049FEF0` .. `0x0049FF05`). In a frame (the race's step time dt):

1. the respawn timer (`+0x44`) falls by dt, not below 0;
2. while the count is above 0 it falls by dt; below 0 it is 0 and the race starts (`0x004A00D0`: every car on the
   automatic box, its engine on, the handbrake off); below its next whole second a beep, and the second goes down;
3. every competitor's frame, in the list's order (`0x0049EF00`);
4. the order by the race's distance (`0x004A0480` with `0x004A00B0`, the further first; up to 16 cars the insertion
   sort keeps equals in their order) gives each competitor that has not finished its place (the old one kept);
5. the events (`0x004A0130`): a new leader is announced after 2 s, a spin at once, then 2 s without events (the
   hold falls by 0.02 a frame; it starts at 5).

### The setup (`0x00495DE0`)

The race is set up on the loading thread (`0x00424270`, after the track), its rand() going on from the track's:

1. the race's state (`0x0049FA40`), reset as above (each competitor's reset draws 4 values, none yet);
2. each car of the race (`0x00485530`): its record's flickering neon (`+0x6C`) cleared and, with the call's tenth
   argument (1 for the arcade's, the custom race's and a replay's cars; in the career only another crew's racer's),
   its tyres' and brakes' wear (`+0x74`, `+0x7C`) 0 and its nitro 15 (`0x0048F210`), before its parts give the body
   its nitro; in the career another crew's racer (`0x004969AA`) races a copy of its car (`0x00434D90`) made after
   its crew's scheme is put on every opponents' car (`0x004A9080`), the player's crew the garage's car itself; its
   model (`0x0048A0A0`); each wheel's line collider (by its ID) takes the x
   and z of the wheel's node and, with a tyre contact of its ID (the first, `0x004EF240`) of stiffness above 0, the
   length its point's y + the sag at rest (LocalGravity / stiffness x the contact's mass x 1000; `0x00485D20`), the
   wheel's pose keeping minus the sag (`+0x1D4`, what `0x00487670` adds to the wheel's travel); for a car not of the
   AI (the seventh argument 0) the centre of gravity's z becomes ((the front wheel's z - the rear's) x
   FrontWeightDistribution + the rear's) x 0.01 (`0x00485DFC`, the nodes' z in centimetres); its body, then its
   definition tuned by its drive
   (`0x0048CE20`): an AI's car (the call's seventh argument 1; `0x00486D20`) has the centre of gravity's z 0.25 with
   the rear wheels driven (else 0.1), SteeringSpeed 2 (read by nothing), the mass at most 1666 and LocalGravity x 1.2;
   another (`0x00486E00`) LocalGravity 0.05 more with the rear wheels driven, 0.05 less with all four; the body's
   inverse mass and inertia stay those it was made with; its grid throttle (`0x0049F500`, 4 values) and its
   competitor (`0x0049FCE0`, reset by `0x0049B5D0`, 4 values);
3. every competitor's neighbours (`0x004A0030`);
4. the cars to the grid (`0x004975A0`): the race's state reset, then each car settled (`16-physics.md`, "The race
   start"), the world's grid made anew for each. Car i of the race's cars (in the order made) takes the route's grid
   point (`0x004976A5`): on the network the session's grid place it was made with (`+0x4F0`); in a career race (mode 2)
   the i-th; else (an arcade, custom, split screen or attract race) the count less i less 1, so the first cars made
   (the players') start at the back (arcade 1-1's player, its first entry, on the sixth point, the second row); a
   sprint's (type 6) moved on by its heat (`[G+0x6C]+0x08`, -1 as 0) within the count. The cars' own places at their
   making (`0x00485530`, the k-th point) are not where they start.

The race runs from there: through the fly-by the count stands at 99 (set on its first frame) and falls, the cars'
physics, the competitors' frames and the grid throttles go on; at the fly-by's end the count is 6
(`15-tracks.md`). Change View while the count is above 4 sends the cars to the grid again (`0x004975A0`, the
game's rand() now) with the count at 4 (`17-race-camera.md`). So does SELECT of the driver's map (RETURN, OneShot;
the controller's `+0x50`) in the driver's control (`0x0041F7C1`, after the pause was not taken, while some player has
not finished): with the count above 4 the cars to the grid again (not online), the player's car put in the first gear
when below it (`0x004F7710` with 2), the count at 4; then the view kept in the settings (`+0x8C`) is taken with a
reset when it is not the camera's (`0x00406990`; the fly-by's mode 9 never is) and the race front end's black is 1.

A competitor finishes (`0x0049BE50`) with its place among the finishers (`0x0049FF30`); the first three earn 6, 4
and 2 points. In a career race other than a sprint the session takes the finish (`0x004B9CF0`, not in type 9): the
racer's crew's count (`+0x31C`) grows and the racer's `+0x28` is set; in an event with `+0x05` the event crew's
challenge (crew `+0x2DC`, the event's `+0x06`) takes the racer's best lap; the first racer so (and its crew) kept at
`+0x368` / `+0x36C`; when the crew's count reaches the round's drivers (`+0x17`, with the round's `+0x08` at 1; else
1) the crew goes to the crews' order (`+0x340`, counted by `+0x364`) with the race time in its odds' `+0x00`. The
first place's racer and crew are kept at `+0x368` / `+0x36C` again. The best lap of the race (`0x0049FDB0`) is a lap time below it, or the first.

The race's types read here: 0 and 1 a lap-time challenge (a competitor whose lap time passes the challenge's
finishes with outcome 2), 2 a speed challenge (passing the speed gives the trap; not before 10 s of the race ends it
with outcome 5, falling below it after the trap with 4), 4 no respawns and a finish roll, 6 a race where the nitro
and the steering away from cars ahead work otherwise. A career race is of type 5 unless its round makes it 4, 5, 6
or 7.

## A competitor's frame (`0x0049EF00`)

With the time dt of the race's step:

1. the timers (`0x0049B880`), not in a replay;
2. the neighbours (`0x0049B2C0`);
3. the line (`0x0049B3F0`);
4. the point the car heads for and the laps (`0x0049BEF0`, then the corner warning `0x0049BD30`);
5. not in a replay: the steering (`0x0049C720`), the speed (`0x0049DBB0`), the skill (`0x0049E6A0`), the mistakes
   (`0x0049EB90`).

A reset (`0x0049B5D0`) clears the controls, the line (FAST) and the point, the times (the best lap kept in a race of
type 6), the place (1), the laps; the rolling start's distance 1500, the progress timer 10, the mistake interval 240,
the throttle factor 1; and makes the car's grid throttle (`0x0049F500`: four rand() % 10, then a step of it).

### The timers (`0x0049B880`)

While the race runs (the count 0) and the car has not finished, the race time, the lap time and the total time (`+0x134`, which the reset keeps: the time over every heat) grow by dt.
While the count is 0: facing away from the line (from the steering) adds up, over 1.5 s the car goes the wrong way;
the line's hold falls; every 10 s a car that made less than 10 m of progress asks to respawn (not in a type-4 race,
not after a point-to-point finish, at most one car in 5 s); the traffic and pressing times fall within 0 .. 10, the
swerve's within 0 .. 2. A contact ahead (from the car's contacts) adds twice dt to the stuck time; past 0.6 the car
backs off for 2.7 s. Backing into something for 0.3 s ends the reverse. A car 10 m below its line's point asks to
respawn too. Then the challenges of the race's type.

### The neighbours (`0x0049B2C0`, `0x0049AC50`)

Every competitor knows every other (`0x004A0030`). For each, in the list's order:

- the time until it is reached: the gap d = (their position - mine) x 0.01, the closing speed c = (v_mine -
  v_theirs) · d as (z, y, x); ((dz² + dy²) + dx²) / c when c > 0, else 99999;
- where it is in my car's frame: its node position by the inverse of my node's matrix;
- the gap along the lap: their distance - mine (each 0 on a point-to-point route before its first point, past its
  finish or before the start line); over half a lap it is taken across the line; how fast it closes ((old - new) /
  dt); its order the gap, below -1000 plus 999999; the same crew.

The nearest ahead and behind are kept; a car of another crew within 10 m behind adds twice dt to the pressing time.
The list is then sorted by order.

### The line (`0x0049B3F0`)

For the first 4 s of the race, and always for a player's car, the line is the nearer of FAST and ATTACK at the car's
point (the offset (p - c) x d across each). Later, with the first neighbour (the furthest behind within 10 m, or the
nearest ahead): when it is between 10 m behind and 20 m ahead and further along the lap, the other line than its
own while no change is held; else FAST. A change is undone while that car is within 8 m along the lap; else it is
held for 3 s and the magnet is let go.

### The point and the laps (`0x0049BEF0`)

The car heads for a point of its line (`+0x80`). When it is behind the point before (along that point's direction,
in x and z) and nearer to it (in x and z) than to the next point (in x, y, z), it heads for the point before (a car
going back over point 0 loses the crossing). Its distance behind the next point along that point's direction gives
the fraction from the point before: 1 - clamp(-along / length, 0, 1). Past the point it heads for the next one;
entering a sector (the next sector's first point reached) a player's car takes the gap to the car that entered it
last when that car is ahead, the AI's give a player's car behind them its gap; the time into the sector against the
lap's best. Reaching point 1 (or the point after a point-to-point finish) is a crossing of the line: a lap after
all five sectors (on a point-to-point route always), its best lap, the race's best lap, the lap time cleared while
laps are left; after the last lap the finish.

The lap's distance is the fast line's: the next point's length x the fraction + the point before's distance; the
race's distance crossings x the lap + that; the progress grows with the race's distance.

The corner warning (`0x0049BD30`, a player's car): the next corner's kind when the car reaches it within 2.5 s at its
speed, or while it is in it.

### The steering (`0x0049C720`)

The car's heading is its node's forward in x and z, normalised. The look ahead is pow(speed, 1.8) cm within 50 ..
1750 (50 when not going forward); the line's direction there (`0x0049C590`: from the point before the next, the
remaining part of that segment, then whole segments) or, on a rolling start, the grid's heading. The angle to it
(acos of the dot, signed by the side) is kept; under a dot of 0.3 the car faces away.

While the car goes mostly forward (|x| < |z| / 4) and not on a rolling start:

- the offset o across the line at the car's point (centimetres, from the curve at the point before the next with the
  car's fraction);
- the magnet (the AI's cars, above 10 m/s, within 12 m of the line and 1 rad of it): the car's origin is moved toward
  the line's point by at most ((clamp(v z 0.025, 0, 1) x 80) x dt) x magnet cm; the magnet grows by dt up to 1, and
  is 0 otherwise;
- with a = clamp(o / 1200, -1, 1) and b = clamp((o_last - o) / 70, -1, 1): the correction -(a² sign(a) π/4), plus
  b² sign(a) π/4 when the signs agree; added as clamp(t 0.1, 0, 1) x (correction x clamp(2 bend, 0.1, 1));
- not while reversing: for each neighbour beside (±2.5 m across, ±3 m along) and further along the lap ±0.25 away
  from it; the swerve's side x its time (once per neighbour); in a race of type 6 away from one more than 6 m ahead
  reached in under 2.5 s.

The steering is then (s² sign(s) + s) / 2. With the rear sliding more than the front (the wheels' slides) by d, 2 d²
against the yaw, within ±1. While reversing the time falls (and ends under |0.2|), and the steering is full lock the
other way. Times SteeringFactor, within ±0.45.

### The speed (`0x0049DBB0`)

While the race runs:

- the bend ahead b (`0x0049D320`): within the distance covered in (1 - skill) + 2 s, about ten samples of the curve,
  each turn (acos of the dot of the curve's direction with the last) weighted by 1 - (d/D)²; √(sum / 3), at most 1;
  the nitro decides with it (`0x0049D6F0`) and keeps it (`+0xA4`);
- the target: 500 under a bend of 0.2; else v = (1 - (b - 0.2) 1.25)² 109.515 + MINCORNERSPEED, p = (min(v, 71.52) /
  71.52)³, ((1 - p) skill + p) v; x 0.95 for the rear-wheel drive, x 1.1 for the four (`0x0048CE20`);
- the corner speed over 6 s; after the finish the target within 0 .. 50; the top speed in miles an hour;
- 1 m/s or more under the target: the throttle (target - speed) / 15 within 0 .. 1, no brake (going back over 1 m/s
  and not reversing: the brake, no throttle); 1 m/s over it the brake (speed - target) / 10 within 0 .. 0.95;
  between, neither.

Always: a car tipped on one side brakes 0.8; a rear sliding more than 0.02 over the front lets go of the brake. While
the race runs above 2 m/s, for each slower car ahead: reached in under 2.5 s within 2.5 m across in a bend over 0.3,
the brake 1 - t / 2.5; within 9 m ahead and 2.5 m across, full brake, no throttle, and a nearly stopped car swerves
for 1.5 s; beside and further along the lap, no throttle (full brake when touching a body, outside type 6). A swerve
holds 0.9 throttle and no brake. After 5 s (or in type 6) the throttle factor grows by ((skill - 0.5) 2, not below
0) x 10 + 2 (Hard 6) per second while the traction control is not cutting or the factor is 0.8 or less; within 0.3 ..
1, it multiplies the throttle; reversing takes 0.75. After a finish on a point-to-point route (or in type 4) the car
brakes 0.4 and, past 17 m per place behind the first, 0.5 and slows by 3 % a frame.

Before the start: on a rolling start the engine runs, the throttle is 0.4 and the car is held to 1 m/s while its
distance runs out; else the grid's throttle (`0x0049F450`: the pattern stepped, in the count's last 8 s its first
throttle and time grow and the chance of the second falls from 0.7 to 0), the handbrake, the engine off, the car held
still, and braking when it moves.

The grid's throttle is the pattern's while `[0x00725578]` is set, which it always is. The other way, `0x0049D1C0`,
is never run: a rand() each frame (u = r / 32767); with the count c held to 0 .. 10 and k = (1 - c x 0.1) + 0.1, when
the timer (`+0x50`) is out the throttle (`+0x54`) is 0 unless k² > u, then 0.85, below 1 s of the count
(1 - c) x 0.15 + 0.85, and the timer is drawn again (rand() x 0.3 / 32767); else the timer falls by the step, within
0 .. 10; either way the AI's throttle is `+0x54`.

### The nitro (`0x0049D6F0`)

When its timer runs out a nitro that was on goes on for 2 s more on a straight (a bend under 0.32) for a rival crew
outside type 6. With no nitro and no timer: a player's crew's car on a high level fires it for 0.5 s on a straight
above 10 m/s (5 in type 6) at full throttle with no yaw and no smoke, else waits 1 s (type 6: 0.1 .. 0.35 s); a
rival on a high level fires it for 2 s beside a car ahead on the attack line, or with the chance 0.75 / laps,
else waits a random 0 .. 7 s; off the straight it waits 5 s. A rival keeps (15 / laps) x the laps left in the tank.
The arcade's nitro race fills the tank and fires above 0.95 throttle. After the finish or with the tank empty it is
off.

### The skill (`0x0049E6A0`)

Hard is the A.I. DIFFICULTY of the game options: the settings' `+0x4C` (`[game+0x64]+0xA0`; the options window
`0x00481770` turns it between 0 NORMAL and 1 HARD), 0 for a new driver, read every frame. The ability's scale and the
two throttle factors of the low and medium levels take it as Hard when it is not 0; the speed's rate (`0x0049DBB0`),
the pull to the player, the lead's penalty, the career's factor and the mistakes (`0x0049EB90`) only when it is 1.

A player's own car has 1. The AI's: the driver's ability (Hard: x 0.75 + 0.25), 0.1 less for the player's crew
without a player; after 5 s by the driver's level (`0x004BEAF0`): low 0.2 less and the throttle x 0.98 (the player's
crew x 0.96, none on Hard), medium the throttle x 0.99 (none for rivals on Hard), high 0.2 more (0.28 for the
player's crew). With a player: with the chance 0.05 dt the place and the gap to the player are taken; behind the
player 0.35 more (Hard 0.5, within 100 m that times the gap / 100); ahead by over 10 m up to 0.3 less (Hard 0.1) by
the gap (all past 150 m), with the throttle x (1 - that x 0.2). Then the arcade's changes, or in the career: unless
the difficulty is 1, with the profile's `+0x00` set 0.2 less and the throttle x 0.98, else the profile's `+0x628`
more (-0.4 in a new career) and the throttle x (`+0x628` x 0.25 + 1); with a bet on the race (the bets' `+0x00`) the
crew bet with drives better: a pink slip 0.25, a bet under 0.75 of that crew's money (`+0x334`, `0x004B1F20`) 0.066,
over 1.25 of it (`0x004B1EE0`) 0.2, else 0.125. Within 0 .. 1.

### The mistakes (`0x0049EB90`)

Not for the player's own car. The mean time between mistakes (`+0xDC`, by the driver's level): for the player's crew
x 10 (none at all when its driver panics on a high level); for rivals x 10 behind the player, x 5 on Hard. While
steering over 0.1 and above 10 m/s with no mistake for 5 s, with the chance dt / that: 9 in 100 (or when panicking) a
spin of 1.2 s (full lock, the handbrake, no brake; it ends below 5 m/s), else a wobble of 0.3 s (0.2 the other way,
the handbrake, the throttle 0.7, the brake 0.5). A panicking rival drops to the low level.

## The cars (`0x00497C50`)

In a frame, after the physics, every car's slipstream links (`+0x440`, `+0x444`) are cleared; then for each car of
the manager in turn (the vehicle's frame `0x00488EE0`, then `0x004890B0`), after the damage's events of the last
frame are cleared (`+0x4F4`, `+0x4FC`, `+0x500`, `+0x504`) and its wheels shown (`0x00487670`, `06-cars.md`):

1. its tilt held (`0x00489740`): a torque about x of −40000 × ((pitch / π)² − 0.3) when that is above 0 (Stock
   problem: the pitch, an arcsine, never passes π / 2, so the term never acts; its square would lose the side) and
   about z of −40000 × roll / π; a pitch or a roll past ±60° (1.0471976) is clamped and its angular momentum taken
   away, the angles and the momentum then set again;
2. its frame (`0x004888B0`): the nitro asked for (with a nitro part and nitro left), the impact's fall; with a nitro
   part the car keeps the nitro left (`0x0048F210`, its `+0x84` within 0 .. 15); while the brake is applied (the
   body's `+0x34C`) the brakes wear (`0x0048F080`, not on the stand nor in a race of type 9): the car's `+0x7C` grows
   by 0.01 x the frame's time within 0 .. 1 (`0x0048F1C0`) and every wheel's BrakeMu becomes the car's `+0x80` /
   (the wear + 1); the time through other cars after a respawn; touching the route above 20 m/s the car slows by
   30 m/s each second;
3. its slipstream (`0x00487D50`): the body's drag factor (`+0x2F0`) back to 1; over the manager's cars from the first
   (the car itself left out), the first whose point 600 cm behind it along its velocity is within 447 cm of the car's
   origin (the squared distance below 199809) gives the drag factor 1 − its speed² × 7e-5 (the cut held within
   0 .. 0.5), becomes the car's followed car (`+0x440`) with the car its follower (`+0x444`), and the distance / 447
   goes to `+0x448`;
4. its chassis node lowered (the model object `+0x430`, see `06-cars.md`), its lamps (`0x00487C70`), its gear's
   sound (`0x004875F0`: the sound object `+0x498` told of the gear, `+0x438` the last gear, `+0x43C` 2 after a
   change up and down by 1 each frame, read by nothing), its exhausts (`0x00487220`: each exhaust's node, `+0x14`
   and `+0x18`, at its place as made, `+0x1C` and `+0x2C`, its y moved by −0.2 × the body's acceleration in its own
   y, `+0x2AC`, within −6 .. 6; on the stand past 2000 rpm by a rand() of −0.5 .. 0.49 instead), then what its
   damage does (`0x00488B50`, see `19-damage.md`);
5. its wheels (`0x00487FE0`): for each wheel the tyre contact of its ID, on ground facing up: the touch, the surface;
   on sand or grass a drag falling toward -20 / -5 (0.3 of it on the front wheels) times the speed, and with the
   chance (speed / 5) dt a jolt of ±250 (four rand()); the slide |slip x / speed of the patch| over 4 m/s, the skid
   and the smoke from the slip; tipped with two wheels down on one side; fewer than two down for 4 s respawns the car
   (before the smoke, the skid and the slide are quartered; a player's car with one player blacks out the race's
   screen, `[[G+0x3C]+8]+0x10C` 1, `0x0048880E`); then the car's effects object (`0x004924A0`): the
   change of the car's velocity over the frame and, on the frames the impacts run, the impact (`0x004916F0`, see
   `19-damage.md`);
6. an AI car asking for it respawns (`0x00487080`): on its line at the point before the next, 0.3 m up, facing the
   line (type 4: its heading), left to settle for 45 steps of 0.05 s, then a ghost time of 3 s (only a player's car
   passes through the others while it lasts, `0x0041E200`); then the car's tracker on the race's centre line
   (`+0x4D4`, made with the car by `0x00419E10` when the race has a centre line, `21-scene-objects.md`; `0x0041A000`):
   its point (`+0x08`) the line's nearest to the chassis at first (and after the grid, a respawn or a replay's first
   frame, from the body's origin, `0x0041BAA0`), back one while the chassis is behind the point before (its offset
   along that point's direction, y not counted, below 0), on to the next (from the last to the first) once past its
   point's; its share of the line (`+0x18`) the offset past the point before plus that point's length and distance
   over the last point's distance, within 0 .. 1. Nothing found reads the point or the share;
7. its projected textures (`0x004890B0`): the neon's colour, the car's (`+0x2C`) with, under an atmosphere, alpha
   0x80 and by day r, g and b x (0.4 − 0.1 × the chassis's light (its scene object's `+0x48`, 1 without one) over 0.7,
   at most 1), then x (1 − the tilt), the tilt (1 − the y of the body's up axis) × 4 within 0 .. 1; a neon that
   flickers draws one rand() and is black on a multiple of 4 (`[0x007253D4]`); without a neon black. The shadow, then
   the beam, laid again (`12-rendering.md`, "The projected textures");
8. while the AI drives it (or the count is above 5): the gear (reverse while reversing, else first from neutral), the
   throttle squared, the steering within ±1, the brake cubed, the handbrake, the horn (`0x0049F360`), the nitro.
   Otherwise, with `[0x007689CC]` set (nothing sets it), the car is held: no throttle, no steering, the brake 1, no
   handbrake;
9. a parked car (`+0x4E8`, which nothing sets but to 0 as the race makes the car) is taken out of the world: its
   centre at (0, 0, -1000000), its node and its state with it.

Then the race's state (`0x004A0220`). A player's controls come after, from the driver (`0x0041E200`).

## The drivers' levels (`0x004BEAF0`)

In the game's frame, before the race's (`0x004BDF00`), with the frame's own time, each driver of the race session that
has a car:

- a rival chooses its level (`0x004BEFD0`) when its timer runs out: by the gaps to the cars ahead (a) and behind (b)
  in metres (1000 without one), a mode of 1; a > 90 m: 0 when b > 100 m and nobody is ahead, 2 when b < 50 m; a < 90 m:
  2 when b < 90 m and either is under 50 m, or b > 90 m and a < 50 m; type 6 always 2. A crewmate within 10 m behind
  gives the driver the order 2 for 10 s instead. Mode 2: high under nerves of 1.75, else the same level, medium when
  the nerves pass 1.99 for over 5 s; mode 1: high under 0.5, medium up to 1.5, else low; mode 0 low. The timer is
  then 0.2 s, -1 after a change;
- a driver of the player's crew on the low level with a crewmate within 10 m behind takes the order 2 (`0x004BF210`);
- under the orders 1 and 2 the driver keeps the low level for 10 s (`0x004BF280`);
- the time panicking (nerves over 1.9) adds up;
- the pressure: the nearer of twice the gap to the car ahead and the gap to the car behind (of another crew; each cut
  to a whole number and read as an unsigned one, so a car behind counts from 2^32): within 50 m (1 - 0.02 m) + 1, else
  1 / place; 2.5 ahead of the player above 20 m/s on the high level with a car of another crew less than 15 m behind;
  x 0.3 with a bet on the race (the session's `+0x370`) for the crew bet with, x 0.3 for a rival behind the player;
- the nerves: on the high level + pressure x gain x time (passing 1.9 from below asks for the crew's orders as the
  chatter does, `0x004BEA60` for no racer: the orders' window opened, no line flashed), on the medium - loss x time,
  on the low - loss x time x 3,
  within 0 .. 2; 0 before the start and after the finish;
- the competitor takes the level, and the mean time between its mistakes: medium (2 - nerves) x 100 + 12, high
  (2 - nerves) x 20 + 0.5, either 0.5 over 1.9; low 200.
