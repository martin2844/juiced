# Models

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.
Game space is left-handed: X right, Y up, Z forward, 1 unit = 1 cm.

## Node tree (`.boh`)

Where each mesh of a model goes: cars (`cars\<car>\models\`), wheels, the scenes of the menus and the 3D maps
(`models\`). Nodes are stored depth first, one top node per file in the shipped data:

| field | bytes | meaning |
|---|---|---|
| name | 32 | zero-padded |
| matrix | 16 floats | row-major, the position in elements 3, 7 and 11; relative to the parent |
| attach | 4 | 0 a mesh node, 2 a light |
| mesh (attach 0) | 32 | the `.d8m` to load, without extension; `NONE` is a slot the game fills (a car part) |
| light (attach 2) | 4 + n | the length, then a text of `KEY value` lines (lens flare keys) |
| children | 4 | the number of child nodes that follow, each with its subtree |

## Mesh (`.d8m` of a model)

The meshes of a model live in a `mesh\` folder (`mesh\`, `carmesh\`, `cars\<car>\mesh\`); a route's `.d8m` is another
layout (the route geometry). Some files in `mesh\` are of an earlier layout and do not read as below; a few are
empty.

```
u32  indexCount, u16 indices[indexCount]       one pool for all parts
u32  partCount, then per part:
     char[32] name                              the part's material (.pmd of that name in mats\)
     u32 indexStart, u32 indexCount             the part's range of the pool; indices are local to its vertices
     u32 faceType                               0 triangle strip (0xFFFF restarts it), 1 triangle list
     bounds                                     centre xyz, w, radius, min xyz, max xyz (11 floats)
     f32 lodIn, f32 lodOut                      squared camera distances (cm²) between which the part is drawn
     u32 renderFlag
     u32 elementCount, D3DVERTEXELEMENT9[elementCount]   the vertex declaration, with its end record
     u32 stride, u32 vertexCount, vertex bytes[stride * vertexCount]
bounds of the whole model (11 floats)
```

The file ends exactly after the model's bounds.

## Material (`.pmd`)

A text file per part, in `mats\` beside the meshes, named as the part:

```
Material Program = <class>
<key> = <value>
```

The first key names the material class (one of the 53 classes of the game, `11-effects.md`); every other key is a
parameter or a texture slot of that class. A texture value is a path of which only the name counts: the texture is
`textures\<name>.ddt`. Keys may come in any order or be left out.
