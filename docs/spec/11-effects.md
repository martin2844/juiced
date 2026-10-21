# Effects and material programs

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## Files

`effects\material_<program>.fxo`: 74 compiled effects (fx_2_0 binaries of the Direct3D extension library, which the
game links statically; the executable imports `d3d9.dll` only). Beside them the textures the effects name
(`cloudcast.dds`, `noise2_64_dxt3.dds`, `st_sinus.dds`, ...) and the procedural textures
`proc_<program>_<function>.dds`.

## The fx_2_0 binary

```
u32 tag 0xFEFF0901, u32 start          the rest of the file is the data; offsets below are into the data
at start: u32 parameters, u32 techniques, u32 (unused), u32 objects
parameters: u32 type offset, u32 value offset, u32 flags, u32 annotation count, annotations
techniques: u32 name offset, u32 annotation count, u32 pass count, annotations,
            passes: u32 name offset, u32 annotation count, u32 state count, annotations,
                    states: u32 operation, u32 index, u32 type offset, u32 value offset
annotation: u32 type offset, u32 value offset
u32 string count, u32 resource count
strings: u32 object id, u32 size, data (padded to 4)
resources: u32 technique (-1 for a parameter), u32 pass or parameter, u32 element, u32 state, u32 usage,
           u32 size, data (padded to 4)
```

The data ends exactly after the last resource.

- **Type** (at its offset): type, class, name offset, semantic offset, element count; then columns and rows for a
  vector, rows and columns for a scalar or matrix, the member count for a struct (the members' types follow). A name
  is a size (with the zero) and the characters. An array's element type is the same record read again per element.
- **Value** (at its offset): a number's values (4 bytes each, row after row); for a string, texture or shader its
  object id; for a sampler a state count and its states. A struct or array is its members' values in order.
- **Strings** give the objects of string and shader parameters (a shader parameter's object is its byte code).
- **Resources** give the data of a state: usage 0 with a shader value — the shader's byte code (the state's object);
  usage 0 with a number value — an expression that computes the value; usage 1 — the name of the parameter the state
  takes its value from (a pass's `VertexShader = VS`, a sampler's `Texture = cloudCastTexture`). For a parameter's
  sampler state the element counts only for an array.

The state operations are a table of 179 entries: 103 render states, 18 texture stage states, NPatchMode, FVF, 4
transforms, 5 material and 13 light members, LightEnable, VertexShader, PixelShader, 16 shader constant forms,
Texture, 13 sampler states and Sampler (`src/Core/Formats/Effect.cpp`). A pass applies its states in the file's order.

### Constant tables and expressions

Shader byte code starts with its version word and comment blocks (`0xFFFE | size << 16`, then a tag). `CTAB` lists the
shader's constants: name, register set (bool, int4, float4, sampler), first register, register count and type.
`PRES` holds the shader's preshader (empty in most shaders of the game).

An expression (a state's, or a preshader) is a program: a version word `0x4658xxxx` and comment blocks — `CLIT`
literals (doubles), `CTAB` its inputs (parameters by register), `FXLC` the instructions. An instruction is a word
(opcode `<< 20`, component count, bit 31 "the first input is one value for all components"), the input count, then
the operands (a relative flag, a table and an offset in components) with the output last. Tables: 1 literals, 2
inputs, 4 float outputs (the shader constants or the state's value), 5 bool outputs, 6 int outputs, 7 temporaries.
The game's expressions are moves and multiplications: the material colours 1.0 of the sky's fixed-function pass,
`c0 = vAmbUnit` (`× 0.5` in the night variants) of the 1.x passes of the opaque world materials, constants of
`tarmacnoshadow` (`cNoiseyVolume × 0.4`, `× 0.5`, `cNoiseScaler1 / 70`) and of `sceneradialblur`.

## Loading a material program (`0x0051B7D0`)

A program is named `Material_<Name>` (`Material_StdOpaqueShad`, ...): the file is `effects\` + the name + `.fxo`
(a developer flag reads `\src\jet\jetMaterials\dx9\fx` instead). The effect is created from the file's bytes with
flags `0x100` and the programs' shared parameter pool, then given the game's state manager. The program keeps the
effect's description, one record per parameter (`0x58` bytes) and one per technique (`0x34` bytes), and picks its
technique.

### Techniques (`0x0051A460`, `0x0051A0B0`)

Annotations read: `CLASSIFICATION` (an int: the shader classes the technique is for), `USERANDOM`, `USEFFV`,
`USECONSTANTMEM`, `CONDITION` (a test of the adapter; false makes the technique unusable). Shader classes of the
device: `0x10000` fixed function; `0x30000` pixel shaders 1.x and the Shaders setting ≥ 1; `0x70000` 2.0 and ≥ 2;
`0xF0000` 3.0 vertex and pixel shaders and the setting 3. Bits of `CLASSIFICATION`: `0x10000` fixed function,
`0x20000` 1.x, `0x40000` 2.x, `0x80000` 3.x.

A technique is usable when `CLASSIFICATION & classes & 0xFFFF0000` is not 0 and:
- a shader technique: the device validates it; one that fails is still taken when it is a 1.x technique (no
  `0x40000` / `0x80000` bit), a 2.x/3.x one is not;
- a fixed-function technique: only when the classes are exactly `0x10000`; validated (with software vertex
  processing switched on around the check unless `USEFFV` is set) and taken even when the check fails.

The program uses the **last** usable technique of the file. The technique's record also lists the bool, int and
float parameters it uses (`IsParameterUsed`).

### The effects' manager and the technique by detail (`0x00519D00`, `0x00519E80`, `0x00519700`)

The material manager's start (`0x005226E0`) makes the effects' manager (`[0x0075B294]`), which makes the programs of
the list `0x00519840` in its order (73 names, among them `Material_2TexModulare`/`...Spec` and
`Material_SceneColorMatrix`; every one of the game's files has a usable technique for any classes: each has a
`0x20000` and a `0x10000` technique). A material finds its program there by name, case aside (`0x00519CB0`), and
keeps it when it has a usable technique at that time.

A technique by level (`0x00519FF0`; the masks `0x00719364`: level 0 `0x10000`, 1 `0x20000`, 2 `0x40000`, 3
`0x80000`): the open pass ended, then from the last technique back the first usable one whose `CLASSIFICATION` has
the mask (written to the debugger: `(a)%s %s %x %x`, the program, the technique, its classification and usable flag),
else the last usable one.

Each frame of the renderer (`0x00514A40`), after every program's open pass is ended (`0x00519F70`), `0x00519E80`:
with the classes `0x70000` and up, the detail (`[0x0079E650]`, the graphics options' slider, live while it moves)
over 0.1 or not against the last frame's (`[0x00725784]`, 1 at the start; a debug toggle `[0x0079E884]` flipping
every 32 frames is never set): crossing to over 0.1 every program takes level 3 on a 3.0 device (`0xF0000`) else 2,
crossing to 0.1 or under level 1 (the 1.x techniques). Until a crossing or a reset the programs keep the last usable
technique. The manager's request (`+0x20`, `0x1000`) is never changed: `0x1001` would make every program again,
another value would pick at that value as a level.

After each reset of the device (`0x00514090` → `0x00519700`) every program is made again from its effect
(`0x0051B620`: the effect created anew from the file's bytes, its values the file's, the last usable technique),
again and again until it has a usable technique; then the shared parameters' handles (`vOne`, `vAmbUnit`) are taken
again, and with the classes `0x70000` and up the level is picked: 1 with the detail 0.1 or under, else 3 on a 3.0
device, else 2 (`[0x00725784]` left as it is).

The effects' debug prints (`%s pass %d`, `%s begin`, `%s BeginPass %d`, `%s end`, `%s setvalue %s`,
`SetVertexShader %x`, `SetVertexShaderConstantF r %d count %d`, `ignoring block const set`) are under `[0x0075F12C]`,
which nothing sets.

### Parameters (`0x0051AAC0`)

- A semantic of the table `0x00719490` binds the parameter to engine constants: `worldViewProjection` (c14, 4
  registers), `world`, `viewprojection` (c18, 4), `ambient` (c25), `position` (c32), `C_LIGHT1` .. `C_LIGHT3` (c26,
  c28, c30; 2 registers each).
- A float scalar keeps its value; annotations `UIWidget = "slider"`, `UIMin`, `UIMax` (default 1.0), `UIStep`
  (default 0.1) make it a slider.
- A texture parameter (types 5 .. 9) with annotations (one without any gets no texture) reads them in order, the
  names case aside: `name` / `resourcename` (its string; the last one counts, one that is not a string gives none),
  `TextureType` (`volume` 8, `cube` 9, `texture` 7, the default; another word prints `unknown procedural texture type
  in fx %s` and stops the game at a breakpoint, `0x0059AF30`), `function` (a string), `width`, `height` (16),
  `miplevels`, `depth` (1) (ints; one that is not an int gives 0), `format` (read, unused). With a name, the file of
  the effects' folder (`0x0051A930`: `D3DXCreateTextureFromFileInMemory`; a file that does not open makes a
  16 × 16 A8R8G8B8 texture, its full chain, never written, and prints `missing texture %s`); without one,
  `proc_<program>_<function>.dds` (`0x0051B2C0`: the `function` annotation, not the parameter's name;
  `Material_TarmacNight`'s `camberTex` is `proc_material_tarmacnight_camber_function.dds`; a texture without a
  `function` copies its name from address 0, a crash). The texture is set on the parameter. The textures are made
  again with the program after a reset of the device.

### The developers' sources (`-localfx`, `-debugfx`)

With `-localfx` (`[0x0075F134]`; `-debugfx` sets it and `[0x0075F138]`) the effects' folder (`0x00519660`) is
`\src\jet\jetMaterials\dx9\fx` (`0x00719378`) mounted under the file system's root `""` (`0x006EE8D8`): the current
drive's root, with a `fx.dat` beside it read as an archive. A program is made from `<name>.fx` (`0x006F7970`;
`.fxo` otherwise, `0x006F7968`): `D3DXCreateEffect` takes the source, compiles it with the flags `0x100`
(`D3DXSHADER_NO_PRESHADER`; `0x141` with `-debugfx`: `DEBUG` and `FORCE_VS_SOFTWARE_NOOPT` as well; the flags are
also given for a `.fxo`, which they do not change) and the include object (vtable `0x006F792C`: Open `0x0051B0D0`
reads the include from `\src\jet\jetMaterials\dx9\fx`, `-localfx` or not, without checking that it opened; Close
`0x0051B1F0` lets go of the file, or prints `unable to resolve a Close of include file` and gives 1); a source that
does not compile leaves the program without an effect. A texture parameter without a name is then made
(`0x0051B400`): the effect's function compiled as a texture shader (`D3DXCompileShader`, `tx_1_0`, flags 0, the
include object; one that does not compile is made from address 0, a crash), a texture made for it (`TextureType` 7:
`D3DXCreateTexture` of width × height with the height as its mip levels; 8: `D3DXCreateVolumeTexture` with depth
and miplevels; 9: `D3DXCreateCubeTexture` of the width with miplevels; A8R8G8B8, managed) and filled by it
(`D3DXFillTextureTX` and its volume and cube forms, `0x005DB7C6`: each texel's position, `(x + 0.5) / w ..`, a cube's
the direction through the texel on its face, in the shader's input register 0, its size in register 1, the shader's
constants at their defaults, the colour x, y, z, w as r, g, b, a), set on the parameter and saved as DDS at
`\src\jet\jetMaterials\dx9\fx\proc_<program>_<function>.dds` (a volume is given to `D3DXSaveVolumeToFile` as its
texture). Without `-localfx` that function (never called so) prints `procedural texture generation expected only
with -l` and compiles the function through an effect compiler of the file.

The folder exists only on the developers' machines. OpenJuice compiles the sources with `d3dcompiler_47.dll`'s
`D3DCompile` (loaded when the game starts the effects; the `fx_2_0` and `tx_1_0` profiles still compile, to the same
layouts as D3DX 9), runs a texture shader on its own expression machine and converts its colours clamped to 0 .. 1,
× 255, rounded (D3DX's converter is not followed further); a volume texture is made and saved but not set (the
device has no volume textures).

### Shared parameters

`vAmbUnit` and `vOne` are shared through the pool (flag 1 in 12 effects): `0x00519DD0` sets `vAmbUnit` to the
ambient colour × 1/255 and `vOne` to (1, 1, 1, 1) on the first program's effect, which sets them for all.

## Material classes

The registry (`0x00525940`) holds the 53 classes of the materials (ids 0 .. 52, the names `.pmd` files use) and 8
internal ones (`0x10000` Font, `0x10001` Line, `0x10002` TexShape2D, `0x10003` MotionBlur, `0x10004` Antialias,
`0x10005` 2DBlur, `0x10006` AntialiasZBuf, `0x10007` HeatHaze) at index `53 + id - 0x10000`. Per class a record — id,
name, the parameters (index, type, name), the texture slots (slot, name), a value and flags — and a creator object
whose second method makes the material object (Standard Opaque: `0x00526150`, 0x3C bytes).

The 3D map of the city menus (`models\angel_city.boh`) uses Standard Opaque, Specular Opaque, Building Opaque,
SpecMap Opaque, Standard Alpha Test, Sprite (Unlit) and Sprite (Lit).
