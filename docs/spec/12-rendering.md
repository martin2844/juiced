# Rendering

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.
The renderer object is `[0x0075F100]`: `+0x684` the shader classes (`11-effects.md`), `+0x7D0` the Direct3D 9 device.
The arithmetic runs on the x87 unit in single precision (the main thread after the device is made), each operation
rounded to a float; the order of the operations below is the game's.

## Vertex shader constants

The engine writes the vertex shader constants itself (`0x00504C60`: start register, count, data): it keeps a copy
(`0x0079BD18`, 16 bytes a register) and calls `SetVertexShaderConstantF` on the device. The registers follow the
`v_constants` struct of the effects:

| registers | name | written by |
|---|---|---|
| c0 .. c2 | cModel2World (3 x 4, row after row) | an object |
| c3 | cModelScale | an object |
| c4 .. c6 | cNormal2World | an object |
| c7 .. c9 | cWorld2View | the view |
| c10 .. c13 | cView2Clip | the view |
| c14 .. c17 | cWorld2Clip | the view |
| c18 .. c21 | cModel2Clip | an object |
| c22, c23 | 2D viewport scale, offset | |
| c24 | numbers: (1, 0.5, 0.25, 0) | the reset |
| c25 | cAmbientColour | ambient |
| c26, c27 | light 1: colour, direction | |
| c28 .. c31 | lights 2 and 3 | |
| c32 | cCameraPosition | the view |
| c33 | camera direction | the view |
| c34 | specular colour | |
| c35 .. c38 | texture offsets and scales | |
| c39 | specular power | the reset: 100 |
| c40 | random | |
| c41, c42 | sprite axes | the view |
| c43, c44 | light 1 axes | the light direction |
| c45 | cTime | |
| c46 .. c49 | cCloudProject | |
| c50 | cFogColor | |
| c51 | cFogDensity | |
| c52 .. c59 | user | materials |

### An object (`0x005564B0`)

From the node's matrix M (3 x 4, the position in the fourth column):

- c0 .. c2 = M;
- c3 = (|column 0|, |column 1|, |column 2|, 0), each `sqrt((m0j * m0j + m1j * m1j) + m2j * m2j)` (`0x00556350`);
- c4 .. c6 = M with its first three columns multiplied by `1 / length`;
- c18 .. c21 = the camera's world-to-clip matrix (4 x 4) times M (`0x0054CE90`), element by element in the order
  `out[r][c] = ((A[r][0] * B[0][c] + A[r][2] * B[2][c]) + B[1][c] * A[r][1])` for the first column and the game's
  own order for the others, the fourth column adding `A[r][3]`.

With the fixed-function classes the object instead sets three directional lights and the transforms.

The fixed-function lights (`0x005463C0`, from the ambient's set `0x00514E10` and the world's region draw
`0x005469A0`), only with the classes `0x10000`: the material (`0x00546320`: diffuse and ambient white, specular white
with alpha 0, power 17), three directional lights, light i its colour (`+0x70 + 0x10 i`) / 256 as the diffuse (the
green taken from the red: a stock problem), specular 0.6, the renderer's ambient × 0.01 as the ambient, its
direction (`+0x40 + 0x10 i`), each enabled; LIGHTING on, AMBIENT the ambient's w, r, g, b each rounded to a whole
number (the low byte), AMBIENTMATERIALSOURCE material, LOCALVIEWER off.

## The renderer's state

The reset (`0x00514330`): c24 = (1, 0.5, 0.25, 0); ambient (51, 51, 51); light colour (255, 255, 255); light
direction (0.717, -0.5, 0); c39.x = 100; specular colour 0xFF000000; render states FOGTABLEMODE = EXP,
FOGVERTEXMODE = NONE, RANGEFOGENABLE = 0, FOGDENSITY = 0, CULLMODE = CCW, ALPHAREF = 0x80, ALPHAFUNC = GREATEREQUAL,
ZFUNC = LESSEQUAL, ZENABLE = 1, DITHERENABLE = 1; the current material ends; TEXTURETRANSFORMFLAGS = 0 on stages 0 .. 3;
the world and view transforms the identity.

The reset runs when the device is made or reset (after a clear and a present), not every frame. c39 there takes
the stack where the light colour was built: (255, 255, 255, 100).

- Ambient (`0x00514E10`): kept at `+0x28` (0 .. 255 a channel); c25 = (r, g, b) × 1/511, w = 1.
- Light 1 colour (`0x005150C0`): kept at `+0x70`; c26 = (`[+0x12C]` × colour) × 1/511, w = `[+0x12C]` (the light
  scale).
- Light 2 colour (`0x00515140`): kept at `+0x80`; c28 = (f × colour) × 1/511, w = 1, f = 1 at night (`+0x38`), else
  the light scale.
- The three colours at once (`0x00514E80`, light 3 kept at `+0x90`): the three vectors as above (light 3 × 1/511,
  w = 1) sorted brightest first by (z + y) + x (a bubble sort while `[0x00725780]` is set, which it is), written as
  c26, c27, c28.
- Light 1 direction (`0x005151D0`): kept at `+0x40`; c27 = the direction d. u = d × (1, 0, 0) with the products by
  zero kept, v = (u × d) / (|v| + FLT_EPSILON), w = (d × v) / (|w| + FLT_EPSILON): c43 = v, c44 = w. With shader
  classes from `0x70000`: c45 .. c49 for the clouds — by `[0x0075F108]` (0: time × 2.25 and scale 1/300000; 1:
  c45 = (time, 1, 0) and scale 1/20000; 2: time × 0 and 1/20000), c46 = (v × scale, time × 0.015), c47 = (w ×
  scale, 0), c48 = (d × scale, time × 0.015), c49 the next static (its first value the initialisation flag of the
  block, 1 as an integer). The time is the renderer's `+0x110`.
- Lights 2 and 3 directions: kept at `+0x50`, `+0x60`; c29, c31.
- Specular colour (`0x00515500`, ARGB, highlight): c34 = ((channel × light 1 colour) × light scale) × 1/130305, w =
  highlight / 255.
- Light scale (`0x00515AF0`): `+0x12C`, then lights 1 and 2 are written again.
- Fog colour (`0x00514D60`, ARGB): FOGCOLOR, c50 = (r, g, b) / 255, w = 0. Fog density (`0x00514CF0`): 0.5 / FogFar
  (0 when FogNear or FogFar is FLT_MAX) as FOGDENSITY and c51.x.

## The graphics settings (`0x0079E600`)

| offset | value |
|---|---|
| `+0x00`, `+0x04`, `+0x08` | the screen's width, height, colour bits |
| `+0x10` | the side of the renderer's cubes (`0x005126F0`) and of the cars' reflection cube |
| `+0x14` | the side of the world's reflection cube |
| `+0x1C`, `+0x20` | the post effects' blur target `+0x1C` |
| `+0x24`, `+0x28` | the post effects' blur target `+0x14` |
| `+0x2C`, `+0x30` | the wet road's reflection |
| `+0x34`, `+0x38` | the scene targets of the post effects (0 without shaders) |
| `+0x0C` | windowed (the registry's `Windowed`) |
| `+0x44` | antialiasing (the registry's `Antialiasing`) |
| `+0x18`, `+0x3C`, `+0x40` | other sizes and flags of the renderer |
| `+0x48` | shadows: 1 off, 2 on |
| `+0x4C` | reflections 0 .. 3 |
| `+0x50` | the detail 0 .. 1 (`[0x0079E650]`) |
| `+0x54`, `+0x58`, `+0x5C` | motion blur, glow, depth of field |
| `+0x60` | the brightness 0 .. 1 |
| `+0x64` | the adapter has no shaders (its vertex shader version 0x10000) |
| `+0x68` | the brightness's gamma: the adapter's `D3DCAPS2_FULLSCREENGAMMA` and `+0x0C` 0 |
| `+0x6C` | the adapter has shaders |
| `+0x70` | the detail factor (`[0x0079E670]`) |
| `+0x74`, `+0x78` | the adapter's vendor and device ids |

The game's start makes it with the defaults (`0x006C37B0` calls `0x005A26C0` before any adapter is known: class 3,
and with no shaders known the three effects 0). The renderer's start (`0x005A25F0`) sets the screen from `0x0071E5B8
..`, the registry's `RezWidth`, `RezHeight`, `RezDepth`, `Windowed` and `Antialiasing` read into them by
`0x0059C5E0` (their defaults 800 x 600 x 16, 1, 1; `+0x34` / `+0x38` the size), the detail 60 x 0.01 and its factor, the
reflections 2 (0 without shaders) and, without shaders or with none on the adapter, the effects 0; without shaders
also the shadows 1, `+0x10` 0x40 and `+0x34`, `+0x38`, `+0x44` 0. Then `0x00512A40` takes the adapter's ids (its
D3DADAPTER_IDENTIFIER9) and its shaders, `+0x10` at least 0x40, without shaders the same limits as above (`+0x10`
0x10), `+0x68`, and the texture limits of its caps. Later in the device's creation (`0x0051347F` → `0x005A2580`) the
motion blur, glow and depth of field become 1 when the adapter has shaders (`+0x64` 0 and `+0x6C` set), else 0,
before a driver's `reg.cf1` is read.

Each frame the renderer's frame (`0x00514A40`) starts with `0x00512920`: with `+0x68` and the brightness other than
the one last taken (`[0x00722EB0]`, -1 at first and after the device's reset, `0x005142E9`), the gamma g =
(brightness - 0.5) x 0.15 + 1 within 0.01 .. 2 and the ramp's 256 steps i pow(i x 256, g) within 0 .. 65535, rounded
to the nearest, the same for red, green and blue (`SetGammaRamp(0, 1, ...)`). A window never takes it. After the
frame's time the profiler's timer 0 ends (`0x00514B28`) and the debug pages' current page is drawn while they are
shown (`0x00514B5A`, never), and before the effects' frame timer 0 starts again (`0x00514C4B`; `01-startup.md`,
"Debug pages").

The defaults (`0x005A26C0`) by the adapter's class: NVIDIA (`0x10DE`) by the device id, ATI (`0x1002`) class 4, any
other vendor class 3 (the lines for ATI device ids compare the vendor id). Then `0x005A2490`: 1024 x 768 x 16, the
texture sizes (`+0x10`, `+0x14` 0x40 << `[0x0071E5DC]`, `+0x18 ..` 0x100 and 0x200, `+0x34` 0x400, `+0x38` 0x200,
`+0x3C`, `+0x40` 1, `+0x44` 0) and the class's row of `0x00725798`, the brightness 0.5:

| class | shadows | reflections | detail | motion blur | glow | depth of field |
|---|---|---|---|---|---|---|
| 0 | 1 | 0 | 0 | 0 | 0 | 1 |
| 1 | 2 | 1 | 0 | 1 | 1 | 1 |
| 2 | 2 | 0 | 0.25 | 0 | 0 | 0 |
| 3 | 2 | 2 | 0.75 | 1 | 1 | 1 |
| 4 | 2 | 2 | 0.99 | 1 | 1 | 1 |

Without shaders the reflections 0 and the detail 0.4; without shaders or with none on the adapter the effects 0;
without shaders the shadows 1 and the limits above.

`reg.cf1` (`03-saves.md`) keeps the shadows, reflections, detail, motion blur, glow, depth of field and brightness
(`0x005A33A0`, written along with each file of a save) and gives them back when a driver is loaded (`0x005A3410`;
without shaders the limits again). The graphics options window (`02-frontend.md`) changes them.

## Drawing a model node

A node of a model (`0x00556900`, from the view's pass with the camera and a pass mode): the object constants
(`0x005564B0`), then (`0x005567D0`), unless the node is hidden (`+0xC0` bit 4) or has no mesh (`+0xA8`):

1. a mirrored node (`+0xC0` bit 1) swaps the cull mode (`0x0071935C`: 3 → 2, else 3) and applies it (`0x00524170`);
2. the renderer keeps the node's position minus the camera's (`+0x138`);
3. the light scale (`renderer.vtable + 0x48`, `0x00515AF0`) = the node's `+0xC4`: `+0x12C` takes it and the light
   colours 1 and 2 are written again (c26; c28 = colour 2 × (1 at night, else the scale) × 1/511, w = 1);
4. c40 = the node's `+0xAC` (4 values) (`0x00515BB0`); a current material whose class has flag `0x1000` is
   forgotten;
5. the mesh draws (`mesh.vtable + 0x24`) with the camera, the node, the pass mode and `+0xC0` bit 8;
6. a mirrored node swaps the cull mode back; the light scale returns to 1.

## Drawing a mesh (`0x00559010`)

The mesh: `+0x4C` the index buffer, `+0x50` the parts (0x74 bytes each), `+0x54` their number. A part: `+0x00`
material, `+0x20` first index, `+0x24` index count, `+0x28` face type, `+0x58` / `+0x5C` LOD range, `+0x60` flags,
`+0x64` stride, `+0x68` vertex count, `+0x6C` vertex buffer, `+0x70` vertex declaration.

- Distance: d = ((dz × dz + dy × dy) + dx × dx) from the node to the camera (world positions), times the node's
  scale (`node.vtable + 0x30`) times `1 - 0.9 t`, t = `([0x0079E670] - 0.5) × 2/3` clamped to 0 .. 1.
  `[0x0079E670]` is the detail factor: the renderer's start (`0x005A25F0`) sets the detail `[0x0079E650]` to
  `[0x0071E5CC]` (60) × 0.01 and the factor to detail × 1.5 + 0.66 (1.56); the graphics options window
  (`0x004820A0`, DONE) sets both from its DETAIL slider. The detail of `reg.cf1` (`+0x50` of the settings
  `0x0079E600`) is loaded with the driver and shown by that window.
- `SetIndices`, then per part, by the renderer's pass kind (`+0x128`): kind 0 — lodIn ≤ d < lodOut, flag 1 clear,
  flag 4 equal to (the node's bit 8, the last argument, and the mesh's `+0x58` set: a part with flag 4; the bit is
  set on a light's node knocked sideways, and none of the game's meshes has a part of flag 4); in pass mode 1 only
  parts whose material class flags have none of `0x24`, in mode 2 only those with one; kind 1 — parts with flag 2.
- `SetStreamSource(0, buffer, 0, stride)`, the declaration (`SetVertexDeclaration` when it differs from the last,
  `0x0075F144`), the material (the renderer's override `+0x124`, else the part's) for each of its passes
  (`material.vtable + 0x1C`): the pass begins (`0x005240E0`), then `DrawIndexedPrimitive` — a strip
  (`TRIANGLESTRIP`, count - 2 triangles) for face type 0, a list (`TRIANGLELIST`, count / 3) for 1, vertices from 0.

One part (`mesh.vtable + 0x28`, `0x00559310`, from the model node's slot 3 `0x00556920`, which is the node draw
above with it in place of the mesh's draw; nothing calls the slot): the same tests and drawing for that part, the
distance without the detail's `1 - 0.9 t`, no pass mode; a node with `+0xC0` bit `0x10` first sets parameter 8 of a
material of class 0 to 1.

## Material passes (`0x005240E0`)

The current material is `[0x0075F16C]`, the class drawn last `[0x00719540]` (`0x10009` after a reset), the pass
`[0x0075B2B8]`.

1. A current material other than this one ends its program (`vtable + 0x18` gives the program, `0x0051BDA0`).
2. Software vertex processing is set by this material's program's technique (`+0x20`).
3. Another class or pass than the last: the class setup (`vtable + 0x10`, the pass).
4. Another material or pass: the binding (`vtable + 0x14`, the pass); this material and its class become the last.

The reset of the current material (`0x00524170`): its program ends, the class becomes `0x10009`, no material.

## Material programs at draw time

`0x0051BCD0` begins a program's pass: the program whose pass is open ends (`0x0051BDA0`), `Begin` with flags 7 (no
state is saved or restored), `BeginPass`. Ending: `EndPass`, `End`; a technique with `USECONSTANTMEM` then puts the
engine's copy of c0 .. c51 back on the device.

The effects run under the game's state manager (`0x00722EB4`, vtable `0x006F78D0`): every call goes to the device as
it is, but `SetVertexShaderConstantF` with more than 29 registers is dropped ("ignoring block const set"), so the
`v_constants` block of an effect never reaches the device; the engine's own values stay. A sampler state also
marks the game's sampler cache (`0x0075B298`, 8 values) unknown.

The effects share two parameters through their pool, `vAmbUnit` and `vOne` (the only shared ones). Their handles are
taken from the first effect of the list (`0x00519700`, the table `0x00719478`: `Material_2DBlur`, the first of
`effects\filelist.txt`, declares both); each frame before the views draw (`0x00514C88` → `0x00519DD0`) `vAmbUnit` is
the renderer's ambient (`+0x28`, the last view's) with r, g and b × 0.003921569 and w as it is, `vOne` (1, 1, 1, 1).
Without it both stay 0, the files' defaults.

## Material classes

### Standard Opaque (id 3)

Object of 0x3C bytes (`0x0052CB70`): the programs `Material_StdOpaqueShad` (`+0x24`), `Material_StdOpaqueNoShad`
(`+0x28`), `Material_StdOpaqueShadNight` (`+0x2C`), each left out when it has no usable technique.

- Parameters (`0x0052CDD0`): 0 Double-sided → `+0x20`; 1 Z-Bias → `+0x30` = round(value × 16).
- Texture (`0x0052A3D0`): slot 0 → `+0x1C`.
- The program (`0x0052CCA0`): without the scene's light tile texture NoShad; with it ShadNight at night, else Shad.
- Drawing (`0x0052CCD0`): the texture on stage 0, the scene's light tile on stage 1, the program's pass 0, then
  pixel shader c0 = (the renderer's ambient × 1/511 for r, g, b; its fourth value) and CULLMODE = NONE when
  double-sided, else the default cull mode.
- Fog on (`0x00529530`): FOGENABLE = 1 on the device.

## Textures (`0x00510E90`)

A texture of `textures\` binds on a stage with the sampler states of its `.ddt` header — MINFILTER, MAGFILTER,
MIPFILTER, ADDRESSU, ADDRESSV (the five values after the size and level count) and MIPMAPLODBIAS — unless the stage
still has this texture's states (`0x0051EF30`, a cache of 8 stages that the effects' state manager clears), then
`SetTexture`.

### Cube textures (`0x00510280`)

The environment textures are `textures\<name>.dds` files with six faces, made into a cube texture by D3DX
(`D3DXCreateCubeTextureFromFileInMemory`, `0x005DCC43`): the file's format and size, all mip levels down to 1 x 1
(the levels the file lacks are filtered by D3DX's box filter: the average of four pixels; DXT2 / DXT4 colours divided
by alpha before and multiplied after), the managed pool. A loaded name is kept and given out again. The texture
object (`0x00511110`) binds with linear MIN, MAG and MIP filters, ADDRESSU and ADDRESSV clamp and no LOD bias. A file
that does not load leaves no texture (stage 1 is then bound to nothing).

The stock city's cubes (`arcade_env`, `dealer_env`, `garage_env`, `scrapyard_env`, `show1_env`) are DXT4, 128 x 128,
one level; their alpha is not 255 (the shaders read it). D3DX compresses the filtered levels to DXT4 again; OpenJuice
keeps them as A8R8G8B8, so the small levels lack the compression error of the stock ones.

## The 3D element's camera

The view (`0x005B36D0`) gives its camera the field of view 0.35 × π (π as 3.14159012), far 10 000 000 and the aspect
value; the camera starts with near 15, far 10 000, aspect 0.75 and the field of view set to (100, 100).

- Field of view (`0x005528A0`): × 1.333 on a wide screen; sin and sin² × 1.5 kept; the height's angle =
  2 × atan(tan(fov / 2) × aspect), × 1.15 on a wide screen.
- Projection (`0x0054CB10`): [0][0] = 1 / tan(fov / 2), [1][1] = 1 / tan(fovY / 2), [2][2] = far / (far − n),
  [2][3] = −[2][2] × n, [3][2] = 1, n = the near plane, at least 15.
- The camera's matrices (`0x00552180`): view = the world matrix inverted, world-to-clip = projection × view.
- The parts' distance scale (`0x0054CA00`): `+0x1DC` (1; 0.5 for the 3D element on a wide screen) × sin² × 1.5.
- Look-at (`0x005B4380`, also `CameraLookAt`): the point is kept (`+0x170`); from the camera's goal (`+0x130`)
  yaw = −atan2(dx, dz), pitch = −atan2(−dy, √(dz² + dx²)), each + 2π when negative, roll 0, become the target
  angles (`+0x140`); `CameraLookAt` makes them the camera's angles (`+0x150`) at once.

Every frame (`0x005B4870`) the camera moves by its mode (`+0x128`), not by the frame time unless said:

- MODEL_AUTO (0): the camera moves 2 % of the way to its goal, its rotation resets, the angles are brought into
  (−π, π] (yaw, then pitch, by one turn) and the camera turns by Y, X, Z.
- MODEL_MANUAL (1): the stick turns the first model: Y by x × 0.1, X by y × 0.1.
- CAMERA_AUTO (2): as MODEL_AUTO with 10 %, and the angles move 10 % of the way to the target angles (the
  difference of yaw and pitch brought into (−π, π] first); all the way on the first frame after the view is made
  or the mode changes (`+0x12C`, cleared after the frame).
- CAMERA_MANUAL (3): the stick turns the camera (Y by x × −0.1, X by y × 0.1); the pad moves it by 1250 along z
  (up +, down −) or x (left −, right +).
- The map mode (4, `0x005B4A0E`): while the input delay (`+0x198`) runs, it goes down by the frame time (within
  0 .. 9999); after it, the stick adds x × dt × 0.2 and y × dt × 0.2 to the rig's speeds (`+0x180`, `+0x184`) and
  holds the turn for 5 s (`+0x194`). Without the stick (or with the delay running) the yaw speed is dt × the auto
  speed (`+0x190`) once the hold is over (the hold goes down by the frame time). The speeds are kept within
  ±0.2; yaw += yaw speed, pitch −= pitch speed, each brought into (−π, π] (as many turns as needed); the speeds
  × 0.85; the pitch and yaw within their limits (`+0x1AC`, `+0x1B0`; `+0x1A4`, `+0x1A8`; 9999 none). The pivot
  node (`+0xD0`) resets, goes to the look-at point and turns by Y (yaw), X (pitch); the arm node (`+0xD4`) hangs on
  it, and its world position becomes the camera's goal; the look-at runs again (without `now`); then CAMERA_AUTO.
- After the mode: the fade (`+0x19C`) moves (target − fade) × dt × 2.5 toward its target (`+0x1A0`), within 0 .. 4,
  to the world's `+0x1C`; the zoom moves 2 % toward its target, both within 0.4 .. 1, and the field of view becomes
  (0.35 × zoom) × π (0.35 the variable `0x00725824`).

The element's listed nodes (`+0x118`, entries of 0x80 bytes, `0x005B4150` / `0x005B34A0`: listing puts the node in
the element's world) turn each frame (`0x005B5100`, `0x005B3660`) by their rates (`+0x60`, `+0x64`, `+0x68`) x the
frame time: with `+0x74` about the parent's Y, X and Z (`0x00534B70`, `0x00534C20`, `0x00534CD0`: the rotation
before the node's matrix, its place kept), else about the node's own Y, X and Z (`0x00534960`, `0x00534A10`,
`0x00534AC0`).

Another mode (`0x005B4490`; nothing when it is the same): out of the map mode the arm leaves the pivot keeping its
world place (`0x005347F0`), the look-at runs again and the camera stays at its world position, zoom target 1. Into
the map mode the arm hangs on the pivot (`0x005348E0`, when it has no parent), the pivot goes to the look-at point,
the arm to the given point (else (0, 0, −500)), the rig angles become (−0.12, −1.57, 0) (`0x00725830`,
`0x0072582C`). Either way the next frame takes the angles at once.

The view's constants (`0x00511CA0`): c7 the view matrix (3 rows), c10 the projection, c14 world-to-clip, c32 the
camera position, c33 the camera's Z axis, c41, c42 the normalised (0, 1, 0) × Z and Z × that; FOGENABLE and ZENABLE
on while the world draws, off after.

## The loading thread and the renderer's queue

The game's loading thread (`0x0059A6F0`: `CreateThread` on `0x0059AD80`, which runs `CoInitialize`, the loop
`0x0059A7A0` and `CoUninitialize`) runs the jobs queued for it (`0x0059A930`) one at a time in the order they came: it
waits for its event, takes the first job off the list under the list's lock (the event reset once the list is empty),
marks it running (`+0x08` 2), runs it (vt+4), waits until the renderer's queue is empty (`0x0051D270`, 10 ms sleeps)
and marks it done (`+0x08` 3). A job still on the list can be taken off (`0x0059A9D0`); one running goes on to its
end. The car view's loads also hold a token (`[0x00715C3C]` under the lock `[0x00767DBC]`, 10 ms sleeps while another
holds it): one car load at a time (`0x00459750`, the parts' load `0x0045AC91`).

The renderer's queue (`[renderer+0x204]`, its thread id `+0x14`): work on the device from the loading thread is
queued (`0x0051CF00`, which runs it at once on the main thread; from another it waits while the device is lost,
`[renderer+0x1D0]`) and the main thread runs it at its frame (`0x00514C96`: one item after another while any are
queued, until 0.25 s went by, `[0x006FFBEC]`, timed by `0x005A2110`) and while it waits for a job (`0x0045A35B`:
10 ms sleeps, one item each time). OpenJuice: a frame holds the device from its begin to its end, and the loading
thread's calls making, freeing, locking or writing resources wait for the device between frames; the main thread
waiting for the loading thread lets the device go meanwhile (the device is not made multithreaded without
`-mtd3d`, as the stock's). The engine's loads (the folders in use, the caches) take a lock the loading thread's jobs
hold from start to end.

## The world (`0x0054C5C0`)

The time (`+0x10`) goes to the renderer (`+0x110`, the step `+0x114`); the current material is forgotten; the
world's environment texture (`+0xD0`) goes to the renderer's `+0x118` and `+0x11C` (Building Opaque binds `+0x11C`);
the lights: with an atmosphere its values (`0x00533B80`), else the world's own (`0x0054DC90`): light 1 colour (`+0x28`),
light 2 (`+0x38`), light 3 (`+0x48`), the three at once, light 1 direction (`+0x68`), lights 2 and 3 directions
(`+0x78`, `+0x88`), the ambient (`+0x58`) and no fog (FogNear and FogFar FLT_MAX). A new world (`0x00530D70`):
ambient 17, light 1 255 from (0, −1, 0), lights 2 and 3 black from (0, −1, 0) and (0, 1, 0); the 3D element sets the
ambient to 50 and light 1's direction to (0, −1, 1). Then the world's roots (`+0xD8`) are drawn as the race's
(`0x0054C6B8`: `0x00550500`, `21-scene-objects.md`): each tree with the parts of opaque classes (mode 1), then
again with the others (mode 2), so a car's glass is drawn over what lies behind it (a spoiler); the sponsors'
decals (Coloured Spec ABlend) are of the late classes too and go with the glass in the parts' order, so the far
side's decals show through the rear window (`stock-problems.md`); then the object constants of no object
(`0x0054D8D0`) and the lens flares (`0x0054FC60`).

The world's clock: the new world keeps the game's clock (`+0x08`, its time `+0x18`) and starts running (`+0x18` 0)
with no lead (`+0x20` 0). Its time (`+0x10`) is taken again as the clock's time less the lead only when read while
running (the main pass `0x00547690`, a cube `0x00547D50`, the particles' frame `0x00503D10`); stopped it holds the
time last read. The race's pause (`0x004263E0`'s end) stops it (`+0x18` 1, `+0x1C` the clock's time); its end starts
it again with the lead the clock's time less `+0x1C` (there and inline in `0x00406AB0`, `0x00424270`, `0x00426B80`,
`0x00426C40`, `0x00426CF0`, `0x00426DF0`, `0x0044ED90`, `0x00455A90`). The shaders' time, the cubes' and the
particles stand still while the race is paused. The lead is the last stop's length, not the sum: after a second pause
the time jumps on by the pauses before it (`stock-problems.md`).

## The projected textures (`0x0054AF00`)

A texture laid on the route's faces under a node: a car's drop shadow with its neon and its headlights' beam. Made by
`0x00531260` (0x1970 bytes, vtable `0x006F9F0C`), it joins the world's list (`+0x98`); the world's pass draws the list
(each `[5]`) after the instances drawn after the cars and the object constants of no object (`0x0054D8D0`), before
the scene objects.

- Made (`0x0054AF00`): the node (`+0x14`), on (`+0x0C`), the box from a low corner (`+0x40`) to a high one (`+0x50`) in
  the node's frame, the bounding radius the longer of the two corners (`+0x28`), the route's collision (the world's
  `+0x24`, `+0x38`), a dynamic buffer of 0x400 vertices of 20 bytes (`+0x2C`: x, y, z, u, v) and its declaration
  (`+0x3C`). With a shadow texture a material of class 15 (Unlit ABlend, `<drop_shadow>`, `+0x30`) with it; with a
  glow texture one of class 28 (Coloured Spec ABlend, `+0x34`) with it, Additive 1 and the colour white.
- `[2]` (`0x0054BF40`) puts a texture on the glow's material (else the shadow's) and the flips of u and v (`+0x60`,
  `+0x64`); `[3]` (`0x0054BF90`) the glow's colour; `[4]` (`0x0054BFB0`) the two corners.
- `[1]` (`0x0054B7A0`, each frame): from the node's world matrix W the node's x and z axes laid on the ground, U =
  (lx W00 + kx W01, 0, lx W20 + kx W21) with lx = √(W20² + W00²) and kx = (1 − lx) × (1 when W21 W20 + W01 W00 > 0,
  else −1), V likewise from the z column (lz = √(W22² + W02²), the sign of W22 W21 + W02 W01); the top
  (`[0x0075B2D4]`) the node's height + the high corner's y. The box across the ground (each axis's ends in the order of
  its sign) goes down the collision's tree (`0x0054BFF0`, `0x0054C050`): a node's low x side when the box starts
  below its split, its high side unless the box ends at or below it (low x before high x, within each low z before
  high z); of a leaf's faces (`0x0054B2E0`), while more than 3 places of 300 are left, each not taken this frame whose
  normal's y is over 0.5 and with a corner, raised by 5, below the top: its three corners raised by 5, the face
  marked (bit 7 of its zone byte) and listed (up to 99). The marks are then cleared. Each vertex's u and v: from the
  box's low corner (the node's place + U × low x + V × low z) along the edges U × (high x − low x) and V × (high z −
  low z) (a flip starts from the far end and runs back), over the edge's length squared. The triangles are cut to
  0 < u, u < 1, 0 < v, v < 1 in turn (`0x0054B650`, through the static list `[0x007A0E20]` and back): each edge that
  crosses the line cut with all five values in between (`0x0054B4E0`), the polygon laid out again from its first
  corner while 4 places are left.
- `[5]` (`0x0054BDF0`): with the shadows' setting at 1 or more (`[0x0079E648]`), vertices kept and the projection seen
  (`0x00552FD0`: on, the sphere of the node's place and the radius in the view, and (dz² + dx²) − r² from the camera
  across the ground, by the camera's scale of distances, at most 100000000): up to 0x3FF vertices into the buffer, a
  triangle list with the shadow's material, then with the glow's.

## The silhouette shadows (`0x005491D0`)

A race car's model (`0x00485530`, at `0x004861CC`, before its projected textures) makes `0x00531380` objects of
`0x9348` bytes (vtable `0x006F9E68`) on its chassis node into the world's list of projected textures (`+0x98`): by day
(no atmosphere, or its `+0x170` and `+0x184` both 0) one of the sun (type 0, the car's `+0x4CC`), else one of the
nearest light (type 1, `+0x4CC`) and one of the second nearest (type 2, `+0x4D0`). Each has `<projected_shadow>`
(Blurred Shadow, class 34, its offset 0.5 / the graphics settings' `+0x18`, 256) with a render target of 256 x 256 as
its texture, `<silhouette>` (Constant Colour, class 2, white), an orthographic camera (near 0.01) with a view of the
target cleared to 0, the bounding sphere of the chassis node's tree (`0x00549500`: the first mesh node's place and
mesh radius, then each one's merged in - its radius divided by the longest axis of its own matrix, a sphere above 700
or 700 away ending that node's walk - as an offset from the node's place) and a dynamic buffer of 1000 vertices of 36
bytes (position, a float colour, u and v).

Each frame of the car (`0x004890B0`) its `[1]` (`0x00549E90`) takes the fade (the chassis's light over 0.7, 1 from 0.7;
the second object 1). The light's direction L is the atmosphere's (`0x005341F0`) or, from the nearest lights' grid of
the world (`0x0054E850`), from the light to the car with the fade times max(0, 2 - 0.001 x their distance across the
ground); without the light the object has nothing this frame. The world's lights are never added (its slot 8, `0x00548760`,
has no caller; the race's load builds the grid from them, `0x0054DF20`, which with fewer than 2 makes none), so the
night's silhouettes never cast a shadow. S = L x (1, 0, 0) and U = S x L, both unit; the camera's
axes are U, S and L, its place the sphere's centre C less L x (r + 5), its size r, far 3r + 10. The car's tree is
drawn into the target with the renderer's material `<silhouette>` and pass kind 1 (the parts with flag 2). The faces
of the route under the box C ± rU ± rS from -rL to (r + 300)L (`0x0054AB30`, `0x00549A80`: normal's y over 0.5, raised
by 9, while at most 997 vertices) take u = 2rU · (p - O) / |2rU|^2 and v = 1 - 2rS · (p - O) / |2rS|^2 from
O = C - rU - rS, the colour 0 and the alpha 0.25 x the fade, and are clipped to 0 < u, v < 1 (`0x00549D80`). The
draw (`[5]`, `0x0054A9E0`) with the shadows' setting above 1, a fade and seen (`0x00552FD0`) puts at most 999 of
them through `<projected_shadow>`: four taps of the target's blue around the vertex's u and v, averaged, times the
vertex's alpha, over black.

## Model files

A model (`0x00535310`, `models\<name>.boh`) is built depth first (`0x00534F50`): a mesh node (also `NONE`) is a
model node that draws four `rand()` values at once (`0x005550D0`: `+0xAC` .. `+0xB8` = rand × 1/32767, the node's
c40), then its mesh loads (`mesh\<name>.d8m`, `0x00558510`); a light node draws none. The material of a part is its
`.pmd` (`mesh\mats\<part>.pmd`, `0x005250A0` → `0x00523760`): the class by name (the last of that name), each
parameter of the class by its key (missing ones 0): int and float by `atoi` / `atof` of the first word, bool true
unless the word is FALSE (without regard to case), colour `R G B` as 0xFFRRGGBB, three floats, a string of up to 255
characters; the textures by slot name, the file name without folders and extension (`(null)` none).

The other way (`0x00523020`, the material's serialisable interface, slot 0 of vtable `0x006F8524` through
`0x00523E40`; nothing calls it): `Material Program = %s` with the class's name, then per parameter `%s = ` and the
value by type (1 `%d`, 2 and 3 `%f`, 4 `TRUE` / `FALSE`, 5 the colour's red, green and blue `%d %d %d`, 6
`%f %f %f`, 7 the string; another type nothing more), then `%s = %s` per texture slot that has a texture (the
texture's object given as the text: a stock problem), each line ending in CR LF, printed to a stream (`0x004FB590`).

## Material classes of the city map

| class | object | parameters (index → meaning) | programs | drawing |
|---|---|---|---|---|
| Standard Opaque (3) | 0x3C | 0 Double-sided, 1 Z-Bias × 16 rounded | StdOpaqueShad / NoShad / ShadNight | above |
| Standard Alpha Test (5) | 0x3C | 0 Single-sided, 1 Z-Bias | StdATestShadow / NoShadow / ShadowNight | as Standard Opaque; CULLMODE NONE unless single-sided |
| Specular Opaque (6) | 0x40 | 0 Shininess, 1 Specular Colour, 2 Specular Highlight, 3 Double-sided, 4 Z-Bias | SpecOpaque | texture, c39.w = shininess, cull, specular colour, the pass |
| SpecMap Opaque (44) | 0x40 | 0 Shininess, 1 Specular Colour, 2 Specular Highlight, 3 Double-sided | SpecMapOpaque | as Specular, the specular map on stage 1 |
| Building Opaque (42) | 0x64 | 0 Side Fresnel, 1 Front Fresnel, 2, 3 Light-Tile-X/Y as 1 / value (1 for 0) | BuildingOpaque / BuildingOpaqueNight | texture; at night the night pass, the night light on stage 1 and pixel c7 = (0, 0, 0, 0.25 or 1); by day the pass and the world's environment texture on stage 1; pixel c0, c1 (the fresnels in w), vertex c38 = the tile values; setup: the default cull |
| Sprite (Unlit) (13), Sprite (Lit) (16) | 0x58 | 0, 1 X/Y-Cells rounded (0 → 1) and their inverse, 2 FPS, 3 Playlist | UnlitSprite / LitSprite | frame = trunc(time × FPS), through the playlist, cell = frame % (x × y): c35 = (column / x, row / y, 0, 0), c36 = (1/x, 1/y, …); unlit: the pass first, c35, c36 by the state manager; lit: c36, c35 by the engine, then the pass |

The setup of these classes (`vtable + 0x10`) turns FOGENABLE on (Sprite (Lit): nothing; Building Opaque: the default
cull mode). A playlist is a list of numbers, ranges `a-b` (either way) and repeats `a*n`; 0 is never added.

## Material classes of the tracks

| class | object | parameters (index → meaning) | programs | drawing |
|---|---|---|---|---|
| Tarmac (11) | `0x0052F8B0`, vtable `0x006F97FC` | 0 Shininess → `+0x28`, 1 Specular Colour → `+0x20` (0xFF000000), 2 Specular Highlight → `+0x24`; 3 Mask Alpha PS2 not kept | Tarmac (`+0x2C`) / TarmacNoShadow (`+0x30`) / TarmacNight (`+0x34`), chosen as Standard Opaque's | `0x0052FA40`: the texture on stage 0; the renderer's half-vector cube (`+0x130`) on stage 1 (none while the environment cube is drawn, `[0x0075F1B4]`); with a light tile: the tile on stage 2, at night the night pass, vertex c52 = (0.5, 0.5, 1, 1) set on the device and the renderer's `+0x164` on stage 3 (none when it is not set), by day the pass; without a tile the NoShadow pass. Pixel c0 = the ambient × 1/511 (its fourth value kept), at night c0 = (stack values, the night road shininess) and c2 = that ambient; c1 = light 1's colour × 1/511; vertex c39.w = the shininess; the specular colour and highlight (`0x00515500`). Setup (`0x0052FA10`): the default cull, FOGENABLE 1 |
| Constant Colour (2) | vtable `0x006F9124` | 0 Colour → r, g, b × 1/255 (`+0x20` .. `+0x28`; `+0x2C` never set) | ConstColour | `0x005286B0`: the pass, the default cull, TEXTUREFACTOR = the four values × 255 rounded (alpha `+0x2C`); no textures, no setup |
| Standard Alpha Blend (4) | vtable `0x006F91A0` | 0 Z-Write, 1 Disable Fog, 2 Z-Bias × 16 rounded (kept) | StdABlend | `0x00528FB0`: the pass, ZWRITEENABLE = Z-Write, the texture, FOGENABLE unless disabled; setup: the default cull |
| Unlit Opaque (14) | vtable `0x006F98E0` | 0 Double-sided | UnlitOpaque | `0x00530B80`: the pass, the texture, the cull (NONE when double-sided); setup: fog on |
| Unlit ABlend (15) | vtable `0x006F9870` | 0 Disable Fog | UnlitABlend | `0x005303A0`: the pass, the texture, FOGENABLE unless disabled; setup: the default cull |
| Unlit ATest (29) | vtable `0x006F97D8` | none | UnlitATest | `0x0052F870`: the texture, then the pass; setup: fog on |
| Unlit Sign Scroll (18) | vtable `0x006F9408` | 0 X-Speed, 1 Y-Speed | UnlitSignScroll | `0x0052B0D0`: the pass, the texture, c35 = (time × speed less its sixteens cut toward zero — made a float as an unsigned number — for x and y; 0, 0) by the state manager; setup `0x0052B050`: the default cull, c36 = (1, 1, 0, 0), fog on |
| Unlit Sign Anim (19) | vtable `0x006F9650` | 0, 1 X/Y-Cells rounded (0 → 1) and their inverse, 2 FPS, 3 Playlist; `+0x24` .. `+0x30` start 0, 0, 0, 1 | UnlitSignAnim | `0x0052D930`: the pass, the texture, c36 = `+0x24` .., then c35 = the cell of the frame (as the sprites) by the state manager; setup `0x00530CF0`: the default cull, fog on |
| Shiny Opaque (24), Shiny Unlit Opaque (39) | vtables `0x006F95B8`, `0x006F962C` | 0 Side Fresnel → `+0x40`, 1 Front Fresnel → `+0x30`; Z-Bias not kept | ShinyOpaque / ShinyUnlitOpaque | `0x0052D5A0`, `0x0052D760`: the pass, the texture, the renderer's environment texture (`+0x11C`) on stage 1, pixel c0, c1 = `+0x24` .. `+0x40` (x, y, z never set); setup: none / the default cull |
| Chrome (26) | vtable `0x006F9778` | 0 Colour → r, g, b, a × 1/255; Z-Bias not kept; the texture (slot 0) at `+0x20` | Chrome (`+0x1C`) | `0x0052F390`: the pass, the renderer's `+0x118` on stage 0; without a texture stages 1 .. 3 emptied, with one stages 2, 3 emptied and it on stage 1; pixel c0 = the colour; no setup |
| Tree-Standard (30), Tree-Sprite (31) | vtable `0x006F9674` (made by `0x0052AA40`, `0x0052DC50`) | 0, 1 Diffuse 2x Min/Max (r, g, b × 1/128, a × 1/255), 2, 3 Ambient 4x Min/Max (× 1/64) kept as the minimum and the maximum less it (`+0x24`, `+0x34`, `+0x44`, `+0x54`); 4, 5 Min/Max XZ Magnitude (× 0.01, `+0xF4` = `+0xFC`, `+0x100` = `+0x108`), 6, 7 Y (`+0xF8`, `+0x104`), 8, 9 Min/Max Speed (`+0x10C`, `+0x110`) | TreeStandard / TreeSprite | `0x0052AB60`: the texture, the pass, c52 .. c55 the colours, c56 .. c59 per sway vector sin(time × speed + phase) × amplitude, both on the device; setup: fog on |
| Anim Alpha-Add (33) | vtable `0x006F90DC` | as Sign Anim, the counts cut toward zero; `+0x24` .. `+0x30` start 1, 1, 1, 1 | AnimAlphaAdd | `0x00528180`: the pass, the texture, c35 the cell and c36 `+0x24` .. together by the state manager; no setup |
| Standard Fade-Out (32) | vtable `0x006F9820` | 0 Double-sided, 1 Fade Start, 2 Fade End (`+0x30` = 1 / (end − start) after either) | StdFadeOut (`+0x24`) | `0x0052FE30`: the pass, the texture, pixel c0 = the ambient × 1/511 (its fourth value kept), vertex c52 = (start, end, 1 / (end − start), 0) by the state manager, the cull; setup: fog on |
| Crowd Sprite (35) | vtable `0x006F90B8` | cut toward zero (0 → 1): 0 X-Cells → `+0x24` = 1/x, `+0x44` = x, `+0x48` = 1/x; 1 Y-Cells → `+0x28` = `+0x40` = 1/y; 2 Part 0/1/2 → 1 / `+0x40` in `+0x34`/`+0x38`/`+0x3C` (the other two 0; other parts leave them), `+0x50` = Part × 4; 3 Ambient-Multiplier → `+0x4C` (0 → 1) | CrowdSprite | `0x00527E10`: the pass, the texture, c36 = `+0x24` .., c52, c53 = `+0x34` .. `+0x50` by the state manager; passes: the technique's; no setup |
| Crowd Sprite Anim (47) | vtable `0x006F9754` | as Crowd Sprite with the cells' roles swapped (X-Cells → `+0x24`, `+0x40`; Y-Cells → `+0x28`, `+0x44` = y, `+0x48`); 3 Min-FPS → `+0x54`, 4 Max-FPS → `+0x58` (`+0x5C` the difference), 5 Ambient-Multiplier | CrowdSpriteAnim | `0x0052F070`: as Crowd Sprite, `+0x60` = the time, c52 .. c54 |
| Water (46) | vtable `0x006F9528` | 0 Front-Opacity → `+0x34` (1), 1 Side-Opacity → `+0x30`, 2 U-Tile → `+0x38`, 3 V-Tile → `+0x3C` (1; `+0x40` 0, `+0x44` 1); textures: slot 0, slot 1 the noise | Water (`+0x24`) | `0x0052C410`: the texture on stage 0; with the renderer's `+0x164` it on stage 1, c54 = (0.002, 0.002, 0.002, a value growing 0.0005 a draw), pixel c2 = (sin g × 30 + 70, cos g × 40 + 60, 0, 0), g growing 0.0348889 a draw (statics `0x007A0B3C`, `0x0079E88C`); without it stage 1 emptied; the noise on stage 2, the normal cube (`+0x134`) on stage 3, c52 = (0.5, 0.5, 1, 1), pixel c0 = (99, 99, wetness × side, wetness × front) (renderer `+0x178`), c53 = `+0x38` ..; setup `0x0052C3D0`: the pass, the default cull, fog on |
| Sea (48) | vtable `0x006F96E8` | 0 .. 3 U/V-Scroll1, U/V-Scroll2, 4, 5 U/V-Tile (0 → 1) | SeaShadow (`+0x20`) / SeaNoShadow (`+0x24`) / SeaShadowNight (`+0x28`), chosen as Standard Opaque's | `0x0052E790`: the texture on stages 0 and 1, the light tile on stage 3, the pass, the half-vector cube on stage 2, the renderer's `+0x118` on stage 4; pixel c0 = the ambient × 1/255 (its fourth value kept); c52, c53 = (time × scroll less its whole part cut toward zero, for both scrolls; 0, 0); pixel c1 = the zenith colour (renderer `+0x168`) × 1/511; c38 = the tiles (1, 1 after); setup `0x0052E750`: the default cull, the specular colour white with highlight 255 |
| Light Flare (17) | `0x00528980`, vtable `0x006F9174` | X-Cells, Y-Cells, FPS, Playlist, Z-Offset (`0x00528CD0`: the cells to the nearest, 0 as 1, their inverses `+0x24`, `+0x28`; FPS `+0x34`; the playlist as Anim Alpha-Add's; Z-Offset `+0x2C`; the flares' materials, made by the code, are given none); the scale `+0x24` .. `+0x30` (1, 1, 0, 1), FPS `+0x34` 0, cells `+0x38`, `+0x3C` 1 | LightFlare (`+0x20`) | `0x00528B10`: the pass, the texture (slot 0, `+0x1C`), c36 = the scale; the frame = the time x FPS cut toward zero, plus c40.x x the playlist's length (with one) or the cells' count, cut toward zero; c35 = (column x `+0x24`, row x `+0x28`, 0, 0), both through the state manager; setup `0x00528AE0`: stages 1 .. 3 emptied. The program: additive blending (SRCALPHA, ONE), no culling, fog or depth writes; the vertex shader takes the place through c0 .. c2, adds the corner (TEXCOORD1 x c3.y) along c7 and c8, moves it by -c36.z along c9 and projects it by c14 |
| AmbientLit (38) | vtable `0x006F93B8` | 0 Ambient 4x Mult → r, g, b × 1/64, a × 1/255 (`+0x24` ..; unset until then) | AmbientLit | `0x0052AD40`: the pass, the texture, vertex c52 = the four values on the device; no setup |

The sway of the trees (`0x0052A840`, 36 `rand()` values): per vector (four) three amplitudes low + (high − low) × r /
32767, three speeds low + r × (high − low, kept as a float) / 32767, three phases r × 2π / 32767 (`0x006FFAF0`). The
object makes it when it is made and again at each of the parameters 4 .. 9; a track's materials are made by the
loading thread (its `rand()`, its x87 at 53 bits).

The routine `0x005CA370` that the classes use to make integers is FISTP in the x87's mode: to nearest on the main
thread once the device is made.

The classes that no material of the game's files names (setups and constants on the program's state manager):

| class | object | parameters (index → meaning) | programs | drawing |
|---|---|---|---|---|
| Vertex Coloured (1), Vertex Coloured Unlit (25) | `0x0052B860`, `0x0052B770`, vtable `0x006F94AC`; program `+0x1C` | none; no texture slot | VertColoured / VertColouredUnlit | `0x0052B840`: the pass; setup `0x0052B920`: the default cull, fog on |
| 2-Texture Modulate (7), Vert-Col 2-Texture (23) | `0x00527B50` (vtable `0x006F9074`), `0x00529670` (`0x006F926C`); program `+0x34` | 0 X-Scale `+0x24`, 1 Y-Scale `+0x28` (1 until set; `+0x2C` 0, `+0x30` 1); textures 0, 1 at `+0x1C`, `+0x20` | 2TexModulate / VertCol2Tex | `0x00527CA0` / `0x005297A0`: the pass, (Vert-Col 2-Texture: the default cull,) the textures on stages 0 and 1, vertex c38 = `+0x24` ..; setup the default cull (`0x00527C80`) / fog on |
| 2-Texture Modulate Specular (8) | `0x0052EC00`, vtable `0x006F970C`; program `+0x38` | as 2-Texture Modulate; 2 Shininess `+0x34`, 3 Specular Colour `+0x3C` (0xFF000000), 4 Specular Highlight `+0x40` | 2TexModulateSpec | `0x0052ED60`: as 2-Texture Modulate, then c39.w the shininess on the device and the renderer's specular (`0x00515500`); setup the default cull |
| Building (10) | `0x0052D0E0`, vtable `0x006F95E4`; programs `+0x24`, `+0x28` | 0 Side Fresnel `+0x48`, 1 Front Fresnel `+0x38`, 2 Opacity `+0x34`, 3, 4 Fade Start/End × (`[0x0071E5CC]` (60) × 0.005 + 0.5) `+0x4C`, `+0x54` with 1 / (end − start) `+0x50`, 5, 6 Light-Tile-X/Y as 1 / the value (0 → 1) `+0x5C`, `+0x60`; textures Texture, Night-Light | Building / BuildingNight (the renderer's night) | `0x0052D2B0`: the texture; at night the night pass and the night light on stage 1, else the day pass and the environment (`+0x11C`) on stage 1; pixel c0, c1 = `+0x2C` .., vertex c52 = `+0x4C` .., c38 = `+0x5C` ..; setup `0x0052D260`: at night the opacity 0, the default cull |
| Grass (12), Vert-Col Opaque (20), Cloud Sprite (37) | `0x00529210`, `0x00529850`, `0x0052A730`; vtables `0x006F91E8`, `0x006F9290`, `0x006F9388` | none; the texture of slot 0 | Grass / VertColOpaque / CloudSprite | `0x0052AF30`: the pass, the texture; setup the default cull (`0x0052A530`) / the default cull and fog on (`0x00529940`) / CULLMODE none (`0x0052A820`) |
| Blurred Shadow (34) | `0x0052A590`, vtable `0x006F935C` | 0 Offset → `+0x24` and `+0x28` | BlurredShadow | `0x0052A680`: the pass, the texture on stages 0 .. 3, vertex c52 = `+0x24` ..; no setup |
| Fade To Const Colour (36) | vtable `0x006F930C` (`0x0052A180`) | 0 Colour → r, g, b, a × 1/255 (`+0x28` ..; 0, 0, 0, 1 at first; read by nothing found), 1 Additive (`+0x24`) | FadeToConstColour | `0x0052A280`: the pass, the texture, the default cull; additive: ALPHATESTENABLE off, blend ONE ONE, else ALPHATESTENABLE on, SRCALPHA INVSRCALPHA; no setup (only `heathaze01`, of the FRAMEBUFFER effect that is never drawn) |
| Shiny Unlit ATest (40) | `0x0052A440`, vtable `0x006F9338` | as Shiny Unlit Opaque | ShinyUnlitATest | as Shiny Unlit Opaque |
| Specular ATest (43), SpecMap ATest (45) | `0x0052E400` (vtable `0x006F96BC`), `0x0052F570` (`0x006F979C`) | 0 Shininess, 1 Specular Colour, 2 Specular Highlight, 3 Double-sided (no Z-Bias) | SpecATest / SpecMapATest | `0x0052E500`: the pass first, then as Specular Opaque; `0x0052F6C0`: as SpecMap Opaque; setup fog on |

The line classes Line Textured Opaque (50), Line Textured ABlend (51) and Line Vert-Col ABlend (52) have no objects of
their own: their creators (`0x00722FA0` .. `0x00722FA8`) make an Unlit Opaque, an Unlit ABlend and a Vertex Coloured
Unlit, objects of those classes' ids.

### The half-vector cube (renderer `+0x130`)

Made at the renderer's start (`0x00513807`, `0x005C9270` with the texel function of vtable `0x006F77F0`,
`0x005126F0`): a cube of the settings' `+0x10` texels a side (64 << `[0x0071E5DC]` = 128), one level, R5G6B5 unless
the display format (renderer `+0x22C`) is A8R8G8B8, managed; linear filters and clamped addresses. A texel of face f,
row r, column c: t = 2 r / n − 1, s = 2 c / n − 1 (each step a float); the direction is +X (1, −t, −s), −X (−1, −t, s),
+Y (s, 1, t), −Y (s, −1, −t), +Z (s, −t, 1), −Z (−s, −t, −1), normalised as v / (|v| + 2⁻²³) with |v|² added z, y,
x. The texel is that direction minus (0, 0, 1), normalised the same way (y, x, z added), each part × 127 + 127
rounded to a byte: red x, green y, blue z, alpha 1; 16-bit texels keep the top 5, 6, 5 bits. The renderer's normal
cube `+0x134` (`0x0051383B`, texel function `0x005127C0` of vtable `0x006F7798`) takes the direction normalised again
(z, y, x added), each part × 127 + 127 cut toward zero.

## Material classes of the cars

| class | object | parameters (index → meaning) | programs | drawing |
|---|---|---|---|---|
| Car Body (0) | vtable `0x006F94D8` | 0 Car Colour (r, g, b, a × 1/255, `+0x2C`), 1 Side Fresnel `+0x68`, 2 Front Fresnel `+0x58`, 3 Metallic Colour `+0x3C`, 4 Pearlescent Colour `+0x6C`, 5 Metallic Shininess `+0x7C`, 6 Pearlescent Shininess `+0x78`, 7 Metallic `+0x80`, 8 Double-sided `+0x84`, 9 Z-Bias (kept); with Metallic `+0x4C` = the metallic colour and `+0x5C` = the car colour × 0.8, without 0 and the car colour | CarBody_Paint_NoReflect (`+0x1C`: the paint work sampled, the scratches the scratch texture's alpha × the vertex colour's alpha) with a paint work texture, else CarBodyBase (`+0x20`: the scratch texture's alpha alone) (the constructor's lookups at `0x0052BA06`, `0x0052BA28`; `0x0052BAF0`) | `0x0052BB00`: the renderer's `+0x118` on stage 2, the scratch & reflect texture on stage 1, c55 = 0, the paint work on stage 0, the pass, pixel c1, c2 = `+0x4C` .. `+0x68`, c52, c53 = `+0x5C` .. `+0x78`, c39.w = the metallic shininess (x, y, z stack values), ZENABLE 1, the cull; no setup |
| Glass (9) | vtable `0x006F9608` | 0 Side Fresnel `+0x40`, 1 Front Fresnel `+0x30`, 2 Z-Bias (kept) | Glass | as Shiny Opaque (`0x0052D5A0`); setup: the default cull |
| Coloured Spec Opaque (27) | vtable `0x006F9894` | 0 Colour (r, g, b, a × 1/255), 1 Specular Colour (0xFF000000), 2 Specular Highlight, 3 Shininess, 4 Z-Bias (kept) | ColouredSpecOpaque (`+0x1C`) | `0x005304E0`: the pass, the texture, pixel c0 = the colour (a fixed-function technique: TEXTUREFACTOR = the colour × 255 rounded), c39.w = the shininess, the specular colour and highlight; setup `0x0052C7B0`: the default cull |
| Coloured Spec ABlend (28) | vtable `0x006F954C` | as above, 4 Additive | ColouredSpecABlend (`+0x1C`), with Additive ColouredSpecABlendAdditive (`+0x20`) | `0x0052C7E0`: not additive: the pass, pixel c0 = the colour, TEXTUREFACTOR, the texture; additive: the pass, the texture, pixel c0 on the device and stage 0's sampler states linear, linear, point, wrap, wrap (fixed function: TEXTUREFACTOR and the texture on stages 0 .. 3); then c39.w and the specular as above |
| Car Glass (49) | vtable `0x006F984C` | 0 Side Fresnel `+0x44` (1), 1 Front Fresnel `+0x34` (1), 2 Base Opacity `+0x54`, 3 Start Fade `+0x48`, 4 End Fade `+0x50` (`+0x4C` = 1 / (end − start)), 5 Colour `+0x28` (0.5 grey at first); textures: slot 0, slot 1 the scratch | CarGlass (`+0x24`) | `0x005300D0`: the pass, the texture on stage 0, the scratch on stage 2, the renderer's `+0x118` on stage 1, pixel c0 = (colour, front), c1 = (0, 0, 0, side), c52 = `+0x48` .. `+0x54`; setup `0x005300B0`: the default cull |

## The 3D element in the frame

The element ends the open view (`0x00516D20`: the interface's states — ALPHAREF 0x7F, ALPHAFUNC GREATEREQUAL,
FOG off, alpha blending of source alpha, no shaders, stage 0 the texture's colour and alpha, stages 1 and 2 off —
and `EndScene`), draws its view (`0x00516880` begins it: `BeginScene`, the viewport of the element, a clear) and begins the next. A
view whose flags have bit 0 (the 3D element's: 1, 5 or 0x11, with 2 added, `0x00512130`) clears with the flags
`[0x00719360]` (2, the depth, and 4 with a stencil, `0x00512E66`): the colour is not cleared, the interface stays
under the element's scene; the view's colour `0x88888888` (`0x005B3978`) is not used.

The race's view (made for the player by `0x0041DE30` as the race track is made) takes its clear colour (`+0x0C`) once:
the atmosphere's SkyColour (`0x005342C0`: `+0x90`, mixed with the next atmosphere's by the set's blend while one
comes, `0x00531B60`), `0xFFAAC4DD` without an atmosphere. Nothing sets it again during the race.

## The post effects

The renderer's start makes, when the settings give the screen's size (`+0x34`, `+0x38`; 0 without shaders), two scene
targets (`0x005146D6`, `0x00514745`: `0x0051F2A0` with the format flags 7, each with its own depth buffer): `+0x1F8`
the frame's, `+0x1FC` the previous frame's. Their textures sample with linear filters and clamped addresses
(`0x005C9730`). The effects' object (`+0x224`, `0x0051FA00`) holds two vertex buffers (`+0x00`, `+0x04`), the sampler
sets `+0x08` (linear, linear, point, clamped) and `+0x0C` (the same, wrapped), the blur targets `+0x14` (settings
`+0x24` × `+0x28`) and `+0x1C` (settings `+0x1C` × `+0x20`), both cleared to `0x00FFFFFF` with no depth buffer, and the
exposure's program (`+0x20`). The exposure's measure (`[0x00722A88]`, `0x00512210`) is a lockable A8R8G8B8 target of
128 × 64 (`+0x04`) and the pixel shader at `0x00719324` (ps_1_1 `dp3 r0, c0, t0`, c0 (0.3, 0.59, 0.11, 0.2)). The 2D
draws use the declaration `+0x1E8` (place float2, colour, u v float2; 20 bytes).

A 3D view whose flags have bit 1 and not bit 0 (the race's: 0x0E) begins (`0x00516880`) by asking for the effects
when there is a frame's target and either: shaders, depth of field in the settings (`+0x5C`) and `-rf` or the
camera's `+0x22C`; the camera's motion blur (`+0x224` or `+0x228` not 0) with the setting `+0x54` (`0x005A3370`, with
shaders); the view's glow (`+0x20`, 1 but for a split screen race) with shaders and the setting `+0x58`; or the
director's transition switch (`[0x0075ABC8]`). Asked, the renderer's `+0x200` is set (`0x00515B70`) and the exposure's
program is loaded (`0x00521170`). The scene then goes into the frame's target (viewport its size) or the view's own;
the clear by the view's flags; c22 = (2 / width, −2 / height, 0, 0) and c23 = (−1, 1, 0, 1) of the view (kept at
`[0x0079BCF0]`, `[0x0079BCE0]`).

The view's end (`0x00516D20`) takes the target again with the view's viewport and the depth test off; with `+0x200`:

1. The motion blur (`0x0051FD50`), when the camera has some and the setting is on: in the frame's target, the
   previous frame's texture with MotionBlur (class 0x10003, `Material_MotionBlur` on stages 0 .. 3), sampler `+0x08`,
   colour writes RGB only, a triangle fan of 10 vertices: the middle, the corners and the edges' middles of the view.
   The middle's alpha is the camera's `+0x224` × 255, the corners' the edge value `+0x228` × 1.5 within 0.05 .. 0.99,
   × 255, the edges' middles (edge + middle) × 128; u v the place over the view's far corner, drawn 0.985 toward the
   middle.
2. The depth of field (`0x005214C0`), when asked as above: the previous frame blurred (`0x00520EF0`: 2DBlur, class
   0x10005, value −2: the source's alpha added; the engine's c52 .. c55 0) into the blur target `+0x1C` (no blending,
   no depth test), then that target blurred over the frame's target with the source's alpha over its inverse, colour
   writes RGB only, the depth test on and depth writes off, as quads of transformed vertices (`0x00520FC0`: FVF
   0x444, the second buffer, four u v sets offset by 1 / 256) at depths around the camera's point (its `+0x250`
   through the world-to-clip matrix, w 1: depth = z / w + k / w within 0 .. 1): k 10 with alpha 64 (the test as it
   is), then with GREATER k 4.5 (31), −12 (31) and −20 (102); LESSEQUAL again, the stages emptied, the target taken
   last forgotten.
3. The copy (`0x005211B0`): stages 0 .. 3 sampler `+0x08`; the frame's target into the back buffer (`0x00520E30`,
   `0x00520AB0`: TexShape2D, class 0x10002, opaque) as a quad of the view's size. With the measure and the glow
   (the view's `+0x20`, shaders and the setting): the pixel constants c0 = exposure × 0.5 (four times), c1 = 0 and the
   exposure's program over the quad, placed at (−0.5 / width, −0.5 / height); without the glow the exposure 1 and its
   speed 0. With the measure: its target taken straight on the device (no depth buffer), the frame's middle (u, v
   0.2 .. 0.8) drawn over it through the luminance shader (`0x00520900`), and the measure read (`0x00512290`): a
   histogram of its first bytes over the 256 levels, normalised; the low end the level where the sum from 0 reaches
   0.02, the high end where the sum from 255 does, each ÷ 255; the mean Σ level × share ÷ 255; the speed (`+0x424`)
   0.5 × speed + 0.5 × (0.8 − (high × 0.7 + mean × 0.3) × exposure) within −0.02 .. 0.02; the exposure (`+0x420`)
   grows by it within 0.8 .. 2, the speed 0 at a limit it pushes against. Stages 0 .. 3 sampler `+0x0C`.
4. The glow (`0x00521C80`), with the glow on: stages 0 .. 3 sampler `+0x08`; the back buffer taken; after the view's
   first frame (its `+0x18`) the blur target `+0x14` blurred with 2DBlur over the view, colour 0x9F9F9F9F, the engine's
   c52 .. c55 (±0.0052734, ±0.0017578), pixel c0 .. c3 (0.25 and 0.23), added by its alpha with no depth test. Then
   the previous frame into the blur target `+0x14` with TexShape2D, alpha LightBlur × 204; with a blend factor
   (SrcBlendCaps) the source's alpha over the factor 0.55 (0 in the first frame), else no blending. Stages 0 .. 3
   sampler `+0x0C`.

Then, with the director's amount (`[0x0075ABC4]`) and a device of more than the fixed function: the cut's transition
(`0x00520410`): into the back buffer the previous frame with MotionBlur (sampler `+0x08` on stages 0 .. 3) as the same
fan, u v the place over the view's far corner; alphas the middle and the corners (1 − amount) × 255, the edges'
middles amount × 0.7071 × 255 + amount × 0.2929. The view's `+0x18` is cleared; with the camera's `+0x234` the
effects manager's SMEAR emitters are drawn (`0x005224E0`: the back buffer taken, `[0x0075F0C8]` vtable `+0x04` with
the view's corners and the camera; `20-effects.md`). With
`+0x200` it is cleared (the target taken last forgotten) and the two scene targets change places. The interface's
states follow (`The 3D element in the frame`).

The targets are taken through `[0x0075F168]` (`0x0051F660`): one taken again is not set, setting one empties the
texture stages 0 .. 3 and sets its colour surface and depth buffer (none for the blur targets).

## The reflection targets

The renderer's start makes three more targets: the cars' cube (`+0x218`, `0x005C91D0`: a cube render target of the
settings' `+0x10` a side), the wet road's reflection (`+0x214`, `0x0051F2A0` of `+0x2C` × `+0x30`, format flags 7,
with a depth buffer) and the world's cube (`+0x21C`, `+0x14` a side), then sets `+0x220`: the world's cube is due. A
reset of the device makes them again (`0x00514216` .. `0x00514248`) and sets `+0x220` again. A cube target
(`0x005C8F00`, `0x005C8D10`): its cube texture (one level, RENDERTARGET, the renderer's format `+0x22C`), a depth
buffer of its side (`+0x18`, the clear flags its `+0x10`), the six faces' surfaces (`+0x1C` ..) and six cameras
(`+0x34` ..: aspect 1, a field of view of a quarter turn across and up, on a wide screen 1.1780972 across that the wide
screen's factor makes a quarter turn; turned about y by −π/2 and π/2, about x by π/2 and −π/2, none, and about y by π,
looking along +x, −x, +y, −y, +z, −z), sampled with linear filters and clamped addresses. Setting a face
(`0x005C9120`) empties the texture stages 0 .. 3, lets go the target taken last (`[0x0075F168]` becomes −1) and sets
the face's surface and the depth buffer straight on the device.

The race's view (`0x00511CA0`) first has the world of its camera (the camera's `+0xC4`, `0x00552A00`) draw its slot
14 (the cars' cube, `15-tracks.md`) with the camera, then begins (`0x00516880`), takes the camera's constants,
ZENABLE and FOGENABLE on, the world's main pass (slot 13), both off, and ends. While a world's cube is drawn
`[0x0075F1B4]` is set (Tarmac leaves out its half-vector cube) and the cube is drawn in its own BeginScene and
EndScene.

After the wet road's reflection the main pass takes the view's target again (`0x00517390`, with the view `[0x0075F110]`
open): with the effects (`+0x200`) the frame's target and a viewport of its whole size, else the view's own target
and its viewport.

## The 3D interface element

`Begin3d` (`0x005AB300`) makes a view object of 0x1B4 bytes (`0x005B36D0`, vtable `0x006FE610`, kind 5): a viewport
over the element's box, lights (`+0xC4`: ambient (50, 50, 50), directional colour (255, 255, 255) and direction
(0, -1, 1)), a camera (`+0xC8`: field of view 0.35 π, far 10 000 000, aspect value 0.75, or 0.5625 wide screen),
and the scene (`+0xCC`). `CameraPosition`, `CameraLookAt`, `Ambient`, `Directional`, `SetModel`, `Atmosphere`,
`MoveMode` set them; `MoveMode` takes MODEL_AUTO, MODEL_MANUAL, CAMERA_AUTO, CAMERA_MANUAL. Drawing (`0x005B4870`)
moves the camera by the mode, then draws the view (`0x00511CA0`). The rig's defaults: limits of yaw none, of pitch
−0.628 .. −0.1, auto speed −0.1, no input delay.

## The career's map (DemoFlyby)

The map object (`0x0045B6B0`) takes the `MAP3D` view: camera near 3999 (`0x4579F000`), far 5 000 000, `0x005B4660`, pitch limits
−1.5 .. −0.15, the atmosphere `map` (`0x005B4610`). The map goes to a city place (`0x0045BE00`: place 4, pitch
−0.4): look-at the place's point, then the map mode with the arm at (0, 0, −240 000) and the rig angles (pitch, the
yaw the rig had, 0). DemoFlyby then sets the auto speed to 0.1 and the input delay to 100 s.

The city places (40-byte records at `0x00767E50`, filled by the start-up code `0x00462310`): a name (`+0x10`), its
picture (`+0x14`) and its point (`+0x18` .. `+0x20`); eight of them, place 4 the Engstrom District.
