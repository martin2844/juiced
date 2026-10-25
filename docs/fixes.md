# The unofficial patch 1.02 (the fixed build)

The branch `unofficial-patch/1.02/dx9` is OpenJuice's fan patch of Juiced: the retail branch of Juiced 1.01 with the
official patch (`retail/1.01/dx9`, `docs/spec/patch-1.01.md`) with the stock game's problems fixed and a few things of
its own (online on OpenSpy, its chat, a guest's login, ..). It grows from the retail branch and is rebased on it
whenever it changes (until 2026-10-24 it was `fixed/1.00/dx9`, on `retail/1.00/dx9`). The code and this file call it
the fixed build. `docs/spec/stock-problems.md` lists the problems the retail port repeats on purpose; this file says
what the fixed build does with each. Its addresses are those of the build `retail/1.00/dx9` follows, as the specs'
(the official 1.01's own are in `patch-1.01.md`).

A fix goes in on its own only when nothing changes with the game's own files and settings: memory written or read
past its place, references never let go, loops without end, lines of a data file past a table that the game's files
never reach. Everything a player could see, hear or feel waits for a decision (below, with the proposed fix); the
decided ones are fixed in the second table.

The patch's version is 1.02 and the retail branches' release (`1.02.5`, the resource's numbers 1.2.0.5; made again
with each retail release): the executables' version resource and the log's first line, which also names the game it
plays, Juiced 1.01 (`e37ac5c` with "-fixed" on 1.00; `src/Game/App/Version.h`).

## Fixed

| problem (`stock-problems.md`) | the fixed build | commit |
|---|---|---|
| the programs made again after a device reset (`0x0051B620`): one without a usable technique is made again for ever, the game hangs | each made once (the making is the same every time) | `d1a615a` |
| the EAX probe (`0x005B8D38`): the DirectSound device and the primary buffer referenced for good; the device outlived the sound system's end and eax.dll was freed under it | no such reference | `889e779` |
| OpenJuiceConfig's EAX question (`0x004058D0`): eax.dll loaded and a device made at each question, never freed | both let go after each question (the same answer) | `889e779` |
| OpenJuiceConfig without DirectX 9.0c (`0x004054A2`): Direct3D's interface not released, the class not unregistered | both let go | `889e779` |
| `CMSCrews.txt`: more `DriverName`, `ShortDriverName`, `RespectFactor`, `Circuit`, `P2P`, `Cruise`, `Sprint`, `Illegal` lines than slots; names longer than their field; `WillAttendAt` of no crew | the lines past the slots passed over, a name cut to its 25 bytes, the word passed over | `3bfadc8` |
| `CMSCrewChallenges.txt`: more than 3 challenges | passed over | `3bfadc8` |
| `ArcadeRaces.txt`: more than 6 `Car` lines, more than 20 `Track` lines (and `BonusTrack`, and an `[UnlockTracks]` past the 17 lists) | passed over | `3bfadc8` |
| a route's `.spl` with more than six `GRIDPOINT` lines (`0x004A1410`) | the six kept, the rest passed over | `3bfadc8` |
| the automatic modification's decal (`0x0048F392`) of a list of 2 .. 9 decals: an unsigned wrap, a decal past the list | decal 0 | `3bfadc8` |
| the online setups' nick of 16 characters or more (online_main, a searched session): its 0 over the playing flag `+0x24` | the flag kept | `0c4b094` |

The game's files reach none of the data rows (checked: crews, challenges, arcade races, 126 `.spl` files; the cars
have 31 decals each); unit tests `CrewFileBounds`, `ArcadeRacesBounds`, `Route.SeventhGridPoint`.

Decided (2026-10-23):

| problem | the fixed build | commit |
|---|---|---|
| the HUD's times (`0x0044FBD0`): hundredths 00 .. 98 (the HUD, the results, the records, the challenges) | 00 .. 99 | `f20765b` |
| the detail factor (`[0x0079E670]`): the saved detail did nothing after a start until the graphics options were left | made from the detail when `reg.cf1` is read (a driver loaded) | `f4f80e4` |
| 8-bit names made wide (`0x0040ACE0`, `0x0049A850`): a byte from 0x80 up no letter | the game's files' names in the language's code page (1252; 1250 Czech, Polish; 1251 Russian; the car names of arcade_race and instant_manu too), `opts_voice`'s devices in the system's | `ba10c28` |
| the arcade map (`0x0045BEC0`, `0x004627D0`) and a meeting's showoff map: the first place in any language but English | the place by the location's English name, in every language | `7627fd6` |
| OpenJuiceConfig: modes 4096 wide and more (5120 read as 1024); no `threading` took the defaults over everything; the threading box clear for 2; a saved `Shaders` past the list selected none | kept; the threading alone defaulted; checked for any value but 0; the last item | `249913d` |
| the exposure's measure (`0x005213B5`): the glow's blur and the cut's transition went into the 128 x 64 measure, not the screen | onto the screen (the measure noted as the target taken) | `6074bf0` |
| the depth of field's blurred quads (`0x00520FC0`): uneven u v offsets | one offset a set at every corner | `45a8f95` |
| the fixed-function lights (`0x00546419`): diffuse green = red | the colour's green | `410d9da` |
| a car's glass (`0x00550500`): drawing no depth, the wheels' smoke and the far side's decals behind it went over it (the glass looked gone) | the glass first among the late parts, writing its depth while drawn; every car's opaque parts before any car's late ones | `da2d118` |
| SPLASH (`0x0050AC94`): the distance test decided nothing | a splash left past the reach put on the road again | `0adc767` |
| crew logos (`0x00718A28`): logo 19 never offered | in its place (01 .. 20); a saved place 18 is now logo 19 | `349be75` |
| the camera's drops, SMEAR (`0x0050F0D9`, `0x0050F2C7`): the view's bottom taken at its right side's x; a blob past its life wrapped to nearly opaque | the bottom; held clear | `a211db3` |
| LEAVES (`0x00508C60`, `0x00508970`): turns 1 and 2 never taken; half the leaves drawn | the index % 4; every leaf | `4ec2fc7` |
| the render tree's regions (`0x00544B90`): the tool's leftover bytes added to the regions | the nodes' own (the walks test each instance: nothing drawn changes, less walked) | `acc0afb` |
| the reflection cubes (`0x00547690`, `0x00547D50`): the view drawn with the world cube's last face's constants; c10 with the cube before's planes | the view's constants again; the planes first | `6a9b830` |
| lights (`0x00514E80`): colours sorted without their directions, to c26 .. c28 (the shaders read c26, c28, c30): the track's light 2 took light 3's (darkest) colour, light 3 none | each colour beside its direction, unsorted, in c26, c28, c30 | `a49b5cf` |
| the exposure (`[0x00722EA8]`, `0x00521308`): 0 at the start, the first frame with the glow black; the copy placed at -0.5 / width pixels, the copies without the glow at 0, 0 (each pixel's centre on the corner of four texels: the picture blurred while the motion blur, the depth of field or a cut drew the scene into the target) | 1 at the start; every copy half a pixel up and left | `7e95ec5` |
| the tyre tracks (`0x0050D490`): the first entry drawn took the level before it was held, its alphas wrapping past a byte | held before each entry | `bc65977` |
| the world's clock (`0x004263E0`): the lead the last pause's length, so after a second pause the world's time jumped on | the pauses added up | `7bef76e` |
| RAIN (`0x0050C33A`): 2 x count - 2 triangles, the last streak not drawn | two triangles a streak | `6a258d2` |
| `<car>_PRIMER` (`0x0048C210`): only the slots' meshes took the body material (the S2000's chassis piece, the Golf's farthest logo grey) | every mesh of the car's model | `c3d356d` |
| a Dial's needle (`0x005181B0`): the second corner's y moved onto the edges as an x, its x as a y | each by its own axis | `06ec1b1` |
| the graph's line (`0x004F8940`, car_rolling): in pixels to 2 x the points, neither scaled nor moved by the place | from the place, 2 x the points long, scaled as the series | `39c6655` |
| hudcolourmenu's dials (`0x0043ED70`): the highlighted dial asks for `ui_dial_choose`, not in the game's files | `ui_dials_choose` | `81c822c` |
| the count before the start (`0x004A0220`): a count landing on exactly 0 stopped there, the start never came | the start at 0 too (in `RaceState::update`, the game's count; `RaceStart` is an unused port) | `77962f2` |
| `start_replay`, `restart_replay` (`0x00455D60`, `0x00456030`): the play started only on the frame the count reached 0; transitions still running, never | the count held at 0 until they end, the play started once | `9965496` |
| the attract's replays (`0x00467260`): back to the first at 2, `attract3` never played | 1, 2, 3 | `27848a5` |
| a replay's frame (`0x004286E0`): the throttle read from the whole byte, with any part knocked off 1 | its two bits | `9fc7499` |
| the HUD's new lap (`0x0044B1C0`): the first player's messages emptied at every player's lap (split screen) | the player's own | `5f456e6` |
| a solo race's or a cruise's first view (`0x00401E80`, the load and the restart): every split-screen player the first driver's kept view | each player's own | `7153f43` |
| the results' empty rows (`0x00451C90`): Team "--:--:--" | "----------" as Name and Car | `d5792a7` |
| `hud_challenge_results` QUIT (`0x00450F80`): "QUIT RACE?" made and dropped, the race left at once | asked, as RESTART | `8c938e7` |
| `CMSCrewmates.txt` `PhoneNumber` (`0x004A9FE0`): read before the crewmate's check, the file's last number for all (the game's four have one number; nothing reads it) | each its own | `89574a7` |
| the betting window, steps 27 and 29: a raise or lower not taken showed the money cut, the amount kept it rounded | shown as kept | `76cb161` |
| CMSCheats' letters (`0x004C55B0`): UP or DOWN with no letter highlighted turned the leaving flag's byte, back to extras | nothing | `9d90e66` |
| the crews' results table (`0x004529E0`): the latest time kept from row to row, a crew's row showing the latest of the crews above it too (hardly seen: the crews come in the order their last driver finished) | each crew's own | `6782573` |
| the betting window, step 7 (`0x0042F910`): `rand() & 1` compared with 1 and 2, `SUGGEST_BET3` never played | one of three, as the other lines (TK's third take; the others' movie open takes 1 or 2) | `21baea9` |
| credits' third page (`0x00469D90`): a line shown by the second page's line (the game's languages read the same either way) | by its own | `93095a2` |
| opts_game_pc's BACK (`0x00481770`): AUTO2 took AUTO's kept value, the difficulty not given back | each its own | `c6d2082` |
| calling a crewmate who joined (`0x004B6710`, `0x004D2140`): the ringing held, SELECT did nothing, only BACK left | rings down to END CALL, SELECT answers, as any call | `948dcb8` |
| the decal's colour in `colourchoose` (`0x0043DB90`): the chart looked for the pearl colour | the decal's | `6883f5d` |
| BACK in `wheelcolourmenu` (`0x0043F600`): a car without a listed wheel colour got the third (also taken as its own) | its own back; the third as any | `6883f5d` |
| the custom race's sprint route (`0x0045DFB9`): a route at place 1 counted as none (the game's tracks have theirs at 9 or 10) | none is -1 | `cc00cc0` |
| `instant_setup`'s route page (`0x0045C6C0`): the race's route highlighted by its place among all the routes | by its item | `cc00cc0` |
| `Arcade_Championship` (`0x0042DA40`): the kept championship highlighted by its number, not its item | by its item | `f1877ed` |
| `CMSRespect`'s territories (`0x004DBE60`): the place given to the list's entry k + 1 (the game's files have all nine: the same) | each its own | `326495d` |
| the driver window's failed delete (`0x004679B0`): "Could not delete driver." closed the next frame by the note's flag | the note closed first | `400c1a2` |
| the screenshot's name (`0x00513C60`): an empty file of the name written over | every file there is passed | `f9c3231` |
| a new voice slot (`0x005BCF45`): the set's voice left busy, a set of one voice could stay silent for the race | given back, as when no slot is found | `56451cf` |
| a movie's volume (`0x0055A790`): hundredths of decibels to Bink, silence until the step | x 32768, as the step | `7461223` |
| clouds (`0x005151D0`): c49 set as the static after the cloud constants (its init guard); no shader of the game reads it | not set (c45 .. c48) | `1ee6f2e` |
| the TV views' turn (`0x00403B00`): a pitch error below 0 taken off the yaw's and roll's rates too; the passing view (`0x004047C0`): the rise added to x and z | the pitch's alone; the height alone | `30f27e5` |
| the race camera's time (`0x0041FE50`): the step over the physics steps (2 or 3 below 20 frames a second) | the step | `166d29f` |
| a spline rider's start (`0x00423B80`): turned by +pitch (the cable cars too), its frames by -pitch when it banks (until the first frame only) | as its frames | `1f6074c` |
| `dynamic_<car>.txt` `HandbrakeMu` (`0x004F5700`): a value between 100 and 10000 made 10000 (27 cars, mostly front-wheel drive, give 3000, 4000 or 8000; 15000 kept) | the file's value (their handbrakes weaker) | `cde1f2d` |
| `cardefs.txt` `CUSTOM` without a supplier (`0x004995F0`): `rand() % 3` compared with 3 for SWEET, never | AERO, SWEET, NUUK a third each (SWEET's logos are in the game's files) | `d404415` |
| split screen's post effects (`0x00516880`, `0x005211B0`): the views shared the screen's pair of targets (each view's previous frame the other's) and drew the passes at the screen's places with the view's constants, the scene into the whole target: the lower view lost its picture with the motion blur (any speed), the depth of field, a cut, the screen's smears | a view not the whole screen its own pair of its size, its passes from (0, 0), the back buffer at its place (`Game/Scene/PostEffectsSplit.cpp`), the pairs kept by the view's place (the next race's views take them); the whole screen's view as the stock | `b81e733` |
| the car's light in the shade (`0x0048A644`): the wing mirrors and the lamps (`WMIR_L`, `WMIR_R`, `HEADLIGHTS`, `REARLIGHTS`, 22 of the game's cars have them) got no light step, so they kept the full light while the body darkened | the chassis's light, as the other parts | `a814b8c` |
| the crews' paint (`0x004A2300`, `0x004A9080`): the new career writes four whites, the fourth over the metallic amount (about -0), and the scheme puts the crew's amounts over its 0.5 and 0.1 (the Xbox builds of 2004 the same; no other value found) | the scheme's 0.5 and 0.1 kept | `b1f8714` |
| an interface view's sun (`0x005B4660`): the reflection on the cars looked up by SunFlare's name (not drawn in a view) | SunReflection's, as the race's sun | `50146fe` |
| a car part's skirts (`0x004191C0`): the right skirt took the left's bit | 0x20 | `ac33868` |
| the atmosphere zones (`0x00418BF0`): outside every zone the last blend stayed | no blend | `682856c` |
| the smoke's colours (`0x005059B0`): the alpha x 0.8 twice | once (the smoke more opaque) | `c06473c` |
| the graphics defaults (`0x005A26C0`): the ATI cards' device ids cases of the vendor's switch, every ATI card class 4 | by the device id (an unlisted ATI card still 4) | `b186399` |
| a movie of one sound track (`0x00559CF7`): the volume and the muting named the language's track; a tiled movie's last tiles (`0x0055B2F0`) copied past the frame | track 0; cut to the frame | `12a14bc` |
| the occlusion (`0x005C22E5`, `0x005C2321`): 0.1 over the 0.5 for good, the step swinging by 0.25; the quietest voice (`0x005C1B60`) compared with no mean from four voices on | by the device, held at the value; the mean | `ad0f787` |
| split screen's car sounds (`0x0041E0B0`, `0x00401DB0`): the second camera turned the first player's car away | a car another player follows left turned to it | `79a43df` |
| `hud_pause`'s PLAYER (`0x0044EC31`): "Player", " " and " 2", two spaces before the number | one | `515db08` |
| the mesh collider against a wall (`0x0053B8D0`): no zone, and the offset of the last face of the pass: after a face of -12 or -6 (the game's routes have such faces) a car's touches with the walls 12 or 6 cm shallower and behind the wall | no offset, as the wall's sphere and line touches, and the wall's zone | `c363ff8` |
| a face cut by the clipping planes (`0x0053B8D0`, `0x0053D0C0`): after an odd number of planes the last cut lost, one plane leaving the face uncut (the game's one definition with planes, the tyre wall, has two) | the last cut | `c363ff8` |
| a clipping plane's crossing (`0x00538310`, `0x005394E0`): a crossing rounded just off its edge was counted but not written, the buffer's leftover a corner | held to the edge | `c363ff8` |
| a sphere beside a face (`0x005388C0`, `0x00538760`): the first edge met gave the point (the tyre wall on the route's faces, a car's mesh collider against the parts' sphere collidees) | the nearest edge's | `c363ff8` |
| `dynamic_box.txt`, `dynamic_bin01.txt`, `dynamic_bin02.txt`, `dynamic_rbump.txt`: `[BeginMeshCollidee]` closed by `[EndSphereCollidee]`, a sphere collidee of radius 0 made and the mesh collidee's Mu and Restitution left 0: a car slid along Southside Beach's bins (its routes 1 and 2; the box stands on no route, the bumper's body never joins a world) with no grip and no bounce | the block closes the mesh collidee: METAL, Mu 1, Restitution 0.5 | `f030beb` |
| the AI's swerve (`0x0049C720`): a car stopped behind another swerves for 1.5 s, the swerve added once for each neighbour (three cars near, three times) | once a frame | `da6948d` |
| a car's contacts (`0x004916F0`): the other body read from the step's last rigid contact, touching or not (a car that came apart within the step still a body's hit, x 4; the AI's brake behind a car it touches) | the last touching contact (the bodies' contacts come after the route's) | `da6948d` |
| a hit's amount (`0x004916F0`): the normal's z + 1 past 2 replaced by the excess (only by rounding, the normal is a unit) | held at 2 | `da6948d` |
| the car's impact (`0x004916F0`, `+0x174`): always the last piece's 50 after a hit, so the wheel's jolt (x 255), the camera's shake, the pad's rumble and the wheels' smoke were those of the heaviest hit | the hit's amount, at most 50: a light hit a lighter jolt (the smoke, the rumble and the shake's size are full from 1 on, the shake's period from 2.5, so heavier hits are as the stock's) | `da6948d` |
| an open spline's length (`0x0041B720`): the last point's way back to the first summed too, `EVEN_SPACING` spreading the riders over it (the game's six open splines have one rider each: for mods) | ends at the last point | `e7ad1d8` |
| a tyre rolling backwards (`0x004F7C40`): with its wheel straight the Magic Formula's longitudinal force taken off and added back, none left (above 8 m/s: a car sliding back after a spin, or reversing fast, had no grip along its wheels; the rear wheels, never steered, always) | taken off once, as a steered wheel's (tested by the community, kept 2026-10-24, as the five below; test) | `b89c206` |
| the slow body's brake (`0x004F2AFE`): under 0.3 m/s the speed loses 2 dt and below 2 dt turns the other way (a car or a prop at rest shook by up to 2 dt m/s) | stops at 0 (test) | `b89c206` |
| the car's tilt held (`0x00489740`): the pitch's torque from (pitch / pi)² - 0.3, which the pitch (an arcsine, within ±90 degrees) never passes, and the square lost its side | the pitch over its own range, pi / 2 (as the roll over pi), with its side: from 49.3 degrees a torque towards level, up to the 60 degrees hold (a guess of the meant threshold; test) | `b89c206` |
| a car made ready again (`0x004894C0`, `0x004896A0`): a turbo lost to the damage came back at 1.3 whatever the car's own (a stock turbo's 1.01, the parts' 1.25 .. 1.4), until the race's end | the car's own Turbo (test) | `b89c206` |
| a tyre contact's drive (`0x004F6050`) and the traction control (`0x004F6F30`): by the wheel at the contact's place in the step's list, not the contact's own; with a wheel in the air the drive slid along the list (a rear-driven car with its front wheels up had neither the throttle nor the traction control on its rear tyres; a front-driven one with a front wheel up drove a rear tyre) | by the contact's own wheel (test: 350Z, Integra) | `389826f` |
| a body's limits (`0x004F1E10`): the angular momentum through the inertia of the state set before and back, at every hold (a held spin by the inertia of another orientation; L changed by the round trip with nothing held) | the held state's own inertia, the round trip only when a part of the spin is held (test) | `389826f` |
| a network game's pause (`0x0041E200`, `0x0041F6A7`): this machine's world stopped as off line (its car and the others' as it saw them) while the race's clock and the other machines went on | the game goes on under the pause window, the car without controls while the window has them (the community, 2026-10-24) | `c9bd82d` |
| a sprint's restart from the pause (`0x0044EF7D`): the session's racers' points cleared, their heats' time (`+0x134`) kept, so it decided the new sprint's ties (equal points, the less time first) | the heats' time cleared with the points (test) | `3eab40d` |
| instant_selcar's "AUTO-MOD CAR?" (`0x0045F340`): asked while the first four body parts are 0, so a mod's car whose model has none of them was asked again at each SELECT after the modification, until NO | asked also only without a neon, which the automatic modification always fits (the arcade collection's cars and cleared parts have none: the game's cars as before; test) | `ca8e92a` |
| a metallic or pearl colour in `colourchoose` (`0x0043DD60`): taken only with more money than the price, else as a base paint, the metallic or pearl unpaid (`colourmenu` lets the money equal to the price in: a metallic for the base's 1000, or nothing when the base was paid) | taken at the price; under it "INSUFFICIENT CASH" and NO (test) | `0f49d2b` |
| a pink slip's stake (`0x00434D90`, `0x00432696`): the player's car back in the garage as a copy with a new grid, its kept dents gone (a repair for nothing) | the stake keeps the car's grid | `0f49d2b` |
| the history's step (`0x004BD8A0`): taken by `0x004BAEE0` once the summaries' lists are empty, and again by the leaves of `CMSMeetingSummary` (`0x004CEC30`) and `CMSRespectChanges` (`0x004D0000`): two or three points of the stats' graph for one event | the event's one step, `0x004BAEE0`'s (test) | `0f49d2b` |
| modshop's upgrades (`0x00442A80`): a class taken again by a stronger set kept the meters (scaled again) and the parts of the weaker set | the class's set made anew (the draws as before; test) | `0f49d2b` |
| a privilege's entry of `CMSRespectChanges` (`0x004CF5A0`, `0x004D1050`): its crew in the change, 0 as the crew, so the player's crew's respect grew by the crew's number | no respect for a privilege's entry (test) | `0f49d2b` |
| a place of the crews' order no crew took (`0x004B9E20`, `0x004DA310`): the odds before their table read (the last racer's fields, the session's `+0x264`): after a cruise winnings written into the session's set-up flag; `CMSRaceResults` showed the row (the player's colours as a logo) and added winnings of those bytes to the money | nothing read nor written, the row hidden (tests) | `0f49d2b` |
| a showoff trick's line (`0x004ABD10`): a trick going on kept the list's last place, so a newer line (another trick, a special move, the zone's BONUS MOVE) took the points it went on adding, and its end's points (which decide the multiplier) were that line's | the trick follows its line up the list; past the top its points kept apart (test) | `0f49d2b` |
| GameSpy's HTTP, the leaderboards' requests: `Content-Length:` read from 16 characters after the name (`0x0057121A`: without a space the first digit skipped, `Content-Length:5` a length of 0 and no body); headers ended by `\n\n` (`0x00571001`: the 0 on the body's first byte, two bytes skipped); the connect's select kept as its low byte (`0x0057074A`: a failed select read as nothing set, the request waiting for ever); a request whose buffers cannot be had counted down though never up (`0x0056F480`) | after the colon and its spaces; `\n\n`'s own places; the whole value; counted before the buffers | `bed38d3` |
| the login of `online_create_gs` / `online_connect_gs` (`0x004720EF`, `0x00472B1F`): a loop inside one frame (`Sleep(50)`, the screen held), the create window waiting for ever on the states 1 .. 8, the connect window on 2, 4, 5, 6, 7, a server that never answers hanging both | a frame at a time (the game's loop runs the user object's frame); 9 .. 13 and 15 their notes, any other state "Failed to connect", no login in 60 s too | `a3fbf9f` |
| the account (`0x00472322`, `0x00472DA4`): kept in settings.dat in plain text, before the login is known to be good (a failed login replaced the account kept) | kept once logged on, protected (Added, below) | `a3fbf9f` |
| the profiles' info (`0x0055ED12`, `0x0055E541` ..): asked blocking, the frame processing the connection until the answer (no end while the server does not answer) | not blocking; our own nick the login's until the answer, a buddy's its record's before | `a3fbf9f` |
| the error callback (`0x0055E230`): any error clears the logged-on flag (a refused buddy request: the network lost, logged off); the processing's result into the state too; the database's error (4) "Invalid password for this email account" | after the login only a fatal error; 4 as any other ("Failed to connect" at the login); each error in the log with the server's text | `a3fbf9f` |
| `gpConnectNewUserW` (`0x00569160`): the email narrowed into 32 bytes the password's follow (from 32 characters on its first 32 and the password) | the email whole | `a3fbf9f` |
| `gpSetStatus` (`0x00569670`): the strings sent not kept, the same status sent again every 60 s | kept, the same status dropped | `a3fbf9f` |
| the first local player referenced and never let go: the presence's timer (`0x0055BB42`), the friends' notes and refresh (`0x0055EA2C`, `0x0055EC6E`, `0x005603FD`), an invitation (`0x00560AAE`) | not referenced | `a3fbf9f` |
| the friends' refresh (`0x0056052C`): a request gone after its 10 refreshes ends the refresh, the friends after it not aged | it goes on | `a3fbf9f` |
| a user object made while an earlier one lives (`0x0055E15B`): the earlier connection left open; the earlier object's sign-out destroys the new one's connection (within the new login's callback when the friends are made again) | the earlier connection let go there; a sign-out lets go only the object's own | `a3fbf9f` |
| gp.info's `sex=` (`0x00576349`): "Female" read as Pat | Female | `a3fbf9f` |
| the users looked at referenced and never let go: FinishedRace's session players (`0x004ECA82`), the reports' local users (`0x0041FCD8`, `0x0044F276`, `0x004550D4`, `0x00462E31`, `0x0055BA81`; the online career's `0x004EA100`) | let go after each | `f392c65` |
| a rating against a racer in our own place (`0x004EC980` with a sign of 0): its step added to both ratings | neither changes | `f392c65` |
| an online career's crew race's FinishedRace (`0x00453420`, hud_race_results' leave): not sent, waiting for the next report's send | sent at once | `f392c65` |
| the leaderboards' table reader (online_stats): a row by its first column alone, so a table whose fields run out early read the other columns past their ends (OpenSpy answers every table `1`, `0`, `0`, `0`, `CHC-Juiced`: top showoff scores read the car's empty column at address 0 and closed the game; GameSpy always answered whole tables) | a row only where the rank, the name and every column its category reads have it | `be6aeb6` |

Decided to keep as the stock (2026-10-23 .. 24):

- Cruise prizes (`0x004A33E0`): a showoff's class 8 reads past the 8 prize factors, the 0 after them, so the showoffs
  pay no money (their score and the respect are their reward); the game has no factor meant for them.
- CMSCheats' codes (`0x004C5CB0`): seven of the eight start with `1` (DOSH, RESP, CARS, CREW, CHAR, WIN., ALL. with
  their first letter turned into `1`), which the letters never reach: the developers' cheats turned off on purpose; only
  PINT (the arcade's) is open.
- The race's respect for a bet (`0x004BCD90`): crew 7's factor for every crew, by design.
- x87 precision (`07-dynamics.md`): each thread its own rounding, by design.
- A physics profile's hulls (`0x00421960`): the first object of a profile gives the profile its hulls and its centre of gravity, and every later object takes them. On Southside Beach's route 1 `dynamic_BIN01` serves 27 objects (5 mailboxes, 9 phone booths, 6 newsstands, 7 bins), all with the first mailbox's shapes. Tried with each object's own (2026-10-23): the phone booth's own collider sinks 72 cm into the ground, and the scene has objects standing in one another (a mailbox and a bin 5 cm apart, two booths 14 cm apart) that with their own shapes push each other through the ground; the game's placing was made with the shared mailbox. Kept.
- Z-Bias (the material classes keep it, the drawing never uses it): only the cars' materials set it (decals,
  logos, body, glass); it is Direct3D 8's ZBIAS (0 .. 16), whose Direct3D 9 depth bias would have to be tuned by
  eye, and no flicker of the cars' decals is known.
- `ClutchSpeed`, `SteeringSpeed` (`dynamic_<car>.txt`), `DrivingLimit`, `RecoverLimit` (`CMSCrewmates.txt`): read, never used. ClutchSpeed is 5 for every car (taken for every shift a shift up would take half its 0.4 s, every car and the AI's); SteeringSpeed 1, 0.8 for the Focus, the Focus 2, the 206, the WRX (the keyboard's and the pad's turn, not the wheel's); the limits are the 100 the code sets. Not used (the user).
- A lens flare (`0x00421D50`): `LIGHTFLARE_BRIGHTNESS` has nowhere to go (the only reader of that place is the
  sun's branch, where it is the sun's angle), and no flare of the game's tracks is on a moving object (the flares
  are in the lamps' files; the cars' flares move with the cars).
- The headlights' beam under the views on the car (`0x0041FF45`): it is the headlights' light projected on the
  road; the stock meant it hidden with the car, but seeing it from the bumper is right.
- gp.info at a sign-out (`0x0055FAC8`): the game resets the connection before `gpDestroy`, so the profiles' cache is
  never written; written, it would give stale nicks in later sessions (a cached info is never asked again).
- online_stats' highlight without our nick (`0x0047EA67`): the sixth row, where the table asked around us puts ours.
- The scene objects' steps (`0x00422BB0`): every other frame with that frame's time, so the skies' clouds, the
  other turning objects and the crowds move at half their rates; the tracks were made looking at that (the clouds
  are fast enough already).

Fixed though the game's own files never reach them (for mods and later use; the user: "what is not used gets fixed too"):

| problem | the fixed build | commit |
|---|---|---|
| 2-Texture Modulate (Specular) (`0x00527BAE`, `0x0052EC6B`): ask for `..Modulate`, the files are `..Modulare`: no program (no material of the game is of these classes) | ask for the files' names | `837112b` |
| Unlit Sign Scroll, UVScroll (`0x0052B0D0`, `0x005185D0`): a negative scroll loses / gains 2^32 (the game's scrolls are positive) | signed | `837112b` |
| `-localfx`: a flat procedural texture (`0x0051B5A6`): its mip levels its height | its `miplevels` | `837112b` |
| the HUD's ABS light (`0x0044B1C0`): without a `SPEED%d` the signed m/s against 5 (no window of the game has an `ABSLIGHT%d`) | the shown units | `837112b` |
| a showoff zone's name (`0x004B0F40`): `CRUISE_MESSAGE` read only with a `CRUISE_NAME` text (the game's helpers have both) | its own key | `837112b` |

SpecMap ATest (`0x0052F5DB`) asks for `Material_SpecMapATest`, which the game never had: it needs a new shader (no material of the game is of the class).

## Added

What the fixed build adds of its own (decided 2026-10-23):

| what | how | commit |
|---|---|---|
| the glow's strength: the graphics options' GLOW a meter, 0 .. 100 % (0 off), in place of YES and NO | the meter made as DETAIL's when `opts_graphics` is read (`Game/UI/WindowFixes`; the file stays as it is); `reg.cf1`'s glow kept as the percent (the stock's on, 1, and the defaults 50 %; a stock game reads any value but 0 as on); the blur added with 0x9F x the strength (100 % the glow as fixed above), x 0.7 where the cars' headlights are on (night or wet) | `a5fb7e8` |
| replays saved and watched on the PC: the stock's replays' device had no replays' object (`0x004FA8B0` called by nothing), extras took its REPLAY item out, and the replay's HUD never asked to save | the device takes the race's replay and `<My Documents>\THQ\Juiced\Replays` (beside the screenshots), a folder of the game's files a replay (USERDATA, replay.hdr, replay.dat) named `Replay_<day>-<month>-<year>_<hour>-<minute>-<second>` (`_2` .. in one second), no limit; the replay's EJECT asks "Quit Replay?" as the stock (its no the replay on), then after its yes "Save replay?" (the game's line 435) for a replay not saved: yes saves, then quits, no quits; savereplay's retry asks the same in place of the PS2's memory card; extras keeps REPLAY for loadreplay (a list of one item with arrows), whose delete asks jettext's "delete replay" line and which lists them by the system's short date and time of their writing; a saved replay plays no fly-by (the sweep, then the view kept, as a replay after a race) | `8b12a7d`, `4fa08bf`, `2ab2ae6` |
| online on OpenSpy: GameSpy's servers were shut down in 2014, so the stock's online modes find no one | the stock's nine service names on `openspy.net` in place of `gamespy.com` (`kServiceDomain` in `Core/Network/GameSpy/GameSpy.h`: the available check, QR2's master, the master list `juicedpc.ms3`, natneg1 and natneg2, the connection manager gpcm and the search manager gpsp, the statistics' `juicedpc.gamestats` and the leaderboards' `gamestats`); OpenSpy serves the same protocols with the game's name and keys (the user, 2026-10-24; JuicedEditor's "patch servers" writes the same over the stock exe) | `7a06aa7` |
| the GameSpy account protected (the user, 2026-10-24): the stock keeps the nick, the email and the password in settings.dat in plain text | settings.dat's fields written empty, the account in the driver's juicey.dat as its own entry `OnlineAccount.dat`, encrypted by Windows (DPAPI, `CryptProtectData`: only the same Windows user on the same machine reads it; the stock game ignores the entry as a mod's and sees no account); a stock save's plain account taken and protected at the next save (`Core/Saves/OnlineAccount`) | `a3fbf9f` |
| the others' cars online nearer to where they are (the user, 2026-10-24: "collisions are worthless at high speeds"): the stock carried a state on by how late it came against the first state only (`0x004E9430`), so the cars were the way here behind (at 250 km/h and 60 ms some 4 m) and met in other places on each machine; 1.01 only made them collide again near the player | each state carried on by the peer's one way time too, half its pings' mean (at most 0.25 s; the session pings its peers each second, the transport's pings the stock never turned on); the states sent each 0.05 s (the stock's 0.1 .. 0.2 s by the players); the smoothing towards them (0.07, 0.21 a frame) by the frame's time, a 1/60 s frame's as the stock's; a stock peer answered and read as before. A contact told to the other car's machine (both feel the same hit) is to be looked at apart | `17c4311` |
| online as a guest (the user, 2026-10-24: no GameSpy account, the matchmaking only, under the save's nick) | online_logon_gs's third item PLAY AS GUEST (the fixed build's text in every language): logged on without a connection under the driver's name, a profile of its own below -1 (the others' machines tell guests apart); quick join, search and create as an account's; no presence, no statistics, no FRIENDS or SCOREBOARD in online_main, no friends' windows from the network windows (keys 8, 9) | `04517be` |
| a chat in an online session's lobby (the user, 2026-10-24: as FlatOut 2's on GameSpy / OpenSpy, the lobby only; the stock game has the voice alone) | GameSpy Peerchat at `peerchat.openspy.net:6667` (`Core/Network/GameSpy/Peerchat`: the cipher's login, the channel `#GSP!juicedpc!<session id>`, the player's nick); link_lobby's SELECT2 (the pad's second button, PAGE DOWN; its hint CHAT with the lines unread) opens the game's keyboard window with six lines of the chat in place of its top frame: DONE or ENTER says the text, BACK or the session leaving the lobby goes back (`Frontend/Network/LobbyChat`, the elements made by WindowFixes) | `22ffea2`, `ed26a4a` |
| the cars' hits told between the patch's machines online (the user, 2026-10-24: collisions' point 4, only where the other is known to be a patch's) | a hello to each peer (`0x30`: "OJFX" and the protocol's version; the stock's session passes over the ids `0x1C .. 0xDD`, `0x004E96A0`), a patch's peer answering once and marked; the hits our cars gave another machine's car (`RigidBody::impulseWith`, `0x004F0F90`) summed each frame and sent to its machine (`0x31`) when it is marked; there its car takes them unless it met ours itself within 0.25 s | `6def657` |
| the wing mirrors: `cardefs.txt`'s `WMIRROR` parts are listed and offered (stylingmenu's wmirrors, modmenu), but 0x0048BC30 fits nothing for categories 5 .. 7 and the unlock schedule never opens 7 | `<part>_L` and `<part>_R` in the model's `WMIR_L` and `WMIR_R` (as the skirts), opened by their own steps of the model's unlocks (the first part from step 14, the second from 26, all from the last, 37; J1CustomCars ties them to the front bumper); for mods: the shipped file has its mirrors' lines commented out (and as `WMIR_L` / `WMIR_R`, no category) and no car's mirror meshes, so the stock cars keep their models' mirrors. The headlights (5) and the rear lights (6) would need their slots and, for 6, a menu item and an icon, their lights (flares, LIGHTSON, the brake lights) the part's: put off (the user) | `d0b601d`, `fca5863` |

## Waiting for a decision

Grouped by what the fix would change. "Bug" marks a clear mistake whose fix shows what was meant; "feel" a change of
handling, AI or balance that players of the stock game would notice.

### 1. Clear bugs, small visible fixes

All decided and fixed (2026-10-23; the decided table above).

### 2. Changes of the look or the sound (the stock game never showed them)

All decided (2026-10-23 .. 24; the decided table above, the kept ones below it).

### 3. Feel: handling, AI, balance, the career

All decided (2026-10-24; the decided table above, the kept ones below it, the rest under "Nothing to fix").

### 4. Texts and console leftovers

| where | the stock |
|---|---|
| autosave / saveprofile (`0x00464A10`, `0x00463F30`) | asks about the PS2's memory card (1994) |
| autosave's space note (`0x0046505D`) | line 4 and the size left out |
| the save windows' flag 19 (`0x00464F1A`) | "THIS MSG SHOULD NEVER APPEAR!" (never set on the PC) |
| the second player's lost controller (`0x0046C228`, `0x00457C24`) | the DUALSHOCK®2 text; the reconnect line left out |

### 5. Online

All decided (2026-10-24; the decided table above, the kept ones below it, the rest under "Nothing to fix").

## Nothing to fix

- **OpenJuice already does it safely**: `CMSCalendar`'s last highlighted button (`[0x0079E850]`: the stock kept a
  pointer to the window's element, freed with the front end before a race, so the first calendar frame after a race read
  freed memory, a random crash that J1Tools' calendar crash fix works round; OpenJuice keeps the name).
- **OpenJuice already does it safely** (the stock reads stack values, null pointers, divides by zero or writes past a
  list; the retail port gives 0, the first entry, nothing or a cut, as `stock-problems.md`'s OpenJuice column says): the
  effects' textures and `-localfx` but the flat mips, the weather's w values, the flares' stack values, every row of
  "Memory the game saves without setting it", the data files' null definitions and short curves, every "Game logic" row
  whose OpenJuice column is not "the same" (null reads, divisions by zero, lists past their end, stack values), the
  movies' null reads, the startup's texts, `juicey.dat`'s name, OpenJuiceConfig's registry sizes, adapters, caps and
  texts, the DirectX check (fixed in retail), GameSpy's rows whose OpenJuice column is not "the same" (a new friend's
  flags, the HTTP status line's place, the leaderboards' row binding, names, columns and first line, FinishedRace
  without our player, GameSpy Voice's device list).
- **The leaders' missing takes** (`COMMENT_CAR_NEG3`, `YES3`, `SUGGEST_BET2` of Carlos, ..): the movie open
  (`0x00559B10`) takes a name ending in a digit that is not there as take 1 or 2 (in the order of `rand() & 1`), so
  the leader says another take; only a line without take 1 and 2 ends at once. (`stock-problems.md`'s row says so since
  `58357b0`.)
- **No change for the player** (2026-10-24): the bet share with no money left (`0x004BC2E0`: infinity, which only ever
  meets the big bet's "over 7.5", as an all-in bet should); the nitrous drift outside cruises (`0x004AFE30`: its lines
  and the multiplier count only in a cruise).
- **Never reached in the PC game**: GameSpy's HTTP posts (`0x0056F9C0`: a character over 0x7F URL-encoded from the bytes
  before the digits; `0x0056FA8F`: a string partly sent sent again from its start; the game makes no post), a prototype
  part event's "any supplier" (`0x004A2C70`: `rand() % n` compared with -1, which never comes; the branch was never
  finished, its prize would be a part of no supplier, which the award does not find), an event round of a challenge -1
  (`0x004A33E0`: challenge events are made with their challenge 0 .. 2 only), the phone's answers 16 .. 19
  (`0x004B6B20`: nothing makes such calls, so type 18's 980 then 580 never comes), the debug pages and the crash report,
  the round index `+0x55` (always 0), the odds and the drivers past their tables, money's most negative amount
  (-2147483648), DemoFlyby without the speech object (made at the start), the windows of no record (`car_maintain`: no
  item of any `.ui` asks for it; `byebye`: the demo build's menu; `CMSCareerSummary`, `opts_ctl_ps2`, `opts_wheel`: code
  never registered), the free views 14 and 15, the replays' save device and the saved replays' last block, the debug
  camera, the scene step's 1 / dt, OpenJuiceConfig's class count (the tool ends right after).

OpenJuice's own known issues (the CHANGELOG's) are not stock problems: they are fixed in the retail branch.
