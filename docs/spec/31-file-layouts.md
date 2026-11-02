# File layouts

The byte layouts of the game's binary files as OpenJuice reads them (the readers named in each section; each is
checked against every file of the game's data by the tests). All values are little-endian; `u16`, `u32`, `i32` and
`f32` are 2- and 4-byte integers and floats; `char[n]` is a name of `n` bytes padded with zeros. Game space is
left-handed: X right, Y up, Z forward, 1 unit = 1 cm. A reader takes a file only when it reads exactly to its end,
except where a section says otherwise. What the game does with the data is in the files named in each section; the
models' files (`.boh`, a model's `.d8m`, `.pmd`) are in `10-models.md`, the atmospheres' text (`.atm`) in
`13-atmosphere.md`.

## The track's routes and layers (`.tdf`)

`tracks\<Track>\<track>.tdf` (`Core/Tracks/Track.h`, `parseTdf`; `15-tracks.md`):

```
char[32] track
u32      routeCount, then per route:
         char[32] name                    "<Route>.wld"
         u32      entryCount, then per entry:
                  u32      hidden         the race conditions the entry is not loaded / drawn in
                  u32      reflected      the conditions a geometry layer is drawn into the wet road's reflection in
                  char[32] name           "<layer>.lyr" (geometry, <track>_<layer>.d8g) or "<name>.dlh" (scene objects)
```

A condition is a bit: 1 day and dry, 2 day and wet, 4 night and dry, 8 night and wet.

## The geometry layers (`.d8g`)

`tracks\<Track>\<track>_<layer>.d8g` (`parseD8g`). An empty file is a layer of no geometry.

```
u32 groupCount (at most 16), then per group:
    u32 primitive                         1 triangle list, 2 triangle strip (0xFFFF restarts it), 3 line strip
    u32 meshCount, then per mesh:
        u32 materialId                    through the route's .d8m table
        f32 centre[4]                     the bounding sphere's centre (w 1)
        f32 radius
        f32 range[6]                      (near², far²) of the main view, of the cars' environment, of the reflection
        u32 elementCount, D3DVERTEXELEMENT9[elementCount]   8 bytes each, the last the end record (stream 0xFF)
        u32 stride, u32 vertexCount, vertex bytes[stride * vertexCount]
        u32 indexCount, u16 indices[indexCount]             not there for a line strip (primitive 3)
        u32 nodeCount, f32 matrix[16] per node              the instances: row-major, the position in 3, 7, 11
```

A mesh without nodes is one instance. A route's instances are the (mesh, node) pairs of its `.lyr` layers in the
`.tdf` order (layer, group, mesh, node).

A `D3DVERTEXELEMENT9` is `u16 stream, u16 offset, u8 type, u8 method, u8 usage, u8 usageIndex`.

## A route's materials (`.d8m` of a route)

`tracks\<Track>\<track>_<route>.d8m` (`parseRouteMaterials`; a model's `.d8m` is another layout, `10-models.md`):

```
u32 materialCount, then per material:
    u32 classId                           the material class (11-effects.md)
    u32 paramCount, then per parameter:
        u32 size, bytes[size]             4 bytes, or 256 for a string
    u32 textureCount, u32 textures[textureCount]   indices into the route's .d8w texture list
u32 idCount, i32 remap[idCount]           a geometry material id -> its material, -1 not used by the route
```

## Texture directories and pixel files (`.d8w`, `.shd`; `.d8t`, `.txb`)

A texture directory (the head of a route's `.d8w`, a whole `<track>_shad_<time>.shd`; `readTextureDirectory`) lists
where each texture's pixels are in its pixel file (`<track>.d8t` for the routes, `<track>_shadowMaps.txb` for the four
`.shd`), which holds only pixels:

```
u32 textureCount                          of all the tables
u32 tableCount
u32 tableBytes                            the sum of the tables' sizes
per table:
    u32 skip                              the cursor moves on by it before the table
    u32 size                              the table's bytes in the pixel file
    u32 count, then per texture:
        u32 dataSize                      its pixels' bytes
        u8  header[44]                    a .ddt header (below)
```

The offsets count from the start of the pixel file: the cursor starts at 0; per table it moves on by `skip`, the
table's textures follow one another from there by their `dataSize`, and the cursor is then the table's start plus its
`size`. A texture as a `.ddt` is its header followed by its `dataSize` bytes. The four `.shd` of a track share one
`.txb` and each counts from its start: their big tables come first one after another, their small tables lie between
one another further on (the `skip`s pass over the other files' tables).

A `.shd` ends with its directory. A route's `.d8w` goes on (`parseD8w`):

```
texture directory
u32 setCount, then per set:
    char[32] name                         shad_morn, shad_noon, shad_eve, shad_night
    i32      tiles[instanceCount]         per instance of the route: its baked-light texture in that time's .shd, -1 none
f32 area[4]                               x0, z0, x1, z1 of the render tree's root
u32 treeSize, bytes[treeSize]             the render tree
u32 0, u32 0
```

### The render tree

The tree is the memory the game walks (`0x00548790`, `0x00548AC0`; `parseRenderTree`), depth first, each inner node
followed by its four children:

```
inner node (0x5C bytes):
    u32 flags                             bit 0 clear
    u32 children[4]                       places the loader fills
    f32 splitX, f32 splitZ                at +0x14
    f32 low[4], f32 high[4]               the box's corners (x y z w), at +0x1C and +0x2C
    u32 regions[8]                        the 256 regions it is seen from, at +0x3C
    then its children: (low x, low z), (high x, low z), (low x, high z), (high x, high z)
leaf (0x4C bytes):
    u32 flags                             bit 0 set
    u32 recordCount
    u32 a place for the records' start
    f32 low[4], f32 high[4]               at +0x0C and +0x1C
    u32 regions[8]                        at +0x2C
    then per record (0xCC bytes): u32 the instance's number in the .ocl order, 0xC8 bytes filled at the load
```

Each instance of the route is in one leaf; an empty leaf's box runs from FLT_MAX to −FLT_MAX.

## A route's regions (`.ocl`)

`tracks\<Track>\<track>_<route>.ocl` (`parseOcl`; `15-tracks.md`):

```
u32 size[3]                               cells along x, y, z (75, 1 .. 4, 60 in the game's files)
f32 min[4], f32 max[4]                    the box's low and high corners
u8  grid[size x * size y * size z]        the region of each cell, x fastest, then y, then z
u32 count, u32 regions[8] per record      per instance of the route (the .tdf order, as the light sets): the
                                          256 regions it is drawn from
```

## The racing lines and the grid (`.spl`)

`tracks\<Track>\<track>_<route>.spl` (`parseSpl`) is a text script (`05-scripts.md`: a keyword and its words per
line, `;` a comment):

```
GRIDPOINT x y z heading                   a starting place (forward = (sin heading, 0, cos heading)); 6, 4 on sprints
ROUTETYPE POINT2POINT <count>             a route that does not loop, its last point
MINCORNERSPEED <speed>                    13.41 without it
BEGINSPLINE FAST | ATTACK | CENTRE        a racing line, then
POINT x y z                               its points
ENDSPLINE
```

The three lines of a file have one length. Stock problem: a seventh `GRIDPOINT` is written past the six places
(`stock-problems.md`).

## The fly-by (`.cam`)

`scripts\<route>_intro.cam` (`parseIntroCam`; read by `0x0041C8D0`, played as `15-tracks.md` "The fly-by" says):

```
u32 shotCount, then per shot:
    f32 wait                              seconds at the first key before the camera moves
    u32 keyCount, then per key (56 bytes):
        f32 time                          seconds from this key to the next; after the last, before the next shot
        f32 fov                           radians
        u32 a word kept as w of the position
        f32 x, y, z
        f32 rotation w, x, y, z           camera to world
        u32 not read
        f32 pitch, f32 yaw                kept for the tool, not read
        u32 not read
```

The key the camera is at is the index cut toward zero (the game's rounding), blended to the next one by the rest.

## A route's collision (`.cld`)

`tracks\<Track>\<track>_<route>.cld` (`Core/Formats/Cld.h`, `parseCld`; read by `0x0053DFF0`; `15-tracks.md` "The
collision"):

```
u32 zoneCount, then per zone (24 bytes):
    i32 flag                              the surface: 0 tarmac, 1 sand, 2 grass
    f32 friction                          multiplies a shape's Mu in a contact with a face of the zone
    f32 bounce                            multiplies a shape's Restitution
    f32 not read
    f32 rangeMin                          faces: the contact offset along the normal; walls: their bottom above the marker
    f32 rangeMax                          walls: their top above the marker
u32 vertexCount, f32 vertex[4] per vertex
u32 faceCount, then per face (28 bytes):
    u32 vertices[3]
    f32 normal[3]                         the unit normal
    f32 d                                 0 in every file
u8  zone[faceCount]                       a face's zone (bit 7 cleared at the load: the query marks collected faces)
    padding to a multiple of 4 from the file's start
u32 markerCount, then per marker (32 bytes):
    f32 position[4]
    f32 direction[4]                      the wall's horizontal normal
u8  zone[markerCount]                     as the faces' (bit 7 cleared)
    padding to a multiple of 4 from the file's start
f32 area[4]                               minX, minZ, maxX, maxZ of the tree's root
u32 treeSize                              the bytes left in the file
the quadtree, depth first:
    inner node: u32 flags (bit 0 clear), 16 bytes of child places, f32 splitX, f32 splitZ, u32 not read,
                then its four children: (low x, low z), (high x, low z), (low x, high z), (high x, high z)
    leaf:       u32 flags (bit 0 set), u32 faceCount, u32 not read, u32 segmentCount, u32 not read,
                u32 faces[faceCount], u32 segments[segmentCount]
```

A wall segment is a marker and the next one (a leaf's segment `s` runs from marker `s` to `s + 1`). The tree covers
the faces' and segments' indices; a reader refuses a file whose indices run past their tables.

## Collision meshes (`.ocd`)

The collision mesh of a car (`cars\<car>\models\`) or a physics object (`Core/Formats/Ocd.h`, `parseOcd`; loaded by
`0x00535840`):

```
u32 vertexCount, f32 vertex[4] per vertex   in the model's centimetres
u32 faceCount, then per face (32 bytes):
    u32 vertices[3]
    u32 not read
    f32 normal[4]                         outward
u32 mappedCount, u32 faceMap[mappedCount]   the faces as the tree's leaves list them
u32 nodeCount, then per node (48 bytes):  a tree of spheres over the faces
    f32 centre[4]
    f32 radius
    12 bytes not read
    i32 sibling                           the next child of the same parent, -1 the last
    i32 child                             the first child, -1 for a leaf
    u32 first, u32 count                  a leaf's faces in the face map
```

Files of older tools end after the faces or after the face map's count of 0; the reader takes them so. The hulls a
`.dlh` holds are this layout with the face map and the nodes always there.

## Scene objects (`.dlh`)

`tracks\<Track>\<track>_<name>.dlh` (`Core/Formats/Dlh.h`, `parseDlh`; read by `0x0054F520`; `21-scene-objects.md`):

```
u32 objectCount, then per object:
    u32      type                         1 mesh, 2 dummy, 3 light, 4 effect, 5 spline, 6 helper
    char[32] name
    the type's data (below)
    u32      flags                        bit 0: a hull follows, bit 1: another hull follows (after the first)
    hull (.ocd layout, bit 0), hull (bit 1)
    u32      length, char text[length]    the 3ds Max properties (zero-padded)
u32 rootCount, then per node, depth first:
    f32 matrix[16]                        the world matrix, row-major, the position in 3, 7, 11
    u32 object                            the object it places
    u32 childCount, then its children
```

The types' data:

```
1 mesh:   u32 indexCount, u16 indices[indexCount]
          u32 submeshCount, then per submesh:
              u32 material                the route's material id (through its .d8m table)
              28 bytes                    the exporter's pointers, not read
              u32 indexStart, u32 indexCount
              u32 list                    1 a triangle list, 0 a strip
              f32 sphere[5]               the centre (x y z), 1, the radius
              f32 boxMin[3], f32 boxMax[3]
              f32 lodMin, f32 lodMax      squared distances
              u32 flags                   2 never drawn, 4 the alternative (hit) set
              u32 elementCount, D3DVERTEXELEMENT9[elementCount]
              u32 stride, u32 vertexCount, vertex bytes[stride * vertexCount]
          f32 sphere[5], f32 boxMin[3], f32 boxMax[3]   of the whole mesh
2 dummy:  nothing
3 light:  u32 kind; kind 1 or 3: f32 colour[4]; kind 2: f32 colour[4], f32 cones[2]
4 effect: char[32] its effect (empty in the game's files)
5 spline: u32 closed, u32 pointCount, then per point f32 knot[4], f32 in[4], f32 out[4] (the handles)
6 helper: u32 shape; 1 a box: f32 min[4], f32 max[4]; 2 a sphere: f32 centre[4], f32 radius
```

## Textures (`.ddt`, `.dds`)

`textures\<name>.ddt`, `ui\textures\`, the tracks' pixel files (`Core/Formats/Ddt.h`, `Ddt::parse`):

```
u32 format                                FourCC 'DXT1' / 'DXT3' / 'DXT5', or 21 A8R8G8B8, 22 X8R8G8B8
u32 width, u32 height, u32 mipLevels
u32 minFilter, magFilter, mipFilter       D3DTEXTUREFILTERTYPE: the sampler states of the texture's stage
u32 addressU, addressV                    D3DTEXTUREADDRESS
f32 mipLodBias
u32 maxAnisotropy
u8  pixels[]                              the levels largest first, as in a DDS file
```

A level of a DXT format is (width + 3) / 4 × (height + 3) / 4 blocks of 8 bytes (DXT1) or 16; a 32-bit level is
width × height × 4 bytes (B, G, R, A). The game's files ask for a mip LOD bias of −1.5; the world's textures
(`textures\`) mostly for linear filtering with linear mips and wrapping (647 of 792), the UI's pictures (`ui\textures\`)
for point filtering (637 of 733; the HUD's leave their transparent texels white), the cars' (`cartextures\`) for
linear filtering with point mips. The effects' textures (`effects\`) and the environment cubes of
`textures\` are DDS files (`Ddt::parseDds`: the 128-byte DDS header, its DXT1 .. DXT5 or 32-bit formats, a cube with
its six faces one after another, each with its levels).

## The ground maps (`.sqh`, `.pqh`)

`tracks\<Track>\<track>_<set>.sqh` (the sun on the cars and the objects that move) and `.pqh` (the puddles of the wet)
(`Core/Tracks/SunMap.h`, `parseSunMap`; read by `0x005053B0`; `15-tracks.md` "The ground maps"):

```
u32 width, u32 height                     pixels over the track's box, in tiles of 64 x 64
u32 bias                                  added to every value
u32 base                                  read and not used
f32 step                                  a value is its 2-bit code over it
f32 min[4], f32 max[4]                    the box (x, y, z)
u32 nodeCount, u32 blockCount
u32 tiles[(width / 64) * (height / 64)]   each 64 x 64 tile's root, row by row (row 0 at the far z edge)
u16 node[4] per node                      a node's quarters (low x low y, high x, high y, both high): a code below
                                          0xFF, else 0xFF + the node (or, in a quarter of 8 x 8, the block) it is
u16 rows[8] per block                     8 x 8 pixels, 2 bits a pixel, the low bits the left
```

A tile's root is 0xFF + its node. The file ends with its blocks.
