# Damage

A car keeps its damage in a grid around its body (the car's `+0xB4`, `0x41C` bytes, vtable `0x006FA1D8`, made by
`0x005562A0`). The grid has 16 x 16 cells, each made when first needed (`0x005554A0`, `0x54` bytes, vtable
`0x006FA1CC`). The body parts of the car's model are zones of the grid (`0x00557260`, `0x18` bytes): each adds up
the damage of the cells its vertices lie in.

The grid is the car record's own object: the race car's model adds its nodes to it (`0x0048A500`) and the race's
dents stay in it after the race, with the cells the race made; writing the car (`0x00557B80`) then adds the race's
damage to the kept damage.

## The cell of a point (`0x00557180`)

For a point of the car (centimetres from the car's centre, x y z):

    across = atan2(x, y)                      (fpatan, stored as a float)
    i = (atan2(x, z) + pi) x 8 / pi           (the sum and the product rounded as the thread rounds, then a float)
    j = (across + pi) x 16 / pi               (likewise)

both cut toward zero (x87 with the chop mode, or `cvtss2si` under the game's MXCSR, which rounds toward zero).
Then: i below 0 is 0; i above 15 is 15 and j loses 16; i from 8 to 15 also loses 16 from j; finally j is held within
0 .. 15. pi is `0x40490FDB`, 8 / pi `0x4022F983`, 16 / pi `0x40A2F983`.

## A cell

| Offset | Meaning |
|---|---|
| `+0x04` | the zones, one entry for every vertex of a zone that lies in the cell |
| `+0x14` | the neighbours |
| `+0x24` | the damage kept with the car |
| `+0x28` | the damage of this race |
| `+0x2C` | the damage kept when the car was made (put back by `0x00557070`) |
| `+0x30` | the vertices in the cell |
| `+0x34`, `+0x44` | the vertices themselves (node, part, index), for a node added with the flag set or not |

A zone holds its node (`+0x08`), its damage (`+0x0C`), its dents' amounts (`+0x10`) and the number of its entries in
the cells (`+0x14`).

## Building the grid (`0x0048A500`)

After the race's load, on the main thread (`0x004255F0`), every car of the race builds its grid from its model:
when the car has a grid, the model's nodes are added (`+0x08`, `0x00555E30`) in this order, with the flag:

1. the chassis (the model's `+0x0C`, the first node whose name contains `_CHASS`), flag 1; its zone is not kept;
2. the parts of the model's table (`+0x30` ..): FBUMP, RBUMP, BONNET, SSKIRT_L, SSKIRT_R, flag 1, their zones kept in
   `+0x5C`, `+0x60`, `+0x64`, `+0x6C`, `+0x70`; the spoiler and the exhausts are not added;
3. DECAL (`+0x54`), flag 0, zone `+0x80`;
4. the first node whose name contains `SPONS`, flag 0, zone `+0x84`.

The nodes are found by `0x00534EC0`: depth first through the children of the model's top node, the first whose name
contains the text (`strstr`). A part is kept by `0x0048A0A0` only when its node is not hidden (`+0x90`).

Adding a node: no node, no zone. Otherwise a zone is made for it and, when the node has a mesh (`+0xA8`), every vertex
of every part of the mesh (the part's vertex buffer from its start, its stride `+0x64`, its count `+0x68`) is placed:
its position (the first three floats) plus the node's offset (`+0x14`, `+0x24`, `+0x34`) gives its cell, made when
there is none; the cell counts the vertex, keeps it in one of its lists, and gets one entry of the zone.

Then the grid is finished (`+0x10`, `0x005574B0`):

1. every cell's neighbours, for the cells that exist, in this order: (i - 1, j), (i + 1, j); when j is above 0,
   (i, j - 1), (i - 1, j - 1), (i + 1, j - 1); when j is below 15, (i, j + 1), (i - 1, j + 1), (i + 1, j + 1). i wraps
   around by 16, j does not;
2. every zone counts its entries over all cells;
3. every cell with kept damage (`+0x24` not 0) is dented again: its kept damage set aside, three `rand()` give a
   direction ((r / 32767 - 0.5) each, the second draw its x, the first its y, the third its z, made unit by
   1 / (length + 1.1920929e-7); the direction only moves the vertices), the cell is dented by half the kept damage
   with a fall-off of 0.5, and the kept damage is put back;
4. every cell's race damage (`+0x28`) is 0;
5. every zone's dents (`+0x10`) are multiplied by 0.2.

The same build runs for the car shown in the garage screens (`0x00435AE0` carsel, `0x00437BB0` carbuy and its mirror
image, `0x004894C0`), on the loading thread: its `rand()` draws are that thread's. OpenJuice makes the carsel car's
model again when its parts change (the stock's `0x0048B440` changes them in the model the grid has), undented.

## A dent (`0x005556C0`)

A dent of a cell along a direction by an amount with a fall-off:

1. when the fall-off is above 0, each neighbour is dented along the same direction by amount x fall-off with no
   fall-off (`0x00556DE0`);
2. every zone entry of the cell: the zone's dents += amount / the zone's entries;
3. when the race damage plus the kept damage is 20 or more, nothing more happens; otherwise the amount is cut to what
   brings the sum to 20, and the race damage grows by it;
4. every zone entry of the cell: the zone's damage += the cell's race damage / the zone's entries (the cell's whole
   race damage, each time);
5. the cell's vertices move, each part's vertex buffer locked (`+0x2C`) while its entries follow and unlocked
   (`+0x30`) after them; a vertex is a position (3 floats), a normal (3 floats) and an ARGB colour:
   - the vertices of the flag 1 (`+0x34`): when the colour's alpha is below a = (race + kept damage) x 15 rounded to
     nearest (at most 255), a is or'ed into the alpha's byte; with a blue (the colour's low byte) above 0 the vertex
     moves by the weight blue x (1 / 255) x the amount (the amount cut in step 3);
   - the vertices of the flag 0 (`+0x44`): all move by the weight of the amount, the colour as it is;
   - a move: the game's lagged generator (`0x0075B2D8`, the particles' own, `0x00509B70`) seeded by (x + y + z) x 0.1
     rounded to nearest; the normal's x and y each += (r - 0.5) x 0.05, the normal times 1 / (length +
     1.1920929e-7); the position's x, y and z each += r x the direction's x, y, z x the weight (r of 0 .. 1, a draw
     each). The generator is left where the move leaves it.

The car body's shader (`Material_CarBody_Paint_NoReflect`) shows the scratch texture's alpha times the vertex's
alpha, and the car glass's likewise: the scratches and the cracks show where the car took damage.

## The car's motion (`0x004924A0`)

The car's effects object (the vehicle's `+0x64`, made by `0x0048FA40`, set up by `0x0048FEA0` as the race is set up,
before the cars settle on the grid) starts with the car's origin (`0x00407B20`: the model's origin x 100, w as it is)
and a velocity of 0. Each frame of the car, at the end of its wheels' frame:

    v = (origin - last origin) x (1 / dt)        (the move itself when dt is 0)
    delta = v - last v;   change = √((dx² + dy²) + dz²)

then the last velocity, the velocity (w the origin's) and the origin are kept. A change above 750 tells a cruise's
stunt object (`0x004AB7C0`, only while one runs). The tyres' effects (`0x004907D0`) and the exhaust's flames
(`0x004912D0`) follow, then the impact, only when the game object's `+0x94` is 0 (it is 1 from the start) or its
`+0x98` is set: `+0x98` is set while the race's load runs and flipped at the end of every frame
(at the end of the game's frame `0x0040CCA0`), so the impacts run every other frame, the race's first frame being one.

## The impact (`0x004916F0`)

1. The body's rigid contacts of the last physics step: those pressed in (`+0x64` above 0) give the mean of their
   arms (x 100); every contact's point lowers the car's lowest touch (`+0x16C`). None pressed in: the side
   (`+0x170`) and the impact (`+0x174`) are 0, nothing more.
2. The side: 2 when the mean arm's z is 0 or less, else 1 (and the competitor's `+0x14C` is set).
3. The energy: 2 x the change, at most 100000; when the last contact of the step (pressed in or not) is against
   another body, the car's `+0x17C` is set and the energy is 4 times as much. Below 50: the side and the impact are 0,
   nothing more.
4. The sparks and the scrape's sound follow the energy (`+0xD0` .. `+0xE0`).
5. Upside down (the orientation's matrix `[5]` below 0) the energy is 3 times as much. At 400 or less, or with the
   competitor finished, or with its race time (`+0xF0`) at 0.25 s or less (not in a replay's race), nothing more.
6. The other car (`0x00498100`: the race's car whose node is the other body's), none for another body.
7. One rand() picks a contact: `r % n`, n being the step's contacts counted at the start. Its arm x 100 is the point
   (its direction, `0x00407C00`, goes to `+0x46C`), its normal in the car's frame (`+0x20`) the normal.
8. The amount: (energy - 400) x 0.003 plus the normal's z + 1 held to 1 at least (above 2 the bound is replaced by
   (energy - 400) x 0.003: a stock problem); an AI car's 1.5 times as much (in a replay's race 2 times, or 1 while
   it runs fast).
9. The career counts the hit (`0x004BAD40`) for this car against the other, then, when there is another car, for
   the other against this one; see below.
10. Pieces: n = (amount x 0.02) cut to a whole number; 0 gives one piece of the whole amount, else the first piece is
    the amount less n x 50 and the others 50. For each piece three rand() move the point by (r / 32767 - 0.5) x 50 in
    x, y and z; the car's grid takes a hit there (`+0x14`) along the normal, the other car's grid one at minus the
    point along minus the normal.
11. The impact (`+0x174`) is 50 (the last piece's); in a race of type 1 (a time trial) the challenge's outcome
    (`+0x7C`) is 3 and the competitor finishes.

The hit counted (`0x004BAD40`), for a car of the player's crew (`+0x47C` 0) in a career race: against no other car
its racer's `+0x38` is 2 (amount above 0.5) or 1; against a car at its place or ahead (the other's `+0x104` not
above) whose forward speed (`+0x1A0`) is above 5 m/s, the other's crew's `+0x330` grows by 10 or 2 and the racer's
`+0x34` is 2 or 1.

## What the damage does (`0x00488B50`)

Each frame of a car, before its wheels, by its zones' damage x 0.05 (FBUMP, RBUMP, BONNET; a zone not made counts as
no damage):

- the headlights go out (`+0x450`) over 0.8 of the front bumper; the rear lights (`+0x44C`) over 0.5 of the rear; the
  rear's other part (`+0x454`: a node hidden, two lights out) over the model's threshold (`+0x460`: 0.1 for the
  models 5, 7, 11, 19 and 35, else 0.5, `0x00486431`);
- the steering: once the front and the rear bumpers and the bonnet (when it has a zone) are all past 0.99, the car's
  `+0x64` is set and a side drawn (`rand() & 1`, `+0x4F8`); from the next frame on the steering moves by 0.05 to that
  side (0 to the left) every frame, held within ±1 (`0x004203E0`);
- the turbo (a turbo part, or the model's own): with the front bumper and the bonnet past 0.99 and the rear past 0.5
  it is lost: the car's `+0x68` is set and the dynamics' turbo factor (`+0x164`) is 1;
- the nitro: with the front bumper past 0.99 and nitro left, the car's `+0x70` is set; from then on a player's nitro
  loses 0.5 % of what is left each frame;
- the neon (the car's part of category 9): past 0.5 of the front bumper the car's `+0x6C` is set (the neon flickers:
  `0x004890B0` draws a rand() each frame); once set, past 0.99 the part is gone (`0xFF`).

The car's flags are the career car's own (`+0x64` .. `+0x70`) and stay until it is repaired.

## Hits and the saved grid

- `+0x14` (`0x00557A30`): a hit at a point dents the point's cell, when there is one, along the direction given (the
  impact's contact normal in the car's frame, minus it for the other car) by the amount with a fall-off of 0.4.
- `+0x18` (`0x00557AA0`): damage to keep, added to the point's cell (made when there is none), at most 20.
- `+0x1C` (`0x00557B00`): the grid read from a car's record: 256 bytes, i outer and j inner; a byte b gives
  b x (1 / 255) x 20, kept in the cell (made for it) when above 0.
- `+0x20` (`0x00557B80`): the grid written: each cell's race damage added to its kept damage and cleared, the byte the
  kept damage x 12.75 rounded to the nearest (no cell, 0).
- `0x00557C00` repairs: every cell's damages 0. `0x00557C90` clears the race damage.

## What a race shows of the damage

All of it comes from the hits above (`0x004916F0`: a car's contacts with energy over 400, every other frame) and from
the damage the car keeps, dented again as the grid is built:

1. The dents: the cells' vertices move along the hit's normal (`0x005556C0`), the body's by their colour's blue, the
   decals' fully; the change shows at once, the meshes' buffers being written in place.
2. The scratches and the cracks: the body's vertices take an alpha of the cell's damage x 15; the Car Body material
   shows the scratch texture's alpha times it, the Car Glass material likewise on the glass of the chassis's mesh.
3. The parts rock (`21-scene-objects.md`, "A car's parts"): a part whose zone took dents (FBUMP, RBUMP, BONNET,
   SSKIRT_L, SSKIRT_R) turns about its own axes by up to 0.03 rad with the car's speed and jolts; the spoiler, the
   exhausts and the decal have no zone and stay still.
4. The lights: past their levels of the bumpers' damage the headlights go out, the rear lights go out and the rear
   part's node (the reversing lights) is hidden (`0x00488B50`, above); the neon flickers and is lost.

No part comes off in the PC game: the parts' objects never leave their nodes (their `+0xD8` stays 1), the model's
byte of parts knocked off (`+0x170`) is only copied by the replays and the network, and the parts' bodies
(`dynamic_<part>.txt`) never join the world.
