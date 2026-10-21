# Scene objects

The `.dlh` files of a route (their layout: the Blender add-on's `docs/technical/DLH.md`) hold everything on a track
that is not plain geometry. The race's load reads them on the loading thread, in the `.tdf` order of the route's
entries, between the geometry layers (`0x00543350`): a `.dlh` entry whose hidden flags have the race's condition bit
(`15-tracks.md`) is not loaded at all; the file is `<track>_<entry name>`. Their making draws the loading thread's
`rand()` among the layers' instances.

## The manager

The race track (`0x00424270`) makes the scene objects' manager first (`0x004208E0`, `0x80` bytes, vtable `0x006EF10C`,
kept at `[0x0075ECDC]` and the race track's `+0x14`): `+0x08` the riding splines, `+0x18` the replay cameras, `+0x2C`
the atmosphere zones, `+0x58` the physics world, `+0x5C` 50 (frames before the objects' time runs), `+0x60` the
physics profiles, `+0x70` the objects. Its slot 1 (`0x00421D50`) is handed every node of a `.dlh` as it is made, its
slot 2 (`0x00422750`) every root once its tree is made.

## Reading a file (`0x0054F520`)

The objects are read first (the readers `0x0054EB20` mesh, `0x0054EC00` dummy, `0x0054EC80` light, `0x0054EF20`
effect, `0x0054F9F0` spline, `0x0054F150` helper; the common tail `0x0054E9C0`: the flags, a hull for bit 0 at `+0x34`
and for bit 1 at `+0x30` — each an `.ocd` read from the stream, `0x00535830`, its face map and tree always read — and
the properties' text at `+0x2C`). Then for each root, depth first (`0x0054F260`):

1. the node of its object (slot 1 of the object's vtable): a mesh a model node (`0x00556AE0`; its making
   `0x005550D0` draws four values, as every mesh node does), a dummy a plain node (`0x00535510`), an effect a plain node
   with the emitter of its name (less a leading `effect_`, `0x0054EF60`; `20-effects.md`), a light a scene light, a
   spline a spline node, a helper a box or sphere node; the node takes the file's world matrix and the object's name;
2. the node handed to the manager (`0x00421D50`, below);
3. its children, in turn.

Then the root through the manager's slot 2 (`0x00422750`, below); when it keeps it, the root's lights are made
(`0x0054F440`) and it joins the world.

The node types (`+0x8C`): a mesh 0, a light 2, a dummy and an effect 4, a helper 5, a spline 6.

## The properties (`0x0042A070`)

The text up to its first NUL: pairs `KEY = value`, white space being space, CR and LF only (a tab is part of a
word). A key is looked up without regard to case among the 42 names of `0x00715960` (`DLH.md` lists them; an unknown
key is 0). A value of digits, `.` and `-` only is a number (`atof`, a float), any other a text. `0x0042A510` tells
whether a key is there, `0x0042A550` gives its first entry, `0x0042A5B0` its number (0 without it), `0x0042A590` its
text, `0x0042A5E0` its number cut toward zero.

## A node handed to the manager (`0x00421D50`)

The properties are made from the object's text when it has one. By the node's type:

- a mesh: with `TIME_OF_DAY` of another time of day than the race's (1 `morn`, 2 `noon`, 3 `eve`, 4 `night`) the node
  is hidden (`+0xC0 |= 4`) and nothing is made; else the object of its name (`0x00420FF0`): `barrel*` a barrel
  (`0x00418CE0`), `bin*` a bin (`0x00429FE0`), `crowd*` a crowd member (`0x0041D870`: its turn per step `rand()` x
  4.5777765e-6, none below 0.03, and its steps to the first look `rand() % 20`), a name containing FBUMP, RBUMP, BONNET, SPOILER, SSKIRT,
  HEADLIGHT, REARLIGHT, WMIR, DECAL, EXHAUST or CHASS a car part (`0x004191C0`), `WHEEL*` a wheel (`0x0041A930`), a name
  containing DRIVER nothing, any other a plain object (`0x0041A820`); the object joins the manager's `+0x70`;
- a dummy named `camera*`: a replay camera (`0x00418EA0`, `+0x18`): `CSPLINE_ON` and `CSPLINE_OFF` less 1, the zoom of
  `CAM_ZOOM_CLOSE` (mode 0), `CAM_ZOOM_WIDE` (1) or `CAM_ZOOM_AUTO` (2, also without any), `SPEED_MATCH` (1 without it,
  0 with a speed); its speed -1, the one of `SPEED_MIN` and `SPEED_MAX` that is 0 or more, or with both a value drawn
  between them (`(max - min) x rand() / 32767 + min`); `env_camera*` sets the environment map's place;
- a helper named `Move*`: a zone of the cruise manager (`0x004B01D0`, `[[game+0x80]+0x60]`; `23-showoff.md`);
- a spline not named `centre_spline*`: a riding spline (`0x004236A0`, `+0x08`): `EVEN_SPACING` (its presence),
  `SPEED_MIN`, `SPEED_MAX` (0 without them), the length of its points' segments; `centre_spline*` a centre line
  (`0x00419CC0`, its path as a riding spline's, `0x0041B720`), the first made the race's (`[0x0075ECD0]`).

Then, with properties: the object made reads them (its slot 2, `0x0041A9A0` for every class): for each axis X, Y, Z,
`ROT?_MIN` gives the turn rate x 10, `ROT?_MAX` a rate drawn between that and its own x 10 (one `rand()`); `NOD_ANG` x
200 the nod. `LIGHTFLARE` makes the node's lens flare (`DLH.md`); `PHYSICS` a physics object (below), also for a
node that made no object (a mesh of another time of day too).

## Physics objects (`0x00421960`)

`PHYSICS = <profile>` names `scripts\<profile>.txt` (`07-dynamics.md`). The manager keeps the profiles read by
`<profile>.txt` (`+0x60`): the first node of a profile reads it (`0x004EFF30`; when it cannot be read nothing is made)
and gives the definition its hulls: the object's `+0x34` hull the mesh collider (`+0x88`, `0x00536590`), its `+0x30`
the mesh collidee (`+0x90`, `0x005365D0`), either none without it; a dynamic object's definition also takes as its
centre of gravity the middle of the collider's box (`0x00535BC0`: from FLT_MAX and -FLT_MAX, a vertex below the least
so far is not compared with the greatest; (max + min) x 0.5) x 0.01, in place of the file's. Later nodes of the
profile take the definition as it is, with the first node's hulls.

By its kind (`+0xA8`) the node's body is made from a copy of the definition: a dynamic object (1) a rigid body
(`0x004EEF00`, `0x004F2060`) with the model's origin at the node's place x 0.01, upright whatever the node's turn; a
collision object (0) a collision object (`0x00536040`, `0x00536F90`) with the node: its step finds its touches and
drops them, its node does not follow it; a car's file (2) nothing. With its last argument set (`0x004226CE`; a car
part's is 0) the body joins the world (`0x0053E450`): not being
primary, the second list, woken when the grid is made; the object made for the node keeps it (`+0x58`).

In the game's tracks only Southside Beach's `SSB_DYNAMIC_r1.dlh` and `_r2.dlh` have physics objects: bins
(`dynamic_BIN01`, `dynamic_BIN02`) and barrels, each object at all the nodes placing it (34 in R1); no route's `.tdf`
names these files (the routes load `ssb_dynamic_r01.dlh` ..), so no race has physics objects. Placed in R1's world,
those off the route's collision fall on for ever, never asleep.

## Drawing (`0x00550500`)

A root that joins the world (a riding spline's too, with its riders under it) goes on the world's list of roots (`+0xD8`, `0x00550400`; `0x00550460` takes it off).
The main view (`15-tracks.md`) draws them after the instances drawn after the cars: for each root its squared distance
from the camera is kept with its number; the list is sorted far to near only when the list's `+0x18` is set (the race
track's is 0: the roots go in the order they joined). A root is drawn when its sphere (`+0x64` .. `+0x74`: a model
node's is its mesh's, `0x00555290`, a spline node's around its knots; any other node's is 0 with a radius of 0) taken through the node's matrix is in
the view (`0x00552BC0`, which a radius of 0 never is: a tree under a root that is not a model node is never drawn).
Then, with the race's lights object (the list's `+0x14`) told the distance, the root and its children depth first
(`0x00551480`, a child with `+0x90` bit 0 left out with its children; children are not tested against the view):
each model node (type 0, `+0x8C`) draws (its slot 2, `0x00556900`; `12-rendering.md`) the parts of opaque classes
(mode 1), then the tree again with the others (mode 2); a root with `+0x90` bit 1 once with every part (mode 0).
With lights near (the lights object's `+0x38` or `+0xE0`) the two nearest lights' directions and fades (1 - the
distance in x and z x 0.0005, from 0, x 512) are set as c29 and c31 first (not modelled).

## Lens flares

`LIGHTFLARE = <texture>` (`0x00421D50`) makes a lens flare node (`0x005428D0` → `0x00541030`, 0x200 bytes, node type
7): the source node's own matrix and name; it takes the source node's place: it joins the source's parent
(`0x005348E0`) and the source node leaves it (`0x005347F0`), so it moves with the parent: a car's light node's flare
(the race's cars load their models with the scene objects as the node handler) moves with the car; a flare of `car_flare01` is turned about its x by -0.0872665 (`0x00534A10`).
It keeps (`+0x1CC` ..) `LIGHTFLARE_SIZE` (100), the cosines of `LIGHTFLARE_FALLOFF_MIN` and `_MAX` (2, 8; degrees x
0.017452778, on the loading thread, `+0x1D4`, `+0x1D8`), `LIGHTFLARE_OFFSET` (20, `+0x1E8`); `+0x1DC` is 0 (its
screen-space branch never runs) and `+0x1E0` the stack's leftover; `LIGHTFLARE_BRIGHTNESS` is read and not kept. The
textures by name (`textures\<name>.ddt`): the flare's, `LIGHTFLARE_REFLECTION`'s (`+0xC4`, its batch `+0xB4`) and
the y reflection's (`+0xC0`, `+0xB8`: `LIGHTFLARE_YREFLECTION`, or in the dry `LIGHTFLARE_DRYYREFLECTION` when given),
each with a Light Flare material (`12-rendering.md`, made once per texture) and a batch of 0x800 vertices of 0x20
bytes (`0x005089B0`, one per material, the list `[0x0079D9FC]`). The y reflection's size across and up and its alphas
at the top and the bottom (`+0x1B4` .. `+0x1C0`) are `LIGHTFLARE_YREFLECT_SIZE_X`, `_SIZE_Y`, `_ALPHA_TOP`,
`_ALPHA_BOTTOM`, in the dry each `LIGHTFLARE_DRYYREFLECT_...` key when given; the reflection's size (`+0x1C4`)
`LIGHTFLARE_REFLECTION_SIZE`; a key not given leaves what the stack held. The flare joins the world's `+0x120`
(`0x005500B0`).

The reflections, each of a lit flare with its texture, into the flare's batch: on the cars (`0x00542100`, each face
of a cube, `0x0054FE30`, ZENABLE on): the sun first takes its place from the face's camera; within 5000 of the camera
(the sun at any distance) a square facing it, u the way to the camera made unit, a = up × u made unit (`0x00407C00`),
b = u × a, half its side s = the reflection's size × 0.5 (the sun's × its distance × 0.00002): the corners p − a s,
p − b s, p + b s, p + a s with the texture coordinates (0, 0), (1, 0), (0, 1), (1, 1), no offsets, white. On the road
(`0x00542530`, the main pass before the region's instances, `0x0054FF00`, ZENABLE on): the highest face under the
flare (`0x0053F2A0`, 0 without one) found again when the flare moved across (`+0x1EC`, `+0x1F0` its x and z, at first
FLT_MAX; `+0x1F4` the height); the place as far under that height as the flare is from it; within 20000 of the camera
t = (the cosine between the flare's y turned upside down (x and z negated, made unit) and the way to the camera − cos
min) / (cos max − cos min) within 0 .. 1; four vertices at the place with the texture coordinates (0, 1), (1, 1),
(0, 0), (1, 0), the offsets (∓ size across × 0.5, ∓ size up × 0.5) and the alphas of the bottom, the bottom, the top
and the top × (1 − t) cut toward zero. Then the batches are drawn (`0x00540EE0`).

The main view (`0x0054FC60`, after the scene objects, with the object constants of no object, `0x0054D8D0`): the depth
test off; without occlusion queries (`[0x0075F1A0]`) every flare's visibility (`+0xEC` per view) is 1. With them
(`0x00540BA0`: the `<visibilty test>` Constant Colour material with a position-only declaration, POINTSCALEENABLE 0,
POINTSIZE 32, the depth test on, COLORWRITEENABLE 0, ZWRITEENABLE 0, ALPHABLENDENABLE 0), each flare (`0x00540C70`):

1. a query it waits for (`+0xE4`) is polled (`0x00540A00`; the first ten polls without D3DGETDATA_FLUSH, then with
   it): once ready (an error counts as 0 pixels) the visibility is its pixels / 1024 within 0 .. 1 and the query
   forgotten; while not ready nothing else happens;
2. without a query: from more than 30000 x the renderer's `+0x188` away, or with the camera's z (its world column
   2) pointing away from the flare (the offset camera - flare along it above 0), the visibility is 0; else a query
   (`0x005409A0`, the next of a ring of 256) is begun, one point drawn at the flare less its y column x the offset,
   the query ended (`0x005409D0`).

Then ZWRITEENABLE on, COLORWRITEENABLE 0xF. Each flare's quad (`0x00541C10`): d the camera's distance; t = (the
cosine between the flare's -y and the way to the camera - cos min) / (cos max - cos min), below 0 taken as 0, above
1 no quad; the fade (1 - (d - 20000 f) / (10000 f)) x the visibility within 0 .. 1 (f the renderer's `+0x188`); with
D = the camera's sine of half its field of view (`[11]`) x d, the half size D / (D + 2000) x the size x (1 - t) x 0.5
over D x 0.002 held within 0.2 .. 1. A fade over 1/255 whose sphere (the half size) is in the view adds four vertices
to its batch: the flare's place, the corners' texture coordinates (0, 0), (1, 0), (0, 1), (1, 1) and offsets (-s,
-s), (s, -s), (-s, s), (s, s), the colour white with an alpha of (1 - t) x the fade x 255 cut toward zero. Then the
depth test on, and each batch with vertices (`0x00540EE0`): its material's first pass, the depth test on only
without the queries, the quads as triangles (each 0, 1, 2, 3, 2, 1 of the shared index buffer, `0x0051DC10`).

The race's sun (`0x00425297`, as the race track loads; its node the track's `+0x28`): with the atmosphere's SunFlare
(`+0xEC`) found as a texture, a flare of size SunFlareSize (`+0x150`) x 2000, the fall-off values 9 and 10 kept as
they are (`+0x1D4`, `+0x1D8`: no angle ever gets within them, t is 0), an offset of 0, `+0x1DC` = 200000, `+0x1E0` the
SunAngle (`+0x15C`), `+0x1E4` SunSize x 2000; SunShape (`+0xCC`) gives a `<light object>` material (`+0xBC`) and
SunReflection (`+0x10C`) the reflection's (its size 200000 x SunReflectionSize x 0.01, else 300), the y reflection
none with its sizes 100 and 200 and its alphas 128 and 64; the light object is not drawn, the reflection is drawn on
the cars.
It joins the world's flares after the scene objects'. Its node starts at the world's origin, its visibility 0. Each
frame, before the visibility tests, the flares' draw (`0x0054FC60`) puts every flare with `+0x1DC` (`0x00540A70`) at the
camera's place + the direction × −`+0x1DC`: the renderer's light direction (`+0x40`), or with a SunAngle a, (0, −sin a,
cos a) (`0x00407A90`) turned about y by atan2 of the light's x and z (`0x00407D30`); the visibility test then skips the
distance and facing checks (a query every time one is not waited for), and the quad (once the visibility is not 0) puts
it there again; the fade is 1 and the half size (1 − t) × the size × 0.5.

An interface view's sun (`0x005B4660`, before its sky dome): with its world's atmosphere's SunFlare found as a texture
(`0x005A8D30`: the interface's `textures` folder) the same record as the race's sun (`0x00420880` then the values
above), but the light object (with a SunShape) and the reflection on the cars (with a SunReflection, its size
200000 x SunReflectionSize x 0.01) both take the SunFlare's texture (Stock problem; neither is drawn in a view: the
flares' reflections are the race world's, `0x0054FE30` and `0x0054FF00` from `0x00547690` only). The flare
(`0x005428D0`) joins the view world's flares (its `+0x10`, `0x0054C860`: `+0x120`), one more each time the sky is
built; the world draws them after its nodes and the object constants of no object (`0x0054C5C0`: `0x0054D8D0`,
`0x0054FC60`); its node, visibility and quad are the race sun's. Atmospheres with a SunFlare: `map` (sun_flare05,
SunAngle 3), `outsidegarage`, `respect`.

A mesh object's parts are those of a model mesh (`0x00558B20`: the part's material word through the route's material
table, as a layer mesh's, `15-tracks.md`). The four values of a mesh node (`0x005550D0`) are its c40.

## A root through the manager (`0x00422750`)

- a dummy named `env_camera*` is not kept;
- a helper: `atmos*` an atmosphere zone (`0x00418B50`, the manager's `+0x28`; `13-atmosphere.md`): its place and
  radius its node's slot 1 as made (a sphere's `0x005543E0`: its centre in the world and its radius; a box's
  `0x00554620`: the box's middle in the world and half its diagonal), its name the helper's from the seventh character
  (at most 16) cut at the first digit after the first character (`+0x18`); `tunnel*` the manager's tunnels (`+0x38`: no
  sun in them, `0x00422D20`), `reverb*` a sound zone; any other helper is not kept;
- a riding spline: each child rides it (`0x00423840` → `0x00423A80`): its speed the one of `SPEED_MIN` and `SPEED_MAX`
  that is 0 or more when the other is not, a value drawn between them when both are (with neither given both are 0:
  one value is drawn and the speed is 0), else 1500; `cablecar*` does not bank; a camera dummy among the children
  takes the rider; each is placed at 0 (below). Then they start (`0x004238E0`): with `EVEN_SPACING` spaced along the
  path (below), else a single rider at its start and each of several at `rand() % points` (the path's points).

## Splines and their riders

A spline node (`0x00540580`, `0x0054F030`) keeps a segment (`0x00540630`, 0x40 bytes) from each knot to the next: the
knot, its out handle, the next knot's in handle, the next knot; a closed spline (the object's `+0x38` bit 0; the
node's `+0xB4` = 0, else 1) one more from the last knot back to the first. Its sphere (`0x00540320`) starts at the
first knot with a radius of 0 and grows to take in each segment's two knots in turn: a point beyond the radius by e
moves the centre by (point - centre) x e / its distance x 0.5 and the radius by e x 0.5. A point of the curve at a
place (the segment and its fraction, `0x0053FF50`): the place cut down (`floor`) gives the segment, on a closed
spline taken modulo their number (below 0: 0), on an open one held within the first (fraction 0) and the last
(fraction 1); the cubic Bezier of the segment's four points.

A riding spline's path (`0x0041B720`, `+0x08`): a point at each whole place (as many as the segments, one more when
open), the curve's point moved by the node's world place (its turn left out); each point the unit direction from the
point before (the first from the last) and that point's length to it (so the last one's back to the first, also on
an open spline), and the length of the path up to it. The spline's length (`+0x38`) sums the points' lengths.

A rider at a place (`0x00423B80`): the point it heads for is the place's segment + 1 (0 at the path's count); its
yaw atan2(dx, dz) and pitch atan2(dy, √(dx² + dz²)) from the curve's point there to the one 0.1 further; its node's
matrix (in the spline's space) the identity turned about y by π - yaw (`0x00534960`, the angle held within ±π by
whole turns, `0x00407910`), then about x by the pitch (`0x00534A10`), at the curve's point. Spaced evenly, each rider
in turn is placed at the whole point reached plus a fraction: from 0, the points' lengths are summed until they reach
the length / (riders + 1), the excess divided by the last length summed is the next rider's fraction, and the walk
goes on from there.

## Each frame (`0x00422BB0`)

After the cars (`0x00425D40`) the manager steps with the physics' step time; for its first 50 frames (`+0x5C`) with
none. Each riding spline's riders move (`0x00423D00`); then, on the frames the game's parity lets the objects step
(`19-damage.md`; the game's `+0x94` 0 or `+0x98` set), each object's step (slot 4) with the same time: a plain object
(`0x0041B050`) with turning axes turns each by dt x its rate (the angle held within ±π, `+0x60` .. `+0x68`; with
`NOD_ANG` the angle used is sin(angle) x the nod x 0.02, held within ±π); the turn is the identity times the turn
about x, then about y, then about z (`0x0041B680`, `0x0041B4E0`, `0x0041B4A0`), and the node's own matrix becomes the
one it was made with (`+0x7C`) times the turn. The barrels' (`0x00418D70`), bins' (`0x0042A060`) and wheels' steps
are their motion and light (below; with the sound on a barrel's or bin's hit plays its trashcan); a barrel's then,
past 300 across the ground (`[0x00725280]`), puts the effects under its node (the children of the effect kind,
`+0x8C` 3) at 0 (vt+0x0C with 0 and 1) and, the first time (`+0xCC`), tells the cruise (`[G+0x80]+0x60`) of a crash
of 2000 by no car (`0x004B0280`; each zone then asked whether it holds the barrel, the answers not kept); the
crowd's (`0x0041DC00`) is below. Stepping on every other frame with that frame's time, the objects turn at half
their rates (`stock-problems.md`).

A rider that still moves (`+0x3C`): from its node's world place, the progress from the path's point before the one
it heads for (its distance from that point over the sum of its distances from both) is kept when larger, within 0 ..
1; past the point it heads for (its offset from it along that point's direction above 0) it heads for the next one
with the progress 0: past the last of a closed path the first, of an open one it stays at the last and stops. Then
the look-ahead (0.6 x the speed, within an eighth of the length from the point before and 50000) is walked along the
path from where it is, giving a place of the curve (the target, `+0x48`). Over 100 from its node's place the rider
turns towards it: the pitch and the yaw each by 3 x the angle left (held within ±π) x dt x 0.1 x 60, at most dt x
0.1 x 60 either way, the roll back to 0; the node is set anew (`0x00534D80`) turned about y by π - yaw and, when it
banks, about x by -pitch. It moves by dt x its speed (on an open path within 2000 of its end that x the distance left
x 0.0005) along cos(pitch) sin(yaw), sin(pitch), cos(pitch) cos(yaw).

## The light of the sun

A step of a barrel, a bin, a wheel (`0x0041B270`) or a car's chassis or part (`0x00419780`, then `0x0041B270`; "A car's
parts" below) measures its node: the node's place in the world against the last step's over the step's time (the
velocity, `+0x28`; none with no time), the length of the velocity before it (`+0x24`), the change of the velocity
(`+0x38`) and its length across the ground (`+0x20`; over 200 the node is passed to its mesh, `+0xC0` | 8); then its
light (slot 1, `+0x48`); then the light goes to the node (`+0xC4`, the scale of the sun on its mesh) unless the
atmosphere was night when it started (`+0x4C`, `0x0041ABF0`); last the turn of an object with axes (`0x0041B050`), given
1 / the step's time (Stock problem: `0x0041B2B4` writes it over the argument). The light is taken from the route's sun
map (`15-tracks.md`, "The ground maps"):

- a wheel, a barrel or a bin (`0x0041AF00`, starting at 1): once the node is over 100 from where it was last sampled
  (`+0xB8`), a sample at its place (`+0xC8`); the light eases toward the sample by at most 0.00004 x the speed (at
  least 500) a step, within 0 .. 1;
- a car's chassis (`0x00419980`, starting at -1): once over 100 from the last sample, the mean of four samples 140
  either way along its node's own x and z axes (its matrix under the car's, so the world's axes whatever the car's
  heading), none in a tunnel; the first sample is the light, later ones are eased to as above;
- a car's part (`0x004191B0`): the chassis's light.

A plain object's light (`0x0041AED0`, a sample at its place, 0 without a map) and a crowd member's are taken when they
start and never reach their nodes. The steps of a car are made with its model (`0x0048A644`, `0x00420FF0`): the chassis
(`_CHASS`, the model's `+0x90`), the four wheels and the parts of the table `0x00717538` that are model nodes (FBUMP,
RBUMP, BONNET, SPOILER, SSKIRT_L, SSKIRT_R, EXHAUST_C, EXHAUST_S, EXHAUST_O, DECAL; DRIVER makes none), each started at
once. A step with `+0xB0` set lays a drop shadow (`gen_shadow01`, its mesh's box x 1.4 across, `0x0041ACF0`) under its
node; no step sets it.

## A car's parts (`0x004191C0`)

The chassis and the parts are objects of `0x004191C0` (0xF8 bytes; the parts' vtable `0x006EEDE8`, the chassis's
`0x006EF0F0`, which differ only in the light, slot 1). They are made by `0x0048A500` after the car's damage grid
(`19-damage.md`), so once the race's load is done (`0x004255F0`, the main thread), and made again when the car is
reset (`0x004894C0`: `0x0048A700` takes every object of the model's nodes off the manager's list first, `0x00420F30`).
Outside a race (no manager, `[0x0075ECDC]` 0) `0x00420FF0` makes nothing.

The constructor keeps the node's parent (`+0xD4`, the node's `+0x80`), sets the light to -1, attached (`+0xD8`) to 1
and clears the node's `+0xC0` bit 0x10. By the node's name (`+0x7C`, compared without case) it sets the axes the part
wobbles about (`+0x5C`), the softness (`+0xE0`, 1.2), the rate (`+0xE8`, 4), the amount (`+0xEC`, `0x399D4952`) and the
part's bit of the model's `+0x170` (`+0xF4`):

| name | axes | other | bit |
|---|---|---|---|
| `fbump` | x, y, z (7) | | 1 |
| `rbump` | x, y, z (7) | amount `0x3951B717` | 2 |
| `bonnet` | x (1) | | 4 |
| `spoiler` | x (1) | softness 0.5 | 8 |
| starting `sskirt` | x, z (5) | | 0x10 |
| starting `exhaust` | x, y (3) | softness 0.5, files named `exhaust` | 0 |

Stock problem: the skirts are told apart by `_strnicmp` with `sskirt_l` and `sskirt_r` over 6 characters, so the right
skirt takes the left's bit (0x20 is never given). Any other name (the chassis, DECAL) has no axes.

A part with axes reads its hulls through the meshes' folder (`[G+0x1C]+8`): `<name>_collider.ocd` and
`<name>_collidee.ocd` (`0x00535830`, either none when it is not there; the game has them for fbump, rbump, bonnet,
spoiler, sskirt_l, sskirt_r and exhaust, and for wmir_l and wmir_r, which no part reads), then makes the physics
object of the profile `dynamic_<name>` (`0x00421960`, "Physics objects" above) with the two hulls and the last argument
0: the body (a rigid body made at the node's place in the world x 0.01) is kept by the part (`+0x58`) and never joins
the world. There is no `dynamic_exhaust.txt`, so the exhausts have no body. Nothing in the PC game reads a part's body;
it goes with the part (`0x0041ADA0`).

The model then gives each part (`0x0048A500`) its zone of the damage grid (slot 6, `0x004191A0`, `+0xCC`: the model's
`+0x5C + 4 i` for the table's part i, so FBUMP, RBUMP, BONNET, SSKIRT_L, SSKIRT_R and DECAL have one, the spoiler and
the exhausts none), the chassis's object (`+0xD0`, its light) and the model (`+0xF0`), and starts it. The chassis gets
neither (its `+0xCC` and `+0xF0` stay 0).

Each step (`0x00419780`), before the light step: while not attached the node is shown again (`+0xC0` & ~4); else the
damage's level is the zone's dents (`+0x10`) x 0.05 (`[0x006FF994]`), 0 without a zone, and with axes the part wobbles,
with the last step's values (the main thread rounds each operation to a float):

    phase = wrap(dt x rate / (level / softness + 0.0125) + phase)           (within -pi .. pi, 0x00407910)
    turn = -(|sin(phase) + sin(phase x 0.3)| x (1 / softness) x level x dt) x amount x speed      (+0x24)

every axis's angle (`+0x60` .. `+0x68`) is the turn; with a change across the ground (`+0x20`) over 20
(`[0x00725298]`) the change (`+0x38`) x 0.001 (`[0x00725294]`) x (1 / softness) x level (`0x00407C90` twice) adds its y
to the turn about x, its z about y and its x about z; the angles about the part's axes are held within ±100
(`[0x00725290]`) x amount. The light step then turns the node (`0x0041B050`, rates 0): its own matrix (`+0x7C`, kept when
it started) times the turns about x, y and z. An undamaged part does not move; a dented bumper, bonnet or skirt rocks
by up to 0.03 rad (the rear bumper 0.02) with the car's speed and its jolts, as long as the race lasts.

When the object goes (`0x004196D0`) the node takes its own matrix back; one not attached would be put back
(`0x00419730`: under its parent again, `0x005348E0`, attached, the node's bit 0x10 and the model's bit of the part
cleared).

Nothing in the PC game takes a part off: no code clears `+0xD8` or sets a node's bit 0x10, the model's `+0x170`
(cleared by its constructor `0x0048A0A0`) is only copied (a replay's frames, `0x00428010` / `0x00428180`; the network's
byte each 5 s, `0x004EB130` / `0x004E95E0`) and read by nothing but `0x00419730`, and the parts' bodies never join the
world. A mesh's draw (`0x005594C6`) would make a Car Body material (class 0) of a node with bit 0x10 double-sided
(its parameter 8 set to 1).

## The end of the race track (`0x00422AC0`)

The race track's making (`0x00424270`) ends, on the loading thread, with each object's slot 3 in the order they were
made. The first time (`0x0041ABF0`, `+0xAC`) the object keeps its node's matrix as its own (`+0x7C`) and its place
(`+0x10`), and its light from the ground's map (slot 1, `0x0041AED0`); with the sound on, one with a sound makes it.
A crowd member (`0x0041D9D0`) then, when the race has a centre line (`[0x0075ECD0]`, the first `centre_spline*` made,
`0x00419CC0`), faces the line: from the line's point nearest to it (`0x0041BAA0`) and the nearer of the points either
side, the heading -atan2(dz, dx) x 8/π (16 steps a turn, `+0xB0`) plus (`rand()` / 32767 - 0.5) x 6, within 0 .. 16.
Each step (`0x0041DC00`) a crowd member counts down; at 0 (again 20 steps) it looks for the nearest car: the cars'
trackers on the centre line (`0x00419E10`, one for each car as it is made, its chassis node; none without a centre line)
in the order made, the first of the nearest by (dx² + dz²) (`0x00419EE0`); -atan2(dz, dx) x 8/π of its offset from
the car less its heading, within ±8 by whole turns, x 0.4 within ±2, is the facing it turns to (`+0xB8`); the facing
(`+0xB4`) turns by at most its turn per step. Nothing found reads the facing. Then the zones and the cruise's helpers
are made ready.

## After the race track (`0x00461390`)

Once the race track is made (the cars set on the grid), the cruise manager gives each child of each zone's `Move*`
helper a crowd sound (`0x004B0330` → `0x0041A430`), each drawing one value.
