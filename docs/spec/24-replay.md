# The replay

The game object's `+0x9C` is the replay (0xD14 bytes, made once by `0x00428970` as the game starts: the base
`0x00426660`, vtable `0x006EF330`, then `0x004289F0`, vtable `0x006EF378`). It records every race (but type 3 and the
network's) and plays it back from the results' REPLAY; the attract mode and the saved replays play it from files.

## The buffer

48 blocks (`+0x0C`) of 0x1EFC bytes at `+0xD0C` (also `[0x0075ECE0]`): each block `+0x00` its number in the order of
writing, `+0x04` the block after it (999 while it is the newest), `+0x08` its interest, `+0x0C` 240 frames of 33 bytes.

| slot | function | what |
|---|---|---|
| 1 `+0x04` | `0x00428CD0` | the next frame read (below) |
| 2 `+0x08` | `0x00428C60` | a frame's first bytes read, a frame ahead or not |
| 3 `+0x0C` | `0x00428C10` | a frame read (`0x00428B70`) and unpacked (`0x004286E0`), some frames ahead |
| 4 `+0x10` | `0x00428F60` | 33 bytes written as a frame (at most 33 asked) |
| 5 `+0x14` | `0x00428F80` | a car's frame packed (`0x004283E0`), the replay's time in its first 4 bytes, written |
| 6 `+0x18` | `0x00429260` | the bytes of the blocks written (48 blocks at most) |
| 7 `+0x1C` | `0x00428AD0` | the blocks in order (below) |
| 8 `+0x20` | `0x00429290` | the buffer emptied, not saved (`+0xD10`) |
| 12 `+0x30` | `0x00426650` | saved (`+0xD10`) |
| 13 `+0x34` | `0x004292C0` | the save into a save's files (below) |
| 14 `+0x38` | `0x00429550` | the read from a save's files (below) |
| 15 `+0x3C` | `0x00429720` | the read of the named files (the attract) |
| 16 `+0x40` | `0x004296F0` | the files named (`+0xCCC`) |

Writing (`0x00428FE0`): a block not yet begun (`+0x70`) takes the next number (`+0x18`), 999 as its next, and its
interest: 0 for block 0 (and the interest carried, `[0x0075ECE4]`, 0), else 0.6 of the interest carried. A full block
(240 frames) adds the frames' interest (the race manager's `+0x1C`: the marks' weights since, `0x00429F50`; their lines
made into texts and dropped) and carries it on; the number goes up, the next block is chosen (`0x00429210`: the block
after it while there is one and fewer than 48 were numbered; else, from the cars' count on, the blocks in turn,
`[0x0079F1F8]` kept from race to race) and linked, the blocks written count one more; the frame goes in at `+0x1C`.

Reading (`0x00428B70`): none once the last block's frames (`+0x14`, `+0x20`) are read; a frame past the block's end is
the next block's. Moving on (`0x00428CD0`): with the whole record in block 0, at its first frame, the fade is due its
time less 1.4 s; past a block's end the next block, and unless it follows the last one written in number, the fade is
due before its end (its frames' time less 1.4 s; its last two frames in a cruise), the time after the gap (`+0x4C`) its
next block's first frame's.

Putting in order (`0x00428AD0`, off the network): unused blocks numbered 999, the blocks sorted by number (`qsort`),
each linked to the one after it; the last block where the newest was, or 47 once the ring went round.

## A frame (33 bytes)

| bytes | unpacked (`+0x5C` bytes) | from the car (`0x00428010`) |
|---|---|---|
| `+0x00` | `+0x00` the replay's time | |
| `+0x04` .. `+0x0B` | `+0x08` the orientation, each part (v + 1) x 32767 to 16 bits, back x 3.0519906e-5 - 1 | body `+0x148` |
| `+0x0C` .. `+0x17` | `+0x18` the position (its w not kept) | body `+0x138` |
| `+0x18` .. `+0x1B` | `+0x28` the four wheels' spins, (v + 100) x 0.6375, back x 1.5686275 - 100 | wheel IDs 0 .. 3 (`0x004F7500`) |
| `+0x1C` | `+0x48` the engine's rpm x 255 x 0.0001, back x 10000 x 0.003921569 | body `+0x358` x 9.549296 |
| `+0x1D` | `+0x4C` the throttle x 3 in 2 bits; `+0x50` the model's knocked-off parts (`+0x170`) above them | body `+0x344` |
| `+0x1E` | `+0x44` the steering (v + 1) x 15 in 5 bits, back x 0.06666667 - 1; `+0x04` the car's number above | body `+0x354` |
| `+0x1F` | `+0x40` the gear (4 bits), `+0x3C` the brake over 0.1 (0x10), `+0x38` the nitro asked (0x20) | body `+0x368`, `+0x34C`, car `+0x190` |
| `+0x20` | `+0x58` the engine sound's value x 127 (7 bits; 0.5 without one), `+0x54` the horn sounding | car `+0x498`, `+0x4C0` |

The values are floats cut toward zero (`0x004283E0` on the main thread: the game's MXCSR, or the x87's
chop). Unpacked, the throttle is read from the whole byte `+0x1D` (a stock problem:
with any part off it is 1).

## The record

At the race's load (`0x00425501`) a replay that does not play is made ready (`0x00426B00`) and left stopped, the race
not a replay's. The race's frame (`0x00425D40`) sets the game's clock to the replay's speed (`+0x30`) while it plays;
not paused, after the physics and the atmosphere, the first time the count is under 4.005 s the record begins (the
replay's step, then `0x00426EA0`) unless the replay plays; then each frame the replay's step (`0x004270C0`) before the
cars. A restart lets the record begin again.

The record's beginning (`0x00426EA0`, off the network): state 0, the blocks, numbers, frames and times cleared, the
buffer emptied, the header (`0x00426870`, not in type 3: the race no longer a replay's, the cars' count, the race's
mode, type, laps, track, route, time of day and weather, every car's record written to the stream `+0x82C`,
`0x0048DD40`), the marks cleared, the period 0.14 s in a cruise, else 0.25 s.

The step (`0x004270C0`, not in type 3, off the network): a new cars' manager gives the cars' count; a play stopped
pauses the race. The time runs (fast forward: the time just past the newest frame read, the speed what that makes of
the frame). The fade: due, its wait runs down and once below 0 while playing the fade out begins (1.4 s or the block's
time); fading out, the black rises (1 - t / 1.4) and at its end the last block ends the play, else the time jumps to
the time after the gap and the fade in begins (1.4 s; its first frame counts as the play's first); fading in the
black falls. Recording (state 0): after the finish the record goes on 5 s (6.4 s with less than 25 x the cars' frames
left in the block) and stops (`0x00426F60`); each period (`+0x2C`) every car's frame is written (`0x00427370`), and in
a cruise the frame's marks (`+0xCB4`, 0x18 bytes) after it. The finish is a cruise's time out or every competitor
finished. Playing (state 1): the frames are read (`0x00427560`), and on the play's first frame the first player's
camera is reset (`0x00401F20`) and the racers placed on their lines (`0x00497BB0` → `0x0049F150`). A play stopped
with its last block 0 pauses the race and the black is full.

The race's windows end the record too: the pause's restart answered yes (`0x0044F490`), the restart answered yes and
REPLAY of `hud_challenge_results` (`0x00451029`, `0x00451157`), `hud_cruise_results` (`0x00451A48`, `0x00451B70`) and
`hud_race_results` (`0x004537BB`, `0x00453A7E`), and the sprint's next heat (`0x0045545B`). A quit answered yes in the
pause, `hud_challenge_results` and `hud_race_results` sets the replay's `+0x828` to 0 and calls its method `+0x2C`
(empty, `0x0055FE30`) (`0x0044F3E1`, `0x00450FB2`, `0x00453732`).

The stop (`0x00426F60`, recording only): the last block and its frames kept, the length the time, the blocks put in
order, the reading back at block 0, the header's last block, frames and length; state 2.

## The play

`0x00426FE0`: the marks cleared; none when no block was written and the header's last block is not 0; state 1, the
places before and after each car's frames and the frames' places cleared, the race a replay's (`[G+0x6C]+0x68`), and
`0x00426B00`: the block, frame, number and timers 0, the first read due (`+0x64`), playing (`+0x828`) and the restart
(`+0x824`) set, the fades off, not ended, the next frame's time -1, the black 0.

The race's next frame (`0x00425D40`) takes the restart (`0x00426300`): the cars to the grid again (`0x004975A0`,
the game's rand()), the record's flag (`+0x24`) cleared, each player's camera by the race's type (`0x00401E80`)
on its car (`0x00401DB0`) and its director on the TV view (11) for 9999 s (`0x00408670`), the sounds stopped, each
car's skids quiet (`0x00492710`), the physics' frames' count (`+0x78`) cleared; the flag is cleared and outside a
replay's race (mode 4) the player's car goes to the AI (`+8` 1).
Reading (`0x00427560`) while the newest frame read is behind the time (twice for the first read): frames are read
until a car comes again; each car's frame before (its place kept as the place before), the frame read next, the place
after it the same car's next frame's (2 frames ahead in a cruise, else the cars' count); the newest time; in a cruise
the marks after it. None left: stopped, ended, the black full. Then each car: between its frames (`0x00427860`: the
orientation by slerp, `0x00554830`, made unit again where a part comes out 0; the place on the Catmull-Rom curve of
the four places, `0x00427D30`, or straight without a place before or after; the wheels straight, standing within
±5 rad/s; the steering, rpm, throttle and engine sound straight; the brake, nitro and gear of the nearer frame; the
horn and the parts of the frame before) or its next frame outside them; ended, its frame before. The frame set on the
car (`0x00428180`): at the normal speed the body goes 0.7 of the way to the place and the turn, else to them; the
wheels' spins, the steering (within ±1), the engine's speed (rpm x 0.10471976, also in the state), the throttle squared,
the nitro (`0x00488FA0`: asked with nitro left or in a replay's race; its flash 3 frames), the gear (`0x004F7610`), the
parts, the brake (0.125 or 0), the engine off, the clutch out.

`0x00426B80` plays on: not fast; an ended play from the start (`0x00426C40`: not fast, the normal speed, the race on,
the play anew), else the race on, playing, the normal speed. Fast forward (`0x00426CF0`): the race on, the normal speed,
fast toggled; turning on, the first player's camera reset with a frame of 0.02 s and the speed kept (`+0x80`). Slow
motion (`0x00426DF0`): not fast, the race on, a quarter of the speed while playing.

## The windows

REPLAY in the results asks for `start_replay` (`0x00455BD0`, `0x00455D60`): the race a replay's, five frames, then
with its transitions done (Stock problem: past 0 the count goes on and the play never starts) the cars' damage and
kept values back as the record began (`0x004973E0`: the stream's cars read, `0x00426A50`; `+0x64`, `+0x68`, `+0x74`,
`+0x7C`, `+0x84`, `+0x88` and the grid's cells, `0x00557D10`), the race back to its start (`0x00426160`: the players'
cameras reset and off their cars, each car made ready, the cars settled on the grid, the camera on the first car),
the cruise's car forgotten, the play from the start and the race front end's black 1. `restart_replay`
(`0x00455E20`, `0x00456030`) does the same without the damage, the cruise and the race's flag. Their leaving
(`0x00455F50`) only turns sounds and the HUD back on.

In a replay's race the window chosen is `hud_replay` (`0x004584C0`). Its frame (`0x0044DE20`): a quit asks for
closedown. BACKGROUND's alpha is the fade; past 0.95 with the play ended, or with the time at the length, the play goes
on and `restart_replay` comes. The attract (mode 4) ends at any input or past 0.8 of the length: the race paused, the
quit set. RP_TOOLTIP names the selected item (1649 + item: HIDE, CHANGE VIEW, RESTART, SLOW-MO, PLAY, PAUSE, FAST
FORWARD, EXIT); the controls hidden by HIDE come back with the next event. RP_METER the part played; RP_INFO (504 +
state) PAUSED once ended, SLOW-MOTION under the normal speed, FAST FORWARD, else PLAYING. In a cruise not fast, the
frame's marks queue the trick's name (339 + trick, not 15) and the special move's (359 + move), shown 2 s each in
CRUISEMOVE. NEXT (event 6) with two cars or more follows the next car (`0x00497530`, `0x00408AC0`: the camera on it, the
director's view 8 for 0 s). SELECT on the shown menu with its transitions done: PLAY plays on, RESTART asks for
`restart_replay`, PAUSE stops the play (or plays on once ended), SLOW-MO and FAST FORWARD (at 2, the queued names
dropped) play on first, EXIT ejects, CHANGE VIEW turns each player's camera to its next view (`0x004069F0`: off the
director to the view behind, else 0 → 1 → 5 → 0; on the bumper the director's trackside view for 10000.94 s), HIDE
hides the controls. BACK ejects. A new selection while fast forwarding plays on. Eject (`0x0044DD00`): play on, the race
paused; a replay saved quits at once, else "Quit Replay?" (433, on NO): yes quits, no puts the race on. Leaving after a quit
the replay stops playing. The save asked (`+0x24`, set nowhere) would ask for `savereplay` (`02-frontend.md`).

closedown stops the replay (`0x0044F9C0`).

## The showoff's marks

In a cruise the manager marks the replay: a trick done its number (`+0xCB4`, interest 1), a special move its number
(`+0xCB5`, interest 5), the crowd's bonus interest 10, a crash 17 + its kind (`+0xCB4`; interest 5 over 3000, 2 over
2000). Each frame's marks go into the record after the frame and come back with it.

## The attract

The title (`start`, `0x00467350` → `0x00467260`) counts its time (`+0x10`, cleared as it enters and while the device is
lost); past 100 s (`[0x00715C4C]`) the next attract replay (`[0x0075ECBC]`, back to the first at `[0x00715C50]` = 2:
`attracts\attract1` and `attracts\attract2` in turn, a stock problem: `attract3` is never played) is named to the
replay (slot 16 `+0x40`, `0x004296F0`: its `+0xCCC`), the race's mode is 4 with one player and `loading` is asked. The
race's load (`0x00461390`) reads the named replay (slot 15 `+0x3C`, `0x00429720`) before the track; the race setup
(`0x00495DE0`, `0x00496512`) makes a car of each car read (`0x00485530`, its grid made when it has none) on the grid
place of its number. hud_replay hides every RP_ element and shows ATTRACT; any input or the play past 0.8 of its
length pauses the race and quits, and the front end comes back to `start`.

## The files

The header (100 bytes from `+0x7BC`): `+0x00` the last block, `+0x04` its frames, `+0x08` the length, `+0x0C` the
blocks' bytes, `+0x10` the track (32), `+0x30` the route (32), `+0x50` the time of day, `+0x54` the weather, `+0x58` the
cars, `+0x59` the mode, `+0x5A` the type, `+0x5C` the cars' stream's size, `+0x60` the laps; after it the cars'
records. `0x00429720` reads `<name>.hdr` and `<name>.dat` (the attract's `attracts\attractN`) through the game's
files: the header, its cars (`0x00426A50`), the blocks (the header's bytes), the race set from the header (`0x004269A0`:
a replay's race, its type (a sprint: heat 0 of 3, of 1 in a network game), laps, track, route, time of day and weather;
the race's mode is left; the cars' count, the length, the last block and frames), saved, from a file (`+0x78`), the
blocks written the header's last block, and the play from the start.

## Saved replays

The replays' save device (`03-saves.md`) hands a write's files and a read's files to its replays' object, whose two
methods have the arguments of the replay's slots 13 and 14; nothing on the PC gives it one (the stock game would call
through address 0), and no window of the PC reaches them: `extras` takes its `replay` item out and `hud_replay.ui` has
no save item. OpenJuice gives the device the game's replay.

The save (`0x004292C0`, slot 13: the files and the date's text) adds three files:

- `USERDATA` (`0x004288B0`, `0x00429A10`; 0x79 bytes): u32 the cars (`+0x814`), u32 the race's type (`+0x816`),
  char[32] the track, char[32] the route, char[17] the date's text and wchar_t[16] the driver (`[G+0x64]` slot 7), the
  texts copied whole (`strcpy`, `wcscpy`). Its record (`0x00429A70`) is cleared but for the last byte of the date, the
  track and the route and the driver's last character, which keep what the stack held;
- `replay.dat`: the blocks written (`+0x08`, 48 at most) from the buffer, their bytes kept in the header (`+0x7C8`);
- `replay.hdr`: the header with the cars' stream's size (`+0x830` into `+0x818`), then the stream from its start
  (`0x004FB630`, at most 1 MB a pass).

The attract's files are such files (`attracts\attract1.hdr` and `.dat` saved again with their blocks come out the same,
byte for byte). The device names the folder `R` and the local time and the date's text "month/day/year hour:minute"
(the hour less 12 past noon, `%2d`).

The read (`0x00429550`, slot 14: the files read) sets saved (`+0xD10`) first; without `replay.hdr` nothing more.
`replay.hdr`'s 100 bytes go into the header (a shorter file leaves the rest), its cars are read after them
(`0x00426A50`) and the bytes they took copied into the cars' stream from its start; without `replay.dat` nothing more,
else its header's bytes go into the buffer, the race is set from the header (`0x004269A0`, as above), saved, not from a
file, the blocks written the header's last block (one less than the blocks there), the play from the start
(`0x00426FE0`), and `[0x0075ED18]` "menu": the window after the race (`0x00461A10` takes it first and empties it).
`loadreplay` then asks for `loading`; `extras` had set the race's mode to 0. The race's setup (`0x00495DE0`) off the
network takes a replay's race (`[G+0x6C]+0x68`, set by the play) before the race's mode, so the replay's cars are made
as the attract's are; the front end made after a race (`0x0046B5A0`) clears `+0x68`. A saved replay quits at once from
`hud_replay` and the front end comes back to `menu`.
