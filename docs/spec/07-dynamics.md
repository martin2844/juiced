# Car physics definitions and the performance rating

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## `scripts\dynamic_<car>.txt`

A car model's physics are read when first needed (`0x004899F0`): the file `dynamic_<name>.txt` (`<name>` the
CarDef's `+0x1C`) is read by the script reader (`05-scripts.md`) into a car definition (0x188 bytes, made by
`0x004F3A20`) kept at the CarDef's `+0x4D0`; without the file it stays empty.

The keyword map is that of the dynamic objects (collision object and dynamic object keywords, then the car's). A key
used outside its block answers 0, which stops the file.

| keyword | handler | effect |
|---|---|---|
| `[BeginCar]` | `0x004F4470` | a new car definition (0 while one is open); also opens the dynamic object |
| `[EndCar]` | `0x004F58D0` | closes the car (0 when none is open); the definition stays the parser's result |
| `TorqueCurveX` | `0x004F4AA0` | the list of words, each `atof x 0.10471976` (rpm to rad/s, rounded once to a float): `+0x104`; its last value also `+0x180` |
| `TorqueCurveY` | `0x004F4C00` | the list (atof): `+0x114` |
| `MaxTorque` | `0x004F4D60` | `+0x124` |
| `ReverseGearRatio` | `0x004F4DC0` | `atof x 0.6`: `+0x128` |
| `ForwardGearRatios` | `0x004F4E20` | the list: `+0x12C` |
| `EngineInertia`, `ClutchSpeed`, `SteeringFactor`, `SteeringSpeed`, `PowerRangeMin`, `PowerRangeMax`, `Turbo` | `0x004F4F80` .. `0x004F51C0` | `+0x14C`, `+0x150`, `+0x154`, `+0x158`, `+0x15C`, `+0x160`, `+0x164` |
| `Nitro <multiplier> <time>` | `0x004F5220` | `+0x168`, `+0x16C` |
| `ABS`, `ASR` | `0x004F52A0`, `0x004F52C0` | `+0x170` = 1, `+0x174` = 1 |
| `ASRFactor`, `TimeFactor`, `FrontWeightDistribution` | `0x004F52E0` .. `0x004F53A0` | `+0x178`, `+0x17C`, `+0x184` |
| `[BeginWheel]` | `0x004F5400` | opens the wheel (0 while one is open) |
| `WheelId` | `0x004F5420` | atol, record `+0x00` |
| `WheelRadius` | `0x004F5480` | `+0x04` |
| `FinalDrive`, `WheelInertia` | `0x004F54E0`, `0x004F5560` | `+0x08` / `+0x10`, and `1 / value` at `+0x0C` / `+0x14` (0 for 0) |
| `Width`, `MaxSteerAngle`, `BrakeMu` | `0x004F55E0` .. `0x004F56A0` | `+0x18`, `+0x1C`, `+0x20` |
| `HandbrakeMu` | `0x004F5700` | `+0x24`; a value between 100 and 10000 becomes 10000 (atof's double compared, rounded to a float only as it is stored) |
| `AxleMu` | `0x004F5780` | `+0x28` |
| `Drive` | `0x004F57E0` | `+0x2C`: 1 for a non-zero number, else 0 |
| `BaseHeight` | `0x004F5840` | `+0x30` |
| `[EndWheel]` | `0x004F58A0` | a copy of the record (13 dwords, `0x004F4100`) joins the wheels (`+0x13C`) of the last car made |

A missing number is 0. The wheel record belongs to the parser: it starts as zeros and is not cleared between the
blocks, so a block keeps the values of the one before it that it does not set. The defaults of a new car
definition: `SteeringSpeed`, `Turbo`, `Nitro`, `ASRFactor`, `TimeFactor` 1.0, `FrontWeightDistribution` 0.5, the
rest 0.

The body keys need an open body: `[BeginCar]` opens one, `[BeginDynamicObject]` (`0x004EF550`, 0 while one is open)
opens one that is not a car, `[EndCar]` and `[EndDynamicObject]` close it. Their values go to the last body opened
(`16-physics.md`, "The definition"). `[BeginCollisionObject]` (`0x005366B0`, 0 while one is open) opens a definition
of the collision part only (`0x00535F20`, 0xAC bytes), `[EndCollisionObject]` (`0x00536740`) closes it; `Name` takes
any open definition. Each definition knows its kind (`+0xA8`): 0 a collision object, 1 a dynamic object
(`0x004EEE20`), 2 a car (`0x004F3A20`). The reader's result (`0x004EFF30`) is the definition made last (its `+0x68`);
a scene's physics objects are read this way (`21-scene-objects.md`). `[EndSphereCollidee]` (`0x00536DE0`) adds a
sphere collidee of the parser's records (Point, Radius, SurfaceType, Mu, Restitution) without regard to the block
opened: a `[BeginMeshCollidee]` closed by it (`dynamic_box.txt`, `dynamic_bin01.txt`, `dynamic_bin02.txt`) leaves
the mesh collidee's Mu and Restitution at 0.

| keyword | handler | effect |
|---|---|---|
| `Mass`, `LocalGravity` | `0x004EF5E0`, `0x004EF6E0` | `+0xAC`, `+0xD0` |
| `Inertia`, `CentreOfGravity` | `0x004EF640`, `0x004EF690` | three numbers (`0x00536610`): `+0xB0`, `+0xC0` |
| `DragCoefficient` | `0x004EF740` | in an airfoil its drag, else `+0xD4` |
| `Sleep <on> <value>` | `0x004EF7F0` | `+0xDC` = on != 0, `+0xE0` = value, `+0xD8` = 1 |
| `[BeginAirfoil]` | `0x004EF870` | opens the airfoil (0 while one is open; no body needed) |
| `Position`, `Force` | `0x004EF890`, `0x004EF8F0` | the airfoil's point and force |
| `[EndAirfoil]` | `0x004EF950` | the airfoil joins the last body opened, also after its end (`0x004EF2C0`) |

The airfoil record belongs to the parser like the wheel record.

### Colliders and contacts

The collision object keys (`0x005366B0 ..`) fill records of the parser that stay between the blocks: `ID` and
`ContactId` share one, so do `Mu` and `Restitution` of collidees and contacts. `ID`, `SurfaceType` (one of
TARMAC_DRY, SAND_DRY, GRASS_DRY, TARMAC_WET, SAND_WET, GRASS_WET, CONCRETE, METAL, WOOD, GLASS, else 0), `Mu`,
`Restitution`, `File`, `Point` (`0x00536960`, `+0x20`), `Normal` (`0x005369A0`, `+0x30`), `Centre` (`0x005369E0`,
`+0x40`; no block reads it), `End` (`0x00536A20`, `+0x50`), `Radius` (`0x00536A60`, `+0x60`, a float; missing, 0)
need no open block; the `[Begin..]` keys of the colliders, the clipping planes and the spheres do nothing
(`0x0042E470`). `Name` needs an open object (63 characters, `+0x08`), `MaxCollidees` (`+0xA0`) and `Primary`
(`+0xA4` = 1) write to the definition unchecked. A car's or a dynamic object's file reaches the same handlers (their
table `0x00723BF0`, filled by `0x006C04F0`, holds the collision object's keys, `0x00724820` by `0x006C2E50`).

| keyword | handler | effect |
|---|---|---|
| `[EndLineCollider]` | `0x00536DA0` | a segment (`0x0049F9B0`): Point, the unit direction to End (divided by the length + 1.1920929e-7), the length, the ID, in centimetres of the model; a wheel's |
| `[EndMeshCollider]` | `0x00536AB0` | `+0x8C` = ID; the `.ocd` of File (`+0x88`): the shape that hits the world |
| `[EndMeshCollidee]` | `0x00536BA0` | `+0x94` = SurfaceType, `+0x98` Mu, `+0x9C` Restitution; the `.ocd` of File (`+0x90`): the shape the world hits |
| `[EndClippingPlane]` | `0x00536CA0` | a plane (0x20 bytes, `0x004C7DF0`) of Point and Normal joins `+0x48`: it cuts the faces the sphere colliders test (`16-physics.md`) |
| `[EndSphereCollider]` | `0x00536D00` | a sphere (0x24 bytes: Point, Radius at `+0x10`, ID at `+0x20`; grown by `0x00537CF0`, `0x00537FF0`, `0x00538030`) joins `+0x58`: it hits the world and the other bodies |
| `[EndSphereCollidee]` | `0x00536DE0` | a sphere (0x2C bytes, `0x00537CA0`: Point, Radius at `+0x10`, SurfaceType `+0x20`, Mu `+0x24`, Restitution `+0x28`; grown by `0x00537EF0`, `0x00538130`, `0x00538180`) joins `+0x78`: the other bodies hit it |

The three write to the parser's definition (`+0x68`) whether a block is open or not; before any definition they
would write through a null pointer (left out by OpenJuice). In the game's data only `dynamic_tyrewall.txt` has a
sphere collider and clipping planes (no track names it); `dynamic_spoiler.txt`, `dynamic_sskirt_r.txt`,
`dynamic_wmir_l.txt` and `dynamic_wmir_r.txt` have two sphere collidees each, `dynamic_tyrewall.txt` one, and
`dynamic_rbump.txt`, `dynamic_box.txt`, `dynamic_bin01.txt`, `dynamic_bin02.txt` one of radius 0 at the origin (their
mesh collidee's block closed by `[EndSphereCollidee]`, above). The car parts' bodies made of these files
(`0x004191C0` → `0x00421960`, its last argument 0) never join a world.

A file read again keeps its last File for a block without one.

The contacts (`0x004EF990 ..`, tyres `0x004F4510 ..`) say how a collider's touch pushes the body. `[BeginRigidContact]`,
`[BeginSpringContact]` and `[BeginTyreContact]` need no contact open; `ContactId`, `ClippingPlane <point> <normal>`,
`NoClippingPlane`, `MaxCompression` need one; `Stiffness`, `DampingRatio`, `WeightDistribution` need a spring or tyre;
the tyre keys (`LateralStiffness`, `LongitudinalStiffness`, `LateralMu`, `LongitudinalMu`, `MagicFormulaB` .. `E`)
a tyre. `[EndRigidContact]`, `[EndSpringContact]`, `[EndTyreContact]` need their kind open (a tyre is also a spring:
`[EndSpringContact]` ends it as a spring) and add a contact to the definition (`+0xF4`; vtable `0x006F6CF0`):

| offset | value |
|---|---|
| `+0x04` | ContactId |
| `+0x08`, `+0x0C`, `+0x1C` | the clipping plane: set, point, normal |
| `+0x2C`, `+0x30`, `+0x34` | MaxCompression, Mu, Restitution |
| `+0x38` | 0 rigid (0x3C bytes), 1 spring (0x4C), 2 tyre (0x74) |
| `+0x3C` | LocalGravity x Stiffness |
| `+0x40` | DampingRatio |
| `+0x44` | the mass carried: Mass x WeightDistribution; a tyre's FrontWeightDistribution (contacts 0 and 1) or 1 - it, x 0.5 x Mass |
| `+0x48` | sqrt(mass x stiffness) x DampingRatio, doubled |
| `+0x4C` .. `+0x68` | LateralStiffness, LongitudinalStiffness, LateralMu, LongitudinalMu, MagicFormula B, C, D, E |
| `+0x6C` | E / B (0 for B = 0) |
| `+0x70` | LongitudinalMu x LateralStiffness / (LongitudinalStiffness x LateralMu) (0 for a divisor 0) |

A tyre's WeightDistribution is not used. A plain dynamic object (not a car) makes a rigid contact for a tyre.

## Fitting the parts (`0x0048E380`)

The rating works on a copy of the definition (`0x004F3B80`; wheels and tyre contacts copied) into which the car's
catalogue parts are put. A part kind with `0xFF` is left out; an index past the end of the list takes its first part;
`DATAVAL` is the part's `+0x1C`.

| kind | effect |
|---|---|
| nitro (2) | `+0x168` = 0.2 + DATAVAL, `+0x16C` = the car's `+0x84` |
| turbo (3) | `+0x164` = DATAVAL; without a turbo part, a CarDef with `STOCKTURBO` gets 1.01 |
| brakes (5) | each wheel's `BrakeMu` x DATAVAL (x 1 without a part); the car's `+0x80` = the last wheel's |
| suspension (0) | each tyre contact (type 2): stiffness `+0x3C` x DATAVAL, damper `+0x48` = 2 sqrt(`+0x3C` x `+0x44`) x `+0x40` |
| exhaust (1) | `MaxTorque` x DATAVAL |
| tyres (6) | each tyre contact (type 2): `+0x54` x DATAVAL, `+0x70` = `+0x58 x +0x4C / (+0x54 x +0x50)` (0 when the divisor is 0) |
| induction (8) | `MaxTorque` x DATAVAL |

Then `MaxTorque` x the car's `+0x60`, and each forward gear ratio x `1 + 2 (car +0x5C - 0.5) x 0.1`. A spoiler
(body part 3 > 0) adds an airfoil (`0x004EF2C0`) at (0, 0, -1.5) with (0, -0.25, 0), a front bumper (body part 0 > 0)
one at (0, 0, 1.5) with (0, -0.15, 0) (the 0.25 and 0.15 are the variables `0x00725478` and `0x00725474`).

## The torque curve (`0x00554740`)

At an engine speed `w` (rad/s), with `x` the speeds and `y` the factors: `w` is held within the first and the last
`x`; `i` is the first index with `w <= x[i + 1]`; the factor is `(y[i+1] - y[i]) x ((w - x[i]) / (x[i+1] - x[i])) +
y[i]`, or `y[i]` when the two speeds are equal.

## Peak power (`0x004F4270`)

From `x[0] x 9.549296` rpm, while the rpm is below `x[last] x 9.549296`, in steps of 250 rpm: `w = rpm x
0.10471976`, `power = factor(w) x Turbo x MaxTorque x w x 2.8`; the first largest power keeps its rpm. The result is
`factor(w) x Turbo x MaxTorque x 2.8 x w` at that rpm. The turbo enters as the plain multiplier `+0x164` (no boost
curve).

## Performance rating (`0x0048E8A0`)

`car +0x88` = peak power with the parts x 0.00048103448 - 0.5, or 100.0 when the model has no dynamic file. The
fitting also sets the car's `+0x80`.

The arithmetic runs on the x87: each result is rounded to the precision of the thread that computes it. On the main
thread (single precision after Direct3D 9 made its device) every operation rounds to a float; in a thread at the
runtime's double precision the intermediate results are doubles and only the stores (rpm, speed, best power, the
fields) round to floats. The two give ratings that can differ in the last bit; the saves hold ratings of both kinds.
