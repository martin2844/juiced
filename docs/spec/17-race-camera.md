# The race camera

Each player of a race has a camera object (0x1F0 bytes, made by `0x0041DE30` → `0x00401760`) with a view mode
(`+0x18`). The race's frame
(`0x00425D40`) steps the physics, then the racers (`0x00497C50`, ending in the count, `0x004A0220`), then the
players (`0x004205A0`). A player's step (`0x0041FE50`) runs the camera (`0x00406300`) with the frame's step time over
the number of physics steps it was split into (`[0x0071593C]`; a stock problem: the views ease slower when the frame
splits), then the camera's controls (`0x00406AB0`), then the driver's control (`0x0041E200`, with the whole step);
at its end the player's car is hidden under modes 5 and 6. The camera follows the player's race car: its node (the
body's model matrix, centimetres) and its body.

The race's setup (`0x00424965`) gives each player's camera (`0x0041DE30` → `0x00401760`, the camera's `+0x10`)
the height's share of the width: on a wide screen (`[0x0075F0FC]`) 0.5625, or 0.28125 with two players (`+0x58` of
the race's rules); else 0.75, or 0.375 with two players.

| Mode | View |
|---|---|
| 0, 1, 2 | behind the car: near, far, farther (`0x00402040`) |
| 3, 4 | on the car's right and left sides (`0x004027B0`, below) |
| 5, 6 | on the car: the bumper, the bonnet (`0x004033E0`) |
| 8 | the replay's next car, a view with depth of field (`0x00403E40`, below; the scene camera's `+0x224 .. +0x230`, `12-rendering.md`) |
| 11 | a TV view beside the line ahead (`0x00403F30`) |
| 12 | a view passing along the line (`0x004047C0`) |
| 13 | a view on the ground ahead (`0x00404E40`) |
| 9 | the fly-by (`0x00405BE0`, `15-tracks.md`) |
| 10 | before the start (`0x00405300`) |
| 14, 15 | free views: one flown by the mouse, one standing still (`0x00406DA0`, `0x00407310`, below) |

The frame dispatches 3 .. 15 by the jump table `0x00406950`; 7 and any other mode take the view behind the car
(`0x0040651D`). Modes 3, 4, 14 and 15 come only from a view kept in the settings (`+0x8C`), and 8 besides from the
replay's next car (`0x00408AC0`): Change View takes 0, 1 and 6, the director 5, 11, 12 and 13.

The values a race starts from: mode 0, reset set, the field of view factor 1 and its target 0, the offset
(0, 210, -630), the shake's state 0, the node's field of view 1.2566371.

## A frame (`0x00406300`)

The step time is kept (`+0xD0`; 0 while the race is paused, `[G+0x34]+0x20`, though the race's step `+0x74` goes
on; modes 14 and 15 take the step again before their own, `0x004064F7`, so they move while paused); the scene
camera's post effects
(`12-rendering.md`) are cleared: depth of field `+0x22C`, `+0x230`, the motion blur `+0x224`, `+0x228`. The point the
depth of field keeps sharp (the scene camera's `0x0054CD90`, `+0x250`) is the car's origin (centimetres, `0x00407B20`)
1 m up; a car without a body would take 30 m ahead of the camera's node (`[0x006FF840]`). The car's speed (`+0x19C`)
is the length of the body's local velocity, sqrt((z² + y²) + x²) (10 without a car). The view mode's step follows;
then the node takes the position (`+0x24` .. `+0x2C`), and the debug camera (`0x0049F910` with `0x00721C60`, whose
switch nothing sets; `01-startup.md`, "Debug pages") would take it over. `+0x198` (1 every
frame) turns the screen's smears on (`+0x234`) once, clearing the camera's rain blobs (`+0x194`, slot 4); a changed
view (`+0xB0`, then cleared) clears them again. The rest of the frame
drives the sound and the speed blur; last, the free view's mouse hook (`0x004016C0`, `[0x0075EC84]`) is put on the
game window's messages in mode 14 (`0x0059BD10`) and taken off in any other (`0x005BA640`).

After the speed blur: the atmosphere's LightBlur (`+0x17C`, `0x004066A3`) is the car's nitro amount × 0.7 under
nitro, else the camera's kept value (`+0xDC`); the atmosphere zones (`0x00422C40`); in the wet (`0x004066EE`), with
the camera in a tunnel (`0x00422D20` at its place, `+0x1A0`) no rain, splashes nor blobs, else the rain and the
splashes show by how much it rains (the atmosphere's `+0x19C`) and the blobs (`rainblob1`'s `+0x80`) by 0.2 of it
plus, behind a car (`+0x440`), (1 − the gap `+0x448`) × that car's effects' smoke (`+0xEC`) × the road's wetness
(`+0x18C`) × 0.8 (`20-effects.md`).

The speed blur (`0x00405FA0`): its aim (`+0xC4`) is the nitro's amount (car `+0x194`) under nitro, else the speed ×
0.0125, within 0 .. 1; the blur (`+0xC0`) moves 0.03 of the way to it each frame. The game's timer (`[G+4]`) takes it
at `+0x20` (with `+0x24` 1) above 0.01, else 0 and 0. The scene camera's motion blur edge (`+0x228`) is the blur, its
middle (`+0x224`) half the blur under nitro, else 0. Views 8 .. 12 and 14, and a replay with `+0x7C`, put the four
values at 0. The views' own motion blur values (0.5 and 0.5 in views 3, 4, 8, 11, 12 and 13) are always replaced.

## Behind the car (`0x00402040`)

The offset of the view in the car's frame (centimetres; x right, y up, z forward):

| Players | Mode 0 | Mode 1 | Mode 2 |
|---|---|---|---|
| 1 | (0, 180, -530) | (0, 210, -630) | (0, 270, -830) |
| more | (0, 170, -700) | (0, 200, -830) | (0, 220, -980) |

In mode 0 the first frame after a reset takes (0, 140, -200). With f the field of view factor (`+0xB8`; at most 1.3
when looking back, also when it is NaN) z becomes ((2 + f) / (f × 3)) × z; on a wide screen z + 70 (looking back
+ 50). A change of looking back (`+0xA8` against `+0xAC`) keeps the new value and sets the reset; looking back
negates z.

The offset goes into the world by the car's rows: x' = (m1 y + m2 z) + m0 x, y' = (m4 x + m5 y) + m6 z,
z' = (m8 x + m9 y) + m10 z, with y' at least 0.7 × y. The pace is 1 at 10 m/s and over, else speed × 0.1 held
within 0.5 .. 1.

With the reset the camera's offset (`+0x94`) becomes this one. Else it is eased:

1. its height: (y' − y) × (pace × dt + pace × dt) + y;
2. its heading: the aim atan2(x', z') less the side look angle (`+0xC8`; plus it looking back); the turn
   atan2(x, z) − aim, brought into ±π by 2π (`0x006FF9A4`); the offset turned in x, z by ((turn × pace) × dt) × 2.4:
   x = cos a x − sin a z, z = sin a x + cos a z;
3. its length: ((|world offset| − |offset|) × pace) × dt × 2.4 + |offset|, the offset made unit (`0x00407C00`,
   1 / (length + 1.1920929e-7)) and scaled to it.

The far plane is 2000000; the yaw (`+0x38`) is atan2(x, −z) of the offset, the pitch (`+0x34`) −0.12 for one player
(−0.1 for more), the roll 0.

A ray (`0x005046E0`) runs from the car's position to the car's position + 1.1 × offset. When it meets a wall, with h
the touch less the car's position, / 1.1, and lost = 1 − |h| / |world offset| (sums (z² + y²) + x², (h.z² + h.y²) +
h.x²): the camera's x, z offset is h's, its height lost × 150 + y, the pitch lost × −0.25 + pitch. Else the offset as
it is. The position is the car's position plus that.

Then the shake (`0x00402F90`) with the amount 0.5 under nitro, else 0.001 × speed + 0.2, and the pace 2; the shake
of an impact (`0x00403320`, while the car's `+0x174` is not 0); the field of view (`0x004060F0`); the reset ends.

## On the car (`0x004033E0`)

Mode 5 is the bumper, (0, 40, 130) from the car's origin, looking back (0, 40, -85); mode 6 the bonnet, (0, 90, 90),
looking back (0, 90, -85). With the reset: not looking back (`+0xAC` 0), the camera's node hangs from the car's
(`0x005348E0`, once, `+0xB4`), no pitch, yaw or roll, the position the view's. Each frame then:

1. the field of view (`0x004060F0`);
2. pitch and roll 0; not looking back, a pad's stick turns the yaw to ±π/2, else a yaw not 0 becomes 0 (each
   change sets `+0xB0`);
3. the tilt: the pitch 0.005 × the local forward acceleration (body `+0x2B0`), the roll 0.1 × the local turn (body
   `+0x1B0`), each within -0.08 .. 0.12; the kept tilt (`+0x84`) moves towards it by 3 × dt (`0x00407980`) and is
   added to pitch, yaw and roll;
4. a change of looking back: kept, the angles 0, the position the view's (looking back its back place and the yaw
   π);
5. the shake (`0x00402F90`) with the amount 0.35 and the pace 1.2, the impact's with 1.6; the reset ends.

After the camera the player's step hides the player's car under modes 5 and 6 (`0x00489370` with 0: the node's
`+0x90`, the car's parts `+0x4C4`, `+0x4CC`, `+0x4D0`); every car is shown again at the start of the next step. The
chase views let the camera's node go from the car (`0x00401F90`) with their reset.

## The ray to the walls (`0x005046E0`, `0x0053F7F0`)

Without a collision no wall. The segment starts at the first point; its direction is the difference made unit with
its length kept (`0x0049F9B0`, `0x00489910`). The quadtree gives the wall segments of the box of the two points
(`0x0053B640`, x and z, the walls only). Each wall's two triangles (as for a body: first marker at the zone's
`rangeMin`, second at `rangeMin`, first at `rangeMax`; first at `rangeMax`, second at `rangeMin`, second at
`rangeMax`; the first marker's direction as normal) are tried in the world when the start does not lie behind the
first triangle's bottom corner ((n.z dz + dy n.y) + dx n.x ≤ 0, d the corner less the start). A touch of the segment
(`0x005390E0`) deeper than the deepest so far (from 0) is kept: the deepest is the one nearest to the start.

## The shake (`0x00402F90`, `0x00402C00`)

With s = speed × 0.01: the pace becomes (0.2 × (1 / (0.01 + s))) × pace (0.02 under nitro, the amount then × the
nitro's amount `+0x194`); the amount ((((|e.y| + |e.z|) + |e.x|) + nitro × 1.4) + s × 1.5) × amount, e the wheels'
effects of the frame (`+0x41C` .. `+0x424`, `0x00487FE0`). A draw of rand() r: when amount × dt > r × 3.051851e-6
(and the setting `+0x68` is 0) the strong shake (0.1, amount × 0.01, amount × 0.02, pace × 6, pace × 8), else the
calm one (0.1, amount × 0.001, amount × 0.004, (1 / (s + 1)) × pace × 8, 10 × that).

The shake step (yaw scale, smallest and largest amount, shortest and longest period) runs with dt over 0 (or NaN).
Its time (`+0x1A8`) grows by dt; from its period (`+0x1AC`) on a new aim is drawn:

1. the time 0; the period (longest − shortest) × r × (1 / 32767) + shortest;
2. the tilt ((largest − smallest) × r × (1 / 32767) + smallest) × π/2 and the spin ((r × (1 / 32767)) − 0.5) × 2π,
   each with its own draw; the direction (sin spin sin tilt, −cos spin sin tilt, cos tilt) (sine and cosine as
   floats, `0x00407A90`, `0x00407A30`);
3. the last aim (`+0x1C0`) becomes the start (`+0x1B0`); the new pitch atan2(y, sqrt(x² + z²)), the yaw
   atan2(x, z) × the yaw scale, the roll ((r × (1 / 32767)) − 0.5) × 2π × largest × 0.25 (a fourth draw; not used);
4. the period × the larger of the pitch's and the yaw's change.

The shake's pitch and yaw go from the start to the aim by (sin((time / period) × π − π/2) + 1) × 0.5 (all the way
with a period of 0). The node becomes the position with the identity rotation, turned about Y by the shake's yaw +
the yaw, about X by the shake's pitch + the pitch, about Z by the roll (`0x00534960`, `0x00534A10`, `0x00534AC0`).

## The field of view (`0x004060F0`)

Not while paused. Under nitro the target (`+0xBC`) is the nitro's amount × 0.8 + 1 at the rate 0.02. Else 1, the
local forward acceleration (body `+0x2B0`) × 0.8 + 1 when over 10, at most 1.35; plus forward speed (body `+0x1A0`,
when over 0) × 0.018642804 × 0.5, at most 1.5; the rate 0.01. The factor moves by ((target − factor) × dt × rate) × 60
and takes the target when that step would pass it (`0x004062A0`). On a wide screen a factor over 1 becomes
(factor − 1) × 0.975 + 1. The node's field of view is factor × 1.1623551 (looking back at most 1.3 × 1.1623551).

## The views on the car's sides (`0x004027B0`)

The scene camera's motion blur (`+0x224`, `+0x228`) 0.5 each. With the reset the camera's node hangs from the car's
(once, `+0xB4`), its turn 0, the race front end's window choice 0, its place (105, 70, -10) for mode 3 or
(-105, 70, -10) for mode 4 (`[0x0071F984]`, `[0x0071F994]`) and the field of view's factor 1. The field of view is
1.1623551 every frame. A changed look back turns it 0 and puts it at the side's place, or at (0, 60, -85)
(`[0x0071F9E4]`) turned half round. The director's cut comes within 2 s when another car's origin is within 0.75 of
its chassis's radius of the camera, or when a wall stands between the car and the camera (`0x005046E0`). The shake
(`0x00402F90`) by 0.005 x the speed at the pace 1.

## The controls (`0x00406AB0`)

Not while paused or in mode 14. When CHANGE_CAMERA goes down while the count is above 4 the cars settle on the grid
again (`0x004975A0`, not online) and the count goes on from 4, and the race front end's black (`[[G+0x3C]+8]+0x10C`)
becomes 1 and clears over a second: so Change View (V, Insert, the pad's view button) skips the fly-by (mode 9 goes
to 0) and the sweep before the start. The driver's control has a second way (`0x0041F7C1`, see `18-racers.md`):
SELECT (RETURN, the pad's button 10). CHANGE_CAMERA goes from mode 0 to 1, from 1 to 6, from any other
to 0, kept in the profile's settings (`+0x8C`, per player). A mode is taken by `0x00406990`: when it differs from the
current one (or the current one is 11, 12 or 13, or a reset is asked for) the mode, the reset (set unless going from
0 to 1) and `+0xB0`; always the ground view's nearness (`+0x164`) and the forced look back (`+0xF4`) cleared; then
the camera runs a frame of 0.02 s at once. In modes 0, 1, 2, 5 and 6 looking back is
REAR_VIEW held (or `+0xF4` set), else off; a change sets `+0xB0`. DRIVER_CAMERA_PAN (an ANALOGUE action: on the
keyboard N towards -1 and M towards +1, at once; a pad's second stick) turned round is `+0xCC`; the side look `+0xC8`
goes toward it by 3 × the camera's time (`+0xD0`) × the difference, and with no pan back toward 0 by 3 × that time
without passing 0 (`0x00406CDB`). The views behind the car turn by `+0xC8`; the views on the car (`0x0040350B`), not
looking back, turn a quarter turn to the side of `+0xCC` (above 0 +π/2, below 0 −π/2, else straight), a change setting
`+0xB0`.

## Before the start (`0x00405300`)

Mode 10 follows the fly-by (`15-tracks.md`) with the count at 6 s. With the reset the camera leaves the car's node and
works out a sweep across the front of the grid from the racers' places (race `+0x30`: the places at `+0x4C`, 16 bytes
each, their number at `+0xAC`; the first place's heading at `+0x58`): c = cos h and s = sin h as floats
(`0x004078D0`, `0x00407960`), nc = c × -1; the middle of the places (each coordinate summed as share × value in the
places' order, share = 1 / number); over the places, with d the middle less the place, the largest
|(dx nc + dz s) + dy × 0| (across) and |(dz c + dx s) + dy × 0| (along); along + 400. The base is (x + s × along,
0 × along + y, along × c + z); the sweep runs from base + (nc × across, across × 0, across × s) to base less that.
Each end's height becomes the ground's under it (`0x00504630`: a ray down from y 1000000 over 2000000 cm against the
faces, `0x0053F500`; the first face met, then any met deeper), plus 65. The sweep's part (`+0x128`) is 0.

Each frame the position is from + (to − from) × part, the field of view factor 0.5, the part + 0.3 × dt, the far
plane 2000000 and the field of view 0.5 × 1.1623551; the camera looks (`0x00403740`) at the point above the car
(`0x00403800`, each frame of the camera, see below). Looking at a point: d = point − position; the flat distance
sqrt(dz² + dx²); over 0 the point rises by (1 − 2000 / flat, held in 0 .. 1) × 120; the pitch −atan2(−dy, flat), the
yaw −atan2(dx, dz), the roll 0, and the node built from them (`0x00402B50`). The reset ends. With the part over 1,
then again with the count below 4, the player's mode is taken with the reset.

## The point above the car (`0x00403800`)

Each frame of the camera, before its view: n = 1 and the sum the car's origin (centimetres, `0x00407B20`); with a
followed car (`+0x440` of the race car) its origin added and n = 2; with a follower (`+0x444`) its origin added and
n + 1. The offset is the car's origin − sum × (1 / n). When n differs from `+0x15C`, or the offset moved more than
100 cm from the last frame's (`+0x14C`), `+0x15C` becomes n and the blend (`+0x160`) 1; the last frame's offset is
the offset. With w = 1 − (sin(blend × π + π / 2) + 1) × 0.5, the eased offset (`+0x13C`) becomes offset × (1 − w) +
eased × w; a blend above 0 falls by 0.5 × dt, held at 0. The point (`+0x12C`) is the car's origin − the eased offset,
50 up.

## The director (`P+0x10`, `0x004085A0`)

After the player's finish (`22-race-hud.md`) the director cuts between views. Starting (`0x00408670`): active
(`+0x00`), the camera's mode set as it is with the reset, `+0xB0` set, `+0x164` and `+0xF4` cleared, a camera frame of 0.02 s, the
time (`+0x0C`) 0 and the period (`+0x10`) given (5 s after the finish, mode 0).

Each player frame before the camera (`0x00408830`, with the frame's whole step; 0 while paused): the counter
`[0x0079E898]` (kept for the whole game) goes up by 1 and the game's `rand()` is seeded with it less
trunc(time × −10000), then one draw is passed over; the time grows by the step; the cut's transition (`+0x28`) grows
by 2 × step (`[0x00725270]`) while below 1, its object (`+0x24`, `0x00516150`) taking the value as the post effect's
amount (`[0x0075ABC4]`, `0x00516120`) or, at 1, stopping (`0x00516130`: the switch `[0x0075ABC8]` and the amount 0).
Past the period: the first time the transition starts (`+0x1C` set; one still running stopped, the switch on
(`0x00516110`), `+0x28` 0; the post effect `12-rendering.md`); the next frame the view is drawn from the table
`0x00711648` (12, 11 × 6, 5, 5, 13, 13, 12, 12, 12, 11 × 4; `rand() % 18`) again while it is the current mode and
not 11 (in a cruise, the replay's race type `[G+0x9C]+0x816` 4, the views 13, 3, 4, 5 and 6 become 12; a replay
playing, `+0x828` and `+0x7C`, takes 6; with view 5 and the car's `+0x444` the camera's `+0xF4` is set), taken
with the reset (`0x00406990`), the time 0 and the period rand() × 5 / 32767 + 3 s: at most 3 s for 13,
9999 for 11 (its own view cuts it short); `+0x1C` and `+0x20` cleared.

## The TV view (mode 11, `0x00403F30`)

With the reset the camera leaves the car's node and is placed (`0x00404150`): the car's forward speed held within
10 .. 50 m/s and a share k from 1; the point 800 × speed × k cm along the competitor's own line ahead
(`0x0049C590`; behind with `0x0049C670` while going back), and those at half and 1.5 times that; the height
(`+0xE8`) 100 cm (200 in a cruise, race type 4) above the point; the base (`+0xE4`) a draw of 0 .. 1, the range
(`+0xF0`) 1 − base when a second draw is over 0.5 else −base, the phase (`+0xEC`) −π / 2; the place drawn within 10 m
of the point in x and z ((draw − 0.5) × 2000, x then z) at base × 250 + height, brought back to 0.9 of the way to a
wall between (`0x005046E0`). When the line ahead climbs (the nearer or the farther point over 2 m above the middle of
the car's and the place's heights) the range becomes 0.1 and the place's height the point's + the height + 250. The
farthest the view may be from the car (`+0xE0`) is 1.01 × the distance; a place the car cannot be seen from (beyond
that or behind a wall) is drawn again with k less 0.1, while k stays above 0. The zoom is a draw held within
0.1 .. 0.8.

Each frame: the zoom (`0x00405DB0`, kind 2: the widest 1 − 0.1 × `+0x15C`, the cars the point above the car takes,
within 10 m, 1 − 0.001 a centimetre from there, 0.1 beyond 20 m, held within 0.1 .. the widest, eased at 0.05); a car
beyond the farthest or behind a wall cuts within 2 s (the director's period = 2 − its time, or 0); the height grows as
cars come near (`0x00404660`: 500 × dt × the most of 1 − distance / radius over the race's vehicles whose chassis node's
sphere holds the camera); the camera's height is ((sin(phase) + 1) × 0.5 × range + base) × 250 + the height; the phase
grows by dt up to π / 2. The reset's frame looks at the point above the car, the next ones follow it (`0x00403B00`): the
turn rates (`+0x54` .. `+0x5C`) grow by |sin| of the pitch, yaw and roll errors × dt × 0.36000144 toward the error's
side while within ±0.008, then the angles turn by 60 × dt × the rates and by the errors × 6.6666665, 10 and 20 × dt,
each angle wrapped into −π .. π (`0x00407910`). The far plane 2000000; the field of view the zoom × 1.1623551.

## The passing view (mode 12, `0x004047C0`)

With the reset the camera leaves the car's node and `+0xB0` is set; the counter `[0x0079E898]` goes up by 1 and
`rand()` is seeded with trunc(the car's x) + the counter; one draw is passed over, then two draws pick the start
(`+0xE4`) and the end (`+0xE8`): under 0.25 10 m ahead, under 0.5 10 m behind, under 0.75 30 m ahead, else 30 m
behind. The phase (`+0xEC`) is −π / 2 and the share kept of the last place 0.

Each frame with a = (1 − sin(phase)) / 2 the distance is a × start + (1 − a) × end; the phase grows by dt up to
π / 2. The point that far along the route's first line (behind when negative) gives the direction from the car; the
place is the car + that direction (normalised, length + 2⁻²³) × |distance|, plus the car's velocity (x and z, m/s)
× 20 × (1 − ((|distance| − 1000) / 2000 held in 0 .. 1)); its height + 200 × cos(phase) + 200 × (200 / (|distance| +
1) held in 0 .. 1) + 85, at least the ground under the camera (`+0x168`, found each camera frame at the node's last
place) + 85; then 200 cm to the side, (z, 0, −x) of the line's direction × cos(phase) × the end's sign. The camera
keeps 0.85 of its last place (all of it while dt is 0, none with the reset) and takes the rest of the new one; it
rises as cars come near (`0x00404660`) on all three axes. The reset's frame looks at the point above the car, the
next ones follow it; the zoom of kind 2 runs, then the zoom is set to 0.7: the far plane 2000000, the field of view
0.7 × 1.1623551.

## The view on the ground (mode 13, `0x00404E40`)

With the reset the camera's object at `+0x1D0` (`0x0041C350`; the chase views clear it too) is cleared, the camera
leaves the car's node and `+0xB0` is set; the point 30 m ahead along the route's first line is found, the ground under
it (`0x00504630`; the car's height where none is found) + 10 cm is the camera's place; it looks (`0x00403740`) at the
point less the line's direction while the director's `+0x20` is set (from the finish to its first cut), else less or
plus it by a draw (under 0.5 less). The shake's phase (`+0xE8`), fade (`+0xEC`) and amount (`+0xF0`) are 0.

Each frame `+0x164` is cleared; over the race's vehicles in order, the first whose chassis node's sphere holds the
camera sets `+0x164` when the camera is within 0.3 of its radius, and shakes it: h = |the body's velocity| × 0.01 ×
(1 − distance / radius); over the amount × the fade it becomes the amount (held in 0 .. 1) with the fade 1. The roll
is sin(phase) and the pitch 0.3 + sin(0.7 × phase) × 0.25, both × the amount × the fade × 0.05 (taken before the
phase grows by fade × dt × 70 and the fade becomes (1 − dt) × fade, held in 0 .. 1); the yaw stays. The far plane
2000000, the zoom 1.2, the field of view 1.2 × 1.1623551.

## The replay's next car (mode 8, `0x00403E40`)

The replay's next car (`0x00408AC0`, `24-replay.md`) puts the camera on the next car (`0x00401DB0`), starts the
director with mode 8 for 0 s and runs the director's frame with no time; the view lasts until the director's next
frames cut away (the transition, then a view of its table): a frame or two.

Each frame the scene camera's depth of field is on (`+0x22C` 1, `+0x230` 2) with the motion blur 0.5 and 0.5 (the
speed blur puts the blur at 0 after the view). With the reset the camera's object at `+0x1D0` is cleared, the race
front end's window choice is 0 (`0x004584C0`), the camera leaves the car's node and looks at the point above the car
(`0x00403740`). Then the zoom (`0x00405DB0`) of the kind at `+0x1C`, the turn toward the point above the car
(`0x00403B00`, as the TV view's), the far plane 2000000 and the field of view the factor × 1.1623551; the reset ends.

The zoom's kinds: 0: 0.01 beyond 2000 m, 1 at a distance below 0, else 200000 / ((500 × distance + 200000) ×
`+0x20`), held within 0.01 .. 1 and eased at 0.5; 1: 1 / `+0x20`, eased at 1; any other the TV view's (kind 2). The
camera's `+0x1C` and `+0x20` are set by nothing: the constructor leaves them and the camera's memory comes uncleared
from the game's heap (`0x0059BA60`: HeapAlloc with no flags), so they hold what the block held before
(`stock-problems.md`). Here they are 0 and 0, as fresh memory: kind 0, whose aim 200000 / 0 is infinite and held at 1
(0.01 beyond 2 km).

## The free views (modes 14 and 15)

Both read the player's actions with `0x00401640` (the player's table `[[G+0xAC] + [[G+0x18]+0x28] × 4]`, its slots
`+0xEC` .. `+0x108`), but the table's making (`0x004080D0`) sets those slots to −1 and never fills them: every action
is 0. Their time goes on while paused (above), and the controls (`0x00406AB0`) do nothing in mode 14: Change View
does not leave it.

The mouse view (`0x00406DA0`): each frame the race front end's window choice 0. With the reset the zoom
(`[0x00711468]`, 1 as the game starts) 1, the pitch, yaw and roll (`[0x0079E774]`, `[0x0079E770]`, `[0x0079E76C]`) 0.
The node's place of the last frame is kept and the node (its matrix and the one after it) set to no turn at no place.
The zoom moves by its two actions × 0.01 (less the first, plus the second) within 0.05 .. 1.5; the far plane 2000000,
the field of view the zoom × 1.1623551. Two turns move by their actions × dt × −0.2375, eased in (0 stops one at
once, else the first frame takes it whole and the next ones 0.05 of it more; `[0x0079E768]`, `[0x0079E764]`), summed
and damped by 0.7 (`[0x00711464]`; `[0x0079E77C]`, `[0x0079E778]`), read by nothing. The mouse's place in the window
(the hook keeps WM_MOUSEMOVE's x at `[0x007289B4]` and y at `[0x007289B0]`) gives the pitch (y − 400) ×
−0.011780973 and the yaw (x − 400) × −0.011780973; the node turns about Y by the yaw, then about X by the pitch.
Ahead is 1 while the left button is down (`[0x0075EC7C]`, WM_LBUTTONDOWN to WM_LBUTTONUP), −1 while the right is
(`[0x0075EC80]`), else its action; its square eased in (`[0x0079E758]`) × its sign × dt × 2200 is added to the speed
ahead (`[0x0079E760]`), then × 0.7; aside the same from its action, × 350 (`[0x0079E754]`, `[0x0079E75C]`). The offset
of each row of the turned node is (ahead × its z + aside × its x) + its place, the height's raised by its action × dt
× 7500; the roll moves by its action × 0.1 within −π/2 .. π/2 and turns the node about Z. The camera's place is the
node's last place plus the offset. The hook also keeps the mouse's moves (`[0x007289AC]` and `[0x007289A8]`, a stock
problem: both from x), read by nothing; the buttons' flags stay as they are while the hook is off.

The still view (`0x00407310`): with `+0x1E8` 0 (from the constructor, never set) the camera's place is the node's
(where the last view left it) and the zoom (`+0x1E4`, 1 from the constructor) is held within 0.05 .. 1.5; the far
plane 2000000, the field of view the zoom × 1.1623551. The other branch, never taken: the window choice 0, the node
reset (`0x00534D80`) with its last place kept, the zoom by the actions within 0.05 .. 1.5, the turns as the mouse
view's (`[0x0079E748]`, `[0x0079E744]`, damped by 0.7 `[0x0071145C]` into `[0x0079E750]`, `[0x0079E74C]`) added to
`+0x1D4` (the pitch, within −π/2 .. π/2) and `+0x1D8` (the yaw: below −π it becomes π, above π −π), the node turned
by them; ahead and aside as the mouse view's (the buttons ± `[0x00711458]` 1, × 2200 and × 350; `[0x0079E738]`,
`[0x0079E740]`, `[0x0079E734]`, `[0x0079E73C]`) from the node's last place; no rise and no roll.

## The start (`0x004A0220`)

The race state's count (`+0x38`) is 99 while the fly-by plays and 6 from mode 10 on. While it is above 0 it goes
down by the step time; below 0 it becomes 0 and the race starts (`0x004A00D0`: every car's automatic gearbox flag
set, its engine started if off, the racer's `+0x40` 0; the airhorn at 55000 Hz). Each time it goes below the next
whole second (`+0x3C`, from 3) the airhorn sounds at 44000 Hz and the second goes down by 1. A count that lands on
exactly 0 never starts the race (a stock problem). While the count is not 0 the driver's control holds a player's car
with the handbrake; from 10 up it does not read the controls.
