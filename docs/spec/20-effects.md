# Particle effects

The effects manager (`0x14C` bytes, made by `0x00504CC0` with the game object at start, kept at `[0x0075F0C8]` and
the game's `+0x20`) holds the effects' definitions, the emitters made from them and their materials. The particles
are stepped and drawn in the world's main pass (`0x00547690` → the manager's `+0x10`, `0x00505030`).

## The definitions

As a race loads (the loading thread, `0x00461390`), the manager's `+0xCC` is set to 1 and two files are read into its
list (`+0x88`): `scripts\effectsdefs_com.txt`, then `effectsdefs_wet.txt` when the race's weather (`+0x70`) is 2,
else `effectsdefs_dry.txt` (`0x00503610`, the script reader with the handler of vtable `0x006F70B8`; the keywords and
their handlers are listed in `05-scripts.md`).

`[BeginEffect] <name>` starts a definition (`0x160` bytes, `0x00508630`) with these defaults (a name of 64 characters
or more is not taken):

| Offset | Keyword | Default |
|---|---|---|
| `+0x04` | (the name) | |
| `+0x44` | `MATERIAL_NAME` (taken below 64 characters) | `smoke_tarmac` |
| `+0x88` | `BASE_TYPE` | SMOKE (0) |
| `+0x8C` | `NUM_POINTS` | 50 |
| `+0x90` | `COLOUR_START` r g b a | 128 128 128 35 |
| `+0xA0` | `COLOUR_END` | 128 128 128 0 |
| `+0xB0` | `COLOUR_VARY` | 10 10 10 0 |
| `+0xC0` | `BRIGHTNESS_RANGE` | 0.25 |
| `+0xC4` | `EMIT_INTERVAL` | 0.1 |
| `+0xC8` | `AGE_RANGE` | 0 |
| `+0xCC` | `START_SIZE` | 20 |
| `+0xD0` | `GROWTH_RATE` | 5 |
| `+0xD4` | `GRAVITY` | 0 |
| `+0xD8` | `FOLLOW_FRAC` | 0 |
| `+0xDC` | `FOLLOW_FADE` | 1 |
| `+0xE0` | `ROTATION_RATE` | 0.3 |
| `+0xE4` | `FADEIN_TIME` | 0.2 |
| `+0xE8` | `INITIAL_TEMP` | 0.7 |
| `+0xEC` | `COOLING_RATE` | 0.02 |
| `+0xF0` | `WIND_RESISTANCE` | 0.95 |
| `+0xF4` | `EMIT_DIRECTION` x y z | 0 1 0 |
| `+0x104` | `EMIT_SPEED` | 10 |
| `+0x108` | `SPEED_RANGE` | 3 |
| `+0x10C` | `RANDOM_DRIFT` | 24 |
| `+0x110` | `TRIGGER_LEVEL` | 0.6 |
| `+0x114` | `MIN_AMOUNT` | 0 |
| `+0x118` | `LIFETIME` | 0 |
| `+0x11C` | `SOUND_VOLUME` | 0.5 |
| `+0x120` | `SOUND_ROLLOFF` | 0.001 |
| `+0x124` | `SOUND_MINDIST` | 1 |
| `+0x128` | `SOUND_PITCHMUL` | 1 |
| `+0x12C` | `SOUND_TRIGGER_LEVEL` | 0.3 |
| `+0x130` | `SOUND_MIN_VOL` | 0 |
| `+0x134` | `SOUND_DOPPLER` | 0 |
| `+0x138` | `SOUND_TRIGGER` | 1 |
| `+0x13C` | (has sounds) | 0 |
| `+0x140` | `SOUND_EFFECT` name [value] (12 bytes each) | none |
| `+0x150` | `LOD_OFF_MUL` x 1000000 | 1000000 |
| `+0x154` | `NUM_TEXTURES` | 1 |
| `+0x158` | `BOUNCINESS` | 0.3 |
| `+0x15C` | `MAX_VIS_SIZE` | 0.1 |

A word missing from a line reads as 0. `NUM_POINTS` is the number (an unsigned one) times the manager's `+0xCC`, cut
toward zero; `NUM_POINTS_PS2` is read and dropped. The base types (`0x00507350`, unknown words leave it): SMOKE 0,
FRAMEBUFFER 1, WHEELSPIN 2, LEAVES 3, TRACKS 4, SPARKS 5, FLAKES 6, GROUNDRUSH 7, FLAT 8, RAIN 9, SPLASH 10, SMEAR 11,
GRASS 12.

`SAME_AS <name>` copies everything of the first definition of that name (`0x00503800`) but the name, its sounds
included (`0x00506F60`, `0x00508730`). `SOUND_EFFECT <name> [<value>]` is read only while the game's sound is on
(`[0x0075F860]`): the sound bank's ids of the name and the value (1 when not given) are added and `+0x13C` set;
`NO_SOUND_EFFECT` clears them.

`[EndEffect]` (`0x00508340`) scales the points by the particle density `[0x0075F794]` (nothing when it is 0), in the
loading thread's double precision, the counts cut toward zero:

- at a density d of 1 or more: SMOKE: f = d x 0.125 + 1, the points x f, the growth rate x 1 / f; LEAVES: the points
  x 2d; SPARKS: the points x d x 0.5, the start size / ((d - 1) x 0.1 + 1), the growth rate / ((d - 1) x 0.05 + 1);
  the other types are kept;
- below 1: any non-zero count x d;
- then at most 3000.

The definition is added at the end of the list and its material loaded (the manager's loader, `+0x84`). A name is
found by its first definition (`0x00503800`, without regard to case). Stock problem: a definition made `SAME_AS`
another takes its already scaled points and sizes and is scaled again unless it gives its own.

The particle density is 0 at start. A command line switch `-1` .. `-7` sets 2, 4, 6, 8, 32, 64, 100; else, as the
device is made (`0x00513136`), it is 8 with pixel shaders 2.0 (the shader classes' `0x40000`), threading on
(`[0x0071E5B4]`) and two processors for the game (`[0x0079E5C0]`), 0.5 with fixed function only (no class of
`0xE0000`).

## Making an emitter (`0x00503850`)

An effect is made by its name: none when the name has no definition; by the base type an emitter of a class:
SMOKE, FRAMEBUFFER, WHEELSPIN `0x0050B1D0` (`0x178` bytes), LEAVES `0x00508B80`, TRACKS `0x00506520`, SPARKS
`0x00509C20`, GROUNDRUSH `0x00508920`, RAIN `0x0050BE60`, SPLASH `0x0050A890`, SMEAR `0x0050A400`. FLAKES, FLAT and
GRASS have no class: the code adds a reference to whatever its pointer held (a stock problem; no definition of the
game is of these types).

Every emitter's making starts with `0x0050CE70`: with the sound on (`[0x0075F860]`) and the manager's sound bank
(`+0x80`), an effect with sounds (`+0x13C`) that loop (`SOUND_TRIGGER` 1) starts its first sound at once, the variant
drawn as `rand() % n` (n the value of its first `SOUND_EFFECT` line). The making's thread draws it: a car's effects
are made on the loading thread.

## The surfaces' effects (`scripts\surfaceeffects.txt`)

Read once, by the first car's effects object (`0x0048FA40`, the reader's map `0x00768948`): each
`[BeginSurfaceEffect] <name>` block (`0x188` bytes) names the effects of a surface — `WHEELSPIN` (`+0x40`), `TRACKS`
(`+0x80`), `SKID` (`+0xC0`), `SPARKS` (`+0x100`), `FLAKES` (`+0x140`), `RUSH` (`+0x180`: `ground_rush_street` 0,
`ground_rush_scrub` 1). `[EndSurfaceEffect]` keeps the block at the place of its name among TARMAC_DRY, SAND_DRY,
GRASS_DRY, TARMAC_WET, SAND_WET, GRASS_WET, CONCRETE, METAL, WOOD, GLASS (`0x0071AB70`; another name takes place 0) in
`[0x0075EF34]`.

## A car's effects object (the vehicle's `+0x64`)

Made with the vehicle in the race (`0x0048FA40`): `+0x54` is 1 in a race of type 4, else 0.4; `+0xF0` the
atmosphere's `Wet` (`+0x174`). Set up as the race is set up (`0x0048FEA0`, in `0x00495DE0` after every car is made,
before they settle): `tracks_tarmac` (`+0xE8`, only when dry); at each exhaust node (the vehicle's `+0x14` / `+0x18`)
a `nitro` emitter (`+0x60` / `+0x64`) and an `exhaust_flame` mesh node (`0x00556AE0`, its making `0x005550D0` draws
four `rand()` as every mesh node does); `Heat_Haze` (`+0xE4`) on the car's node; the car's
origin x 100 (`+0x30`); `explode_sparks` (`+0xD0`), `explode_smoke` (`+0xD8`), `chassis_smoke` (`+0xDC`), the
SPARKS (`+0xD4`) and FLAKES (`+0xE0`) of TARMAC_DRY; for each wheel the SKID of TARMAC_DRY (`+0x98`) and of SAND_DRY
(`+0xAC`) — TARMAC_WET and SAND_WET when wet — and `wheel_sand` (`+0x80`; `wheel_spray` when wet); then the WHEELSPIN
of the two surfaces (`+0xA8`, `+0xCC`). In a race of mode 3 the wheels' effects and the WHEELSPINs are not made.
Every wheel's surface counts as changed (`+0x234`).

The exhaust nodes (`0x00485A0E`): with a part node EXHAUST_C (the model's `+0x48`) the node found by `EXHAUST_C`;
else with EXHAUST_S and EXHAUST_O (`+0x4C`, `+0x50`) the nodes of both; else the node found by `EXHAUST_S`, if any.

Each frame of the car (`0x004924A0`, after its motion, see `19-damage.md`):

- the tyres (`0x004907D0`): a wheel whose surface changed (and is known) is cleared of the change and takes the skid
  and the WHEELSPIN of its surface: of sand for sand and grass (`+0xAC`, `+0xCC`), else of tarmac (`+0x98`, `+0xA8`);
  `+0xBC` of each wheel falls by 0.05 to 0; for each wheel not on the ground (or the car out of the race,
  `+0x4EC`, or a replay playing) the squeal's pitch (`+0x94`) is 0; once the wheels are done, with a squeal and a
  pitch of 0 the pitch is drawn: (`rand()` / 32767 - 0.5) x 8000;
  a wheel down whose level as made (dry or wet) is over 0.05, the car not on the stand (`+0x19C`), wears the tyres
  (`0x0048EF70`): the car's `+0x74` grows by 0.0004 x the frame's time within 0 .. 1 (`0x0048F170`), and each tyre
  contact's LateralMu (`+0x54`) becomes the car's `+0x78` x (1 - 0.1 x the wear), its `+0x70` worked out again;
  `+0x78` is the last tyre contact's LateralMu as the race sets the car up (`0x00485CC5`);
- the flames (`0x004912D0`): on nitro (`+0x190`) their level (`+0x5C`) is (the engine's speed - 160) x 0.002 within
  0 .. 1; at each exhaust node, while it is above 0.005, the fade in (`+0x50`) grows by 3 x the step to 1, one
  `rand()` r (over 32767) gives the flame's mesh node (`+0x68` / `+0x6C`) its matrix, a turn of 45 degrees about z
  (`0x006FFDD0`, a float's pi / 4) times the scales (c1 r + 0.007, c2 r + 0.007, -(c0 r + 0.01 x the level)), each x
  the fade in, 35 cm behind the exhaust (`0x0041B520`), c (3, 2, 2), in a replay's race (`[G+0x6C]+0x68`) (5, 3, 3),
  and shows it (`+0xC0` & ~4); then the level falls by 4 x the frame's time; at 0.005 or less the fade in is 0, the
  emitter's level 0 and the mesh node hidden (`+0xC0` | 4);
- the impact (`0x004916F0`), on the frames the impacts run (the damage it does: `19-damage.md`). With the energy at 50
  or more (a hit) the speed against the other side is the car's velocity of the frame before x 0.01 less the other
  body's (`+0x178`); unless `explode_sparks` still bursts (`+0x78`), it, `explode_smoke` and the FLAKES take the level
  0.0004 x the energy (within 0 .. 1), the touching contacts' mean arm as their place and the car's origin and velocity;
  the scrape's level (`+0x58`) becomes 0.04 x that speed (within 0 .. 1) when that is higher, and the scrape's SPARKS
  and `chassis_smoke` take it, the same place, origin and velocity. Without a hit the explosion's three are put at 0
  (again unless the sparks burst) and the scrape's two take `+0x58` with the car's origin and velocity. Then `+0x58`
  falls by 5 x the frame's time to 0, and both sparks and the FLAKES take the car's lowest touch (`+0x16C`) as their
  ground.

## The particles

The effects manager's lists keep its emitters by their class: FRAMEBUFFER at `+0x08`, SMOKE and WHEELSPIN at `+0x18`,
SPARKS at `+0x28`, LEAVES, GROUNDRUSH, RAIN and SPLASH at `+0x38`, TRACKS at `+0x48`, SMEAR at `+0x58` (each joins its
list as it is made, `0x00422DA0`; at the end of the race track's making, `0x004254AF` → `0x005043A0`, each list is
sorted by the BASE_TYPE, the higher first, the same type by the emitters' addresses). The world's main pass draws them
after the lens flares (`0x00547CB8` → the manager's `+0x10`, `0x00505030`): the object constants of no object, CULLMODE
none and the declaration `+0x144` (place, colour, u v); the groups `+0x68` (the lists `+0x48`, `+0x38`, `+0x28` in that
order, `0x005030C0`) and then the smoke's list (`0x00504300`) each give every emitter its frame (`+0x14`), the last that
answers 0 leaving its list (and each emitter's sound, its slot 7, placed where it is); then the default cull again. The
FRAMEBUFFER list (`Heat_Haze`) is drawn only by the manager's slot 2 (`0x00504ED0`: unless `[0x0075F0D8]`, the vertex
constants c0 .. c2 and c4 .. c6 the identity, c18 .. c21 the camera's, CULLMODE none, the declaration, the list's frames
(`0x00504290`), the default cull), which nothing found calls: the heat haze is never stepped nor drawn.

The frame's time (`0x00503D10`): the race world's time (`+0x9C` the world, set as the race track is done,
`0x004254A9`; its clock stops while the race is paused, `12-rendering.md`), the step
from the last frame's time at most 0.04 and not below 0 (`+0xC4`); without a clock 0.02. `+0xC8` is the renderer's
`+0x188`. The camera's axes are kept (`+0xD0`, `+0xE0`, `+0xF0`), and the quads' axes (`+0x110` ..) are those of the
camera turned about its view by 0.3 x the time (none with `+0x98`). The wind steps (`0x00506340`).

The colours of the particles in the sun (`+0xA0`) and in the shade (`+0xB0`) come from the atmosphere (`0x005339A0` →
`0x00503480`): by day the ambient A and the first light's colour x the height of the sun (the light's direction's y,
reversed) B; at night A the ambient x 0.5 and B the ambient. The sun's colour is (A + B) x 0.6, the shade's A x 0.6.

The particles have their own generator (`0x00509B70`): 17 words, two places running down from 0 and 10 (each back to
16 below 0), the sum of the second word turned left by 13 and the first by 9 replacing the first, its high 23 bits a
float of 0 .. 1. Its words are 0 until it is seeded (`0x00509BE0`: each word 1 - the last x 0x53A9B4FB, from the seed);
only the sparks seed it, with 0.

### The tyre tracks (TRACKS, `0x00506520`)

A car's `tracks_tarmac` (set up at `0x0048FED2` with the race's cars, `0x00506560`) keeps a ring of NUM_POINTS / cars
entries (`+0x6C` .. `+0x78`; the last place is never used), each 0x10 bytes of the car's origin and 8 bytes a wheel: the
touch less the origin in steps of 2.2 (signed bytes within ±127), two alpha bytes and the strip's side (two signed
bytes); and a track per wheel (`0x0050D3E0` on the strip base `0x0050C390`, vtable `0x006F75CC`) with 240 / cars strip
places (0x3C bytes: the bounds, the first entry, the entries, the definition) and a buffer of 0x36 vertices.

Each frame of the car (`0x004907D0`): a wheel whose surface changed asks its track for its surface's TRACKS
(`0x00503800`; a different one is taken at the next advance, `+0x80`, `+0x84`); the ring advances when a mark asked
it last frame (`0x00506800`, each track's `0x0050C6C0` with the next entry); the current entry takes the car's origin;
a wheel not in the race puts its mark off (and the tracks' level 0); then each wheel down marks its touch with two
levels from the wheels' skid and smoke (the axle's mean and its own; the right wheels the mean first).

A mark (`0x0050C7D0`): with a touch the track is on unless the two levels' sum is below the asked definition's
TRIGGER_LEVEL; a change of on or off, or a touch more than √5000 cm across the ground from the last change, asks the
ring to advance. On, the wheel's slot of the entry takes the touch, the alphas ((the level + MIN_AMOUNT − the trigger)
/ (1 − the trigger) × (1 − BRIGHTNESS_RANGE + a draw × BRIGHTNESS_RANGE), × 255), and the side: the move's normal
across the ground × (a draw × 10 + 117) each (four draws of the particles' generator). The entry the mark went on at
takes the side of the next and its alphas MIN_AMOUNT rounded (`0x00424230`).

An advance: the oldest strip starting where the entry after the next is loses that entry, or is dropped at 3 entries
or less; then the track starts a strip (on: two entries from the marked one, its bounds the origin with a pad of 200,
`0x0050C5E0`), closes it (off, `0x0050C670`), closes and starts again (26 entries, or the definition changing this
frame), or the strip grows by the entry (`0x0050D0C0`: the box and the radius, half the diagonal + the pad); the next
entry takes the origin and the wheel's slot.

Drawing (`0x0050D490`, from the manager's `+0x38` list): from the oldest strip, each of more than one entry whose
distance across the ground from the camera (squared, less the radius squared) × the sine of half the field of view is
within LOD_OFF_MUL and whose sphere is in the view (`0x0050C560`) is a triangle strip with its definition's material:
each entry's place 10 up, ± the side × START_SIZE, COLOUR_START's colour, alphas the entry's bytes × the level, u 0 and
1 by turns, v 0 and 1 across. The level starts at (the ring's entries − the entries drawn last frame) × the step
(COLOUR_START's alpha / the ring's entries) and grows by the step to the alpha / 255 after each entry.

At a restart (`0x00492710`) each wheel's mark is put off, the tracks' level 0 and every track reset (`0x0050C440`).

### The wind (`0x00505740`)

Its air is still with a sway of 50; each car's chassis node (`0x00486408`) is a source. Each frame a source's velocity
is its node's move over the step (a move over 10000 a second stops it). The air at a particle (`0x00505CF0`): for each
source whose three times squared distance is under 100 x 200², the particle is pushed away from it by the speed of the
source against the emitter / (that distance less 200², at least 1) x 50 (at most 15) times the offset; a particle not
yet caught, ahead of the point 200 behind the car along its z axis (a tenth of its y axis down), is caught in its wake
(`0x005059B0`): it turns about a line 20 up and 40 to its side of that point, its radius its distance from the line (at
least 10), its frequency the car's speed x 20 / (radius² x distance + 5), at most 2.5 (below 0.05 none). A caller
that asks for a kick (neither the smoke's `0x0050B5C6` nor the leaves' `0x00509034` does) gets, for a particle caught
with a radius within 0 .. 200 by a car faster than 2500, the wake's axis x the car's speed x -0.95 (`[0x007256EC]`) x
`rand()` / 32767. Then the sway: cos(time + x x 0.002) x 50 across x, sin(1.5 x time + z x 0.003) x 50 across z.

### Emitters with particles (`0x0050F490`)

NUM_POINTS particles of 0x48 bytes each: a place, an age (-1 when not alive), the emitter's level when let out and
whether its light was over 0.5, the turn in a wake. Its life is LIFETIME, or the points x EMIT_INTERVAL. Its colours in
the sun and in the shade are those of the manager and COLOUR_START halved, the alpha COLOUR_START's. An emitter's light
(`0x0050F8C0`) is the sun map at its place. Each frame (`0x0050F920`): the light sampled again once it moved 300 across
the ground (with a node); without a node its velocity 0; with its level at the trigger or above, its timer falls by
0.02; while the trigger x `+0xC8` is not above the level, the timer not above 0 and fewer than 12 let out, the timer
gains EMIT_INTERVAL and the next particle (round the ring) starts at its place less its velocity x a spread (0.01,
then 0.007 more each).

SMOKE (`0x0050B1D0`, also FRAMEBUFFER and WHEELSPIN): its colours x 0.8 (the alpha twice). Each particle's step
(`0x0050B4F0`) draws three values of the generator (r1 - 0.5, r2, r3 - 0.5, alive or not); alive, it ages by the step
and past its life ends; else its velocity is the wind x WIND_RESISTANCE less GRAVITY in y, x (1 - k), plus the emitter's
velocity x k (k = (1 - min(age / life x FOLLOW_FADE, 1)) x FOLLOW_FRAC), plus the drift (r3, r2, r1) x ((1 - MIN_AMOUNT)
x its level + MIN_AMOUNT) x RANDOM_DRIFT, plus in y INITIAL_TEMP / (COOLING_RATE x age + 1); caught in a wake it turns
on by the step x its frequency x 2 pi (its frequency x 0.995). Its size is GROWTH_RATE x age x min(level + 0.25, 1) +
START_SIZE, its colour the sun's or the shade's, its alpha x (1 - age / life) and over FADEIN_TIME faded in. Each live
particle is a quad facing the camera, its centre moved toward the camera by the size, the texture's corners in one of
four turns by its number; the colour a, r, g, b cut toward zero, the alpha x 1.5. The quads go into the emitter's buffer
and are drawn with the effect's material over the shared index buffer (each quad 0, 1, 2, 3, 2, 1).

### SPARKS (`0x00509C20`)

No particles are kept. A level above the trigger begins a burst lasting 500 x AGE_RANGE seconds (`0x00506B40`; each
raise of the level makes it last that long from now, `0x00506A00`), drawn while its sphere of EMIT_SPEED is seen; at
its end the level is 0. The level also sets the spray: the velocity's direction x -0.5 x the level. Each frame the
generator is seeded with 0 and each point gets its speed (r + 1) x EMIT_SPEED, its time the fraction of the burst's time
/ (speed x AGE_RANGE), its colour from COLOUR_START to COLOUR_END by that time (rounded), its direction the spray plus
EMIT_DIRECTION's spread (x, z within ±, y up) x the level, its width (r + 1) x the level x START_SIZE and its fall
(r + 0.25) x GRAVITY x time²: a streak from its place at that time to its place GROWTH_RATE later, as wide as its width
across the camera's view.

### The camera's emitters (`0x0050D7F0`)

RAIN, SPLASH and GROUNDRUSH share a base (vtable `0x006F75F4`, `0x2E0C` bytes): NUM_POINTS (`+0x5C`), RANDOM_DRIFT
(`+0x60`) and twice its square (`+0x64`), START_SIZE (`+0x68`), GROWTH_RATE (`+0x6C`), FOLLOW_FADE (`+0x70`), the
colours COLOUR_START and COLOUR_VARY (`+0x7C`, `+0x8C`), BOUNCINESS's size (`+0x78`) and whether it is below 0
(`+0x9C`), the definition's material (`+0x2E04`) and NUM_TEXTURES (`+0x2E08`); how much shows (`+0xA8`) 1. Slot 10
(`0x0050D970`) gives it a camera (`+0x100`) and a value (`+0x74`) and keeps the Night of the camera's world's
atmosphere (`+0xA0`); slot 11 (`0x005088D0`) how much shows, held within 0 .. 1. A frame begins (`0x0050DA40`) only
for its camera: the camera's place (`+0xB0`), how much shows × the renderer's `+0x188` (`+0xA4`), the move since the
last frame (`+0xC0`, the last place `+0xD0` starting at −FLT_MAX) and whether it is over 1000 across (x² + z² above
1000000, `+0xAC`). A point on the road (`0x0050DB50`): two `rand()` × 1/32767 r1, r2, the offsets across x (r2 −
0.5) × 1.5 × RANDOM_DRIFT and z (r1 − 0.5) × 1.5 × RANDOM_DRIFT from the camera, a tenth of its move added; the
highest face under it (`0x00504630`, the world's collision), else under the place halfway back, else the world's
origin; 2 below the face.

GROUNDRUSH (`0x00508920`) is the base and draws nothing (its frame `0x0042E470`). RAIN (`0x0050BE60`): its colour
the sun on particles with alpha 0x38 (each channel cut toward zero), a vertex buffer of NUM_POINTS × 4 and the shared
index buffer; a frame with something showing makes NUM_POINTS × the share (cut toward zero) streaks, each from three
`rand()` × 1/32767 r1, a, c: the point camera + ((a − 0.5) D, (r1 − 0.5) × 0.75 D, (c − 0.5) D) × 1.5 + the camera's z
axis × D (D = RANDOM_DRIFT); the quad from it, START_SIZE up plus the camera's move, BOUNCINESS's size along the
camera's x axis, u v (0, 1), (0, 0), (1, 1), (1, 0); drawn with its material as a list of 2 × count − 2 triangles,
and the place kept. SPLASH (`0x0050A890`): its colour with alpha 0x80; NUM_TEXTURES frames in rows of four (a frame
1 / ((n − 1) & 3 + 1) across and 1 / ((n − 1) >> 2 + 1) down); each splash's frame starts at `rand() % n` (the making
thread's). A frame: per splash its frame number (cut toward zero; from n on back to 0), the frame grows by
FOLLOW_FADE; a camera that jumped or a frame number of 0 puts the splash on the road again; a quad START_SIZE up and
BOUNCINESS's size along the camera's x with its frame's corner of the texture; six vertices a splash, the last
repeated and the next splash's first before it, drawn as one triangle strip of 6 × count − 4 triangles.

SMEAR (`0x0050A400` on the base `0x0050EA80`, vtable `0x006F7494`): raindrops on the screen. It keeps NUM_POINTS
(`+0x64`), START_SIZE (`+0x68`), GROWTH_RATE (`+0x6C`), EMIT_SPEED (`+0x70`), GRAVITY (`+0x74`), RANDOM_DRIFT
(`+0x78`), how much shows (`+0x80`, slot 10 `0x005BE6C0`, not held; 1 at first), a TexShape2D material (`+0x5E90`, made
by the material manager) with the texture `rainsmear` (`+0x5C`) and a blob of 0x98 bytes per point (`+0x60`): its life
(`+0x10`), size (`+0x14`), place on the screen (`+0x18`), the trail's last point (`+0x20`), colour (`+0x28`), the
trail's oldest pair, the pair written next and their count (`+0x2C` .. `+0x34`) and six pairs of points (`+0x38`).
Clearing it (slot 4, `0x0050ECC0`) empties every blob with a life of `rand()` × 1/32767 and white, the camera's last
place the origin. The effects manager keeps it on its list `+0x58`, drawn by its slot 1 (`0x005041F0`) when the view
ends with the camera's screen smears (`12-rendering.md`): the view's corners kept (`+0x100` .. `+0x10C`), each frame.

A frame (`0x0050A4A0`) first moves the blobs (`0x0050ED60`): the step (at most 0.05; none at 0); the camera's move
since the last place, L its length + 0.1, made unit; its parts along the camera's x and y axes and their change over
the step; a move over 1000 only takes the place. The view's middle across and 3/4 down it, its width and height and a
tenth of each; the move's parts made unit: with its part along the camera's z above 0.1 the spread is EMIT_SPEED,
else 0. How much shows × the renderer's `+0x188` (`+0x7C`) gives the blobs taken again: NUM_POINTS × it, cut toward
zero; each of those whose life reached 1 or that left the view by more than its tenth (the bottom taken at the right
side's x plus the tenth, a stock problem) starts again: life 0, size (`rand()` × GROWTH_RATE / 32767 + START_SIZE) ×
(shown + 1) / 2, its place drawn across the view, one pair of the trail there. Every blob then moves by its size × the
step × (sin(θ) r − 0.1 × the change across + cos((0.004 L + 0.1) x) w, cos(θ) r + GRAVITY − 0.1 × the change up +
sin((0.003 L + 0.1) y) w), θ the angle of its place from the middle (atan2 of x, y), r = (its distance + 10) × L² ×
the spread, w = L × RANDOM_DRIFT + 4; its alpha (1 − life) × 0.4 × 255 cut toward zero, its low byte (past its life it
wraps around); its life grows by (shown × 0.2 + 0.8) × (10 + L) × 0.01 × the step. Over 10 from the trail's last
point a pair of points half its size each side of the way it went is written and the next pair moves on (six, the
oldest given up); the pair written next always holds the blob's place twice. Then the material (its texture
`rainsmear`, parameters 0 and 1 cleared), CULLMODE none; each blob's trail as a strip of its pairs (z 10, u v 0.5 and
0.6) with its alpha growing from 0 by the blob's alpha / the pairs (drawn with more than two pairs); then every blob as
a quad of its size each way (z 5) in one triangle strip of 6 × NUM_POINTS − 4 triangles.

The race's camera (`0x00401760`, made with the player on the loading thread, `0x0041DE30`) keeps the atmosphere's Wet
(`+0xD8`) and LightBlur (`+0xDC`) and makes (`0x00401AE0`) in the wet `rain1` (its value 0, `+0x18C`), `rainsplash1`
(1, `+0x190`) and `rainblob1` (`+0x194`), in the dry `ground_rush_street` (0) and `ground_rush_scrub` (1). Its frame
(`17-race-camera.md`) sets how much they show.

### LEAVES (`0x00508B80`)

On the base `0x0050DD60` (vtable `0x006F7624`; LEAVES's `0x006F7440`, `0x16C` bytes). With F = the particle density
(`[0x0075F794]`) × 0.4 + 1 it keeps NUM_POINTS (`+0x5C`), the radius F × RANDOM_DRIFT (`+0x60`) and its square
(`+0xB4`), START_SIZE (`+0x64`), GROWTH_RATE (`+0x68`), ROTATION_RATE (`+0x6C`), COLOUR_START, COLOUR_END and
COLOUR_VARY (`+0x70`, `+0x80`, `+0x90`), BRIGHTNESS_RANGE (`+0xA0`), WIND_RESISTANCE (`+0xA4`), RANDOM_DRIFT × 0.05
(`+0xA8`), GRAVITY (`+0xAC`), FOLLOW_FRAC (`+0xB0`), the definition's material (`+0xC8`) and a leaf of `0xB4` bytes per
point (`+0xCC`, its memory not cleared). The game's leaf colours are set by every one made: in the sun
(`[0x007845C0]`) (the sun on particles `+0xA0` + COLOUR_START) × 0.75, in the shade (`[0x00790150]`) (the shade's
`+0xB0` + COLOUR_START) × 0.4. The first made in the game's run (`[0x0075F0F0]`) fills the leaves' tables of 3000
entries, three `rand()` of the making thread each: two angles A, B (`rand()` × 2π / 32767, `0x00790160`,
`0x00790164`), a rate (`rand()` / 65534 + 0.25, `0x00790168`); the leaf's light 0 (`0x00757CE0`); the corners of its
texture by i % NUM_TEXTURES in an atlas four across (`0x00754E00`, `0x0074C160`, `0x00751F20`, `0x0074F040`, read by
nothing found); the axes of a leaf lying flat (cos A, 0, sin A) and (−sin A, 0, cos A) (`0x00778A40`, `0x007845D0`).

LEAVES itself: two vertex records (`+0x104`, `+0x130`: their count of leaves and NUM_POINTS × 4 vertices of place,
colour, u v), the shared quads' indices (`+0x160`), a vertex buffer of NUM_POINTS × 4 (`+0x168`, its key 0x1000 + 20 ×
a counter `[0x00725714]`). Each leaf's u v in both records: the corners of its texture in the atlas (by i %
NUM_TEXTURES) turned by i × 4 cut to a whole number: 0 gives (u0 v0, u1 v0, u0 v1, u1 v1), 1 (u1 v0, u1 v1, u0 v0,
u0 v1), 2 (u1 v1, u0 v1, u1 v0, u0 v0), any other (u0 v1, u0 v0, u1 v1, u1 v0), so every leaf but the first takes the
last.

Clearing it (slot 4 `0x0050E2B0`; slot 1 `0x0050E2A0` calls it) puts every leaf anew (`0x0050E6B0`, the caller's
`rand()`), then its bounds about its place with the radius R. A leaf put anew: at the emitter's place; two draws r1
(/ 32767), r2 (/ 65534 − 0.25); without a node it lives (`+0x10` 0) at the place + ((r1 × 0.5 − 0.25) R, R, r2 R),
with one it is gone (−1); velocity 0; its size `rand()` × GROWTH_RATE / 32767 + START_SIZE (`+0xAC`); an angle a
(`rand()` × 2π / 32767) and its drift (sin a, 0, cos a) × RANDOM_DRIFT × 0.05 (`+0x5C`); r3, r4 (/ 32767): its alpha
COLOUR_START's + COLOUR_VARY's × r3, its colour (COLOUR_START + COLOUR_VARY × r3) × (r4 × BRIGHTNESS_RANGE + 1 −
BRIGHTNESS_RANGE) (`+0x6C`), each within 0 .. 255 (only the alpha is used); no turn in a wake; its last place
(`+0x8C`) here; the road under it (`0x00504630`), else under the place moved half the offsets back, else it is gone;
its ground (`+0x48`) the road + 2 and the leaf on it; its light (`+0xB0`) the sun map's there, 1 without one.

A frame (`0x00509890`, never done): nothing with no points or the detail (`[0x0079E650]`) below 0.1; its bounds about
its place with 500; with `[0x00725708]` (1) nothing when the camera is further across the ground (x² + z²) than
LOD_OFF_MUL × 6. With the job thread (`[0x00719210]`, 1) the step goes to it (`0x00509870`, the emitter's count of
users raised for it) and the frame draws the record the last finished step wrote; else the step runs at once. The
record drawn: its material, CULLMODE none, the NUM_POINTS × 4 vertices written to the buffer, drawn (`0x00508970`) as
a list of as many triangles as leaves, so only the first half of the leaves shows. Under `[0x0075F0DC]`, which nothing
sets, the bounds are drawn as a circle on the screen (`0x0050D1B0` → `0x005118F0`: the place and the place the radius
along the camera's x axis on the screen of 640 x 480, neither behind the camera; round(radius x 0.1) + 8 points, at most
36, a line strip): red after the bounds are made, yellow once the camera is near enough, olive after the draw.

The step (`0x005095D0`, on the job thread with its `rand()`): the other record emptied; the leaves moved
(`0x00508F70`, with a step above 0): the bounds about the place with 200; per living leaf the wind at it
(`0x00505CF0`, no velocity of its own, which may catch it in a car's wake), its velocity × (1 − WIND_RESISTANCE); above
its ground + the wind × WIND_RESISTANCE, − GRAVITY, − (0.7 − its table light) × 20 (`[0x00725710]`); its place +
(velocity + drift × L) × step (L its table light in the air, 0 on the ground); turning in a wake as the smoke does but
its rate × 0.99 a step and let go below 0.8; across the ground beyond the radius from the place, or else by a draw
below EMIT_INTERVAL, it is gone with a node, else put again in the air (half the radius up, the offsets drawn anew);
moved over 300 across the ground from its last place: the road under it again (else put again in the air and the road
there, else at 1000000), the ground + 2, its light from the sun map, its last place here; below its ground: the road
again (else at 1000000), on it, its velocity 0; the bounds grown by it. The bounds' radius + START_SIZE. Then each
leaf's quad (`0x0050E3D0`): gone, no size (its colour the last leaf's); lying (at or below its ground, not in a wake)
the table's flat axes × its size, its light half the sun map's; else tumbling: t = the manager's time × 10 × its rate,
the angles t + A and t + B, a = (cos B, −sin B, 0) × size, b = (sin B sin A, cos B sin A, cos A) × size, the table's
light |cos B cos A| (for its fall), its light the sun map's × that. Its colour the shade's × (1 − light) + the sun's ×
light, at most 255, its alpha the leaf's; the vertices place + a, + b, − b, − a in that colour (the bytes as the job
thread's SSE conversion makes them, to the nearest). The record's count NUM_POINTS, and it is the one drawn.

As the race's load ends on the main thread (`0x004255F0`), for each point of the fast line a `rand()` below 0.3 makes
an emitter there: `Leaves` on a track whose first entry of the routes' table (`0x00426570`, its name) has the location
1, 2 or 8 (Angel North Central, Campbell Hills, Angel Westside), else `Litter`; its place the point (`+0x0C`), cleared
(slot 1) with the main thread's `rand()`, the manager's list keeping it. An effect node of the scene of a LEAVES type
is made without being cleared.

### The cars' emitters

Made with each car's effects object (`0x0048FEA0`) under the car's node: the flames at the exhausts' nodes (in the
car's neon colour, each of r, g, b at least 96, alpha 191 to 0), the heat haze, the explosion's and the chassis's, each
wheel's skids (level 0) and spray, the squeals (level 0). Each frame (`0x004907D0`) a wheel down (and no replay going
fast forward) gives its skid of its surface its level at the wheel's slot 25 below, with the car's origin and velocity,
and its spray its own level there with the wheel node's move; a wheel up puts its skid to 0. The flames (`0x004912D0`):
while on, at (0, 0, -30) under the exhaust with the level x the fade in, else 0. Each effect node of the scene
(`0x0054EF60`) has its emitter under it.
