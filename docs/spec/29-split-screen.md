# The split screen race

A race of two players on one machine (the race's mode 3, `[G+0x6C]+0x58` players). The front end's `multi` offers it
("split") when the input manager counts two devices; `instant_manu` (mode 3: on to `instant_setup`), `instant_setup`
and `split_selcar` lead to the race (docs/spec/02-frontend.md, "The custom race"). The code's tables go up to four
players (the views' corners, the controllers by number), which nothing on the PC reaches.

## The second player

The second player has its own save system object (`[G+0x68]`, made as the first's, `0x0040A510`, with the start driver
"Player2", the game's text 168, `0x0040C56E`): its arcade cars (`+0x40`, the second load of `arcadecollection.txt`),
its car among them (`+0x50`), and its own groups of the custom race's cars (`[G+0x88]`, the second load of
`ArcadeUnlock.txt`). Its settings are the first driver's: the race reads each player's own fields at `+4 x` its number
(the view kept `+0x8C`, the automatic gearbox `+0x84`, the force feedback `+0xA4` and `+0xB4`, not speed sensitive
`+0xAC`, the steering's sensitivity `+0xC4`; `opts_ingame_game_ply2` edits the second's).

Its device is the input manager's `+0x2C` (`0x005918E0` with 1 takes one: a device with input that is no player's,
the last of them; an unmapped keyboard has input only by RETURN). The front end makes its frame each frame
(`0x0046C18B`) and the windows take both players' events (`+0x88`, `+0x8C`, `0x005A9E80`); a menu whose `+0x1A0` is set
takes the second's when the first has none (`0x005B1207`).

## The players of the race

The race track's making (`0x00424965`) makes a player (`0x0041DE30`, 0x58 bytes, the track's list `+0x0C`) for each
of the race's players: its number (`+0x00`), its car (`+0x04`), its controller (`+0x08`: the first player's the input
manager's `+0x28`, the second's `+0x2C`; in a replay and for a third or fourth player the controller of its number),
its camera (`+0x0C`, `0x00401760`, with its own fly-by at `+0x1D0`), the camera's director (`+0x10`), its view
(`+0x14`, `0x00512130`: its corners on the screen, the clear colour of the atmosphere, the glow off in mode 3), the
finish (`+0x18`, `+0x1C`) and the steering's rumble timer (`+0x28`). The views' corners (the screen W x H):

| players | views |
|---|---|
| 1 | (0, 0) .. (W, H) |
| 2 | (0, 0) .. (W, H/2); (0, H/2) .. (W, H) |
| 3 | (0, 0) .. (W/2, H/2); (W/2, 0) .. (W, H/2); (0, H/2) .. (W/2, H) |
| 4 | the four quarters |

The camera takes the height's share of the width: on a wide screen 0.28125 with two players, else 0.5625; else 0.375
and 0.75. The race's cars go to the players by their numbers (`0x00425750`: player i the race's car i; a custom race's
players come first on its grid), each player's camera on its car (`0x0041E0B0`: the car the player's, `0x00401DB0`,
the car's sounds to it; the second camera's turns the first's car's away). Each camera starts with the fly-by
(`0x00401E80`; a solo race or a cruise the first driver's kept view, whatever the player).

## The players' frame

The players' manager (`0x004205A0`) runs each player's frame (`0x0041FE50`), the current one at its `+0x18`: every car
shown, the director, the camera (its controls by the player's controller), the driver's control (`0x0041E200`, from
the player's controller; PAUSE pauses while some player has not finished, `0x00420570`, and makes the player the race
track's pausing player `+0x10`), the finish (`0x0041FB60`: the controller let go only with one player), the car its
camera follows hidden under the views on the car, and the view drawn in its part of the screen (`0x00511CA0`).
Then the 3D world's listeners (its `+0x28`: the players' count, each camera's matrix and place); each emitter takes
the nearest listener (`0x005C2890`) unless its instance names one (`+0xF0`).

The race front end makes the second player's frame too (`0x00457BB4`) and watches its controller: lost, the note
"<device + 1>." (Stock problem: the number alone) and the pause with it as the pausing player; back, both players'
notes counted as closed.

## The windows

With more than one player the race front end's HUD is `hud_split` (`0x004563E0`: the HUD object of every race type,
without the map); `0x004584C0` in mode 3 asks for `hud_race_results` once every player finished, else `blank_split`
(the views' divider) while the first player's camera plays the fly-by or the sweep, else `hud_split`. The HUD's
elements carry the player's number (`SPEED1`, `REVS2`, ...).

`hud_pause` (`0x0044E920`) shows PLAYER "Player" + " " + " <n>" of the pausing player (Stock problem: two spaces); the
second player's pause takes FRIENDS and RESTART out and BackMenu's first two items. Its frame takes each player's event
(`0x0044F0B5`): BACK goes on with that player's controller on DRIVER1 again, CONTINUE every player's. The options
from the pause: `opts_ingame_menu` without VOICE for the second player's pause, GAME the second player's
`opts_ingame_game_ply2` by its event; `opts_ingame_audio` each player's event in turn; `opts_ingame_game` with the
second player's pause takes its event and moves the highlight with its UP and DOWN itself (`0x004817C4`).

A race of more players draws each view through the post effects with the view's own rectangle; with the scene drawn
into the effects' target (motion blur, depth of field, a director's cut) the target takes the whole target's size
(`0x00516880`) and the copy to the screen the target's size (`0x005211B0`): the views do not keep their halves.
