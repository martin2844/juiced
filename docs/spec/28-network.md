# The network

The game plays over the LAN and online through one transport of its own ("jetNet"); GameSpy's services only find
the sessions online and open the way through NATs, and GameSpy Presence (below) keeps the player's account, friends and
presence; the voice (below) is GameSpy Voice 2 with Speex, its packets on the transport's socket.
The reconnaissance of all of it is in `.ai/ONLINE.md`.

## The transport (vtable `0x006FD728`)

The object (`0x10C` bytes, `[0x0075F758]`, the game's `[0x0075EF8C]`) is made by `0x00599ED0` with a buffer of
0x800 bytes (`0x004E9C40`). Its constructor (`0x0059A3A0`, the base `0x00598760`) takes four random bytes for its
counter (`+0xF0`) and two UDP sockets on every interface, both non-blocking: **8666** (`SO_BROADCAST`) for the LAN's
search and **8667** for the game.

The random bytes (`0x00598710`) each seed the CRT's `rand()` again with a count (`[0x0079E898]`, one more each byte)
plus the object's address and take its next value's low byte (Stock problem: the game's `rand()` goes on from a seed
of where the object lies).

### The wire

Everything on 8667 is XOR-coded byte by byte by a stream: k = 0xD2, then for each byte k = k x 0x5D - 0x3D (mod 256)
and the byte ^= k (the keys 0D 7C CF F6 21 C0 83 5A ...). A packet whose first bytes are a filter's (`+0xD4`,
`0x005997A0`; at most 8 bytes) goes to that filter as it came (`0x00597C40`): the online services (QR2's `FE FD`,
NatNeg's `FD FC 1E 66 6A B2`) share the socket. Else it is decoded and read by its first byte (`0x00598460`):

| id | from → to | bytes | |
|---|---|---|---|
| 0xD2 | any | [1..] a filter's bytes | to the filter whose bytes follow the 0xD2 (`0x00597CF0`) |
| 0xD5 | any | [4] its number, [8] a packet, 3 bytes more | reliable: 0xD4 back; taken once by its number and sender (10 s); the packet inside read again |
| 0xD4 | answer | [4] the number | the reliable packet of that number no longer sent again |
| 0xDB | joiner → host, reliable | [4] its players | the host: the counter as its id (then one more); no room (the players over the most) → 0xDA, reliable; else 0xDF of the host (id 0, its players, its own address) to it, and for each peer before it 0xDF of it to that peer and 0xDF of that peer to it, all reliable |
| 0xDA | host → joiner | 1 | refused: not joining, the state 2 |
| 0xDF | host → peers, reliable | 0x1C: [4] id, [8] players, [0x0C] sockaddr | a peer at that address (one already there first goes); id 0 the host: in the session |
| 0xD9 | leaving → all | 1 | the peer at the sender's address goes |
| 0xD8 | all → all | 1 | keep-alive: the peer's silence 0 |
| 0xD7 | pinging → all | 8: [4] its number | 0xD6 with the number back to the sender |
| 0xD6 | answer | 8: [4] the number | the last ping's: its time into the peer's ring of four |
| others | peer → peer | | the game's: to its handler with the peer's id, from a known address only |

0xD4 and 0xD6 are coded by hand with the first eight keys; the reliable packet's three last bytes and the bytes 1 .. 3
of most packets are whatever was in memory.

The LAN's search is plain on 8666: the searcher broadcasts 0xDD with its 8 random bytes (9 bytes) at once and every
second; a host answering searches (`+0x10`) broadcasts 0xDC (100 bytes: [1..8] the question's bytes, [0x0C] the
session: its name (32 UTF-16 characters), players, the most; [0x54] the host's sockaddr). The searcher keeps the
sessions whose bytes are its own, by address, each gone after 4 s without an answer.

A sockaddr on the wire is a `sockaddr_in` of 16 bytes (the family 2, the port and the address in network order, 8
zeros). The host gives as its own the first address of `gethostbyname(gethostname())` with the port 8667.

### Its calls

- Host (`0x0059A590` → `0x00598A20`): its players, host, answering, not searching nor joining, in the session at its
  own address; the name (31 characters at most), the players taken and the most.
- Search (`0x00598AC0`): the found sessions emptied, new random bytes, the first 0xDD, searching.
- Join (`0x00598BC0`): the state 0, 0xDB (reliable) to the host's address, its players, not hosting nor searching,
  joining.
- Leave (`0x00598C20`): in a session 0xD9 to every peer (not reliable); out of the session, its players 0, not hosting
  nor joining, the state 0, the peers, the reliable queue and the taken numbers emptied (no events).
- Send (`0x00599120`): to a peer by id, or every peer (-2); reliable ones wrapped in 0xD5 (the number the counter,
  then one more), sent at once, sent again every 2 s until answered or sent 30 more times (`0x005995C0`).
- Update (`0x00599920`) each frame: searching, the search (0xDD each second; the sessions' ages, all 0 after a frame
  over 2 s); joining, 15 s without the host's 0xDF → not joining, the state 1; in a session 0xD8 to every peer each
  1.5 s, the peers' silence (one silent past 25 s goes; all 0 after a frame over 2 s) and with pings on (`+0x90`) 0xD7
  each 5 s; the reliable queue, the taken numbers (10 s); 30 times a packet of 8666 then one of 8667; a failing
  `sendto` counted for 3 s sets `[0x0075F74C]` (the network lost).

A peer leaving (0xD9) or silent goes (`0x00597B90`): the host gives back its players; the host gone sets the state
(3 it left, 4 silent) and the session ends. The handler (`+0xC4`) hears of peers coming (0xDF: [4] id, [8] players)
and going (0xDE: [4] id, [8] 1 silent, 0 left) as messages of peer 0.

OpenJuice: `Core/Network/NetTransport` (the ports, its own address and the broadcast's address and port may be given for
tests on one machine).

## The session (`0x004E66E0` .. `0x004EBD60`)

Made by `0x004E9C40` on the transport (its handler); the game's globals from `[0x0075EF90]`. It keeps the machines
(0x0C bytes each, `[0x0076A1D0]`: the peer, only watching, loaded) and the players (0x208 bytes, `[0x0076A1E0]`: the
peer, -1 this machine's, the index on its machine, the name of 26 characters, the car of 0x74 bytes, 13 values of the
race from +0xBC, the race's car, the last state taken and its times).

A car (0x74 bytes, its defaults `0x004E6920`, of a car record `0x00470A50`): the CarDef's name (24), the record's body
parts (+0x04, 10) and catalogue parts (+0x0E, 10), then its +0x2C, +0x18, +0x1C, +0x20, +0x38, +0x4C, +0x48, +0x24,
+0x28, +0x30, +0x34, +0x3C, +0x40, +0x44, +0x58, +0x5C, +0x60 and +0xB0.

### The stages (`[0x0075EF90]`)

0 the lobby, 1 the race's type, 2 .. 7 the setup's pages, 8 the cars, 9 loading, 10 loaded, 11 the count, 12 the
race. The host sets a stage with its number (one more each) and the setup (0x0A); a client takes it when the number
is newer, the round's end first (`0x004E7EB0`: in the race the places' points 10, 6, 4, 3, 1 to those who finished
but in a showoff, one race more to those in it; the gone players dropped, everyone's race values cleared).

- 8: the host starts the race when every player is ready, or 30 s after its own first ready (`0x004EAAA0`): the
  ready ones in the race, the grid shuffled (`0x004E9B50`, each draw seeding `rand()` as the transport's random bytes
  do), the places from the grid, each machine its 0x0E. Each 2 s our readiness is sent again.
- 9 → 10 the game's: the race loaded (`0x004EA910`, 0x0F to all).
- 10: the host says go (0x10) when every racing machine is loaded, or after 60 s (a client goes itself then).
- 11: 3 s of the count; a client asks the host's clock each 0.4 s (0x11 → 0x12) and takes the mean of its own and
  the host's plus half the trip.
- 12: each player's car's state at its period (0.1 s up to three players, 0.12 four, 0.16 five, 0.2 more); each
  second the places by distance, the host telling each its place (0x17) and, past 10 s with no one finished, a new
  leader (0x18); each 20 s the leader said again on the HUD. The first finish starts the wait (63 s, the notes at 60
  and 10 s left); all finished (or gone), the host goes back to the lobby after 10 s.

### The messages (ids under 0xD2)

| id | bytes | |
|---|---|---|
| 0x00 | 8: [4] index | the player's info asked (each 2 s for those not known) |
| 0x01 | 0xDC: [4] index, [8] name (narrow, 28), [0x24] the 13 values, [0x58] the profile | the info: known; the host sends it its stage (0x0A), or the race (0x0E, watching) when past 8; we send it our car (0x02); the HUD's note of a player joining |
| 0x02 | 0x7C: [4] index, [8] car | the player's car |
| 0x03 / 0x04 | 0x21: [1] the place's / route's name (32) | the setup |
| 0x05 .. 0x09 | 8: [4] value | laps, time of day, weather, the race's type (`0x004E8750`), difficulty |
| 0x0A | 0x60: [4] stage, [8] number, [0x0C] place, [0x2C] route, [0x4C] laps, [0x50] time of day, [0x54] weather, [0x58] type, [0x5C] difficulty | the stage |
| 0x0B | 0x80: [4] index, [8] ready, [0x0C] car | the player's readiness (the host's first ready starts its wait) |
| 0x0C | 0x10: [4] the sender's index, [8] the receiver's, [0x0C] the kind | the voice's event (below), reliable but for the kinds 0 and 1 |
| 0x0D | | the Xbox's voice relay (below): no handler takes it |
| 0x0E | 0x444: the setup, [0x58] watching, [0x5C] count, then six of: peer (as the receiver knows it: -1 itself, 0 the host), index, name (26), car, gone, the finish's two values, place on the grid | the race: the stage 9 |
| 0x0F | 1 | the machine loaded (past 10 answered with 0x10) |
| 0x10 | 1 | go: the stage 11 |
| 0x11 / 0x12 | 8 / 0x0C | the count's clock asked / answered |
| 0x13 | 0x10: [4] index, [8] [0x0C] the two values | finished (the first one on the HUD) |
| 0x14 | 0x4C: [4] index, [8] the sender's race time, [0x0C] the car's state (0x40) | unreliable; bit 4 of [0x49] put back on the line, bit 5 of [0x49] (the sender's race paused) applied as it is (else carried on and smoothed towards); a state not newer is dropped |
| 0x15 | 0x0C: [4] index, [8] a byte | the car model's byte (+0x430 +0x170), each 5 s |
| 0x17 | 0x0C: [4] index, [8] place | our player's place, from the host |
| 0x18 | 0x0C: [4] peer, [8] index | the leader (the HUD) |
| 0x19 / 0x1A / 0x1B | 0x0C: [4] index, [8] value | the cruise's score / trick, the points |

A machine coming (0xDF) adds a player for each of its slots; one going (0xDE) drops its players, or in the race marks
them gone (their cars stopped).

OpenJuice: `Core/Network/NetSession` (the game reached through its hooks).

### The car's state (0x40 bytes)

| at | | made (`0x004E7740`) | taken |
|---|---|---|---|
| 0x00 | position (3 floats, metres) | the body's +0x138 | as it is |
| 0x0C | velocity in the body's frame (3 int16) | +0x198 x 242.72, to the nearest | x 0.004119936 |
| 0x14 | orientation (w, x, y, z) | +0x148 | as it is |
| 0x24 | angular velocity in the body's frame (3 int16) | +0x1A8 x 3276.75 | x 0.000305185 |
| 0x2A | the wheels' spins, IDs 0 .. 3 (4 int16) | x 131.07 (0 without the wheel) | x 0.007629511 |
| 0x32 | throttle (int8) | +0x344 x 127 | (x 1/127)² |
| 0x33 / 0x34 | brake, handbrake (uint8) | +0x34C / +0x350 x 255 | (x 1/255)³ / x 1/255 |
| 0x35 | steering (int8) | +0x354 x 127 | x 1/127, within -1 .. 1 |
| 0x36 | the engine's speed (uint16) | +0x358 x 41.72 | x 0.22888532 x 0.10471976 |
| 0x38 | the race's distance (float) | the competitor's +0x98 (0 without) | the player's distance (the session) |
| 0x3D | bits 0 .. 2 the gear, 3 the engine on, 4 put back (the session's), 5 the race paused ([G+0x34]+0x20), 6 the horn sounding (+0x4C0's +0x48), 7 the nitro asked (+0x190) | | |

Stock problem: the throttle and the brake are sent as they are but taken squared and cubed. Taken
(`0x004E73E0`): the position unless the car is parked (+0x4E8), the orientation (`0x004F2CD0`), the velocities
(`0x004F2F80`, `0x004F3080`), the spins (into the state vector too), the pedals and the steering; the engine on
(off: the clutch out) and the gear (`0x004F7610`, the clutch out first with the engine on); +0x4EC the race paused,
the body then out of the world (`0x0053E500`; Stock problem: a primary body stays on the active list), else into
it; +0x190 the nitro; the horn (+0x4C0) at 1300 played or stopped. Smoothed towards (`0x004E6E30`, each frame of
the others' cars while their race goes on): the position by 0.07 (y by 0.21) unless 30 m away (y 3 m), the
orientation 0.07 of the way (`0x00554830`), the velocities and the spins by 0.07; the rest taken as it is. Carried
on (`0x004E6B60`): the position by the velocity turned to the world x dt, the orientation by q + (q x (0, ω)) x 0.5
dt made unit (+ 1.1920929e-07), ω the angular velocity turned to the world.

OpenJuice: `Core/Network/NetCar`.

## The windows

The main menu's MULTIPLAY (`0x00468B9D`) starts Winsock (`0x00597690`; failing, "An error occurred starting the
network adaptor. Please check hardware." 513) and goes to `link_search` with a custom race (mode 1) of type 5. The game's network (`0x004E9C40`, the transport and the session) is
made by `link_search`'s enter when there is none and ended (`0x004E9DD0`) by its BACK.

- `link_search` (enter `0x0046E800`, update `0x0046E880`): a search, GAMELIST each second (`0x0046E3E0`): "NEW GAME"
  (1647), then "<name> (<players>/<most>)" of each session found, the session highlighted before kept. SELECT: the
  race a custom one (mode 1, type 5, one player, no opponents, one lap), everything reset; "NEW GAME" hosts with the
  driver's name (at most 6 players), the stage 0, `link_lobby`; a session is joined: "JOINING GAME..." (680, type 0)
  until in the session (`link_lobby`) or lost (searching again).
- `link_lobby` (enter `0x0046EBB0`, update `0x0046FB20`): every element shown, then SELECTA / SEL_TEXT off the host,
  clienttext always, the online buttons and ratings off line hidden; the slots (`0x0046F250`): the players by points
  (a bubble sort starting again after each swap), each slot's name (multiline), points, races, `playericon` for the
  host's machine's first player (Stock problem: the index looked at is the unsorted player's of the slot), `CPU` for a
  machine's second player; the host's SELECT: the stage 1 and `instant_manu`.
- `multi` (enter `0x0046CAA0`, update `0x0046CB10`; after a split screen race): its "link" item reads "LAN MODE"
  (1661); "split" (two players, mode 3, type 5, `instant_manu`) only with two input devices or more (else closed and
  "link" highlighted); "link" as MULTIPLAY (the players kept).
- The setup (`instant_manu`, `instant_setup`, see 02-frontend.md): the host's windows send it, the clients' follow.
  `instant_manu`: on the network every car open; a client's TITLE "please wait..." (1551), its sel_text / selecta
  hidden, Menu following the group the host sent (msg 8 carries the group: `0x004E8750` puts it in `[G+0x84]+0x784`
  off the online career) and taking no events; the host sends the group on UP / DOWN and SELECT, and SELECT goes to
  `instant_setup` (Stock problem: nothing is sent at the enter, the clients show the group sent last). `instant_setup`:
  the host's enter sends the laps it sets (3 for type 5), the difficulty 3, the time of day (its rand()) and the
  weather 1 (Stock problem: a client draws its own until the next 0x0A brings the host's); each page's fill sets the
  stage (location 2, time limit 3, route 4, laps 5, time 6, weather 7) and each change sends its value (3, 4, 5, 9 to
  the nearest, 6, 7); a client takes the page of the stage, runs the location's and the route's pages once when it
  skipped them, highlights the host's value (the place by its index among all the city places, the route by the first
  item holding it as a part: Stock problems) with its menu taking no events, and takes no SELECT nor BACK. On the
  network the weather's SELECT goes to the race (no opponents): `link_selcar` and the stage 8. Every message's value
  goes into the race's rules (the laps only into the session; the difficulty also into the settings).
- `link_selcar` (enter `0x0046FF10`, update `0x00471160`, exit `0x004709B0`): six car views (CAR3D1 .. 6, the camera
  at (0, 180, -500)), each player's slot by the players' order: NAME, READY or NOTREADY, CARNAME; the car of a slot
  made again from the player's config when its name or parts change (`0x0048E7D0`), shown turned 3π/4 and turning
  0.4 a second, its paint followed each frame (the list of a slot kept to ten cars). The enter sends our car (the
  arcade car of the group's place); "location", "route" ("POINT-TO-POINT", the host then sending one lap; else the
  route's number; " Reversed") and "laps". SELECT turns our readiness (Stock problem: always of the local index 0);
  "CHANGE CAR" (6, not ready) `instant_selcar` (mode 3), whose SELECT on the network sets the car and comes back and
  whose BACK asks "Back out?
ARE YOU SURE?" (422, 86), yes putting the car back as it was. COUNTDOWN
  (`0x00471740`) the seconds left of the stage's wait under 15. Its exit takes the automatic box of the settings.
- The windows' shared checks (`0x0046E0F0`): BACK asks (starting on no) the host "Leaving the game now will end this
  session for all other players. .." (1598) in the lobby, else "Abandon race set up and quit back to the lobby?"
  (1536), a client "Leave game session?" (1537); yes: the host past the lobby takes everyone back to it, else
  `link_search` and everything reset. Then `0x0046DFE0`: past the stage 8 the race loads; a client follows the host's
  stage (`0x0046DE30`: 0 `link_lobby`, 1 `instant_manu`, 2 .. 7 `instant_setup`, 8 `link_selcar`), or the session lost
  (`0x00471860`: "The connection to the host has failed." 1645 for the states 1 and 4, "This game session is full"
  1635, "Host has left the game" 49) goes back to `link_search`.
- The network's frame (`0x004EBAE0`) runs each game frame after the race's, the front end's and the sound's
  (`0x0040CF30`).

OpenJuice: `Game/Frontend/Network/LinkWindows.cpp`; `--net <lan>,<game>,<to>[,<ip>]` gives the ports and the search's address for two
games on one machine.

## The race

- The cars (`0x00495DE0`'s network branch, in place of every mode's): the session's racers from the players in the
  race and not gone (`0x004B8C90`: the driver our first player, crew 0 for ours and the index on its machine + 1 for
  another's, numbered by the grid place; Stock problem: every racer named "Online Test !!", here the players' names
  as the port's HUD reads the racers'); the race's state with the session's laps; each player a new car of the CarDef
  of its config's name ("_STOCK" added) with the config applied, on its grid place (+0xC4), kind 0 ours of index 0, 1
  ours of another index, 2 another machine's; its player's car (+0xF0) it, let go again as the race ends
  (`0x00497310` → `0x004EA270`).
- No fly-by: the first view is the sweep before the start (`0x00401E80`: mode 10); the count starts at 4.005 (a
  spectator's 0.005, the second's beep 0; `0x0049FDF0`) and holds until the session's stage 12 (`0x004A0220`), so the
  machines start together 3 s after "go". Change View and SELECT before the start leave the grid alone.
- The race's first frame tells the session (`0x00425D6F`, `0x004EA910`); paused, the race's state and the cars'
  lights go on (`0x00426000`). In 1.01 another machine's car within 30000 of our car's race distance collides again
  and a session hosted without collisions takes them off (`0x004996F5`, `patch-1.01.md`). A finish of our car (not of kind 2) outside a cruise is sent with the race time and
  the best lap's bits (`0x0049FFB1`). RESET held tells the session (`0x004EAA40`; not bound on the PC). No replay is
  recorded; the crowd's trick sounds are none; a restart repairs as a race does; the cruise's bank is not loaded; every
  place and route is open in the setup's pages (`0x0042B7B0`, `0x0042B810`). A car paused on its machine (bit 5) is
  not put back after 4 s upside down (`0x00487FE0`'s +0x4EC).
- Only watching: the camera on the race's first car (`0x00425750`), CHANGE_CAMERA the replay's next view, SELECT or
  the watched player gone the next player in the race (none: the director's mode 13 for good, `0x0041F703`), the view
  behind the car under 4 s of the count (`0x00405300`), no cruise and no finish; the race front end shows
  `hud_spectator` alone (`0x004584C0`; RP_NEXTCAR, RP_NEXTCARTEXT, RP_BG1 hidden with fewer than two racing,
  `0x00455880`).
- The race front end (`0x00457180`): in the session with the host's stage under 9 the message closes and closedown
  follows (the round ended: back to `link_lobby`); the host silent (state 4) asks `hud_pause`, resets everything and
  says "The connection to the host has failed." (1645), the host gone (3) "Host has left the game" (49), both in
  `msgbox_network`, their answer closedown; the network lost ("A network error has occurred.", 1678, with CONTINUE)
  quits the link-up race (everything reset, the network game ended). The overlay (`0x00456BA0`): "Waiting for
  players..." (54) in yellow goodtime11 in the stages 10 and 11; for 2 s after a tactics key (or always for a
  spectator) the names of the other cars 1.2 m above them, " (PAUSED)" (53) for a paused one, fading to 100 m.
- The windows: `hud_pause` without RESTART, a client's QUIT "Leave Session" (2008) asking "Leave game session?" (1537);
  `hud_race_results` without RESTART and REPLAY, QUIT "End Race" (2007, the host) or "Leave Session" asking "End
  Race" / "Leave game session?" with "ARE YOU SURE?"; their quit takes everyone back to the lobby (the host:
  `0x004EA850` with 0) or leaves the session (a client: `0x004EA4E0` with 1). The results' table on the network
  (`0x00452030`): the session's order, a finished player's name, its race time (the finish's first value) and its
  car's model, ours highlighted, else "--------" and "--:--:--"; Team hidden.
- The cruise (an online career's; a LAN race is always of type 5): our score (rounded) sent not reliably with [4] 0
  (0x19) six times at the cruise's setup (`0x004AA730`; Stock problem) and, counting the frame's time within 0 ..
  0.04, every 2 s or with under 0.25 s left (`0x004AAEE7`), each time with the board; a trick worth 10 or more its
  number, a special move its number + 100 (0x1A, `0x004AC240`, `0x004AC0A0`; received, `0x004E9680` -> `0x004B0E90`:
  "<name> <the trick's name>", 339 + trick, past 100 359 + trick - 100, the first player's note for 2.5 s with the race
  front end up); before the start the board each frame;
  the frame goes on while paused; the time run out ends the cruise (`0x004AB460`, again each frame after: Stock
  problem): the racing players' scores in six places sorted (the places' "ours" by the players' order: Stock problem),
  our first place gives our finish (0, 0) and its points (10, 6, 4, 3, 1, 0) to our total (0x1B). The board
  (`0x004B0A50`, also on each 0x19 received): the first three with a score of 0 or more on mplay1 .. 3, "<n><st|nd|rd>
  <name> <score>" (thousands marked); mplay hidden but in a network cruise (`0x004B09D0`). `hud_cruise_results`
  hides its menu and the frame's buttons on the network.
- The notes (the first player's messages): "Spectator <name> has joined" (55, 52), "[Spectator ]<name> has left"
  (51), "WINNER: <name>" (81), "NEW LEADER: <name>" (1633; the 20 s repeat "1st: <name>", 37) with the HUD up, the
  player not gone, no one finished, past 10 s and not a cruise (the leader's timer again at every call), "Race ends in
  <n> seconds" (1616, 1617; 3 s).

## GameSpy

The exe links GameSpy's SDK as it was: query and reporting (QR2) for a hosted session, server browsing for the
search, NAT negotiation for the way in, and the available check. The game's name is "juicedpc", its secret key
"g3J8df2" (built byte by byte at `0x00471981`). The hosts are fixed: `juicedpc.master.gamespy.com`:27900/UDP (QR2),
`juicedpc.ms3.gamespy.com`:28910/TCP (the list: "ms" and `0x0056A670`'s hash of the name, h = tolower(c) + h x
0x9CCF9319, modulo 20), `natneg1.gamespy.com` and `natneg2.gamespy.com`:27901/UDP, `juicedpc.available.gamespy.com`:
27900/UDP. The SDK keeps overrides of the first, the second and the last (`[0x007A4340]`, `[0x0075F5E4]`,
`[0x007A43E0]`) that nothing writes. All its timers are `GetTickCount()` milliseconds; QR2's init and the browser's
`srand(GetTickCount())` again (the game's `rand()` goes on from there).

The available check (`0x00569C00`, its think `0x00569E20`): the name copied to `[0x007A43A0]` (NAT negotiation's INIT
carries it), `09 00 00 00 00 juicedpc\0` sent; an answer `FE FD 09` from that address with the big-endian value at [3]:
bit 0 unavailable (2), else 1 | bit 1 (1 available, 3 temporarily unavailable); no answer, sent again each 2 s, after
the eighth available; a name that does not resolve, available at once. Its result (`[0x007A4380]`) must be 1 for a
browser to be made and a negotiation to begin.

The SDK's unique id of the machine (GOAGetUniqueID, `0x0056E5B0`) is linked but called by nothing (its pointer
`[0x0071B1C8]`, beside the NAT negotiation servers' names, is read by nothing): the `Crypt` value of
`HKEY_CURRENT_USER\Software\GameSpy\GameSpy 3D\Registration` when it holds 19 characters, else 19 made
(`0x0056E4F0`: the generator `[0x0071B1CC]` seeded by the performance counter's halves, `time`, `clock` and the tick
count XORed, 0 as 1; each character of `0-9a-zA-Z` by the next value (x 16807 modulo 2^31 - 1, `0x0056E6E0`) modulo
62, `-` at 4, 9 and 14) and written there (the key made when it could not be opened; Stock problem: then read through a
handle never set); the dashes taken out, 16 characters.

The wide strings of the SDK become UTF-8 by its own converters (`0x00568C70`: one to three bytes a character;
`0x00568D00` back: a sequence of two or three bytes, else the byte's character under 0x80, else '?').

OpenJuice: `Core/Network/GameSpy/GameSpy` (the shared parts, the hosts, the game's socket the parts send on).

### Query and reporting (`0x0056A180` .. `0x0056C290`)

The game's record is the static one (`0x0071ACA0`, 0x118 bytes) on the transport's 8667: QR2 sends out of it and
never reads it (its init on a given socket clears the "read" flag, `+0xC4`); the packets beginning `FE FD` reach it
through the transport's filter (`qr2_parse_query`, `0x0056B8F0`).

- Init (`0x0056A740`): the name, the secret key, the port as given (the heartbeat's `localport`), listed and NAT
  negotiation; `srand(GetTickCount())` and four `rand() % 0xFF` as the instance key (`+0x84`); the listed state 1; the
  local addresses once a process (`gethostbyname(gethostname())`, five at most); listed, the master resolved (3 when it
  does not).
- Think (`0x0056ABD0`, only when listed; the socket none, nothing): waiting for the challenge (the listed state over
  0) and 10 s past the last heartbeat: a heartbeat with `statechanged` 3 and the state one more, or past three of them
  the state 0 and the add error 5 "No challenge value was received from the master server." (no keep-alive that time);
  else a heartbeat 1 when a change was asked and 10 s passed, or 0 each 60 s (or none yet, or the clock gone back). A
  keep-alive (`08` and the key) each 20 s. The first think so sends `statechanged` 3 at once, then twice 10 s apart.
- A change (`qr2_send_statechanged`, inlined by the matchmaking): a heartbeat 1 now when 10 s passed, else asked.
- The heartbeat (`0x0056BC10`, 0x800 bytes): `03`, the key, then strings: `localip<n>` and the dotted address of each
  local address, `localport`, `natneg` ("1"), `statechanged` (not 0), `gamename` ("juicedpc"), and with a public address
  callback `publicip` (the u32 as stored, `%d`) and `publicport` (`%d`); then for 2 (the shutdown: the master drops
  the host) one 0 byte, else the server's keys (name and value of each of the key list's, a 0 byte), the players' and
  the teams' (u16 BE count, the names, a 0, the values). The last heartbeat and keep-alive now; the change asked
  cleared. Stock problem: the matchmaking reports its session name as key 2, so `gamename` comes twice.
- The shutdown (inlined): listed, a heartbeat 2; the socket none, the last heartbeat 0.

The packets (`0x0056B8F0`): `;` the cd key's callback (none); `\` a GameSpy 1 query: `\name\value` of the server's
keys, `\<name><n>\value` of the players' and the teams', then `\final\\queryid\1.1` (`0x0056B780`); else at least 7
bytes `FE FD [type] [key 4]`, the listed state 0 when over 0, the answer `[type] [the same key]`:

| type | | answer |
|---|---|---|
| 0x00 | query: `[n][ids]` of the server, the players, the teams (0xFF all, 0 none) | the values of the ids asked (the players' and teams' after their u16 BE count), or for 0xFF the names and values as in the heartbeat; nothing written gives "" |
| 0x01 | challenge, a string of 1 .. 0x41 bytes with its 0 | base64 (no '=') of gs_encrypt(secret, challenge) and a 0; 13 bytes or more with a public address callback: the last 13 are `%08X%04X` our address and port, a new non-zero one stored and told |
| 0x02 | echo | `05`, at most 32 bytes back |
| 0x04 | add error (our key, 2 bytes or more): `[code][UTF-8 message]` | none; the listed state -1, the add error callback |
| 0x06 | client message (our key, 4 bytes or more): `[message key 4][message]` | `07 [key] [message key]` always; a key not among the last ten: a message beginning with NAT negotiation's magic (10 bytes or more) gives the cookie (u32 BE at 6) to its callback, else the client message's callback |

Others are not answered. The keys' names (`0x0071ADC0`): 1 hostname, 2 gamename, 3 gamever, 4 hostport, 5 mapname, 6
gametype, 7 gamevariant, 8 numplayers, 9 numteams, 10 maxplayers, 11 gamemode, 12 teamplay, 13 fraglimit, 14
teamfraglimit, 15 timeelapsed, 16 timelimit, 17 roundtime, 18 roundelapsed, 19 password, 20 groupid, 21 player_, 22
score_, 23 skill_, 24 ping_, 25 team_, 26 deaths_, 27 pid_, 28 team_t, 29 score_t; the game registers its own from 50
on (50 .. 254; Stock problem: 254 is one past the table).

gs_encrypt (`0x0056AE60`) is an RC4 of a sort: the state 0 .. 255 swapped with y = key[x] + state[i] + y (x through the
key), then for each byte x += byte + 1, y += state[x], the two swapped, the byte ^= state[state[x] + state[y]].

OpenJuice: `Core/Network/GameSpy/GameSpyQr2`.

### Server browsing (`0x0056CDB0` .. `0x0056D9C0`, `0x0057C480` .. `0x0057EBA0`, `0x0057FFA0` .. `0x00580200`)

A browser (`0x0056CFB0`, 0x630 bytes; none unless the available check gave 1) is a query engine (`+0x00`: its UDP
socket, two queues of servers, the basic keys) and a server list (`+0x48`: its state 0 the LAN's, 1 disconnected, 2
connected, 3 the list coming; the servers; the TCP socket; our address as the master saw it at `+0x4E0`).

- Update (`0x0056D1B0`): the fields `\<name>` of the keys asked (255 characters at most, the first 20 kept as the
  basic keys), then the request (`0x0057CD30`): a blocking `connect()` to the master list server (2 not resolved, 3
  refused; a socket still there is connected already and fails), a challenge of 8 characters (`0x0057CBB0`: c0 =
  rand() % 93 + 33, then each rand() % 93 + 33 made odd or even by a parity of the ones before), and
  `u16 BE length | 00 | 01 | 03 | u32 game version | juicedpc\0 | juicedpc\0 | challenge | filter\0 | fields\0 | u32 BE
  options` (+4 the alternate address with 8, +4 the most servers with 0x80, Stock problem: in memory order). The list's
  buffer is 0x1000 bytes. Not async, run until the list is in and the queries done.
- The reply (`0x0057D570`): `[n ^ 0xEC]`, n bytes, `[k ^ 0xEA]`, the server's key of k bytes, then everything is
  decrypted by GOA keyed by the challenge (each byte of the server's key: challenge[(i x secret[i % 7]) % 8] ^=
  challenge[i % 8] ^ key[i]). Then our address (u32) and the default port (u16, network order; 0xFFFF: an error
  message follows). With options 2 (no server list) that is all; else the keys (`[n]`, then `[type][name\0]`: 0
  string, 1 byte, 2 short), the popular values (`[n]`, strings), and the servers until one of the address FF FF FF FF:
  `flags | u32 address | [u16 port: 0x10] | [u32 private address: 0x02] | [u16 private port: 0x20] | [u32 ICMP
  address: 0x08] | [the keys: 0x40] | [name\0value\0 .. \0: 0x80]` (a string key an index into the popular values, 0xFF
  a string following; a port not given the default one). A record not complete waits for more (Stock problem: with
  both 0x40 and 0x80 the rules are checked from where the keys begin). Then the list is connected; with
  disconnect-on-complete the socket is closed.
- The flags: 1 unsolicited UDP, 2 a private address, 4 connect by negotiation, 8 an ICMP address, 0x10 a port, 0x20 a
  private port, 0x40 keys, 0x80 full rules. A server (`0x0056A6D0`, 0x24 bytes): the public and private addresses
  (network order), the state (1 basic keys, 2 full keys, 4 a basic query, 8 a full query), the flags, its keys (names
  without case; one given again replaces it), the query's start then its time (the ping).
- After the list (`0x0057DCC0`), messages `[u16 BE length ≤ 0x1000][type]`: 1 the keys again, 2 a server (its strings
  always following, never popular; added when new, then updated), 3 keep-alive (sent back as it is, plain), 4 a server
  gone (`[address][port]`), 5 a map loop, 6 a player search (none in the game; Stock problem: its callback is called
  unchecked and never set). Stock problems: a length under 3 is never taken off (0 loops for ever); with the buffer
  full the next read asks for 0 bytes and is taken as the connection closed.
- A closed connection or a bad message (`0x0057C970`): a buffer beginning "Query Error: " gives the error, then the
  list is disconnected.
- A server's keys again (`0x0056D4E0`): through the master `00 09 01 | address | port` (the answer a server message,
  with the full rules); disconnected, the list is connected again with options 2 first (a new challenge and cipher). Or
  queried directly (added when not listed). Not async, run until it answered.
- A message to a server (`0x0057DF50`): `u16 BE (9 + length) | 02 | address | port` then the message; the NAT
  negotiation's cookie (`0x0057E000`) is `FD FC 1E 66 6A B2` and the cookie (u32 BE): the master gives it to the host's
  QR2 as a client message.
- A server come without keys (not during the update of one) is queried directly (`0x0057E5C0`) from the engine's
  socket, at most 10 at once, the rest queued: basic `FE FD 00 | u32 id | n | the keys | 00 00` when the list is not
  the LAN's and there are basic keys, else full `FE FD 00 | u32 id | FF FF FF`, the id the query's start; to its
  private address when its public one is ours and it has one. The answer `00 [id]` and the values in the keys' order
  (or the full form), from that address, gives its keys and its time; 2.5 s without one, failed (Stock problem: the
  failed state goes into its flags, 0x10 set and 4 and 8 cleared, and its pending bits stay).
- The game's callback: 0 added, 1 updated, 2 update failed, 3 deleted, 4 update complete (the list in with no server
  or no query, or the last query done), 5 query error.

GOA (`0x00580070`, `0x00580120`): 256 cards shuffled from 255 down by a key-driven random (each draw s = cards[s] +
key[i], s += the key's length at its end, masked, taken modulo after 11 tries), the rotor, ratchet, avalanche, last
plain and last cipher bytes from cards 1, 3, 5, 7 and s; each byte: ratchet += cards[rotor++], four cards turned,
avalanche += cards[t], the byte ^ cards[(cards[avalanche] + cards[rotor])] ^ cards[cards[(cards[last plain] +
cards[last cipher] + cards[ratchet])]].

OpenJuice: `Core/Network/GameSpy/GameSpyBrowser` (the LAN's answers `0x0057E070` are there but nothing in the exe sets the list's
state 0).

### NAT negotiation (`0x0056C290` .. `0x0056CC90`)

Two machines sharing a cookie each tell the natneg servers of themselves; the servers give each the other's address,
and the two ping each other until both have heard. Every packet: `FD FC 1E 66 6A B2 | 02 | type | u32 BE cookie`.
The negotiators (0x40 bytes each, `[0x0075F4C8]`) each have the game's socket (the transport's) and one of their own.

- Begin (`0x0056C7A0`): 2 when the available check did not give 1; the natneg servers resolved once (3 when not;
  Stock problem: the negotiator added stays zeroed, its null callback called at its timeout); its own socket (2
  without); the INITs.
- INIT (`0x0056C4C0`, 22 + the name's length bytes): `[12]` port type, `[13]` the client's index (0 the host, 1 the
  joiner), `[14]` the game's socket used, `[15]` the local address (BE: the first of 10/8, 172.16/12, 192.168/16, else
  the last not the loopback), `[19]` the local port (BE), `[21]` the name and its 0. Not acknowledged yet: type 0 from
  the game's socket to natneg1, 1 from its own to natneg1 (both with the port 0), then the port (the game socket's when
  used, else its own's) and 2 from its own to natneg2. Again each 500 ms, 30 times.
- INITACK (21 bytes or more): that type acknowledged; all (0 only with the game's socket): the state 1 (acknowledged,
  60 s for the CONNECT), progress 1. ERTTEST: sent back as ERTACK (type 3, 21 bytes) from its own socket.
- CONNECT (20 bytes or more): `[12]` the other's address, `[16]` its port (BE), `[18]` got your data, `[19]` finished.
  Not finished: a CONNECT_ACK (21 bytes: `[13]` the index; Stock problem: `[12]` the port type and the rest are what
  the stack held) to the sender from its own socket. Before the pings: finished, the result (1 the partner never came,
  2 its INITs timed out, else 3) and cancelled; else the other's address guessed, the state 2, progress 2, a PING.
- PING (`0x0056C650`, 20 bytes): `[12]` the guessed address, `[16]` its port, `[18]` we have its data, `[19]` not in
  the state 2; from the game's socket (else its own) to the guessed address; again each 700 ms, 20 times. One received
  (pinging): the sender's address taken, its data had; with "got your data" the state 3 (finished, 5 s on), and with
  the game's socket the success at once with the sender's address; else (or still not finished) ping again.
- Think (`0x0056CA20`, the negotiators from the last): cancelled ones go; its own socket read; the INITs or PINGs again,
  past their tries the result 2 (Stock problem: also for the pings) and cancelled; finished past 5 s, without the
  game's socket the success with its own socket handed over, cancelled; acknowledged past 60 s, the result 1 and
  cancelled. Cancel (`0x0056C840`): its socket closed, the state 4.

OpenJuice: `Core/Network/GameSpy/GameSpyNatNeg` (the list freed closes the negotiators' sockets, the stock leaves them open).

### The matchmaking (CJETMatchmaking_gs, vtable `0x006FA880`)

The online windows make it (`0x004E1A00`: the one before released, `0x00560D50`: 0x63C bytes) as `[0x0075EF84]` and
release it (`0x004E1A20`). Its constructor (`0x00562360`) takes the transport (`[0x0075F758]`, a reference kept) and
puts two filters on it: NAT negotiation's magic (to `NNProcessData`) and `C8 2A` inside a 0xD2 packet (its messages).
Its destructor (`0x00562500`) frees the browser and the negotiators, stops hosting, clears the presence and takes the
first filter out (Stock problem: not the second, left with the freed object).

| slot | | |
|---|---|---|
| +0x04 `0x00562640` | Host(private, players, open slots, count, attributes) | needs the transport; hosting stopped first; the session id: `srand(++[0x0079E898] + this)` twice, then `rand() + (rand() << 16)`; key50 .. key53 and `key<54 + i>` of the i-th attribute registered (Stock problem: everything else takes 54 + its id); the attributes copied (1 an int, 2 a wide string, 3 bytes); QR2 on the transport's socket (listed, NAT negotiation; its callbacks below), failing: stopped, 0; the `FE FD` filter, the NAT negotiation's callback (the cookie queued as a request {cookie, -3, 0, 0}) and the public address's, up |
| +0x08 `0x00562A40` | Update(players, open slots, count, attributes, unused) | the counts, the values of the attributes of the same ids; `qr2_send_statechanged` on the static record |
| +0x0C `0x00562B10` | SetInfo(name, playing, mode) | the keys 1/2, 11 and 6 |
| +0x10 `0x00562B80` | StopHosting | the negotiations cleared; up: QR2's shutdown; the `FE FD` filter out; the attributes freed; +0x18 1 |
| +0x14 `0x00562C90` | SetSearchAttributes(most, count, attributes) | key50 .. key53 and `key<54 + id>` registered; the attributes (16 bytes: id, type, length) kept |
| +0x18 `0x00562F60` | StartSearch (its arguments unused) | negotiations cleared, the search stopped, +0x0C and +0x10 0, the timers 0, the results cleared; the browser made when none ("juicedpc", 10 at once, QR2 queries); the fields key50 .. key53 and 54 + each search attribute's id; the browser cleared and updated (async, disconnect on complete, the filter `+0x54` in UTF-8, options 0) |
| +0x1C `0x00563200` | SearchFriend(a, friend) | with the friends' object (`[0x0075F284]`) and the index not -1: as a search, then the friend's address (its record's +0x14) asked through the master |
| +0x20 `0x00563530` | RefreshResult(a, position) | with the friends' object, the position under the results' count: the address of the result at that sorted position asked again through the master (the results cleared) |
| +0x24 `0x00563820` | SearchAddress(a, address) | with the friends' object and an address: asked through the master |
| +0x28 `0x00563950` | StopSearch | with a browser: +0x0C and +0x10 1, the browser halted (disconnected, its queries forgotten) |
| +0x2C `0x00563990` | Join(index, slots) | below |
| +0x30 `0x00563D00` | Update(dt) | below |
| +0x34 `0x00563F40` | Leave | hosting stopped, the presence cleared |

The fields the windows read: +0x08 our public address known, +0x0C / +0x10 the search over (always set and cleared
together), +0x24 the most results, +0x28 the results (0x30 bytes each: valid, the server's address, key53, the server,
key50, key51, key52, the attributes, the quality), +0x38 their indices by quality (`0x005645E0`: the CRT's qsort,
`0x00564550` the better first, the same 1), +0x48 the selected one (-1), +0x50 the presence's address, +0x54 the filter
(`0x00564500`, under 0x200 characters).

QR2's callbacks: the keys reported (`0x00561570`) 1, 2, 8, 10, 6, 11, 50, 51, 52, 53 and 54 + each attribute's id; the
values (`0x005613C0`): 1 and 2 the session's name (Stock problem: `gamename` so twice in the heartbeat), 6 the mode, 8
the players, 10 the players and the open slots, 11 `openwaiting` / `openplaying` / `closedwaiting` / `closedplaying`,
50 the players, 51 the open slots, 52 private, 53 the session id, 54 + id the attribute's value. The public address
(`0x00562230`) goes to the presence (`[0x0075F1E4]`->vt+0x44) and sets +0x08.

The search's callback (`0x00561750`): a server added or updated makes a record: its address, the quality 1 - ping x
0.0033 within 0 .. 1; with its keys key50 .. key53 and an attribute per search attribute from `key<54 + id>` (an int,
or a wide string of the attribute's length); valid when 0 < key50 < 6 and 0 < key51 < 6. It goes in (`0x005640A0`) in
place of one of the same address, else appended under the most, else in place of the first valid one of a lower
quality than a valid new one (never +0x48); else dropped. The update complete sets +0x0C and +0x10.

Join (`0x00563990`; needs the transport, the browser and the index under the results' count; Stock problem: the index
is into the results themselves, every window passes a sorted position): negotiations cleared, the search stopped; the
transport's online join (the slots, not hosting nor searching, the join's timer 0, connecting: its 15 s cover it all);
the session id taken; the presence told the result's address; then by the server's flags: a private address and its
public one ours (behind the same NAT): JOIN to its private address with our game port; unsolicited UDP: JOIN to its
public address; else a cookie (`srand(++[0x0079E898] + this)` twice, `rand() + rand() x 0x10000`) as the request
{cookie, 0, slots, 0}, sent to the server through the master, and a NAT negotiation as the client 1.

Update(dt) (`0x00563D00`, each frame after the network game's, `0x0040CF84`): QR2 (listed: its heartbeats); the
browser's think; NAT negotiation's think; a negotiation on: 20 s and it is cancelled and the negotiators freed; else
the next request of the ring of 16 begins one on the transport's socket (its index). With a browser and the search
not over: 10 s and it is over (halted); else each 1 s the results not valid asked again through the master.

The messages (`D2 C8 2A [type]`, reliable through the transport, `0x00562130`):

| type | bytes | |
|---|---|---|
| 0 JOIN | 0x2C: [4] direct (1) or after a negotiation (0), [8] public sockaddr (our address as the master saw it, our game port), [0x18] private sockaddr (`gethostbyname(gethostname())`'s first, our game port), [0x28] slots | the host (`0x00561D10`): after a negotiation only with one on (then off), at the negotiated address, else the sender's; the transport's id count taken; no room: REFUSED; else the clients no longer peers dropped, PEER of the host (id 0, its slots) to it, and for each client before it: the same public IP, PEER of each other's private address to both; else a cookie and NEGOTIATE to both (index 0 to the older, 1 to the new); the client kept |
| 1 REFUSED | 4 | not connecting, the state 2 (`0x00562110`) |
| 2 NEGOTIATE | 0x14: [4] cookie, [8] peer id, [0x0C] slots, [0x10] index (Stock problem: 3 bytes after it what the stack held) | queued in the ring (dropped when full) |
| 3 PEER | 0x1C: [4] sockaddr, [0x14] id, [0x18] slots | a peer: id 0 the host at its known address, else at the address given |

A negotiation's end (`0x00561B00`, with one on): success: its address kept (+0x5AC); hosting, nothing more (the JOIN
follows; the negotiation stays on); else to the host (peer id 0) a JOIN after the negotiation with the transport's
slots, or the peer added. Then off.

The windows' readers: `0x004E6300` the valid results; `0x004E6360` the quick match (the first valid result not playing,
its attribute 1, else the first valid one, else -1); `0x004E6650` the quality's marks (1 + 5 x quality truncated, 1 ..
5; 0 for a quality 0); `0x00564060` a result's attribute by id.

The session tells it (`0x004E66E0`, at a stage set, the race's start, a machine come (each of its players) or gone (each
of its players left), a player dropped): hosting with a matchmaking, SetInfo(the hosted session's name, the stage 9 or
more, "Career" / "Arcade" by `[0x0075EF88]`) and Update(the players not gone, (2 for a one-on-one career, `[0x00728D00]`,
else 6) less them, the attribute 1 = the stage 9 or more). Its reset (`0x004EA4E0`) makes it leave after the
transport.

OpenJuice: `Core/Network/Matchmaking` (the address table's indices are addresses; GP's presence and friends are hooks for the GP
port; the hosted sessions' names and the mode the online windows will write; `[0x0079E898]` is `seedCount()`, the same
count as the transport's and the race director's).

## GameSpy Presence

The game uses GP's connection manager only: `gpcm.gamespy.com:29900`/TCP (`0x00573930`, the name in the writable
`[0x0071B258]`, never changed). The search manager, the peer connections, the transfers, the profile updates and the
answers of a new profile and of a unique nick are linked but nothing of the game reaches them (below, "What the game
never reaches").

### The wire

Every message is `\key\value...\final\`; the client cuts its stream at each `\final\` (`0x005794B0`). A value is found
anywhere in the message by its key (`strstr`, `0x00579AF0`) and runs to the key's first character (`\` or, inside a
`\msg\`, `|`) or the end. A message holding `\id\` answers the operation of that id (the connect is always 1, the others
count from 2): the connect `0x00574690`, the info `0x0057A2C0`; its error ends the round. Else `\error\` (the code
`\err\`, the text `\errmsg\`, fatal with `\fatal\`; `0x00579A10`), `\bm\` (`0x00575140`) or `\ka\` (nothing). The
client's messages wait in a buffer sent at the next processing; a blocking request processes there with `Sleep(10)`
until its answer.

| step | message |
|---|---|
| server, on connecting | `\lc\1\challenge\<server's>\id\1` |
| login (`0x00573F00`) | `\login\\challenge\<32 of A-Za-z0-9 by rand() % 62>\user\<nick>@<email in lower case>[\userid\..\profileid\..]\response\<r>\firewall\1\port\0\productid\0\gamename\juicedpc\namespaceid\0\id\1` |
| new user (`0x005742E0`) | `\newuser\\email\..\nick\..\passwordenc\<e>\productid\0\gamename\juicedpc\namespaceid\0\uniquenick\\id\1`, answered `\nur\\userid\..\profileid\..\id\1`, then the login |
| server, logged in | `\lc\2\sesskey\..\userid\..\profileid\..[\uniquenick\..][\lt\..]\proof\<p>\id\1` |
| info (`0x0057AC30`) | `\getprofile\\sesskey\..\profileid\..\id\<op>`, answered `\pi\\profileid\..\nick\..` ... `\sig\..\id\<op>` (`\profileid\` and `\sig\` required) |
| status (`0x00569670`) | `\status\<n>\sesskey\..\statstring\..\locstring\..` |
| invitable (`0x00569970`) | `\inviteto\\sesskey\..\products\10362` |
| buddy request, answer, removal | `\addbuddy\\sesskey\..\newprofileid\..\reason\`, `\authadd\\sesskey\..\fromprofileid\..\sig\<the request's 32>`, `\delbuddy\\sesskey\..\delprofileid\..` |
| invitation (`0x00569A70`) | `\pinvite\\sesskey\..\profileid\..\productid\10362\location\<loc>` |
| buddy messages from the server | `\bm\<type>\f\<profile>[\date\..]\msg\..`: 2 a request (`<reason>|signed|<32>`), 100 a status (`|s|..|ss|..|ls|..|ip|..|p|..`; a profile's first makes it the next buddy), 101 an invitation (`|p|<product>|l|<location>`), 102 a ping (answered `\bm\103\sesskey\..\t\<profile>\msg\1`), 1 and 103 nothing for the game |
| logout (`0x00574E00`) | `\logout\\sesskey\..` |

`pwhash` = MD5 of the password in lower-case hex; the response `md5(pwhash + 48 spaces + user + client's + server's +
pwhash)`, the server's proof the same with the challenges the other way (else 0x0108, fatal). The new user's
password: each byte XORed with the SDK's generator (seeded `0x79707367` for each string; x 16807 modulo 2^31 - 1 by
halves; the byte ^ seed % 255) in base64 with `[`, `]` and `_` for the padding. The SDK's errors: 1 parse, 5 network,
7 the server closed (the state 4), 0x0107 refused, 0x0108 the proof.

The profile cache `gp.info` (the current directory) is read by `gpInitialize` and written by `gpDestroy` when its
state is not 0: `2`, then each profile in the table's order (its id modulo 32, the last of each list first) `[<id>]`,
`userid=`, with its info `valid=1` and its keys, an empty line. A cached profile's info answers at once; the login of
a cached nick and email carries its ids.

### What the game never reaches

The linker kept these parts of the SDK while the calls that start them were dropped; they are ported as they are
(`Core/Network/GameSpy/GameSpyPresenceSearch.cpp`, `Core/Network/GameSpy/GameSpyPresencePeers.cpp`).

- The search manager (`gpsp.gamespy.com:29901`, `[0x0071B218]`; `0x00571500 .. 0x00573170`): each search has its own
  non-blocking socket and operation (`0x005718B0`); processed after the connection manager's round whether connected or
  not (`0x00572FA0`). The request by its kind, then `\gamename\juicedpc\final\`: 1 `\search\\sesskey\\profileid\
  \namespaceid\` with the criteria given (`\nick\`, `\uniquenick\`, `\email\`, `\firstname\`, `\lastname\`, `\icquin\`,
  `\skip\`), 2 `\valid\\email\`, 3 `\nicks\\email\\pass\\namespaceid\`, 4 `\pmatch\\sesskey\\profileid\\productid\`,
  5 `\check\\nick\\email\\pass\`, 6 `\newuser\\nick\\email\\pass\\productID\\namespaceid\\uniquenick\[\cdkey\]`, 7
  `\others\\sesskey\\profileid\\namespaceid\`, 8 `\uniquesearch\\preferrednick\\namespaceid\`. The answers (`\bsr\` ..
  `\bsrdone\\more\`, `\vr\`, `\nr\` .. `\ndone\`, `\psr\` .. `\psrdone\`, `\cur\`, `\nur\`, `\others\` .. `\odone\`,
  `\us\` .. `\usdone\`) go to the search's callback; a profile search with more to come and its callback leaving it so
  searches its next page by itself (`\skip\` the matches so far). An error of a search reports 16 bytes of result
  whatever its kind (Stock problem: the larger answers read past them).
- The answers `\npr\\profileid\..` (`0x00576A10`) and `\rn\` (`0x0057C3A0`) of operations nothing makes; the profile's
  updates (`0x0057AAE0`, each round first: `\updatepro\\sesskey\..<pairs>` and `\updateui\..`, which nothing fills).
- The simulation (`+0x108`, 0 from `gpInitialize`, set by nothing): the connect, the new user and the info call their
  callback at once with zeros, a buddy's status is zeros, the status, invitations and buddy requests do nothing.
- The peers (`0x0057AEF0 .. 0x0057C020`): TCP straight between two players. Ours listen only without the firewall
  flag (`0x00573930`: any address, a free port, a backlog of 5, its port in the login's `\port\`) — the game always
  logs in behind it. Each round after the connection manager (`0x0057B980`) a connection come is a peer authorizing;
  it must send `\auth\\pid\<its>\nick\..\sig\<md5hex("<our password><our profile><its profile>")>\final\`, answered
  `\aack\\final\` (else `\anack\\final\`, which goes unsent with the peer closed at once). A buddy message for a
  profile goes through its connected peer, else (the profile a buddy with a port, which no player of the game has) a
  new peer of ours: the server's signature for it asked (`\getprofile\` without a callback; the `\pi\`'s `\sig\` kept
  for the peers waiting, `0x0057A2C0`), the connect, our `\auth\` (the signature then let go), the answer (`\anack\`
  once: the signature asked again and the peer taken as connected all the same, Stock problem; twice: dropped), else
  through the server (`0x00575A00`). Peer messages are `\m\<type>\len\<n>\msg\` and a line's end, the text and a 0: 1
  a buddy's message, 102 a ping (103 back), 103 the pong for our transfers, 200 .. 208 a transfer's. A peer silent
  for 300 s, closed or failing goes, its messages under 100 sent through the server instead and every transfer told
  its connection lost (Stock problem: not only that peer's); peers are never let go otherwise, not at the disconnect
  nor at `gpDestroy`.
- The transfers (`0x00576E70 .. 0x00578FE0`): messages `\m\<type>\xfer\<profile> <count> <time>..\len\<n>\msg\` over
  a peer. A request (200: `\version\1\num\<n>` and each file's `\name<i>\`, `\size<i>\`, `\mtime<i>\`) is taken only
  with the transfer callback set (the game sets none: answered 201 `\version\1\result\2`, not accepting). Ours
  (`gpSendFiles` was not linked) wait for the profile's pong (through the server: no connection), send the request,
  and once accepted (201 result 0) each file: 202 `\file\..\size\..\mtime\..`, 204 its bytes 8 KB at a time while the
  peer's output is empty, 203 `\file\..\md5\<hex>`; 205 skip or failure (Stock problem: our own reasons go under the
  type 0x80E, which the other side never takes), 206 the rate (0 pauses), 207 cancel, 208 the keep-alive after 4
  minutes silent; no peer after 60 s: no connection. A received file is written to `<current
  directory>gpt_<id>_<file>_<rand()>.gpt` (Stock problem: no separator after the directory) and checked by its MD5.
  Each step is an event to the callback (`0x800 .. 0x80F`, the reasons `0x900 .. 0x902`).

### The user object (`0x0055E110`, vtable `0x006FA638`)

`0x0055F090` gives it the account (its state 0, signed out) and starts the available check; its frame (`0x0055F880`,
the game's loop `0x0040CF48`) processes the connection, and once the check says available makes it (`gpInitialize`
with the product and namespace 0) and logs in (behind a firewall, not blocking). Logged in (`0x0055ECB0`): a record of
our profile (its nick asked, blocking), a local player online, logged on, invitable to 10362, the logged-on hook
(`0x004EE990`: each local player online as the settings' `+0x44`, the voice, the friends made again). A call's GP result
gives the state while it is 0 (parameter 1, memory 2, network 3, server 4); 5 the service unavailable; every error's
code gives it (`0x0055E230`: 4, 0x0104, 0x0202 11; 6 6; 7 7; 0x0101 8; 0x0102, 0x0201 9; 0x0103 10; 0x0105 12; 0x0106 13;
0x0107 14; 0x0108 15; others 1) and clears the logged-on flag. Each 60 s, and within a second of a player's change, the
first player's presence (`0x0055EDC0`): Offline 0, Online 1 or Playing 2 with the location
`juicedpc://<ip>:<port>/?session=<n>` (`0x0055EF50`; `://0.0.0.0:0` without an address). While logged on the available
check runs again 10 s after the object was made, then 2 s after each answer. The buddies' status, requests and
invitations go to the friends object (`0x0055FFC0`): their entries (online, playing, the same game, the address and
session of their location string), our requests (gone after 10 refreshes), our invitations.

### The windows

- The main menu's `online` goes to `online_main` when logged on, else to `online_logon_gs`; its enter and frame show
  "ONLINE: <nick>" (`0x00473CD0`). Its enter takes `-host` / `-connect a.b.c.d:port` (the account of `-name`, `-email`,
  `-ppass` into the edit buffers; `online_connect_gs` logs in at once; `online_main` then goes on to `online_mode` or
  joins the address).
- `online_logon_gs`: brings the services up (`0x00471920`: Winsock, the user object); `connect` takes the driver's
  account (settings `+0xE4`, `+0x122`, `+0x188`, UTF-16, the password plain) into the edit buffers, `create` empties
  them; BACK lets the user object go.
- `online_create_gs` / `online_connect_gs`: the keyboards (the nick 30 characters with a space and the name filter, the
  email 50, the passwords 30; none of `@+:#` first), SUBMIT's checks (1538 .. 1541), the question 1644 when it replaces
  a kept account, the account stored and "Logging into account..." (1670), then the login's loop inside the frame
  (`Sleep(50)`, the user object's frame with 50); its states' notes (765, 766, 1637, 768, 769, 770, 771).
- `online_main`: the network game and the matchmaking made, the online setups' nick, the autosave of a changed account;
  quick / search / create `online_mode`, friends `online_bg`, rankings `online_stats`, sign out (1583: everything
  online let go); BACK keeps the login.

OpenJuice: `Core/Network/GameSpy/GameSpyPresence` (the SDK), `Core/Network/GameSpy/GameSpyUser` (the user and friends objects, the hooks the
matchmaking's port fills), `Game/Frontend/Network/OnlineWindows.cpp`.

### The online career

An online session of the career ([0x0075EF88] 1, `0x004E1AC0`) follows the LAN's stages with the career's windows:
the stage 1 is `instant_seltype`, the stage 8 the race setup (`0x004A44E0`: `CMSOnlineRaceSetup` for a solo session
or a race of type 4, else `CMSOnlineCrewRaceSetup`) until the grid walk was entered ([0x0075EE6C], cleared by
`link_lobby`'s enter), then `CMSOnlineGridWalk`. The race setup's GO goes through `careerload` with the career's
screen 8, the grid walk.

- `CMSOnlineCrewRaceSetup` (enter `0x004D6090`, update `0x004D6550`): the offline window's drivers, the player taken
  at once and never given back, no CLICK; its enter hides only `driver<n>value` and `selection<n>`, so past the
  members `border<n>` and `crewpic<n>` stay as the .ui has them (a white box in its border; the offline window,
  `0x004D8AC0`, hides them).
- `CMSOnlineGridWalk` (enter `0x004D3310`, update `0x004D40B0`, leave `0x004D3850`): six car views (Car3d1 .. 6, the
  camera at (0, 150, -560)), `<n>Info` the player's name, `bph<n>` the car's display name (" (P)" for a prototype;
  the rating's "<n> bhp" written first and replaced at once), `ready<n>` / `notready<n>`. Each slot's car is made
  again from the player's config when it changes (the decal colour left out of the config seen), its paint followed
  each frame. SELECT turns our readiness (sent for the local indices 0 .. 2); with a pink slip, not ready: a garage
  of 55 cars "Cannot enter a pinkslip race. .." (1614), not two players "You must have 2 players for a pink slip
  race" (1531), else the question 669 + 1942 whose yes turns it; a pink slip without two players takes it back.
  Past the stage 8: "loading"; with a pink slip first the stakes (`0x004A49A0`: copies of our car and the rival's
  into the career's +0x88 and +0x8C lists, +0x90 1), our car taken out of the garage, autosave (mode 5) with the
  network's delayed call (50 of the frame time; the autosave's leave cancels it; `0x004D4090`: the session left and
  "Host has left the game" with `online_main`), then a copy of our car back at the garage's end (Stock problem: made
  again each frame a request waits) and "loading".
- The pink slip's end (`0x004A4C60`, with the stakes set): won, a copy of the rival's car at the garage's end (no
  check of the 55 cars) and the profile knowing its catalogue parts; lost, the garage's last car taken out; a tie
  changes nothing; the stake lists freed. `link_lobby`'s frame ends it by the points of the first two players (under
  two by the career's +0xAC), "CONGRATULATIONS! You have won your opponent's car!" (670) or "BAD LUCK! You have lost
  your car!" (671), then autosave, then everything reset and `online_main`. A reset of the session (`0x004EBBF0`)
  ends it by the race when there is one (won: our car finished first, the host left, or hosting a player left the
  race, `0x004EBCF0`), else drops it.
- `link_lobby`'s SELECT online: the online career `instant_seltype`, an arcade session of any group `instant_manu`,
  else its group sent and `instant_setup` (the stage 2).

OpenJuice: `Game/Frontend/Network/OnlineSessionWindows.cpp` (the session windows, `0x004A44E0`), `Game/Frontend/Frontend.cpp` (the race
setups), `Game/Frontend/Network/OnlineCareerWindows.cpp` (the grid walk, `0x004EBBF0`), `Game/Frontend/Network/LinkWindows.cpp` (the lobby),
`Core/Race/RaceSession` (the stakes and their end).

### The friends' windows

The online windows open over `online_bg` (its enter opens the window named in `[0x0072182C]` in mode 5; they open
each other in mode 7). The network windows' keys 8 and 9 (with the matchmaking) take our readiness back and go to
`online_bg` with `online_players` or `online_friends`.

- `online_players` (`0x00479CF0`, `0x00479F80`, `0x00479EB0`): the session's players of other machines and the players
  met lately (`[0x00728D58]`, ten, the latest first: a player leaving goes there, `0x004E7D30`; one joining is taken
  off, `0x004E7C90`; the session's reset lets them go), each nick cut to the list's width with its friend icon
  (`live_friendinvite_rec` / `_sent`, `live_friendonline`); the list again each second; SELECT on a player not among
  the friends `online_player_options` ("Send friend request"); BACK the host's stage's window.
- `online_friends` (`0x0047B4C0`, `0x0047B9D0`, `0x0047B870`): the friends with their state (PLAYING, ONLINE,
  OFFLINE) and icon (`live_gameinvite_rec` / `_sent` first), the name highlighted kept across visits; 6 on a friend
  playing the same game asks its session (`0x004E4460`) and opens its create window in the details mode; SELECT
  `online_friend_options`.
- `online_friend_options` (`0x0047C0B0`, `0x0047C870`): a request (accept, decline; a temporary friend's accept asks
  to send ours), an invitation (accept: the session reset and the friend's session joined, link_lobby; decline;
  remove), else join, send invite, remove. The console's voice mail and feedback parts of the user object do nothing on
  the PC (their items never come).

OpenJuice: `Game/Frontend/Network/OnlinePlayers`, `OnlinePlayerOptions`, `OnlineFriends`, `OnlineFriendOptions`,
`OnlineVoice` (the windows), `OnlineFriendWindows.cpp` (the online keys, the invitation's note), `Core/Network/NetSession` (the players met
lately), `Core/Network/GameSpy/GameSpyUser` (the friends' and the user's calls).

## The statistics

GameSpy's statistics SDK (gstats) reports the online races' results to `<game>.gamestats.gamespy.com`:29920, and its
HTTP SDK (ghttp) reads the leaderboards from `gamestats.gamespy.com`.

### gstats (`0x005669C0` .. `0x005687A0`)

The connection (`0x00566A90`, its frame `0x00566D30`): the server's challenge answered with
`\auth\gamename\<game>\response\<md5hex(challenge + secret key)>\port\<port>\id\1`, then the session key. Every
message is XORed with "GameSpy3D" and ends with `\final\` (`0x00567710`). A game (`0x00566FD0`) is announced with
`\newgame\connid\<id>\sesskey\<key>`, its players numbered (`0x00567270`) and its data kept in buckets (a hash table
of 32 lists; a player's bucket `<name>_<n>`, a team's `<name>_t<n>`), sent as
`\updgame\sesskey\<id>\done\1\gamedata\<"\name\value" each, '\' made 0x01>`. Without a connection both are kept in
`gstats.dat` (XORed with "gstats.dat", hashed) and sent at the next connection.

### ghttp (`0x00568850` .. `0x00568BD0`, `0x0056F3D0` .. `0x00571550`)

Only `ghttpGet` (`0x00568AD0`) is linked of the requests: a URL `http://<server>[:<port>]<path>` (the path's spaces
'+'), its request `GET <path> HTTP/1.1` with `Host: <server>` (`:<port>` but for 80), `User-Agent: GameSpyHTTP/1.0`
and `Connection: close`. A request (0x138 bytes, in slots grown four at a time) goes through its states each frame
(`0x005688F0`): the address (a dotted one, else looked up), the non-blocking connect, the request sent, the answer
waited for, the status line (`HTTP/<major>.<minor> <code>`, 100 .. 599), the headers (a 1xx's status again; a 3xx's
`Location:` followed up to ten times, a path made `http://<server>:<port><path>`; `Content-Length:`; chunked), the
body (0x2000 bytes at a time for up to 500 ms a frame). A 401 gives Unauthorized, 403 Forbidden, 404 and 410
FileNotFound, another 4xx RequestRejected, a 5xx ServerError; the connection closed short of the length
FileIncomplete. The completion gets the body and its length; the request is freed after it.

Stock problems: `Content-Length:`'s value is read from 16 characters on (the character after the colon skipped);
headers ended by `\n\n` lose the body's first two bytes (the first made a 0); a failed select while connecting is
never seen (its value's low byte compared with -1). The code of the requests the game never makes (saved, streamed,
HEAD, posted: the posts' URL-encoding reads the bytes before its hex digits for a character over 0x7F, and a string
partly sent is sent again from its start) is kept as it is.

### The statistics objects (STATS.CPP)

The game's statistics object (`[0x0075F274]`, made at the login by the game's frame `0x004EC5B0`) narrows the game's
name and secret key for gstats, begins its connection and makes the reporter and the leaderboards; its frame
(`0x0055C8C0`) runs the leaderboards' think, keeps the connection (begun again when lost, or after a failure other than
waiting with "Failed while processing async stats connection!!! (%d) Retrying...") and the reporter's think.

- The reporter (`[0x0075F28C]`, vtable `0x006FA97C`): a gstats game made at the first begin, each user a player of it
  once (its "pid" and "auth": md5 of the game's challenge and the user's password), the keys set on the current
  player (key 1 "player", the others by their numbers), sent as one snapshot.
- The leaderboards (`[0x0075F280]`, vtable `0x006FA444`): a table's URL
  (`http://gamestats.gamespy.com/juicedpc/leaderboard.asp?leaderboard=<n>&pid=<profile>&rows=<rows>&v=3` around a user,
  `records.asp?v=2&leaderboard=<n>` for one row, `leaderboard_<n>.html`), a users' table
  (`userstats.asp?v=2&ps=1&lb=1`, `&pid<i>=<profile>` for each), the columns or rows bound, asked for over ghttp; the
  answer read line by line (a table: its row count, its total, then each row's columns; a users' table each user's
  bound values). A failure sets the problem 3, the player asked for not in the table 6 (`[0x0075F27C]`).

### The online records (`0x004EC590` .. `0x004EE1C0`)

Each user met gets a record (`[0x0076A240]`, 0x2C bytes): rating (10000 at first, key 2), rank (4), races won (12),
lost (13), disconnects (14), pink slips won (33), lost (34) and their money (37), "racing" as read (36) and as sent.
Every 5 s with a front end up to six records not read yet are asked for in one users' table (boards 1 and 4); a record
read racing while we had it racing (or unknown) takes a disconnect, a loss and 20 off its rating, reported and sent.

The reports: a lap time (the route's board, route + 20: the time, the car's model and performance rating, the top
speed), a showoff score (the location's board, location + 10), a race begun (key 36 1), FinishedRace (key 36 0; won or
lost; the ratings against each other racer by the places between: `0x004EC980` steps of 0 .. 10 by the ratings'
difference, table `0x00725630`), a pink slip (board 4), a race left. The game calls them at the player's finish
(offline sent at once, online by the round's end), the pause, the sprint's results, the load (race begun), the round's
end, the online pink slip's stakes and end; a route's world record is read for the HUD at the race's enter and raised
by the race's best laps and showoff scores.

`online_stats` shows the boards: lap times by route, the world ranking, the top cruise score by location, the pink
slips; the table's top (20 rows) and our position by turns, our row highlighted, the total under it.

Stock problems: FinishedRace references every session player's user it looks at and never lets them go; the users of
our local players taken for the reports are never let go; the crew race's FinishedRace (results' leave) is sent only
with a later report; a racer in our own place still moves the rating (a sign of 0 adds its step).

OpenJuice: `Core/Network/GameSpy/GameSpyStats` (gstats), `Core/Network/GameSpy/GameSpyHttp` (ghttp), `Core/Network/OnlineStats` (the objects),
`Core/Network/OnlineRecords` (the records, ratings and reports), `Game/Frontend/Network/OnlineStatsWindow.cpp`.

## Voice

### GameSpy Voice 2 (`0x0057EBC0` .. `0x0057FC50`, `0x005804C0` .. `0x00580A10`)

Startup (`0x0057ED00`, with the game's window `[0x0075F79C]+0x0C`): COM, the format (PCM, one channel, 8000 Hz, 16
bits) and a list of devices. The codec is the game's (`0x0057EC10`, gvSetCustomCodec): 160 samples a frame and Speex's
encoded size; 200 free frames of that size are made (`0x0057FBD0`, more made as needed, all freed at the end).

The devices are DirectSound's: listed (`0x0057F190`) by `DirectSoundCaptureEnumerateA` and `DirectSoundEnumerateA`
(the primary driver's NULL GUID passed over; a GUID met again takes the kind of this enumeration; at most the room
given), the default voice devices marked. A device (0x5C bytes, its methods `+0x18` .. `+0x48`, the unplugged callback
`+0x4C` and the filters `+0x50`, `+0x54`) is made of its GUID (`0x0057FA20`): a capture device's looping capture buffer
of 16000 bytes is started at once (the volume 1, the last stamp at the threshold -21); a playback device has
DSSCL_NORMAL with the window, a looping buffer of 3200 bytes (GETCURRENTPOSITION2, GLOBALFOCUS, CTRLVOLUME: ten frames)
and eight sources.

- Capture (`0x0057F590`): the frames read since the last (the read cursor against the last read position), as many
  as the packet's room; not capturing (its start `+0x24`, its stop resetting the Speex encoder) they are passed over.
  The peak of the samples read (as shorts over 32768) is the volume; at the threshold (0.15) or above, or within 20
  frames of the last stamp that was, each frame is scaled by the capture volume (under 1), filtered and encoded, the
  packet worth sending. A DirectSound failure calls the unplugged callback and frees the device.
- Playback (`0x0057EDD0`, each device each frame from `gvThink` `0x0057F060`): the play cursor (a frame down) against
  the last gives the bytes played; those are locked from the last cursor and mixed, the clock (a frame stamp) moved on
  by their frames. A lag (the milliseconds since the last mix, less those played, less the cursor's bytes within its
  frame: the units mixed as the stock mixes them) of half the buffer or more moves the clock on by whole buffers.
- A packet played (`0x00580880`) goes to its source (the sender's profile): a new one (the first free, else the first
  without frames, with its Speex decoder) takes d = 10 - stamp + clock as its offset and average; else the average
  follows d by 1/32 (a lap of 65535 taken when they wrapped apart, kept within 0 .. 65535), the offset drops by one
  (and the packet its first frame) when it is more than 10 over the average, rises by one when more than 5 under. A
  packet behind the clock is dropped; its frames go into the source's list by stamp (`0x00580780`).
- The mix (`0x00580A10`): each source's frames up to the mix's end are decoded and added at their places (one before
  the start dropped), the source talking from then; a source without frames whose end is past is freed. The talking
  sources are listed from that flag (`0x005806A0`, `0x005806D0`).

The codec (`0x00580230` .. `0x00580450`): Speex 1.0.3's narrowband (the version told by its strings: see
third_party/speex-1.0.3) at quality 4: 8000 bits a second, 20 bytes a frame; the decoder with its enhancer; decoding
adds into the mix (16 bits, no saturation).

### The voice object (`[0x0075F1E8]`, 0x64C bytes)

`0x00565360` makes the singleton (two references: its own and the caller's); the game's reference is `[0x0075F044]`,
made by the login's hook (`0x004EE990`, when there is none) and by opts_voice's enter (which lets it go at its leave
when it made it); `0x004EECA0` lets both go (with the online objects). Its `+0x1C` is the game's hook of the chat's
switch (`0x004EEB50`: the settings' `+0x48` the other way), called by nothing on the PC.

Its base (vtable `0x006FA3C8`, `0x0055BBE0` .. `0x0055C600`, the interface the Xbox's voice shared) keeps up to four
local players (the user object's, referenced: their devices opened `+0x08`, their chat `+0x0C`, the users they muted)
and up to five users of others (each with an entry for every local player: answered, muted, muted us). The game's
hook (`+0x0C`, set by the network's start `0x004E9C40`: `0x004E99E0`) hears a player's events: 0 a user to be heard
(on adding it, then every 2 s until it answers), 1 the answer, 2 / 3 the player's devices opened / closed, 4 / 5 a user
muted / unmuted. The hook sends them as 0x0C to the user's machine (or every machine for the player's own), and there
`0x004E9050` gives them to `+0x3C`: 0 answered (1), 1 the entry answered (the devices told again (2), a mute again),
2 / 3 the user's devices, 4 / 5 the user muted us.

GameSpy's part (vtable `0x006FA9B8`, `0x00565360` .. `0x00566970`): `0x005659C0` starts GameSpy Voice and the codec;
the devices are listed for opts_voice (`+0x04` .. `+0x0C`); a local player added (`+0x18`, by the session's
`0x004E9E40` with the settings' chat and devices) takes the transport's filter of "VG" and a talker; its devices are
opened by name (`+0x10`: the first of the name and kind; one device of both names for both, never on the PC), asked
for again every 2 s while missing; every 0.15 s each talker's capture device gives a packet ("VG", the first local
player's profile, the stamp, the volume, the bytes and the frames: the bytes + 0x20 sent) to every peer of the
transport as it is. A "VG" packet that comes is played by each talker's playback device unless the talker muted its
profile's user. `+0x2C` (talking) asks the first talker's capture device, which never plays: no user ever talks.

The Xbox's relay (the session players' `+0x14C`: 0A 00 0D, the index, the bytes; sent every 0.2 s by `0x004EB210` with
a coding of its own) is filled by nothing; its handler (`0x004E9980`, the transport's `+0xCC`) is never called.

### The icons and windows

The icons `live_comm_off`, `live_comm_mute`, `live_comm_on` (and `live_comm_tv`, shown by nothing) are taken once from
the front end's textures (`0x004EEB70`). A user's icon for our first local player: mute when muted, else for a user
whose devices are opened on while talking (never), off. The race's overlay draws it 25 pixels square right of the
name (`0x00456BA0`); link_lobby (`0x0046F250`) and link_selcar (`0x00471160`) on voice<n> (ours by our devices and
sending), online_players (`0x004791B0`) and online_friends (`0x0047AC70`) on m_voice_icon; online_players' SELECT
always opens the options with the voice; online_player_options offers "Mute" / "Remove mute" (`0x0047A380`,
`0x0047A5C0`). online_friends adds our player to the voice outside a session and takes it off at its leave. online_voice
(`0x0047DD50` .. `0x0047E020`) is the console's voice mail: its methods (`+0x44` .. `+0x70`) do nothing on the PC.
The game's loop gives the voice its frame (`+0x74`) every frame (`0x0040CFBC`).

OpenJuice: `Core/Network/GameSpy/GameSpyVoice` (the SDK and the codec), `Core/Network/VoiceChat` (the object), third_party/speex-1.0.3.
