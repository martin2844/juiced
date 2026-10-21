# The race: loading and the track

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build. The
byte layouts of the track files are those of the Blender add-on's `docs/technical` (TDF, D8G, D8M_ROUTE, D8W_D8T,
SHD_TXB, SPL, DLH); this file says how the game uses them.

## From `loading` to the race

The game object runs its phases each frame (`0x0040CCA0`): `+0x40` (loading: update, then its `+0x08` method tells
it has ended and it is deleted), `+0x34` (the race), `+0x3C`, `+0x38` (the front end), `+0x44`; then it draws.

The front end state `loading` (record `0x00721548`, update `0x0046AE50`) makes a loading phase named "load"
(`0x00462C30`, 0x5C bytes, vtable `0x006F1E24`: update `0x00463B50`, ended `0x004FA5D0` = its `+0x0C`) and puts it in
the game object's `+0x40`. Its update runs `0x00462D90` (mode `+0x08` 0) or `0x004634B0` (1); while a load job is
held (`+0x58`) it sleeps 5 ms a frame, without one it has ended.

`0x00462D90`, the first time: its own front end (`+0x54`) with the window by the race mode (`[game + 0x6C]`): 0
`arcade_loading` (with `IMAGEBOX1` = `UI_ARCADE_IMAGE<n>`), 1 `custom_loader`, else `loader`, and over it
`arcade_loader_anim` / `custom_loader_anim` / `loader_anim`; three frames of that front end are drawn; the music bank
of the race (`0x0040D330`: `race<language letter>`); then the load job (`0x00463BB0`, vtable `0x006F1E40`) goes to the
worker thread. Later frames run the loading front end until the job's state (`+0x08`) is 3: then the race is taken
(`0x004255F0`), the front end phase goes (`0x0040D030`, `0x0040D180`), the renderer is set for the race and the race
music starts (`0x0040D9F0`).

The worker thread has its own `rand()` (the C runtime's generator is per thread): the draws of the load do not move
the main thread's sequence.

The job (`0x00461390`) logs "TRACK LOAD TIME" and its parts: SoundMgr (the `audio\3d` bank, 350 sounds; for a race of
type 4 without the network also the `cruise` bank), EffectsDefs (`effectsdefs_dry.txt`, or `effectsdefs_wet.txt`
with weather 2, and the `wet` audio bank), RaceTrack (`0x00424270`), then `0x004B0330` and InGameUI (`0x004563E0`).

### The loading window (`0x00462840`)

The record `0x00721250` serves `arcade_loading`, `loader` and `custom_loader` (enter only). `TIP_MSG` takes several
lines. In race mode 4 the elements holding `TIP` are hidden and nothing more is done. Otherwise (the record's `+0x0C`
set, as the load sets it): the career tip counter `0x00715C40` (-1 at start: `rand() % 16` signed, else + 1, past 15
→ 0) and the general one `0x00715C44` (-1: `rand() % 13`, else + 1, past 12 → 0) move; the `TIP` elements are shown.
`TIP_MSG` by the race's type (`+0x04`):

| type | tip |
|---|---|
| 0 | line 76 "Complete 1 lap within the target time." |
| 1 | line 74 (… without crashing) |
| 2 | line 75 (… above the target speed …) |
| 4 | `rand() % 3`: 1 line 1878, 2 line 1879, else line 77 (the show-off moves) |
| 6 | line 73 "Caution! - Manual gears only in sprint mode."; the race's `+0x54` = 0 |
| other | a career race (mode 2) with teams (the round's `+0x17` above 1, not type 9) and `rand() & 1`: line 1822 (the winning team); else in a career race with `rand() & 1` the career tip (lines 1862 + `0x00715C40`); else the general tip (lines 1849 + `0x00715C44`) |

As a network spectator the profile's race members are cleared and `TIP_MSG` is line 1597. `TRACKPIC` shows the
picture of the track's city place (the table `0x00767E60`, by name). `TEXT1` .. `TEXT4`, `WHITE_BUT`, `DPAD_UP` take
several lines.

### RaceTrack (`0x00424270`)

The atmosphere of the race is `<track>_<time>_<weather>`: the time of day (`+0x6C` of the race: 1 `_morn`, 3
`_eve`, 4 `_night`, else `_noon`) and `_wet` for weather 2 (`+0x70`), else `_dry`.

### The ground maps (`0x004245C2`)

The race track then reads two maps of the light set (the atmosphere's ShadowSet, else the time of day less 1; a
time outside 1 .. 4 leaves -1 and names no file): `<track>_<set>.sqh` (`+0x80`, also the camera's `+0x140`), how much
the sun lights what moves, and `<track>_<set>.pqh` (`+0x84`), the puddles the wheels spray from in the wet. A map
(`0x005053B0`, 0x64 bytes) is u32 width, height (pixels, multiples of 64), bias, a first node reference (read, not
used: 0xFF is), f32 step; f32[4] the box's low and high corners; u32 the counts of nodes and blocks; a u32 per tile
of 64 x 64 pixels (row by row) naming its root node; the nodes, four u16 each (the quarters: low x low y, high x,
high y, both high; under 0xFF a code for the whole quarter, else node or block reference - 0xFF); the blocks, 8 u16
rows of 2 bits a pixel (pixel x at bits 2x). A file of another size is not read.

A point's value (`0x00505690`): the column (x − low x) / ((high x − low x) / width) + 0.5 and the row height −
(z − low z) / ((high z − low z) / height) + 0.5, both cut toward zero and kept within the map; down the tile's quarters
from 64 (`0x00505240`) to a code or, at 8, the block's bits; the code (a signed byte) over the step, cut toward zero,
plus the bias, as a byte (`0x005055D0`), x 1/255.

## The track's files

`tracks\<Track>\`: `<track>.tdf` (the routes and their layers), `<track>_<layer>.d8g` (geometry), `<track>.d8t`
(the pixels of every texture), per route `<track>_<route>.d8m` (materials), `.d8w` (textures, light tiles, render
tree), `.spl` (grid, lines), `.cld` (collision), `.ocl` (regions); `<track>_shad_<time>.shd` and
`<track>_shadowMaps.txb` (baked light), `<track>_<name>.dlh` (scene objects). The route of an event is named by the
routes table of the executable (`0x00711A3C`: track, route); in the `.tdf` it is "`<route>.wld`", its files the
names in lower case.

A route's instances are the (mesh, node) pairs of its `.lyr` layers in the `.tdf` order (layer, group, mesh, node; a
mesh without nodes once); the light sets of its `.d8w` give each instance a tile per time of day.

A texture directory (the head of a `.d8w`, a whole `.shd`) counts from the start of its pixel file: per table the
cursor moves on by the table's `skip`, the table's textures follow one another by their `dataSize`, and the cursor
is then the table's start + its `size`. The four `.shd` of a track share one `.txb` and each counts from its start:
their big tables come first one after another, their small tables lie between one another further on.

A route's `.d8w` ends with the render tree's area (x0, z0, x1, z1), the size of the tree and the tree as the
memory the game walks: depth first, an inner node (0x5C bytes: u32 flags with bit 0 clear, four child places, f32 the
split's x and z, the box's low and high corners (x y z w) at `+0x1C` and `+0x2C`, 32 bytes of regions at `+0x3C`)
followed by its four children ((low x, low z), (high x, low z), (low x, high z), (high x, high z)); a leaf (0x4C
bytes: u32 flags with bit 0 set, u32 the records' count, a place, the box at `+0x0C` and `+0x1C`, the regions at
`+0x2C`) followed by its records (0xCC bytes: u32 the instance's number in the `.ocl` order, the rest filled at the
load); then two u32 0. Each instance of the route is in one leaf; an empty leaf's box runs from FLT_MAX to −FLT_MAX.

A `.spl` has a grid of 6 places (4 for the sprint routes) and three lines of one length.

A route's `.ocl`: u32 cells along x, y, z (75, 1 .. 4, 60); f32[4] the box's low and high corners; a byte per cell
(x fastest, then y, then z), the region number; u32 a count and 32 bytes per instance of the route's geometry layers
in the `.tdf` order (the same order as the light sets): the 256 regions the instance is drawn from.

## The route's world

### Loading (`0x00543610`, `0x005450C0`)

The condition bit of the atmosphere's Night and Wet (1 day dry, 2 day wet, 4 night dry, 8 night wet) decides per
`.tdf` entry: a `.dlh` whose hidden flags have the bit is not loaded. A geometry layer is always read (`0x00543D20`):
when its hidden flags have the bit, the main view's range of its meshes becomes 0 .. 0; when its reflection flags do
not, the reflection range does; a layer in neither has no buffers.

A mesh's record (0x58 bytes): the three ranges (`+0x00`), the bounding sphere (`+0x18`, radius `+0x28`), drawn after
the cars (`+0x2C`: its material's class flags have `0x24`), the primitive (`+0x30`), the material (`+0x34`), the
vertex and index counts, the declaration, the stride, the buffers (`+0x38` .. `+0x4C`), the instances (count `+0x50`,
array `+0x54`). An instance (200 bytes; a mesh without nodes has one, with the identity):

| offset | value |
|---|---|
| `+0x00` | the regions it is drawn from (32 bytes of the `.ocl`, all set without the file) |
| `+0x20` | the node's three rows: c0 .. c2 |
| `+0x50` | the lengths of the node's columns: c3 |
| `+0x60` | the columns over their length + 2⁻²³: c4 .. c6 |
| `+0xA0` | the mesh's radius × the longest column |
| `+0xA4` | the light tile (−1 none) |
| `+0xA8` | four `rand()` × 1/32767 (c40) |
| `+0xB8` | the bounding sphere's centre in the world |

The loading thread's x87 works at 53 bits: a column's length is the square root of its squared length stored as a
float (z, y, x added); the fourth values of `+0x50` .. `+0x8C` are never written.

The light set is the atmosphere's ShadowSet; a ShadowSet of −1 takes the time of day − 1 for the times 1 .. 4
(RaceTrack writes it back). The `.d8w`'s set of that number gives each instance its tile, and its name the `.shd`
(`<track>_<set name>.shd`, pixels in `<track>_shadowMaps.txb`); without the `.shd` there are no tiles.

The lists of the regions (`0x00543610`, world `+0x174`): for each of the 256 regions, the (mesh, instance) pairs
whose region bit is set, in the order of the meshes and their instances, then ordered by the class number of the
mesh's material (0 .. 52, keeping that order within a class).

The world's `+0xA8` is the sky dome of the atmosphere (as the interface's views make it); the main view draws it
after the region's opaque instances and before the ones drawn after the cars.

### The main view (`0x00547690`, `0x005469A0`, `0x00546540`)

Before the region's instances (the reflections, below): the world's cube when the renderer's `+0x220` is set; the
lights; the time; the cars' environment (renderer `+0x118`); the wet road's reflection; then with the object
constants of no object the flares' reflections on the road (`0x0054FF00`) and the view's target taken again
(`0x00517390`, `12-rendering.md`).

The scale of the distances (`[0x0075B2CC]`) is the camera's (`0x0054CA00`) / ((renderer `+0x188` + 1) × 0.5). The
camera's position gives the region (`0x00551C00`: each coordinate brought into the box — below it or not a number the
low side, above it the high side — the cell n × (v − low) / (high − low) cut toward zero, the high side the last cell;
no grid: region 0). For each pair of the region's list: d = ((dz² + dy²) + dx²) × the scale from the camera to the
instance's centre; it is drawn when detail × near < d ≤ detail × far (the detail factor `[0x0079E670]`, the main range),
its radius is not 0 and its sphere (the mesh's centre through the instance's rows, the instance's radius) is in the view
(`0x00552A70`). A pair drawn after the cars goes to a list; that list is drawn after the cars, the last first. The sky
dome (`+0xA8`) follows the region's list; after the list drawn last the light tile is let go and the scene objects'
roots are drawn (`+0xD8`, `0x00550500`; `21-scene-objects.md`), then the lens flares (`+0x120`, `0x0054FC60`) and the
effects (`[0x0075F0C8]` slot 4). Before the roots and again before the flares the object constants are those of no
object (`0x0054D8D0`: c0 .. c2 and c4 .. c6 the identity's rows, c3 (1, 1, 1, 1), c18 .. c21 the camera's
world-to-clip). At the end the renderer's `+0x118`, `+0x11C` and `+0x164` are let go.

Drawing a pair: the stream, the indices (lists and strips), the declaration when it changed; the light tile
(renderer `+0x120`; a change forgets the current material when its class has flag `0x200`); unless the program's
technique uses the fixed-function vertices: c0 .. c6 the instance's `+0x20` .. `+0x8F`, c18 .. c21 the camera's
world-to-clip × the instance's rows (`0x00511620`), c40 the instance's random value when the technique uses it; then
per pass of the material its pass and DrawIndexedPrimitive: a list (index count / 3 triangles), a strip (index count
− 2), or DrawPrimitive of a line strip (vertex count − 1). After the view the light tile is let go.

### The reflections (`0x00547D50`, `0x00548450`, `0x00546BF0`)

What the load keeps for them:

- the cars' environment reach (`+0x190`): the greatest far of the meshes' environment ranges (their `range[3]`, as
  the file gives it); the reflection reach (`+0x1A0`): the greatest far of the reflection ranges, each first lowered
  to 5000 × the square of the greatest radius of its mesh's instances (`0x00545E6F`; both start at 0);
- the render tree (`+0x194`, `0x00548790`; its root `+0x58`): the tree at the end of the `.d8w` (below), each record
  given its mesh and a copy of its instance (`0x00543230`); the nodes' regions (`0x00544B90`) take those of a leaf's
  records and of an inner node's children, added to what the file holds there;
- `+0xCC`: the class flags of the route's materials together (`0x00543A15`); with bit 0 or 1, `+0x1E8` is
  `textures\testenv.dds` as a cube (`0x00510280`, `0x00543F05`);
- the environment's place (`+0xAC`, (0, 10000, 0) at first): an `env_camera*` dummy's (`21-scene-objects.md`).

The world's slot 10 as the race's load ends on the main thread (`0x005485A0` from `0x004255F0`) and the main pass when the renderer's `+0x220` is set draw
the world's cube (`0x00548450`): the world's `+0x198` is the renderer's cars' cube and `+0x19C` its road reflection;
with a world's cube, a camera of the world (`0x005311D0`) at the environment's place, its focus there (`0x0054CD90`),
near 1 and far 10000000, the scale `[0x0075B2CC]` 0.9, the lights; with the reflections 3 and the renderer's `+0x188`
1 for the time, the cube is drawn into the world's cube (slot 14, `+0x198` the world's cube); then `+0x198` the cars'
cube again and `+0x220` cleared.

A cube (`0x00547D50`, the world's slot 14), with the renderer's `+0x188` 0.8 or more and the reflections on: the
frames' count (`+0x1A4`) grows; the lights; the region of the camera's place; the time as the main pass takes it (the
renderer's `+0x110` and `+0x114`, `0x005155D0`); `+0x118` and `+0x11C` the world's `+0x1E8`; ZENABLE and FOGENABLE
on. With a cube (`+0x198`): the faces' cameras at the camera's focus (its slot 25, `0x0054CE60`; `0x005C9190`); c10 the
first face's projection; the instances within reach collected (`0x005471E0`, the radius the square root of `+0x190`);
then each face, all of them with the reflections 3, else only those whose number has the parity of the frames' count
(a face when (count ^ face) & 1 is 0): the face set and cleared (black, depth 1, the depth buffer's flags); the face's
camera takes the camera's near and far and its matrices; c7 its view, c14 its world-to-clip, the projection transform
(`0x005117E0` turns it over), c33 its direction (its w a stack value), c41 and c42 the sprites' axes as the views make
them; the opaque instances whose faces have this face's bit (`0x00546540`), the sky dome (slot 1), the instances
drawn after the cars that reach it, the last first; with the object constants of no object the flares' reflections
(`0x0054FE30`, `21-scene-objects.md`). The lists are emptied.

The collection (`0x005471E0`): with `[0x0071AB9C]` (1, and nothing writes it) the region's list, else the render tree
walked as the reflection walks it with the sphere of the focus and the radius against the nodes' boxes (`0x00546FD0`)
and the regions. An instance is taken when the cars' environment range holds its distance from the focus (((dx² +
dy²) + dz²) × `[0x0075B2CC]`, detail × near < d ≤ detail × far); its faces (`0x00547040`): with d the centre less the
focus and k = −radius × 1.41421, twelve tests (dz + dx, dz − dx, dy + dx, dy − dx, dz + dy, dy − dz, −dx − dz, dx −
dz, −dx − dy, dx − dy, −dy − dz, dz − dy above k, bits 0 .. 11) give +z (bits 0, 1, 4, 11), −z (5, 6, 7, 10), −x (1,
3, 6, 8), +x (0, 2, 7, 9), +y (2, 3, 4, 5) and −y (8, 9, 10, 11) when all four of a face's are set. The instances drawn
after the cars go to the world's `+0x1A8` with their faces (`+0x1C8`), the others to `+0x1B8` (`+0x1D8`).

The cars' environment in the main pass (renderer `+0x118`): the cars' cube with the reflections on and the
renderer's `+0x188` of 0.8 or more, else the world's cube. The wet road's reflection (with the world's `+0x19C`):
`+0x11C` the world's cube, `+0x164` the reflection's texture; the target taken (`[0x0075F168]`) and cleared black;
the camera's place kept (`[0x0079E22C]`); the heights of the route's faces under the camera (`0x0053F2A0`: a ray from
y 1000000 down to −1000000 through the faces of the collision's tree at (x, z), their least and greatest); the scale
`[0x0075B2CC]` the camera's. With a face under the camera and a reflection reach: the camera's far plane the reach, the
default cull 2, the render tree drawn upside down about the greatest height h (`0x00546BF0`, with the reflections on
and `+0x188` above 0.8), the sky dome upside down (slot 2, `0x0054D6C0`: about the camera's place 50 up), the
instances drawn after the cars upside down, the last first (the same condition), the list emptied, the cull 3 and the
far plane back.

The walk (`0x00546BF0`): a stack from the root; nodes without the region's bit are passed over; a node's box upside
down (y from 2h − its top to 2h − its bottom) against the camera (`0x00552D20`: outside a side plane, or beyond the
near and far distances, 0; inside all six 1; else 2): 0 leaves it out, 1 marks the stack's depth (no box or sphere is
tested again until the walk is back above it). An inner node pushes its children so that the one the camera's place
(x, z) is in comes first, then the one beside it across x, then across z, then the far one. A leaf's records with the
region's bit whose reflection range holds their distance (((dz² + dy²) + dx²) × the scale, the centre's y taken as 2h
− y) are turned upside down (their y column negated, their height 2h less it), drawn (those drawn after the cars to
the list) when the walk is inside the mark or their sphere (the mesh's centre through the turned rows, the instance's
radius) is in the view, and turned back.

## The fly-by (`0x00405BE0`, `0x0041C530`)

The race's camera step starts the fly-by of `scripts\<route>_intro.cam` (`0x0041C8D0`, the route in lower case):
without the file the count is set to 6 s and the camera to its mode 10; with it the count waits (99) while the
fly-by plays. The file: u32 shots; per shot f32 wait, u32 keys, then per key 56 bytes: f32 time (to the next key;
after the last, before the next shot), f32 fov, a word kept as w of the position, f32 x, y, z, f32 w, x, y, z of the
rotation (camera to world), a word, f32 pitch, f32 yaw (not used), a word.

Each frame (dt):

1. The fade of a new shot: its time left (1 s, `[0x00711A08]`) less dt; while above 0 the race's fade object takes
   1 − left, then its end.
2. While the shot's waited time is below its wait: the waited time + dt; the camera at key 0.
3. Else: index = (1 / time of key ⌊index⌋) × dt + index; k = ⌊index⌋ (cut toward zero: the x87 chop, or CVTSS2SI with
   the game's MXCSR rounding toward zero), t = index − k. With k past the last key the next shot begins (index and
   waited time 0, a fade of 1 s; not in race mode 3); after the last shot the fly-by ends. Else the position is key
   k's at the last key, else the Catmull-Rom curve (`0x0041C010`) through the keys k − 1, k, k + 1, k + 2 (the ends
   repeated): (((2 P1 + (P2 − P0) t) + (((2 P0 − 5 P1) + 4 P2) − P3) t²) + (((−P0 + 3 P1) − 3 P2) + P3) t³) × 0.5;
   the rotation the slerp (`0x00554830`) from key k to key k + 1 the shorter way (the second negated for a negative
   dot product; an angle within 10⁻⁶ of 0 blends straight), the fov (1 − t) fov_k + t fov_k+1.
4. The camera (`0x0041C450`): the rotation matrix of the quaternion (`0x0041D790`) with the position.
5. The step: the fov brought into 0.05 .. 1.5, the far plane 2000000, the camera's field of view fov × 1.1623551.

The fly-by's first frame sets the race's count to 99; the race (its physics and competitors) runs on during the
fly-by. When the fly-by ends the camera goes to mode 10 and the count to 6 s; below 4 s the player's camera takes
over.

## The collision (`.cld`)

A route's `<track>_<route>.cld` (read by `0x0053DFF0`; the layout is the add-on's `CLD`): the zones (surface flag,
friction and bounce factors, the contact offset and the walls' bottom and top), the vertices, the faces (three
vertex indices, the unit normal, a word 0), a zone byte per face (bit 7 cleared: the query marks collected faces with
it), the markers (position, the wall's normal) and their zone bytes, each byte block padded to 4, the tree's area and
the quadtree.

The query (`0x0053B200`) walks the tree with a box (x, z): a node is entered when the box meets its area, the edges
counting; an inner node's children cover its area split at its point in the order (low x, low z), (high x, low z),
(low x, high z), (high x, high z). In a leaf a face is collected when some corner lies strictly beyond each side of
the box (some x above the box's low x, some below its high x, the same for z) and it is not collected yet; a wall
segment (a marker and the next one) the same with its two ends. A box strictly inside a node sets the caller's
`+0xA0` (`0x0053B1D0`).
