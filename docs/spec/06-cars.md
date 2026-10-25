# Car models of the stock game

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## Tables in the executable

The game relies on name tables built into the executable:

| table | address | entries | meaning |
|---|---|---|---|
| car models | `0x007167B8` | 53 x 64 bytes | the internal name of each car id, `NONE` first (`celica`, `civicR`, ...); the saves keep their per-car tables in this order |
| manufacturers | `0x007162F8` | 18 x 64 | `NONE`, `HONDA`, `TOYOTA`, ... |
| regions | `0x00716178` | 64-byte names | `NONE`, `JAPAN`, `USA`, `EUROPE`, `AUSTRALIA` |
| suppliers | `0x00717970` | 50 x 68 | a name and its id (the index): `BILSTEIN` 0 .. `SWEET` 49 |

OpenJuice reads them from the `Juiced.exe` of the game folder, found by their first entries.

## `scripts\cardefs.txt`

A text script (`05-scripts.md`) read by `0x0048C650` into the car definitions (`[game + 0x54]`). Keywords:

| keyword | handler | effect |
|---|---|---|
| `[BeginCar] <name> "<display name>"` | `0x00499370` | a new car (0x4D4 bytes, `0x0049AB10`): id = the index of the name in the car model table (without regard to case; 11 when it is not there), `+0x1C` name, `+0x5C` display name, `+0x9C` `<name>_STOCK` (the model name the saves keep), `+0x19C` `<name>_PRIMER`, the drop height 6.0, a first "neon" part in list 9 |
| `MANUFACTURER <make> <region>` | `0x00499430` | `+0x04` make, `+0x08` region: their index in the tables, else 0 |
| `VALUE <n>` | `0x00499460` | `+0x0C` (atol) |
| `STATE <word>` | `0x004994B0` | `+0x10`: 0 for `NEW`, else 1 |
| `STOCKTURBO` | `0x00499500` | `+0x14` = 1 |
| `STANDARD <category> <part>` | `0x00499510` | a part "Standard", brand -1, marked standard |
| `CUSTOM <category> <part> [<supplier>]` | `0x004995F0` | a part "Option" with the supplier's id (-1 if unknown); without a supplier `rand() % 3` = 2 gives 48 (NUUK), any other 47 (AERO) |
| `ALLOW_NONE <category>` | `0x00499700` | the category may be left empty (`+0x3F0 + category x 0x18`) |
| `STANDARDWHEELS <name>` | `0x00499590` | `+0xDC` |
| `STANDARDEXHAUST <name>` | `0x004995C0` | `+0x11C` |
| `ENGINE_SOUND <name>` | `0x00499760` | `+0x1DC` |
| `MAXDROPHEIGHT <f>` | `0x00499790` | `+0x18` |
| `[EndCar]` | `0x004997E0` | the car joins the list (the file's order) |
| `[BeginColour]`, `BaseColour r g b`, `MetallicColour r g b f`, `PearlescentColour r g b f`, `[EndColour]` | `0x00499840` .. `0x00499B20` | a paint (0x14 bytes): base `+0`, metallic `+4`, pearl `+8` (BaseColour also sets the pearl colour), metallic amount `+0x0C` (0.5), pearl amount `+0x10` (0.1; 0.05 after BaseColour) |
| `[DialColour] r g b r g b`, `[NeonColour] r g b`, `[DecalColour] r g b`, `[WheelColour] r g b` | `0x00499B70` .. `0x00499D80` | colour lists |

Colours are read as numbers and cut toward zero to bytes (the game runs with the x87 rounding toward zero), with
alpha 255.

Categories (`0x00499290`): `FBUMP` 0, `RBUMP` 1, `BONNET` 2, `SPOILER` 3, `SKIRT` 4, `HEADLIGHTS` 5, `REARLIGHTS` 6,
`WMIRROR` 7, `DECAL` 8; others are ignored. List 9 holds the neon. A category's list is at `+0x3E0 + category x
0x18`; a part (0x2C bytes) has its label `+0x00`, its index in the list `+0x24`, the standard flag `+0x2A`. Adding a
part (`0x0049A8D0`) gives it the next index; when it is the list's first part and its name is not `NONE`, the list
takes the part's supplier (`+0x14`).

## The parts catalogue

`0x00498BC0` makes ten lists (0x18 bytes each) and reads `gearboxes.txt`, `suspensions.txt`, `exhausts.txt`,
`wheels.txt`, `nitros.txt`, `turbos.txt`, `brakes.txt`, `tyres.txt`, `ice.txt`, `induction.txt` in that order
(`0x00498D20` each). Keywords:

| keyword | handler | effect |
|---|---|---|
| `[BeginMod] <kind>` | `0x00499F30` | the list: SUSPENSION 0, EXHAUST 1, NITRO 2, TURBO 3, WHEEL 4, BRAKE 5, TYRE 6, ICE 7, INDUCTION 8, GEARBOX 9 (`0x00499E40`; others -1, the part is dropped); clears the level, TEXT, the UI textures, PHYSICS and DATAVAL (SHAPE, SUPPLIER and COST carry over) |
| `LEVEL <word>` | `0x00499FB0` | prototype 3, street 0, sports 1, anything else 2 |
| `SUPPLIER <name>` | `0x0049A030` | the supplier's id, -1 if unknown |
| `TEXT "<text>"`, `SHAPE <name>`, `UIONTEX <name>`, `UIOFFTEX <name>` | `0x0049A060` .. | the label, the mesh, the shop pictures |
| `COST <n>` | `0x0049A0C0` | atol |
| `PHYSICS a b c d` | `0x0049A110` | four numbers |
| `DATAVAL <f>` | `0x0049A220` | |
| `[EndMod]` | `0x0049A270` | the part joins its list (`0x0049A8D0`): cost `+0x20`, DATAVAL `+0x1C`, level `+0x2B`, PHYSICS x 255 cut to bytes `+0x26 .. +0x29` |

## The car object

0xBC bytes, made by `0x0048DB40`:

| offset | default | meaning |
|---|---|---|
| +0x00 | 0 | the CarDef |
| +0x04 | 0xFF x 10 | the body part of each category (an index in the CarDef's list) |
| +0x0E | 0xFF x 10 | the catalogue part of each kind |
| +0x18, +0x1C, +0x20 | 0xFF8B8B8B, 0xFFC3C3C3, 0xFF8B8B8B | base, metallic, pearl colour |
| +0x24, +0x28 | 0.5, 0.1 | metallic and pearl amount |
| +0x2C | 0xFFFF0000 | neon colour |
| +0x30, +0x34, +0x38, +0x3C | 0xC82627C2, 0xFFB6B6FD, 0xFF808080, 0xFF808080 | colours |
| +0x40, +0x44 | 0, 0.0 | the wheels take the colour `+0x3C` (`0x0048C446`); the windows' tint |
| +0x48, +0x4C | 1, -1 | the sponsors' decals shown (`sponsordecalmenu`) and their colour (the decal paint) |
| +0x50, +0x54 | 0 | a metallic and a pearl paint bought (the paint windows), or a colour unlike the base |
| +0x58 | 0 | the lowering: the chassis sits lower by it x MAXDROPHEIGHT (the suspension's height slider; 1.0 for a prototype, else 0.8, when modified automatically) |
| +0x5C | 0.5 | gearing: the forward ratios x `1 + 2 (value - 0.5) x 0.1` (`07-dynamics.md`) |
| +0x60 | 1.0 | multiplies `MaxTorque` |
| +0x64 .. +0x70 | 0 | the race's damage kept until repaired (`0x00488B50`): the steering pulls, the turbo is lost, the neon flickers, the nitro leaks |
| +0x74, +0x78, +0x7C | 0 | the tyres' wear (`0x0048EF70`), the grip (the last tyre's LateralMu as the race made the car, `0x00485CC5`), the brakes' wear (`0x0048F1C0`) |
| +0x80 | 0 | the `BrakeMu` of the car's last wheel with its brakes, set with the rating |
| +0x84 | 0 | the nitro time |
| +0x88 | 0 | the performance rating (`0x0048E8A0`, `07-dynamics.md`) |
| +0x8C .. +0x94 | -1.0 | the last dyno run's best torque, power and speed (`car_rolling`, `14-career-windows.md`) |
| +0x98 .. +0xA6 | 0 | 16-bit counters of the career's races with the car (`0x004BA4F0`): sprint races and wins, circuit races and wins, showoffs, pink slip races, wins, races but showoffs (CMSStatsCarCollection's filters) |
| +0xA8, +0xAC | 0.0 | the money spent on the car and the money it won ("SPENT", "WINNINGS") |
| +0xB0 | 1 | the horn (`hornmenu`; the counter of the automatic modifications when modified so) |
| +0xB4 | null | the 16 x 16 grid object (vtable `0x006FA1D8`, made by `0x005562A0`) |
| +0xB8 | | an event handle |

The other fields up to +0xB0 are 0.

The save record (`0x0048DD40` writes, `0x0048E020` reads): the CarDef's 64-byte model name field as it is in
memory; u32 +0x18, +0x1C, +0x24, +0x20, +0x28, +0x2C, +0x30, +0x34, +0x38, +0x4C, +0x3C, +0x40; the 10 body parts;
the 10 catalogue parts; u32 +0x58, +0x5C; +0x44 (the windows' tint) as a 16-bit number of hundredths (x 100 to
the nearest, `0x005CA370`; read back x 0.01);
u16 +0x98 .. +0xA6; u32 +0xA8, +0xAC, +0xB0, +0x50, +0x54, +0x7C, +0x80, +0x74, +0x78, +0x84, +0x88, +0x8C, +0x90,
+0x94; u32 1 and the grid's 256 bytes when the grid exists, else u32 0; u32 +0x60, +0x64, +0x68, +0x6C, +0x70, +0x48.
A model name is found again by comparing it with the CarDefs' model names (`0x0048CC30`, without regard to case;
"_STOCK" is added to a name without it; the first CarDef when none matches).

## The car's model

The race builds a car's model (`0x00485530`, the model object `0x0048A0A0`, vtable `0x006F3BC0`) from
`cars\<car>\` (the interface from `cars\<car>_ui\`): the node tree `models\<car>_stock.boh`, meshes from the car's
`mesh\`, materials from the car's `mesh\mats\` (OpenJuice tries `carmesh\` and `mesh\` after them for a file missing
there; every shipped car has its meshes in its own folder), textures from the car's
`textures\`, then `cartextures\` (the cars' shared folders `cartextures`, `carmesh`, `carmodels`, `carmesh\mats`,
`0x00498610`), then `textures\`; the exhaust, a catalogue wheel and the driver from the shared folders alone (below).

The engine's managers read from one set of folders at a time: the game's (`textures`, `mesh`, `mesh\mats`,
`models`) or the cars' (`0x00498610`). `0x00498780` puts the cars' in use (the textures' manager `[0x0075EC94]+0x14`
slot 0, the meshes' `+0x1C`, the materials' `+0x10`), keeping the ones before (`[0x0075EF54]`, `[0x0075EF4C]`,
`[0x0075EF50]`), and `0x00498840` puts those back. Every load of cars has the cars' in use: the race's
(`0x00495DE0`), the garages' (`0x00436380`, `0x00438150`, the loader `0x00459730`, the jobs `0x0045A750`,
`0x0048B800`), the cut scene's (`0x004C6B80`) and the rolling road's (`0x004DD5DB`). So the car's `gen_shadow`,
`gen_neon` and `headlight_b` (by name, `0x00510190`) and the models `gen_shadow`, `gen_neon`, `arcade_shadow` with
their materials are the cars' folders': `textures\` has other images under the same names (a wider neon ring at the
box's edges, a wide cone of a beam, a fuller shadow).

- Slots: `FBUMP`, `RBUMP`, `BONNET`, `SPOILER`, `SSKIRT_L`, `SSKIRT_R`, `EXHAUST_C`, `EXHAUST_S`, `EXHAUST_O`, `DECAL`,
  `DRIVER` (the table `0x00717538`, 0x44 bytes each; the model object's `+0x30 ..`) and the wheels `WHEEL_FL` ..
  `WHEEL_RR` (`+0x10 ..`), found when the model is made (`0x0048A0A0`) as `0x00534EC0` finds a node: depth first below
  the top node, the first whose name contains the slot's (`strstr`, case sensitive), a slot only when it is a mesh
  node. The 206's exhaust node is `EXHAUST_S33`, the MR-S's decal node `MRS_DECAL`.
- The folders a part is read from: the model's nodes (`0x00485530`), the body material, the body parts (`0x0048C2E0`)
  and the standard wheels (`0x0048ABC0` with the wheel -1) with the car's own folders pushed (`0x0040D510`,
  `0x0040D5C0`: `<car>\mesh\` for both mesh kinds, its `mats\` and `textures\`; `0x0040D700` puts the ones before
  back); the exhaust, a catalogue wheel and the driver after that, from the cars' shared folders (`carmesh\`,
  `carmesh\mats\`, `cartextures\`), as are the garage's changes of the exhaust (`0x0048BE30`, the slot job
  `0x0048BA80`) and of a catalogue wheel. Most cars' own `mesh\` folders keep older exhausts under the shared ones'
  names (`exhaust_s8` of the NSX has both pipes in one mesh, 119 cm apart); they are never read.
- The body parts (`0x0048C2E0` as the model is made): per category the car's part (`+0x04`): 0xFF takes the first
  part when the list has one and its `+0x14` is not 0 (else none: the decals are CUSTOM parts alone); 0x7F a part
  without a mesh; another value a part of the list; each given to `0x0048BC30`, which writes it into the car's record
  (the model's `+0x8C`, `+0x04 + category`; a category of no parts and the neon passed over) and puts the part's mesh
  (`0x00447690`, the part's `+0x10`; none for -1, 0x7F or one past the list) in the category's slot, none emptying
  it: categories 0 .. 3 to the first four slots, the skirt to `SSKIRT_L` and `SSKIRT_R` as `<mesh>_L` and `<mesh>_R`,
  the decal to `DECAL`; the lights and mirrors categories have no slot. A new car with none chosen keeps part 0 (its
  decal coloured, its own part in the workshop). After the parts changed (`0x0045A8A0`) `0x0048BC30` takes the
  record's values as they are: the parts cleared (`0x004600C0`, the decal -1) take the decal off.
- The wheels (`0x0048ABC0`): the catalogue wheel's mesh (the car's wheel kind `+0x12`; past the list the wheels as
  they were), else the CarDef's `STANDARDWHEELS`, front and rear alike, with `<mesh>_blur` for speed; the exhaust
  likewise from the catalogue (the car's exhaust `+0x0F`, its SHAPE; none past the list) or `STANDARDEXHAUST`, the one
  mesh put in each of `EXHAUST_S`, `EXHAUST_C` and `EXHAUST_O` the car has (`0x0048B3C0`; no mesh leaves them as they
  are); the driver (`0x0048B860`) the mesh `DRIVER`.
- The sponsor decals (`0x0048B440`, `0x0048B690`, also when the model is made): with a `SPONS` node the materials
  `decal_spons_1` .. `9` (the model object's `+0xF8 ..`) take `alpha2` of `cartextures`; with the car's sponsor
  decals shown (`+0x48`, 1 for a new car) each one whose kind of catalogue part the car has (`+0x0E ..`; the first
  part past the list) takes `decal_<the part's supplier>` of `cartextures` and, of Coloured Spec ABlend, the car's
  sponsor colour (`+0x4C`) as its colour.
- The tint (`0x0048C040`, also when the model is made): the glass material `<car>_GLASS` (the model object's
  `+0xF4`) black (Colour, parameter 5) with the car's `+0x44` as its parameter 2.
- The wheel colour (`0x0048C440`, also when the model is made): with the car's `+0x40` set, the materials of the
  wheel mesh's name, `<mesh>_BS`, `<mesh>_B`, `<mesh>_LOD` and the `_BL` name (the mesh's name up to its first `_`,
  `_BL`, the rest from that `_`, cut at the last `_`: `WHEEL_BBS_VS1_17` gives `WHEEL_BL_BBS_VS1`) (the model object's
  `+0x11C .. +0x12C`, found by name in the material manager `[G+0x10]`, `0x0048ABC0`) take the car's colour `+0x3C` as
  their colour (parameter 0). The manager keeps one material of a name: the catalogue's wheels' blur meshes
  (`AddWH_..._blur`) use the parts' names of their meshes, so the colour is on both. The wheels' fit (`0x0048ABC0`)
  lets the five go and finds them for a catalogue wheel alone: the CarDef's standard wheels (the wheel -1) have none,
  and the wheel colour does not reach them. Each found keeps its file's colour (the manager's description, its first
  value, `0x0048AED8`; `+0x134 .. +0x144`, -1 for a name not found), which is put back on it. `0x0048C5D0` keeps the
  car's colour as theirs (wheelcolourmenu entered with the car's own colour, a colour bought), `0x0048C530` puts the
  kept ones back (schememenu putting on a look without a wheel colour, wheelcolourmenu's BACK before the car's own
  colour, `0x0048C4F0`).
- The body material (`+0xEC`) is the mesh material `<car>_BODY` (the CarDef's `+0x15C`); a slot's mesh takes it for
  its parts of that material and of `<car>_PRIMER` (`0x0048B9D0`, `0x0048C090`). The primer's (`0x0048C210`: the
  material found by the CarDef's `+0x19C`, then each of the slots `+0x30 .. +0x58`) runs after the body parts as the
  model is made (`0x0048C2E0`), as a job after the parts changed (`0x0045A8A0`, for the model and its mirror image)
  and as colourchoose and colourfocus are entered (the garage's car). A mesh in no slot keeps the primer's own
  material (`<car>_primer.pmd`, Car Colour 128 128 128): a part of the S2000's chassis (S2000_PRIMER) and the Golf's
  farthest logo stay grey, a stock problem. The paint (`0x0048BF90`): Car Colour = the base colour, Metallic Colour,
  Metallic Shininess = 30 − the metallic amount × 25, Pearlescent Colour, Pearlescent Shininess = the pearl amount ×
  6 + 0.01.
- The race's wheel records (`0x00485270`, 0x9C bytes from the car's `+0x1A0`: WHEEL_FL, WHEEL_FR, WHEEL_RL,
  WHEEL_RR, the right ones flagged `+0x48`) keep the slot's place and diagonal. Each frame (`0x00487670`): the node's
  place against the last frame's; the turn less dt x the wheel's spin (0.05 of it for the front wheel whose steering
  pulls, the car's `+0x64` with its side `+0x4F8`), held within ±π; past 45 rad/s the blurred mesh (`<wheel>_BLUR`,
  else `RX7_WHEEL_BLUR`, made with the wheels by `0x0048ABC0`; the wheel that pulls keeps its own), under it the
  wheel's again; the travel eased toward the tyre's compression x 100 by 150 x dt at most; the steering toward (3 -
  2|s|) s (0.5 rad off for the wheel that pulls) x MaxSteerAngle by 4 x dt at most; the node's matrix the diagonal at
  the place raised by the travel, turned about Y by − the steering (π − it on the right) and about X by the turn
  (its opposite on the right). On the dyno stand (`+0x19C`) only driven wheels blur and the others stand.
- Each frame of the race (`0x00488EE0`, after the slipstream) the chassis node (`_CHASS`, the model object's `+0x0C`)
  is put at (0, −MAXDROPHEIGHT (the CarDef's `+0x18`) × the car's `+0x58`, 0): the body sits lower by the ride
  height.
- The lamps (found by name as `0x00534EC0` finds a node): `_BRKLIGHT` (`+0x3C`) and `_REVLIGHT` (`+0x54`) start hidden
  (node flag 4); `LIGHTSON` (`+0x50`) is shown when the race's atmosphere is night (`+0x170`) or wet (`+0x174`), else
  hidden (`0x00486340`). Each frame (`0x00487C70`) the brake lights are shown while the brake (`+0x34C`) is not 0
  and the reversing lights in gear 0, until the damage puts them out: the rear lights out (`+0x44C`) leave the brake
  lights as they are, the rear part out (`+0x454`) hides the reversing lights (`0x00488B50`).
- The lamps' lens flares: the model's light nodes with LIGHTFLARE become flares (`21-scene-objects.md`) under the car
  and join the world's flares as the car is made. `RBRKLIGHTON_R`, `_L` (`+0x40`, `+0x44`) start off (`+0xA4` 0),
  are on while the brake lights are shown and off once the rear lights are out; `HLIGHTON_R`, `_L` (`+0x48`, `+0x4C`)
  are on at night or in the wet, off otherwise and once the headlights are out. A flare off leaves its visibility at 1
  and makes no quad.
- The projected textures (`12-rendering.md`) under the chassis node (`+0x10`), its mesh's box (the node's `+4`
  method): the shadow (`+0x4C4`, `0x00485420`) with `gen_shadow`, and with a neon (the car's `+0x0D` not 0xFF)
  `gen_neon` in the car's colour (`+0x2C`), the box x 1.25 across and x 1.1 along; at night only (the atmosphere's
  `+0x170`, `0x00486245`) the beam (`+0x4C8`) with `headlight_b` on the glow's material, v turned over, colour
  0xFF62607C, the box x 7 across and along from 0.8 of the front to three of the car's lengths before it. The beam is
  put off for good when the headlights go out (`0x00488B50`); each player frame (`0x00489370`) every car's shadow is
  on, then under the bumper and bonnet views the followed car's off (Stock problem: the beam's `+0x10` is set off with
  it but never read: the beam stays).

## Car collections

`CMSPlayersCrewCollection.txt`, `CMSCrew01Collection.txt` .. `CMSCrew18Collection.txt` and `arcadecollection.txt`
are read by `0x00489DA0` into a list of cars. Keywords (`0x0049A380` .. `0x0049A7E0`):

| keyword | effect |
|---|---|
| `[BeginCollection]` | |
| `BeginCar <model>` | a new car (`0x0048DB40`) of the CarDef found by the name (`0x0048CC30`); base and pearl white, metallic black |
| `BaseColour r g b` | the base colour (atol), also the pearl colour |
| `MetallicColour r g b`, `PearlescentColour r g b` | (a fourth number is not read) |
| `Modification <part> <n>` | the body part of FBUMP 0, RBUMP 1, BONNET 2, SPOILER 3, SSKIRT 4, HEADLIGHTS 5, REARLIGHTS 6, WMIR 7, DECAL 8; `WHEELS` sets the catalogue wheel |
| `Prototype`, `AutoMod` | flags |
| `EndCar` | with either flag the car is modified automatically (`0x0048F2C0` with the prototype flag and level -1), then it joins the list |

## Automatic modification (`0x0048F2C0`)

`(car, prototype, level)`:

- `+0x58` = 1.0 for a prototype, else 0.8; `+0x44` = 0.8 / 0.5; `+0xB0` = the counter `0x00717868` (starts at 1),
  which then goes up and wraps from 6 to 1.
- Body parts: the neon (9) gets `rand() % 1 + 1` = 1. For the other categories n = the list's size, less 1 when it
  holds more than one part; with n > 0: the decal (8) gets `rand() % (n - 8) + 1` (0 for a prototype; the division
  is unchecked: n = 8 stops the game), the others `rand() % n + 1` when the list has more than one part, else 0.
- Catalogue parts, per kind with n parts: a prototype takes the first part of level 3 (else the last); otherwise
  level -1 takes `rand() % n`, another level draws `rand() % n` until the part has that level (giving up after 100
  draws).
- The neon colour `+0x2C` = a random entry of the `[NeonColour]` list, `+0x38` = a random `[DecalColour]`, `+0x84`
  = 15.0 (kept within 0 .. 15), and the wheel = `rand() % ` the catalogue's wheels.
