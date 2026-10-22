# Stock problems

Mistakes and oddities of Juiced 1.0 (PC) that OpenJuice repeats on purpose: the engine behaves as the stock game,
fixes are left to mods. Where the stock game only crashes or reads memory it does not own, OpenJuice cannot repeat it
exactly; the entry says what OpenJuice does instead. The code marks each place with "Stock problem".

## The post effects (`12-rendering.md`)

| where | what | OpenJuice |
|---|---|---|
| the exposure's measure (`0x005213B5`) | its target is set straight on the device, not through the target taken last (`[0x0075F168]`), which still holds the back buffer: the glow's blur over the view (`0x00521C80`) and, without the glow, the director's cut transition (`0x00520410`) go into the 128 × 64 measure, not onto the screen | the same |
| the exposure's start (`[0x00722EA8]`) | 0 until the camera first leaves a car's node (`0x00401F9D`); the first frame with the glow is black | the same |
| the copy with the exposure (`0x00521308`) | the quad is placed at (−0.5 / width, −0.5 / height) pixels, not half a pixel | the same |
| the blurred quads (`0x00520FC0`) | the four u v sets repeat two offsets in some corners and differ between corners | the same |
| the tyre tracks' drawing (`0x0050D490`) | the first entry drawn takes the level before it is held to COLOUR_START's alpha / 255: its alphas wrap past a byte | the same |

## Material classes (`12-rendering.md`)

| where | what | OpenJuice |
|---|---|---|
| 2-Texture Modulate (`0x00527BAE`) and 2-Texture Modulate Specular (`0x0052EC6B`) | they ask for the programs `Material_2TexModulate` and `Material_2TexModulateSpec`; the game's files and the programs loaded at start (`0x00519800` ..) are `Material_2TexModulare` and `Material_2TexModulareSpec`: no program | the same (no file of the asked names) |
| SpecMap ATest (`0x0052F5DB`) | its program `Material_SpecMapATest` is not among those loaded at start nor in the game's files: no program, and the object marks itself (bit 0x40 of `+7` of its base) | no program |
| the programs made again after a reset of the device (`0x0051B620`) | each is made again until it has a usable technique: one with none (its file not read, or no technique for the device's classes) is made again for ever and the game hangs (every program of the game's files has a usable technique for any classes) | the same |
| the fixed-function lights (`0x00546419`) | each light's diffuse green is its colour's red | the same |
| the world's clock started again (`0x004263E0`, `12-rendering.md`) | the lead is the last pause's length, not the sum of the pauses: after a second pause the world's time (the shaders', the cubes', the particles') jumps on by the pauses before it | the same |
| a car's tree drawn in two modes (`0x00550500`) | the sponsors' decals (Coloured Spec ABlend, flags `0x2004`) are of the late classes as the glass (Car Glass, `0x145`) and go with it in the parts' order: the far side's decals show through the rear window | the same |

## The effects' textures and sources (`11-effects.md`)

| where | what | OpenJuice |
|---|---|---|
| a procedural texture's name (`0x0051B2C0`) | a texture parameter without `name` and without `function` copies its name from address 0 | the name `proc_<program>_.dds` |
| a texture's `TextureType` (`0x0051ACDD`) | a word other than volume, cube or texture stops the game at a breakpoint (`0x0059AF30`) | printed; the type stays |
| `-localfx`: the include object's Open (`0x0051B0D0`) | an include that does not open is read through address 0 | the compile fails |
| `-localfx`: a procedural texture (`0x0051B400`) | a function that does not compile leaves the shader at 0 and the texture shader is made from it | no texture |
| `-localfx`: a flat procedural texture (`0x0051B5A6`) | its mip levels are its height, not `miplevels` | the same |
| `-localfx`: a volume's save (`0x0051B600`) | `D3DXSaveVolumeToFile` is given the volume texture, not a volume of it | its levels written |
| the texture shader's fill (`0x005DB6E4`) | a flat texture's position and size leave z and w as values of the stack | 0 |

## The camera's weather (`20-effects.md`)

| where | what | OpenJuice |
|---|---|---|
| RAIN's draw (`0x0050C33A`) | 2 × count − 2 triangles of count quads: the last streak is not drawn | the same |
| SPLASH's frame (`0x0050AC94`) | its distance from the camera (z² + x² / 2) against twice RANDOM_DRIFT² decides nothing | the same |
| SMEAR's blobs (`0x0050F0D9`) | the view's bottom is taken at its right side's x | the same |
| SMEAR's alpha (`0x0050F2C7`) | the low byte of (1 − life) × 102: a blob past its life (one beyond the count shown) wraps to nearly opaque | the same |
| the base's ground point (`0x0050DCDE`) and SMEAR's clear (`0x0050ECC3`) | a point not found takes a w left on the stack; the camera's last w a stack value | 0 |
| LEAVES's texture corners (`0x00508C60`) | the quad's turn is the leaf's index × 4 cut to a whole number: only the first leaf takes the turn 0, every other the last; the turns 1 and 2 are never taken | the same |
| LEAVES's draw (`0x00508970` from `0x00509B13`) | the count of triangles is the count of leaves: only the first half of the leaves is drawn | the same |
| LEAVES's step on the job thread (`0x00509870`) | runs while the main thread goes on: the frame draws whichever record the step last finished, the cars' places it reads may be of either frame | the step runs at once, before the draw |

## The reflections (`15-tracks.md`)

| where | what | OpenJuice |
|---|---|---|
| the render tree's regions (`0x00544B90`) | the regions of the nodes are added to the bytes the `.d8w` holds there, left over from the tool that wrote it, so most nodes have regions they hold nothing of | the same |
| the world's cube in the main pass (`0x00547690`) | drawn after the view's constants: the view is drawn with the last face's c7, c10, c14, c33 and c41, c42, and inside the view's BeginScene, whose scene the cube's EndScene ends | the constants the same; OpenJuice's device has no scenes of its own |
| a cube's faces (`0x00547D50`) | c33's w is a value of the stack; c10 is the first face's projection with the near and far planes of the cube drawn before | 0; the same |
| a flare's y reflection and reflection (`0x00421D50`) | a key not given leaves what the stack held as the size or the alpha | 0 |

## Memory the game saves without setting it

| where | what | OpenJuice |
|---|---|---|
| `settings.dat` (`03-saves.md`) | the bytes of the settings object no code sets carry whatever the memory held (pieces of files read earlier) | 0 |
| `CMSGlobal.sav`: calendar events past the 20 made, `+0xF8C`, `+0xF90`; dealer offers past the count (`+0x00 .. +0x2B`); the phone's contacts; the rest of the crew member names; crewmates 1 .. 3 `+0xB8`, `+0xBC` | the career objects are allocated without clearing (`08-career.md`) | 0 |
| `CMSGlobal.sav`: the profile's member slots (`+0x500`) | the members' addresses; a free slot's value is a value of the writer's stack (`0x004B77C6`) | 0 |
| car records: the model name field | the CarDef's 64-byte field as it is in memory, after the name what the memory held | 0 |
| vertex constants (`12-rendering.md`): w of c27, c33, c43, c44; x, y, z of c39 for the specular classes; y, z, w of c51 | values of the stock code's stack | 0 |
| Building Opaque: x, y, z of pixel c0, c1 | fields of the material object never set (a 0x64-byte block of the heap, 0x00526966); the stock game seen live (2026-10-19, a custom race): mostly 0, else what the block held before (the same values in c0 and c1, colours such as 0.4392 / 0.0245 / 0.0241, or rubbish such as 1.36e-19); the shaders read c0.x and c1.x | 0 |

## Data files

| where | what | OpenJuice |
|---|---|---|
| `cardefs.txt` `CUSTOM` without a supplier (`0x004995F0`) | `rand() % 3` is compared with 2 (NUUK) and 3 (SWEET): 3 never comes, so a part is NUUK or AERO | the same |
| `cardefs.txt` `CUSTOM` of an unknown category | no part and no `rand()` draw (`WMIR_L`, `WMIR_R` in the shipped file instead of `WMIRROR`) | the same |
| `CMSCrews.txt` `WillAttendAt` with a name of no crew | index -1: the field before the table (`+0x260`, the count of `Illegal` routes) becomes 1 | the same |
| `CMSCrews.txt`: more `DriverName`, `ShortDriverName`, `RespectFactor`, `Circuit`, `P2P`, `Cruise`, `Sprint`, `Illegal` lines than slots; names longer than their field | the values are written past the table into the next fields (a second `Cruise` writes over the cruise km and count) | the same within the crew object; what would land past it is dropped |
| `CMSCrewmates.txt` `PhoneNumber` | not checked against the crewmate being read: every crewmate takes the file's last number | the same |
| `CMSCrewmates.txt` `DrivingLimit`, `RecoverLimit` | the value is read and not used: 100.0 | the same |
| `CMSCrewChallenges.txt`: more than 3 challenges | written past the table | the same within the crew object |
| `ArcadeRaces.txt`: more than 6 `Car` lines in a race, more than 20 `Track` lines | written past the tables into the race's paint fields / the next championship's list | the same within the arcade object |
| `dynamic_<car>.txt` `TorqueCurveX` with no value | the last point is read from before the list | the last point is kept |
| `dynamic_<car>.txt`: fewer `TorqueCurveY` than `TorqueCurveX` values, a curve of one point | the curve is read past the lists | 0 for missing values; the first value |
| `dynamic_<car>.txt` `HandbrakeMu` (`0x004F5700`) | a value between 100 and 10000 becomes 10000 (atof's double is compared: 100.000001 becomes 10000) | the same |
| `dynamic_<car>.txt` `[EndAirfoil]` before any body | the airfoil is written through a null definition (the game stops) | not added |
| `dynamic_<car>.txt` `MaxCollidees`, `Primary`, the collider ends before any object | written through a null definition (the game stops) | left out |
| `dynamic_*.txt` `[EndTyreContact]` in a plain dynamic object | the object makes a rigid contact (0x3C bytes) and the tyre's values are written past it | the rigid contact |
| `dynamic_<car>.txt` `ClutchSpeed`, `SteeringSpeed` | read, never used | the same |
| the betting window's clips (`14-career-windows.md`) | movies the talk asks for that are not there: `COMMENT_CAR_NEG3`, `_NEUTRAL3`, `_POS3`, `YES3` of every leader, `CHALLENGE_INTRO1` but TK's, and others (`HOW_MUCH3`, `OKAY3`, `DECLINE_BET3`, ... of most leaders); such a clip ends at once and the talk goes on | the same |
| a route's `.spl` with more than six `GRIDPOINT` lines (`0x004A1410`) | the seventh point's x is written over the count that follows the six slots, its y, z and heading into the slot of that new count (x's bits x 16 bytes on) and the count becomes x's bits + 1: an x of 0 (bits 0 .. 5) gives the slot of the six its y, z, heading and the count bits + 1, any other x writes far past the slots (the game stops) | the same for bits 0 .. 5; else dropped |
| `cardefs.txt` without a `[NeonColour]` (`0x004590E0`) | the neon colour is `rand() %` 0, a division by zero (the game stops) | the colour stays |
| a car's `<car>_PRIMER` material (`0x0048C210`, `06-cars.md`) | only the meshes in the model's slots take the body material for it: a part of the S2000's chassis and the Golf's farthest logo keep the primer's grey (128 128 128) | the same |

## Game logic

| where | what | OpenJuice |
|---|---|---|
| automatic modification (`0x0048F2C0`) | a decal list of n gives the decal `rand() % (n - 9) + 1`, unchecked and unsigned (`0x0048F392`): 9 decals divide by zero (the game stops), 1 .. 8 give an unsigned wrap (the game's cars have 31) | the wrap is repeated; 0 for 9 decals |
| parts of a car (`0x0048E380`, `0x004A8D20`) | an index of 0x80 .. 0xFE (negative) reads before the list | the list's first part |
| a new career, crew colours (`0x004A2300`) | 16 bytes are set to 0x80FFFFFF as four colours, but the fourth is the metallic amount (a float) | the same |
| event rounds (`0x004A33E0`) | a crew without a route of the type divides by zero (the game stops) | route slot 0 |
| event rounds of a challenge (`0x004A33E0`) | challenge -1 (none) reads the fields before the challenge table | the same |
| event prizes (`0x004A33E0`) | class 8 (a cruise) reads past the 8 prize factors (`0x0071875C`): the word after them (`0x0071877C`, 0; nothing refers to it), so the prizes are 0 | the same |
| `0x004A2C70` | a draw is compared with -1 (`rand() % n == -1`), which never happens | the same |
| performance ratings (`07-dynamics.md`) | the x87 precision of the computing thread decides the last bit: the main thread rounds to floats, other threads to doubles; the saves hold both | both, chosen by the caller |
| lights (`0x00514E80`) | the three light colours are written to c26, c27, c28 at once, but the shaders read them from c26, c28, c30: c27 is then overwritten by light 1's direction, c28 holds the third colour (sorted, the darkest), c30 none | the same |
| clouds (`0x005151D0`) | c49 is the static after the cloud constants: its first value is the block's initialisation flag (1 as an integer) | the same |
| material classes with a Z-Bias | the value is kept and never used when drawing | the same |
| atmospheres (`13-atmosphere.md`): ambient, light colours, directions, fog colours, FogMin, FogMax, SunFlareSize, `+0x1C0` | no default: the memory the object was made in when the file leaves them out; `+0x1C0` has no key: the set's loading makes it 1 for the atmosphere it puts in use (`0x00533950`), the others keep the memory, and it goes to the renderer's `+0x188` every frame | 0 (1 for the one in use) |
| atmospheres: w of the ambient, light colours and directions | a value of the handler's stack (c27, c29, c31 take it) | 0 |
| front end, crew logos (`0x00718A28`) | the table jumps from `ui_chooselogo_18` to `_20`: logo 19 is never offered (the logo menu, `0x004C3B70`, takes its 19 entries) | the built-in table is the executable's, gap and all; the menu takes it |
| 8-bit names made wide (`0x0040ACE0`, `0x0049A850`) | the characters are signed: a letter above 0x7F becomes 0xFF80 .. 0xFFFF | the same |
| the round of an event (its index `+0x55`) | an event keeps one round; a later index would read the next events' bytes, but the index is always 0 (the event builders clear it, nothing else writes it) | the same bytes within the calendar, else the event's round |
| a car of the opponents' list (`0x004A9540`) | an empty list divides by zero (the game stops) | index 0 |
| the odds (`0x004B9920`) | a leader placed past the seven place shares would read the values after the table (`0x004B99D9`, not checked), but the place is the racer's number, 0 .. 5 (`0x004B8A70`, at most six racers) | no share |
| the drivers (`0x004BDCC0`) | a member past the four would read the fields after the members (`0x004BDDA8`, not checked), but the member is always 0 .. 3 (`rand() & 3` at `0x004A98DD`, 0 for a leader, a slot of the members menu) | the first member |
| the session and the race start (`0x004B8570`, `0x004A3CC0`) | without an event of the current day the code reads through a null pointer | nothing set up |
| the bet share (`0x004BC2E0`) | with no money left a division by zero (infinity) | the same |
| money as text (`0x004A3AF0`) | the most negative amount stays negative after its sign is taken | the same |
| the day as text (`0x004A68F0`) | a negative month reads before the month names | the first month |
| DemoFlyby without the speech object | the counter `+0x20` (10, less the frame time, rounded each frame) never changes at frame times under 0.5 s, and its end is tested as an unsigned number below 0: it leads nowhere | the same |
| the betting window, step 7 (`0x0042F910`) | the clip is picked with `rand() & 1` but compared with 1 and 2: `SUGGEST_BET3` never plays | the same |
| the betting window, steps 27 and 29 | without a raise / lower taken BET_CASH shows the money cut, the amount keeps it rounded | the same |
| the betting window's character test mode (cheat 5, `0x0042F140`) | the count goes through 81 clips, the list (`0x00715AE0`) has 75 names and six zeros: on those the name's length is read at address 0 (the game stops) | the leader's folder alone is played |
| the phone's contacts (`0x004B6640`) | past 26 contacts the next one is written after the list | not added |
| track instances (`0x005450C0`) | the fourth values of the column lengths and of the normalised rows (c3.w .. c6.w) are never written: the memory the instances were made in | 0 |
| Unlit Sign Scroll (`0x0052B0D0`) | the whole sixteens taken off the scroll are made a float as an unsigned number: a negative scroll loses 2^32 more | the same |
| Sea (`0x0052E790`) | w of pixel c1 is a value left on the stack | 0 |
| the trees (`0x0052AB60`) | w of the sway constants c56 .. c59 are values left on the stack | 0 |
| AmbientLit (`0x0052AD80`) | the multiplier is not set until the parameter comes (the memory the object was made in) | 0 |
| Tarmac (`0x0052FA40`) | x, y, z of vertex c39, and at night of pixel c0, are values left on the stack | 0 |
| a car's tyre contacts (`0x004F6050`, `0x004F6F30`) | the drive of a contact's wheel and the traction control's driven wheels are those of the wheel at the contact's place in the list of the step, not of the contact's own wheel; past the wheels the memory after them | the same; past the wheels not driven |
| a car's tyre contact without a wheel of its ID (`0x004F6050`) | read through a null pointer | left out |
| a car with an empty `TorqueCurveX` (`0x004F6A80`, `0x004F6730`) | the speeds around the list are read and its last one written | 0, nothing written |
| a gear past the car's ratios (`0x004F7610`) | the ratio is read after the list | neutral |
| a tyre rolling backwards with its wheel straight (`0x004F7C40`) | the Magic Formula's longitudinal force is taken off and added back: none is left | the same |
| a body's limits (`0x004F1E10`) | the angular momentum is turned into an angular velocity and back with the inertia of the state set before, not of the state being held, and makes this round trip at every hold even when nothing is held | the same |
| the detail factor (`[0x0079E670]`) | the detail saved in `reg.cf1` is loaded but the factor keeps the start's 1.56 until the graphics options window is left with DONE | the same |
| the count before a race's start (`0x004A0220`) | a count that lands on exactly 0 stops there without the start (the cars are let go by the handbrake, but nothing else of the start happens) | the same |
| the TV views' follow (`0x00403B00`) | a pitch error below 0 takes its change off all three turn rates (pitch, yaw and roll), not only the pitch's | the same |
| the crews' results table (`0x004529E0`) | the latest race time is set to 0 once before the rows (`0x00452A2C`) and only raised after: a crew's row shows the latest time of its drivers and of the crews in the rows above it | the same |
| the race's respect for a bet (`0x004BCD90`) | the bet's respect is lowered and weighted by crew 7 (the career's `+0x24`), not by the crew bet with: by design, crew 7 is the one crew whose Bets factor is not 0 in CMSCrews.txt, as crew 5 the best car's (`+0x1C`) and crew 6 the collection's (`+0x20`) | the same |
| the results' empty rows (`0x00451C90`) | past the finishers Team takes the time's "--:--:--" while Name and Car take "----------" | the same |
| the passing view's rise (`0x004047C0`) | the rise as cars come near is added to x and z as well as to the height | the same |
| the race camera's time (`0x0041FE50`) | the frame's step is divided by the number of physics steps it was split into: with two or three steps the views ease that much slower | the same |
| a body's mesh collider against a wall (`0x0053B8D0`) | the touches carry no zone and their points and depths take the offset of the last face of the pass (0 without faces) | the same |
| the AI's throttle factor (`0x0049DBB0`) | with the traction control cutting, the factor drops to 0 when the body's `+0x1A4` is over 22.35: the fourth value of the local velocity, which the transform leaves as whatever its stack slot held | 0: the factor never drops |
| the AI's steering (`0x0049C720`) | the swerve (its side x its time) is added inside the loop over the neighbours, once for each | the same |
| a car's contacts (`0x004916F0`) | whether the car touches another body is read from the last rigid contact of the step, touching or not | the same |
| a hit's amount (`0x004916F0`) | the normal's z + 1 is held between 1 and 2, but past 2 the bound is replaced by (energy - 400) x 0.003 | the same |
| the impact left (`0x004916F0`) | the vehicle's `+0x174` is set after the hit's pieces, always the last piece's 50 | the same |
| the line's choice without neighbours (`0x0049B3F0`) | the line is taken from `[0x0079E808]`, which nothing writes | FAST (0) |
| the slow body's brake (`0x004F2AFE`) | under 0.3 m/s the speed loses 2 dt, and below 2 dt it turns the other way: a car at rest keeps moving by up to 2 dt | the same |
| a physics object's profile (`0x00421960`) | the hulls and the centre of gravity of the first node of a profile serve every node of it | the same |
| a dynamic object without a collider hull (`0x00535BC0`) | its box is read through a null pointer | the file's centre of gravity |
| `dynamic_box.txt`, `dynamic_bin01.txt`, `dynamic_bin02.txt`, `dynamic_rbump.txt` | `[BeginMeshCollidee]` is closed by `[EndSphereCollidee]`: a sphere collidee of radius 0 is made and the mesh collidee keeps Mu and Restitution 0 | the same |
| `[EndClippingPlane]`, `[EndSphereCollider]`, `[EndSphereCollidee]` before any definition (`0x00536CA0`, `0x00536D00`, `0x00536DE0`) | they write through the parser's null definition | nothing is added |
| a face cut by the clipping planes (`0x0053B8D0`, `0x0053D0C0`) | the planes cut the first buffer into the second and back, and the sphere colliders take the first buffer with its count: after an odd number of planes the last cut is lost (with one plane the face is not cut) | the same |
| a clipping plane's crossing (`0x00538310`, `0x005394E0`) | an edge parallel to the plane, or crossing it past its end, writes no corner, yet the corner is counted: it keeps what the buffer held | the same (the buffer's last contents) |
| a sphere beside a face (`0x005388C0`, `0x00538760`) | the least squared distance of the edges' points starts as the count less 1 read as a float (about 1e-45) and the first point met never sets it: the first edge met keeps its point | the same |
| a mesh collider against a sphere collidee (`0x0053A890`) | the touch's normal has a w of the stack | 0 |
| a spline rider's start (`0x00423B80`) | its node is turned about x by the pitch, while each frame's turn (`0x00423D00`) uses -pitch, and only when it banks | the same |
| a spline's path (`0x0041B720`) | the last point's length is the way back to the first also on an open spline: the spline's length and the spacing of `EVEN_SPACING` count it | the same |
| the scene objects' steps (`0x00422BB0`) | they step only on every other frame, each with that frame's time: the turning objects (the skies' clouds) turn at half their rates | the same |
| a lens flare (`0x00421D50`, `0x00541030`) | `LIGHTFLARE_BRIGHTNESS` is read but not kept; `+0x1E0` takes a stack value left over (read only by the screen-space branch, which `+0x1DC` = 0 never runs) | the same |
| a lens flare of a moving node (`0x00421D50`) | the flare is taken off its parent at once: it stays where the node was at the load (a rider's flare does not ride) | the same |
| an interface view's sun (`0x005B4660`) | the light object (with a SunShape) and the reflection on the cars (with a SunReflection) are looked up by the SunFlare's name, not their own; neither is drawn in a view | the same |
| the HUD's enter (`0x004476D0`) | the font colours of `SPEED%d` and `GEAR%d` are written without checking that the window has them: a hud_* window without one would write through a null pointer | skipped when missing |
| the HUD's times (`0x0044FBD0`) | the hundredths are the fraction of the second x 99: they run 00 .. 98 | the same |
| the HUD's new lap (`0x0044B1C0`) | each player's new lap empties the first player's messages, not the player's own | the same |
| the HUD's ABS light (`0x0044B1C0`) | without a `SPEED%d` element the light compares the car's signed speed in m/s with 5, not the shown units | the same |
| the race map's markers (`0x0044A100`) | each vehicle's paint is put into the material named `ui_map_car`, which every marker shares: all markers take the first vehicle's paint | the same |
| a session's end (`0x004BA8A0`) | the racers not finished are shuffled by `rand() % ` the round's laps: a round of no laps divides by 0 | no swaps from it |
| a session's results (`0x004B9E20`) | a round past the first (the event's `+0x55` over 0) would write the crews' order past the stats' 0x90 bytes, but `+0x55` is always 0 | not kept |
| a session's results (`0x004B9E20`) | a place no crew took (-1) after the player's crew counts in the profile's word before the crews' words (`+0x36`) | the same |
| a session's results (`0x004B9E20`) | with no crew first (-1: none placed, as after a cruise, whose racers `0x004BA8A0` leaves unfinished) the odds before their table are read and their winnings written (`0x004BA195`): the last racer's level timer (`+0x260`, as a whole number) is the stake, its order (`+0x258`, a whole number as a float) and order time (`+0x25C`) against and for, and the session's set-up flag `+0x264` the winnings, which takes the float (any stake makes it set: the next `0x004B8570` sets the session up again) | the same |
| leaving `CMSRespectChanges` (`0x004D0000`) | the history takes a step on leaving, after `0x004BAEE0` has taken one: two steps for one event | the same |
| `CMSRespectChanges`' enter (`0x004CF5A0`) | an entry of a crew outside 1 .. 8 writes its change into the record's fields before or after the components | not kept |
| an event search (`0x004A6A70`) | an event of more than one round has its later rounds' types read at the places after its round: the event's later fields and the next events | the same (0 past the calendar) |
| the career's race results (`0x004DA310`) | a place of the crews' order no crew took (-1) reads the odds before their table (the last racer's `+0x54` .. `+0x60` and the session's `+0x264`) and the logo before the crews' (the flyers' `+0x34`: the player's colours 13 .. 20 as a name), and shows its row with them; a stake there (the last racer's level timer not 0) adds the prize to the profile's `+0x10` | the same |
| a car made ready again (`0x004894C0`) | a lost turbo comes back as the body's Turbo 1.3, whatever the car's own Turbo was | the same |
| the car's tilt held (`0x00489740`) | the pitch's torque is from the pitch / π squared less 0.3, which the pitch (an arcsine, within ±π / 2) never passes, and the square would lose the pitch's side | the same |
| a Dial's needle (`0x005181B0`) | of its second corner the y is moved onto the screen's edges as an x and the x as a y | the same |
| the month's days (`0x004A6650`, `0x004A6530` and others) | a negative month reads before the table `0x00718EDC` | the first month's |
| the meeting's cars (`0x004C27A0`) | past 55 fitting cars the profile's list runs over its count (`+0x5F0`) and on | at most 55 |
| the flyer's colours (`0x004B4060`, `0x004B4130`, `0x004B2E20`) | the player's colour is read at the chosen logo, which a flyer writes over the last colour (`+0x50`): logo 20 is its own colour and a logo past 20 reads the crews' | within the 21 colours |
| CMSAtMeeting's location (`0x004C01E0`) | the crew of the event's `+0x0D` is read without a check: outside 0 .. 8 past the crews | no location |
| CMSRaceSetup's car taken (`0x004D8410`) | with no free slot a team size of 0 reads before the cars (the `FADEBOX` pointer) and one past 3 after them (the count) | within the three slots |
| CMSRaceSetup's drivers (`0x004D69F0`) | a member slot without a crewmate reads through its old pointer | nothing shown |
| CMSGridWalk's racer buttons (`0x004C91B0`) | a highlighted button whose name is not a place (`atoi` 0) reads the racer before the first | the first racer |
| CMSCrewRaceSetup's drivers (`0x004D8AC0`) | a member slot without a crewmate reads through its pointer | nothing shown |
| CMSCrew's list (`0x004C6070`) | a member slot without a crewmate reads through its pointer | nothing shown |
| the dealer's lists (`0x004B4440`, `0x004B4620`) | past 159 entries the list runs into the dealer's fields after it | at most 159 |
| Car_Stats2's icons (`0x004C4920`) | without a `VB<n>` or `V7-<n>` element its colours or flag are written through a null pointer (the game stops) | the element passed over |
| CMSCheats' codes (`0x004C5CB0`) | seven of the eight codes start with `1`, which the letters (A .. Z and `.`) never reach | as the stock |
| CMSCheats' letters (`0x004C55B0`) | with no letter's button highlighted UP or DOWN turns the byte after the letters (`+0x20`, the leaving flag's low byte): the window goes back to `extras` | as the stock |
| credits' third page (`0x00469D90`) | a line 1311 + n is shown when the second page's line 1100 + n is not empty, not when it is not empty itself | as the stock |
| opts_game_pc's BACK (`0x00481770`) | AUTO2 (`+0x88`) gets back the kept value of AUTO (`+0x84`), and the difficulty (`+0xA0`) is not given back | as the stock |
| the dealer's garage car (`0x004B5470`) | a list made before a first visit emptied the garage (`0x004C2AD0`) names cars past it, read anyway | nothing done for them |
| CMSHostMeetingSetupRace's sprint distance (`0x004CBA80`) | a sprint route past 0 reads the distance at `+0x250 + 4 x route` (the crew's later fields) | the sprint's distance |
| the hosted event's making (`0x004CD6F0`) | rounds past the first are written over the event's fields after its round; a full calendar leaves no event and it is read through a null pointer | one round; nothing done without the event |
| the adapter's class of the graphics defaults (`0x005A26C0`) | the ATI cards' device ids are compared with the vendor id, so they never match and every ATI card is of class 4 | as the stock |
| the phone's answer of type 18 (`0x004B6B20`) | no break: the respect is set to 980, then to 580 | as the stock |
| calling a crewmate who joined (`0x004B6710`, `0x004D2140`) | the call answers at once (`+0xF98`), so its ringing never runs down and SELECT does not end it; only BACK leaves | as the stock |
| a crewmate contact of crewmate -1 (`0x004B6710`) | read through a null pointer | taken as one who has not joined |
| the contacts' picture with an advised crew not among the contacts (`0x004D2D90`) | reads the contact after the last | nothing shown |
| a part drawn for the tuning shop (`0x00498F40`, `0x004990F0`) | with no part of the level the draw divides by zero and reads the first part through a null pointer | rand() drawn, no part chosen |
| the tuning shop's classes (`0x00442A80`) | a class taken again by a stronger set keeps the meters and the nitro, suspension, brake and tyre parts of the one before: the meters add up and a part of a weaker set may stay | as the stock |
| the decal's colour in `colourchoose` (`0x0043DB90`) | the chart looks for the pearl colour instead of the decal's own (`+0x4C`) | as the stock |
| a metallic or pearl colour chosen in `colourchoose` (`0x0043DD60`) without the money over the price | taken as a base paint: the base is charged, the metallic or pearl colour stays on the car unpaid | as the stock |
| BACK in `wheelcolourmenu` (`0x0043F600`) on a car without a wheel colour of the list | the car gets the third wheel colour (`+0x10` starts at 2) | as the stock |
| the crew's scheme on its cars (`0x004A9080`) | a wheel colour past the CarDefs' list is read after it | the car's wheel colour stays |
| the sprint route of the custom race (`0x0045DFB9`) | the check takes a route found at the second place of the track's routes for none: "No Sprint Route Found" stops that race, and a track without a sprint route races the route it had | as the stock |
| the route page of `instant_setup` (`0x0045C6C0`) | the race's route is highlighted by its place among all the .tdf's routes, though the cruises, sprints and locked routes before it have no item | as the stock |
| the arcade race window's map (`0x0045BEC0`, `0x004627D0`) | the place is found by comparing the location's text of the language with nine English names exactly: other names and other languages look at the first place; the ninth (Super Speedway) is past the eight city places | as the stock; past the table the map looks at (0, 0, 0) |
| `Arcade_Championship` (`0x0042DA40`) | the championship kept is highlighted by its number as the item's place, though the developers' and extreme nitrous items may be left out before it | as the stock |
| `ArcadeRaces.txt`'s `[EndRace]` (`0x0042C430`) | the race's keywords after it write through address 0 | they are ignored |
| `hud_challenge_results` QUIT (`0x00450F80`) | the question "QUIT RACE?" / "ARE YOU SURE?" is made and dropped; the race is left at once | as the stock |
| a sprint's restart from the pause (`0x0044EF7D`) | the racers' points are cleared, their heats' time (`+0x134`) is kept and decides ties of the new sprint | as the stock |
| `hud_sprint_results`' end (`0x00454DB0`) | the vehicles are sorted as four indices on the stack: with more than four the sort reads past them | the vehicles there are sorted |
| a pink slip's stake (`0x00434D90`, `0x00432696`) | the player's car comes back into the garage as a copy with a new grid: its kept damage is gone | as the stock |
| leaving a pink slip event (`0x004A3A60`) | the event's round flag is written at `+0x1D` + the round (`+0x55`, signed) x 0x38: a round other than 0 writes past the event's round (the next or earlier events of the calendar) | as the stock while it stays in the calendar |
| `opts_voice`'s device names (`0x00484250` through `0x0049A850`) | each byte of a device's ANSI description is widened with its sign: a character from 0x80 up (a name in a national code page) becomes 0xFF80 .. 0xFFFF and draws as no letter | the same |
| GameSpy Voice's device list (`0x0057F0B0`) | a device is added without a look at the room: a playback device met with ten capture devices listed is written past the voice object's array, over its count | the device is not added |
| `opts_songlist` (`0x004828A0`) | a music bank of more than 128 songs aborts the game ("bitset") | the bits stop at 128 songs |
| a showoff trick's line (`0x004ABD10`) | a trick keeps the list's last place, not its line: a newer line (another trick, a special move, a bonus) takes the points it goes on adding | as the stock |
| a showoff zone's name (`0x004B0F40`) | `CRUISE_MESSAGE` is read only when the helper's `CRUISE_NAME` has a text (the check looks at the wrong key); a message of a number copies from a null text | the same; a message of a number gives no name |
| a showoff zone's flags (`0x004B0F40`) | the active and done flags (`+0x0C`, `+0x144`) are not set until a cruise makes the zone ready: outside a cruise they hold what the heap gave (seen live in a custom race: both rubbish, not 0, so the zone's check `+0x0C && !+0x144` fails as with 0) | they start at 0 |
| a replay's frame unpacked (`0x004286E0`) | the throttle is read from the whole byte `+0x1D`, the model's knocked-off parts with it: with any part off the throttle is 1 | the same |
| a car part's object (`0x004191C0`) | the skirts are told apart by `_strnicmp` with `sskirt_l` and `sskirt_r` over 6 characters: the right skirt takes the left's bit of the model's `+0x170` (0x10, never 0x20) | the same |
| a scene object's step (`0x0041B270`) | the turn (`0x0041B050`) is given 1 / the step's time, written over the argument (`0x0041B2B4`); the car parts' rates are 0, so it changes nothing there | the same |
| `start_replay` and `restart_replay` (`0x00455D60`, `0x00456030`) | the play starts only on the frame their count reaches 0 with the window's transitions done; past 0 the count goes on down and the play never starts | the same |
| the headlight beam under the in-car views (`0x0041FF45`) | its `+0x10` is set off under the bumper and bonnet views but never read: the beam is drawn under them while the car is not | the same |
| the attract's replays (`0x00467260`) | the count of the next one goes back to the first at 2: `attracts\attract3` is never played | the same |
| `hud_replay`'s save (`0x0044DE20`) | `savereplay` is asked only when `+0x24` is set, which nothing sets: a replay is never saved from the race | the same |
| the showoff's nitrous drift (`0x004AFE30`) | unlike the drift it is not kept to a cruise: in any race it builds a line and ends tricks (which bring the multiplier back to 1) | as the stock |
| an atmosphere zone's name (`0x00418B50`) | `atmos` alone is read from past its end; a name of 16 characters or more after the sixth has no end, and its cut runs on into the zone's place | empty, or cut at 16 |
| a privilege's entry of the respect changes (`0x004CF5A0`, `0x004D1050`) | the entry keeps its crew in its first number and 0 in its second, which the window takes as the crew: the player's crew's respect grows by the crew's number | the same |
| the earlier career menu (`0x004CDC50`, never registered) | SELECT on Cars asks for a window named "" | the same |
| a window asked for by a name no record takes (`0x005A6DB0`): `car_maintain` (`0x004374A7`), `CMSCareerSummary` (`0x004CDF6A`), `byebye` (`0x00435675`), `opts_ctl_ps2` and `opts_wheel` (`0x00483B08`, `0x00483B53`) | the window of the name's .ui (an empty one without the file, `0x005A9490`) comes up with no state: nothing answers its events and it cannot be left | the same |
| `opts_playlist`'s enter (`0x004833C0`) | without the sound system (`[0x0071E5D0]` & 1) the playlists' source `[0x0075F85C]` is not made and is read through null | it answers as the PC's (no playlists) |
| `opts_ctl_port`'s update (`0x004838F0`) | each of the input manager's devices sets the `+0x00` of MENU's item of its place: a device past the items writes through null | the keyboard alone never does |
| a crowd member's start (`0x0041D9D0`) | a centre line of no points is read out of its list | its heading stays 0 |
| a car's tracker on the centre line (`0x0041A000`) | a centre line of no points is read out of its list | the tracker stays |
| a crowd member's look (`0x0041DC00`) | without a centre line the cars' trackers are read through null; with no car the place is the stack's | the look is passed over |
| the atmosphere zones (`0x00418BF0`) | nothing is set while the camera is outside every zone, so the blend of the last frame inside stays | the same |
| the impact's emitters (`0x004916F0`) | with `explode_sparks` made, `explode_smoke` and the FLAKES are used without a check, and `chassis_smoke` with the scrape's SPARKS: a race's effects without one of them would crash | a missing one is passed over |
| `CMSCutScene`'s cars (`0x004C6B80`) | the place handed to the car's making has no heading set: a word of the stack | 0 |
| `CMSCutScene`'s record (`0x004C6B80`) | room for four cars; a fifth and more (the profile's list `+0x5F0`) write over the record's next lists | four at most |
| `CMSCutScene`'s shadows (`0x004C71F0`, `0x004C72D0`) | each `gen_shadow` is scaled to its car's chassis, then the script sets its matrix anew before the first drawn frame | the scale is not made |
| `car_rolling`'s car (`0x004DD490`) | the place handed to the car's making has no heading set: a word of the stack | 0 |
| the graph's line (`0x004F8940`) | the line under the series goes from the graph's place to (2 x the points, its y) in pixels: neither scaled to the screen nor moved by the place, while the series are scaled from 640 x 480 | the same |
| the graph's series (`0x004F8410`) | the next point past the points drawn reads past the values | the series stops there |
| a car without forward gears put at rest (`0x00486E90`) | the first gear's ratio is read before the list | neutral's ratio |
| the smoke emitters' colours (`0x005059B0`) | the sun and shade colours are scaled by 0.8 and their alpha by 0.8 once more | the same |
| `CMSRespect`'s territories (`0x004DBE60`) | each one's place goes to the list's entry k + 1, another model's when one before was not added | the same |
| `CMSRespect`'s camera (`0x004DB610`) | a crew past 8 reads past the points looked at | not looked at |
| the event's prototype part won (`0x004B9E20`, `0x004990F0`) | with no part of the kind and supplier the game stops | not fitted (the event is only made with such a part) |
| the driver window's failed delete (`0x004679B0`, flag 22) | the close flag it sets for the "Deleting..." note after making "Could not delete driver." closes the new note the next frame | the same |
| autosave and saveprofile with the saving turned on by the window (`0x00464A10`, `0x00463F30`) | a "no" to "continue without saving?" asks "Are you sure you want to save driver on memory card (PS2) in MEMORY CARD slot 1?" (1994), a console's text | the same |
| the save windows' flag 19 (`0x00464F1A` ..) | its note is the stock's debug text "loadsaveUI.cpp: THIS MSG SHOULD NEVER APPEAR!" (the PC device never sets it) | the same |
| the save list (`0x004FDF70`) | FindFirstFile failing with ERROR_FILE_NOT_FOUND on SAVE goes on with FindNextFile on no handle and waits there | the list ends (FindFirstFile of a folder always finds its "." entries) |
| the replay's next car view (`0x00403E40`, view 8) | its zoom's kind (`+0x1C`) and the zoom's divisor of kinds 0 and 1 (`+0x20`) are set by nothing, not by the constructor (`0x00401760`) either, and the camera's memory comes uncleared from the game's heap (`0x0059BA60`): whatever the block held | 0 and 0, as fresh memory: kind 0, whose aim 200000 / 0 is held at 1 (0.01 beyond 2 km) |
| the free views (`0x00406DA0`, `0x00407310`, views 14 and 15) | their actions' slots of the player's table (`+0xEC` .. `+0x108`) are never filled by `0x004080D0` (-1): the zoom, the turns, aside, up and the roll read 0; the mouse hook's moves (`[0x007289AC]`, `[0x007289A8]`) both take x and are read by nothing; the still view's moving branch (`+0x1E8`) is never on | the same |
| a car of a garage's scene (`0x00488EE0` → `0x00487FE0`) | it has no competitor (`+0x494` 0): turned over for 4 s it is put back on its line (`0x00487080`) and a body contact ahead is noted for its competitor (`0x0049184F`), both reading address 0 | neither: without a race the wheels' effects leave out the line and the impacts |
| the replays' save device (`[game + 0xA0]`, `0x004FA600`, `0x004FA810`) | its replays' object (`+0x0C`) is set by nothing on the PC (the constructor `0x004FA8E0` leaves it, `0x004FA8B0` is called by nothing): writing or reading a replay calls through address 0 | none is given either: the write's source has no files and a read counts as corrupt (the replay's slots 13 and 14 are ported, `ReplaySaves`, and work when given) |
| a saved replay read (`0x00429550`) and the attract's (`0x00429720`) | the blocks written (`+0x08`) become the header's last block, one less than the blocks there (0 for a replay within block 0): saved again, its last block is left out | the same |
| a saved replay read (`0x00429550`) | the header's bytes (`+0x7C8`) are read into the buffer whatever they are: more than its 48 blocks run past it | cut to the buffer |
| a replay's `USERDATA` written (`0x004288B0`) | the last byte of the date's text, of the track and of the route and the driver's last character keep what the stack held; the driver's name is copied whole (`wcscpy`): 16 characters or more run past its field | zeros; cut at 16 characters |
| a replay's `USERDATA` (`0x00502420`) | 0x79 bytes are copied whatever the file's size | a shorter file's missing bytes read as zeros |
| savereplay's failure (`0x004654CE`) | asks the PS2's "save the replay on memory card" (434), again each frame while unanswered | the same |
| savereplay's other flags of kind 1 (`0x004658D7`) | end the game (`0x0059AF30`) | nothing |
| autosave's space note (`0x0046505D`) | the flag's line, "KB", " are required", " to save this driver.": line 4 and the size are left out | the same |

## The interface (`04-interface.md`, `01-startup.md`)

| where | what | OpenJuice |
|---|---|---|
| UVScroll (`0x005185D0`) | the scroll less its whole sixteenths (v x 1/16 cut toward zero, x 16) is loaded back as unsigned: a negative scroll gains 2^32 and its texture coordinates leave any useful range | the same |
| the screenshot's name (`0x00513C60`) | a file of the name holding no bytes counts as free and is written over | the same |
| hudcolourmenu's dials (`0x0043ED70`) | the highlighted dial asks for `ui_dial_choose`, a texture the game's files do not have (they have `ui_dials_choose`): the highlighted dial is drawn with `ui_dials_off` | the same |
| the second player's lost controller in the front end (`0x0046C228`) | its note keeps the PS2's text "To continue, please reconnect the DUALSHOCK®2 analog controller into controller port " (1632) and the number; the first player's is emptied before it opens | the same |

## Movies (`25-movies.md`)

| where | what | OpenJuice |
|---|---|---|
| the open of a movie of one sound track (`0x00559CF7`) | it is opened again with track 0 while `+0x68` keeps the language's track: `BinkSetVolume` names a track the movie does not play, so its volume and its muting never reach the sound | the same |
| the step's volume (`0x0055A790` before `0x0055A3C0`) | the element's draw sets the volume in hundredths of decibels (0 for full), which Bink takes as silence, until the step sets it as x 32768 | the same |
| the open without a rad3d (`0x00559EE4`) or a file Bink does not open (`0x00559E55`) | reads through a null pointer | the player has no movie |
| the last tiles of a tiled image (`0x0055B2F0`) | their power-of-two sides are copied from the frame whole, past its edge | the same |

## GameSpy Presence (`28-network.md`)

| where | what | OpenJuice |
|---|---|---|
| `online_create_gs` / `online_connect_gs` (`0x004720EF`, `0x00472B1F`) | the login waits in a loop inside one frame (`Sleep(50)`): the screen holds; the create window loops for ever on the states 1 .. 8 (a gpcm that does not resolve gives 3, then 1), the connect window on 2, 4, 5, 6, 7; a server that never answers hangs both | the same |
| the login's own info (`0x0055ED12`) and the buddies' (`0x0055E541` ..) | asked blocking: a server that does not answer hangs the frame | the same |
| the error callback (`0x0055E230`) | any error clears the logged-on flag, a refused buddy request too | the same |
| the error code 4 (database) | shown as "Invalid password for this email account" | the same |
| `gpConnectNewUserW` (`0x00569160`) | the email is narrowed into 32 bytes the password's follow: an email of 32 characters or more is sent as its first 32 and the password | the same |
| the account (`0x00472322`, `0x00472DA4`) | kept in settings.dat in plain text, before the login is known to be good | the same |
| `gpSetStatus` (`0x00569670`) | the strings sent are not kept, so the same status with its strings is sent again | the same |
| the presence's timer (`0x0055BB42`), the friends' notes (`0x0055EA2C`, `0x0055EC6E`, `0x005603FD`), an invitation (`0x00560AAE`) | the first local player is referenced and never let go | the same |
| the friends' refresh (`0x0056052C`) | a request gone after its 10 refreshes ends the refresh: the friends after it are not aged | the same |
| a new friend (`0x00560120`) | its session and same-game flag are what the stack held | 0 |
| a user object made while an earlier one lives (`0x0055E15B`) | the connection global is cleared, the earlier connection left open; the earlier object's sign-out later destroys the new one's connection (within its own callback when the friends are made again) | the earlier connection kept, never destroyed; the new one goes once its processing returns |
| a sign-out logged in (`0x0055FAC8`) | the connection is reset before `gpDestroy`, so gp.info is not written | the same |
| gp.info's `sex=` (`0x00576349`) | "Female" is read as Pat (the second letter compared with 'F') | the same |
| the online setups' nick (`0x00473F84`) | a nick of 16 characters or more puts its 0 at the setup's +0x24 | the same |

## The sound library (`26-sound.md`)

| where | what | OpenJuice |
|---|---|---|
| the EAX probe (`0x005B8D38`, `0x005B8D47`) | the device and the primary buffer are `AddRef`'d and never released for it: neither is freed when the system ends | the same |
| the occlusion's scale (`0x005C22E5`) | `[0x00725858]` (0.5) is overwritten with 0.1 the first time the device is seen without its flag 0x10, for every later call too | the same |
| the occlusion's step (`0x005C2321` .. `0x005C2353`) | the voice's occlusion moves by 0.25 toward the value without being held at it: a value between two steps makes it go back and forth by 0.25 each call | the same |
| a stream set's occlusion | its table ends before the entry `+0x88`; called, it would be the next table's destructor (never reached: streams are not 3D voices) | nothing |
| a 3D voice's place (`0x005C211E`) | asks for the min distance `[0x0079E9C8]`, never set: 0. Windows refuses it (the buffer keeps 1); Wine takes it and silences every 3D voice (the stock game under Proton: no cars, no world) | not asked for: 1 everywhere |
| the quietest voice (`0x005C1B60`) | the unrolled loop adds the last 4 × (n / 4) volumes to the slot holding 1 / n and only the first n % 4 to the sum: below four voices the quietest is compared with their mean, from four on with that product (0 for a multiple of four: the quietest taken unless silent) | the same |
| a new slot still used (`0x005BCF45`) | the voice the set's free search took stays busy as the instance gets none that frame: a set of one voice never played (its start 0, so the oldest search finds nothing) can give no voice for the rest of the race | the same |

## The startup's texts and code nobody calls

| where | what | OpenJuice |
|---|---|---|
| the startup's texts (`0x0059BC2C`) | asked before the game's language is loaded, the reference build, made for another region, reads them in that region's language (a constant) whatever the game's | the game's language (the region binding removed) |
| the material's text (`0x00523276`, nothing calls it) | a texture slot's text is the texture object itself | an empty text |
| `juicey.dat`'s write (`0x00501846`) | a file's name is copied whole into its 128-byte field: a longer name runs past it | cut at 128 |

## The debug pages and the crash report (`01-startup.md`)

| where | what | OpenJuice |
|---|---|---|
| the pages' font (`[game + 0x74]`, `0x0040BB21`) | never set: the profiler's labels, the memory page's texts and the pages' text line would call through address 0, the memory page's rows divide by a line height of 0 | the texts are left out; never reached, as nothing shows the pages |
| a page removed (`0x0059CDD0`) | the only page, current, stays current after it is freed | none current |
| the profiler's page object (`0x0059D030`) and the memory page (`0x0059D910`) | freed, their pages stay in the manager's list; the memory page does not let the manager go | the same (never freed) |
| the memory page's tree (`0x0059DE80`) | built of every tracker and its groups, but only the root ("Total") is drawn | the same |
| the memory page's gather (`0x0059DBB0`) | the trackers' slots are taken by the count of slots not empty: with a gap, the wrong slots (or empty ones) are read | the same, an empty slot skipped; never reached, nothing registers a tracker |
| a group's name (`0x005A4250`) | for an empty file name the text starts at the character before it | empty |
| the debug camera (`0x0049F910`) with `+0x54` | it steps the first player's camera (`0x00406300`), which calls it again, without end | the same; never reached, nothing sets `+0x60` |
| the pool's statistics (`0x0040D9F0`) | the pool's use is the stock allocator's (`0x005A10AB`) | OpenJuice has no pool: 0 |

## The split screen race (`29-split-screen.md`)

| where | what | OpenJuice |
|---|---|---|
| the second player's note of a lost controller in the race (`0x00457C24`) | the text is only "<device + 1>.", the reconnect line is left out | the same |
| `hud_pause`'s PLAYER (`0x0044EC31`) | "Player", " " and " %d": two spaces before the number | the same |
| the second camera taking its car (`0x0041E0B0` for each player) | 0x00401DB0 turns every other car's sounds away: the first player's car's too | the same |
| the views through the post effects' target (`0x00516880`, `0x005211B0`) | the scene goes into the whole target and the copy is of the target's size: with motion blur, depth of field or a director's cut the views do not keep their parts of the screen | the same (to be seen in the stock game) |
| a solo race's or cruise's restart (`0x00401E80`) | every player's camera takes the first driver's kept view (`+0x8C`), not its own | the same |

## GameSpy's HTTP (`28-network.md`, "The statistics")

| where | what | OpenJuice |
|---|---|---|
| `Content-Length:` (`0x0057121A`) | the value is read from 16 characters after the name's start: the character after the colon is skipped (without a space, the first digit) | the same |
| the headers' end (`0x00571001`) | ended by `\n\n`, the 0 put after them falls on the body's first byte and the body is taken from two bytes on | the same |
| the connect's select (`0x0057074A`) | its value is kept as its low byte and compared with -1: a failed select reads as one with nothing set | the same |
| the status line not matching (`0x00570A6B`) | the place after it (`%n`) is not set and is read | 0 |
| a post's URL-encoding (`0x0056F9C0`) | a character over 0x7F takes its signed nibbles' "digits" from the bytes before "0123456789ABCDEF" | the same bytes (never reached: no post is made) |
| a post's string partly sent (`0x0056FA8F`) | the rest is sent from the string's start again | the same (never reached) |
| a request whose buffers cannot be had (`0x0056F480`) | its freeing counts the requests in use down though they were never counted up | the same |
| the leaderboards' row binding with no player yet (`0x0055D080`) | it goes into the row before the first, outside the rows | not kept |
| a table's names past its first line's count (`0x0055D5BA`) | written after the names' storage | not kept |
| a table read with no columns bound (`0x0055D52E`) | the read goes on without end | it stops |
| a table's first line below 0 (`0x0055D529`) | the names' resize to it fails | no names |
| FinishedRace (`0x004ECA82`) | each session player looked at has its user referenced (vt+0x10), never let go | the same |
| the reports' users (`0x0041FCD8`, `0x0044F276`, `0x004550D4`, `0x00462E31`, `0x0055BA81`) | a local player's user taken referenced (vt+0x38) and never let go | the same |
| FinishedRace without our player among the session's (`0x004ECB60`) | its race read at address 0x10 | a race of zeros |
| a crew race's FinishedRace (`0x00453420`, hud_race_results' leave) | not sent: it waits for the next report's send | the same |
| a rating against a racer in our own place (`0x004EC980` with 0) | its step is added | the same |
| online_stats' highlight without our nick in the table (`0x0047EA67`) | the sixth row is highlighted | the same |

## The configuration tool (`30-juiced-config.md`)

| where | what | OpenJuiceConfig |
|---|---|---|
| the DirectX check (`0x0040543B`) | DirectX 9.0c or later by DxDiag's letter (`'c'` or later) whatever the major: DirectX 10 .. 12 report a blank letter (Windows 11: 12.0 and a space), so the tool shows "DirectX 9.0c needed" and never its window | fixed (a departure): a major past 9 is enough, the letter counts only for 9; without DxDiag's answer the Direct3D 9 interface made with SDK version 32 answers it |
| the start without DirectX 9.0c (`0x004054A2`) | Direct3D's interface is not released, nor the class unregistered | the same |
| the window class's count (`0x004020B0`) | raised also when the registration fails or the game's window is found | the same |
| eax.dll's EAX (`0x004058D0`) | each question loads the library again and makes a device never released; the window asks as it is filled and on each turning of the controls with All Audio | the same |
| the texts (the string table) | the French, Italian, German and Spanish texts went in through the Cyrillic code page: accented letters read as Cyrillic ones | their own letters back, the texts through the wide functions (each language whole on any system) |
| the registry's reading (`0x00402ADC`) | a missing `threading` takes the defaults over every value read, and the antialiasing's list is made as by Reset To Defaults | the same |
| the threading's box (`0x00402532`) | checked by the processors' flag ANDed with the registry's value: a `threading` of 2 leaves it clear | the same |
| a saved `Shaders` of the list's count or more (`0x0040288B`) | the selection is set past the last item: none, and OK keeps the window until one is picked | the same |
| the modes' keys | widths from 4096 lose their high bits (5120 reads as 1024) | the same |
| the registry's values' sizes (`0x004027AE` ..) | one size slot for every read, set to 4 or 16 before some only: a value bigger than 4 bytes can be read past its 4-byte variable | each read with its own size |
| the adapters' identifiers (`0x00401201`, `0x0040294D`) | without a saved `Adapter`, the stack's bytes are compared | zeros compared |
| the caps (`0x00401000`) | a failed `GetDeviceCaps` leaves the stack's bytes as the caps | zeros |
| a text that does not load (`0x004012C0`, `0x00401950`) | `sprintf` is given a null format | an empty text |
