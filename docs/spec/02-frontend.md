# The front end of the stock game: from the first window to "start"

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## Languages and texts

The reference build was made for another region: its language index is fixed to that region's (the application
object's method returns a constant). OpenJuice has the region binding removed (below). The index picks:

| index | language | window texts | controls |
|---|---|---|---|
| 0 | english | `.eng` | `controls_dx9_e.txt` |
| 1 | french | `.fre` | `_f` |
| 2 | german | `.ger` | `_g` |
| 3 | italian | `.ita` | `_i` |
| 4 | spanish | `.spa` | `_s` |
| 5 | czech | `.czk` | `_c` |
| 6 | polish | `.pol` | `_p` |
| 7 | russian | `.rus` | `_r` |

It also picks the speech banks (`audio\speech\ui<letter>`, `race<letter>`; the installed data may hold a few
languages' only, the others then have no speech) and the movies' sound track (the index, `[0x0075F79C]`'s method
`+0x08`; a movie of one track plays that one).

The English build 1.0.1 picks the index at the start (`0x0059E300`, kept in the application object's `+0x08`): the
settings' `language` when it is not negative, else the user's language (`GetUserDefaultLangID`), taken as a Windows
language id; by its primary language German (0x07) 2, Spanish (0x0A) 4, French (0x0C) 1, Italian (0x10) 3, any other
English 0. OpenJuice picks it the same way and also takes Czech (0x05) 5, Polish (0x15) 6 and Russian (0x19) 7, so
the eight languages of the data are reached; `[Game] Language` of `OpenJuice.ini` names one instead. With none picked
the language is English.

- The game's text table: `scripts\<language>.txt`, UTF-16 with a byte order mark, one quoted text per line; the index
  of a text is its line number from 0 (line 24 is "PRESS ENTER").
- A window's texts: `ui\windows\<language>\<window>.<ext>`, UTF-16, one text per line, the last line "END"; the
  window's `Caption n` is line n. A language's set may lack some files of the English one (in the installed data the
  Russian set has no `legals`, `legal_car1..3`, `videos`, `vid_demo`, `vid_extras`, ...): those windows show no texts.
- Both are read by `0x005AE520`: the first two bytes (the byte order mark, or a space in the files without one) and
  a space after them are passed over; a line ends at CR LF (or CR CR LF); the reading stops at a line starting "END"
  (not "ENDE"), " END", " end" or "end". Every `"` of a line is taken out wherever it stands (so `"LEVEL 2` without
  its closing quote and `"QUIT CAREER" ` with a space after it read "LEVEL 2" and "QUIT CAREER "), the two characters
  `\n` become a line break, and a line holding `#` is a comment that gives no text. A text "ILLEGAL", "Illegal" or
  "illegal" is replaced by "PINKSLIP", "PinkSlip" or "pinkslip" (the pairs set up by `0x006C40E0`).

## Interface windows and fonts

See `04-interface.md`.

## Front end states

Every state names its window (`ui\windows\<state>.ui`); a state asks for the next one by writing its name (`+0xC0`
of the front end object, flag `+0xBC`). States are registered by name at `0x0046AF80` (`splash`, `juicesplash`,
`skippable`, `disclaimer`, `legals`, `menu`, `start`, `loading`, `driver`, `credits`, ...); each has enter / update
methods.

| state | enter | update |
|---|---|---|
| `disclaimer` (`0x00466D00` / `0x00466D70`) | timer 5 s; BOX01..03 flag +0x128 | timer -= frame time; below 0 → `legals` |
| `legals` (`0x00466DF0` / `0x00466FC0`) | hides elements whose name holds DOLBY, LOGITECH, ESRB, ACCLAIMLOGO, JUICEDACCLAIM; text flag +0x128 on OWNERS, NOKIA, BINK, DEVELOPED, JUICEDTHQ, ONLINE, DOLBY, LOGITECH; timer 9 s | a press (the input manager's `+0x08`, see "Input") with less than 4 s left, or the timer below 0 → `splash` |
| `splash` (`0x00466840` / `0x004668C0`) | SPLASH (movie THQ) `+0x114` = 0.3; the window's transitions started again (`0x005AA0A0`); `+0x0C`, `+0x10` 0; FADEOUT3's transition waits 12 s (`0x0046C970`: its delay, below 0 none, past 9999 10000) and starts again | a press sets `+0x10`, which makes `+0x0C` count the frame time; the window's transitions over, SPLASH's movie finished, its frame past 181, or `+0x0C` past 0.4 → `juicesplash` |
| `juicesplash` (`0x00466C00` / `0x00466C30`) | SPLASH (movie juicegames) `+0x114` = 0.7 | a press, or SPLASH's movie finished → `skippable` |
| `skippable` (`0x00466A40` / `0x00466AD0`, leave `0x00466AB0`) | SPLASH plays JuicedCGI_PC once (`0x005AA300`) at `+0x114` 1.0; on a wide screen (`[0x0075F0FC]`) SPLASH at (0, 0.05), 1 x 0.9 of the screen; `+0x08` 1 | `+0x08` cleared once no key is down; then a press, SPLASH's movie finished or its frame past 2900 → `start`; the leave starts the front end's music (`0x0046BDD0`) |
| `start` (`0x00467060` / `0x00467350`) | the save system's settings back to a new driver's (`0x004097F0`); the music playlist shuffled again (`0x00408D20`) | RETURN held (a first player's device taken, see "Input") → sound CLICK, `driver`; idle for a while → an attract replay (`attracts\attract1..3`) and `loading` |

The first state is `disclaimer` (or `loading` with `-i`). The "PRESS" text of `start` has no caption in its window.

## The frame time

The game's frame time is the time between two presents (`0x00514A40`: the application's timer, `QueryPerformanceCounter`
ticks x (1 / the frequency as a float)); its sum is the game's clock (the application's `+0x18`), which the scrolling
and animated textures of the interface and the 3D materials go by. The front end's frame (`0x005A6AB0`) keeps the
frame time held to 0.1 s at most (`+0x104`): the states' timers, the transitions, the menus and the 3D views run by
it. It draws each window of the stack (`0x005A9D20`, every element's draw stepping its transition), the top state's
own draw, the window over the windows (`+0x30`) and the message (`+0x44`); while a message is up the other windows
draw only their elements whose name holds `FRAME_` (`[0x0071E710]`, 1 but while `CMSRespectMsg` is up, then none). The frames are held to 60.1 a second (after the present the game yields until 0.016638935 s have passed since the
last frame; no limit with `-r`).

## Switching windows

A state asks for a switch with `0x005A8520` (or an inline copy of it): the front end's `+0xBC` takes the mode and
`+0xC0` the name, only while no other switch waits (`+0xBC` 0); mode 3 also sends the top window's elements out
(`0x005AA0E0`: each transition not going out goes out). The frame (`0x005A6AB0`) begins with the switch
(`0x005A8580`):

| mode | switch |
|---|---|
| 1 | mode 2 with `+0x100` = 0 |
| 2 | `+0x10C` 0, `+0x100` less the frame time; below 0: the switch (below) |
| 3 | once the top window's (the last of the stack) transitions are over (`0x005AA120`): the switch |
| 4 | `0x005A6F90` with 1: the switch without the name's check |
| 5 | the new window opened over the current ones |
| 6 | the top window closed (its exit); the window under it marks its elements (`+0xB8` = 1) |
| 7 | 6, then 5 (the top window closed, the new one over the rest) |
| 8 | `0x005A7040`: the window over the windows (`+0x30`): one there freed without its leave, the window of the name taken from the loaded ones (`+0x10`) or loaded, its state entered |

The switch: when the bottom window has a state and the name that state was registered under (`0x005A8F20`, the first
name of its record: `careerload` and `garageload` share one, `garageload` first) is the name asked for (case aside),
nothing happens; else every window and `+0x30` (its state's leave first) closed and the new window opened (enter). A
window opens even when no state has its name (a window named "" has no elements and no state).

A message window closes at the switch when its close flag (`+0x48`) is set. Then the windows run: with the message
window (`+0x44`) it runs first (`0x005AA010`: the events, the elements' updates, the highlight, the state); a message
that waits for an answer (`+0x4C`, types 1 and 3) or mode 2 stops the rest. Of the stacked windows the top one runs
fully, the others only their 3d elements (`0x005AA060`); then the window over the windows (`+0x30`) fully. Drawing:
the windows from the bottom, the window over them, the message window, then the front end's black (`+0x10C`, alpha x
255, falling by the frame time; careerload's exit sets 1.0). The window over the windows is `CMSLoader` (whose enter
`0x004CA910` keeps the music's volume and silences it, its leave `0x004CA950` giving it back) or
`car_StatsLogos`.

## The windows a manager keeps

A window opened by `0x005A6DB0` (or `0x005A7040` for `+0x30`) is first looked for in the manager's list of kept
windows (`+0x10` .. `+0x14`, by name, case aside): one found has only its elements' transitions started again
(`0x005AA0A0`: each transition's time back to its length, its delay back, progress and alpha 0, coming in, the element
shown) and is opened as it was left: its elements' hidden flags, texts, colours, places, menus and 3d views stay.
Another is made from its file (`0x005A9490`, its Movie lines giving the interface's movie back then). A window closed
(`0x005A6D50`: its state's exit) is freed unless the list holds it.

`0x005A8430` makes a window and adds it to the list. The manager (`0x005A5F90`) keeps `msgbox`, `msgbox_critical`
and `msgbox_network`. The game's front end (`0x0046B5A0`) then, outside a replay's race (`[G+0x6C]` mode 4), opens
`startup` (no record: its ACCLAIMLOGO elements hidden, none; JUICELOGO at x 0.2) and draws three frames of it, and
makes the rest on the loading thread (`0x0046C8A0` running `0x0046AF80`), the startup window drawn every 10 ms
meanwhile: instant_seltype, instant_setup, instant_selcar, instant_manu (`0x0045FBC0`); menu, start, loading, driver,
credits, vid_tutorial, loadreplay, viewhiscores, bootupcheck, extras; opts_audio, opts_bg, opts_menu, opts_game_pc,
opts_songlist, opts_ctl_port, opts_graphics, opts_ctl_pc, opts_wheel_dx9, opts_voice (`0x00484CF0`); garageload,
careerload, custmenu, wheelsmenu, decalmenu, sponsordecalmenu, tuningmenu, stylingmenu, modmenu, neonmenu,
hudcolourmenu, wheelcolourmenu, icemenu, hornmenu, tintsmenu, colourmenu, colourchoose, colourfocus, schememenu,
suspensionmenu, enginechip, suppliers, exhaustmenu, performtype, performmod, performance, gearboxmenu, modshop, buymod
(`0x00446CE0`) and Car_Stats2, Car_Stats2purple, Car_StatsLogos (`0x004C5470`); character, characterPostRace;
CMSRespect, CMSRespectOverview, CMSRespectMsg, CMSRespectHelp (`0x004DC1E0`); CMSRaceResults, CMSGridWalk,
CMSMeetingSummary, CMSRespectChanges, CMSBet, CMSLoader, DemoFlyby; saveprofile, savereplay, autosave,
loadhighscores, savehighscores (`0x00466650`); link_search, link_lobby, link_selcar, split_selcar (`0x00471640`);
Arcade_Race, Arcade_Championship, Arcade_Results, Arcade_Loading (`0x0042C530`); CMSMainMenu, CMSCalendar,
CMSCalendarHelp, CMSCrew, CMSAtMeeting, CMSChallengeResults, CMSHostMeetingSetup, CMSHostMeetingSetupRace,
CMSHostMeetingSummary, CMSMobilePhone, CMSMobilePhoneMenu, CMSMobilePhoneContacts, CMSRaceSetup, CMSCrewRaceSetup,
CMSCarCollection2, CMSCarDealership; CMSNewCont, CMSChooseLogo, CMSMobileChoose (`0x004C3780`); CMSCheats,
CMSAdvice; CMSStats, CMSStatsMenu, CMSStatsCarCollection (`0x004E1610`); CMSOnlineRaceSetup,
CMSOnlineCrewRaceSetup, CMSOnlineGridWalk. The 3d elements of these windows are made then: their SetModel models
(Angel_City of Arcade_Race, CMSAtMeeting and the host meeting windows) draw their `rand()` values before the music
bank's. The loading front end (`0x004634B0`) keeps `loader`; the race's front end (`0x004563E0`) its HUD windows, the
in-game options' (`0x00484E20`), the online windows' (`0x0047FB30`) and autosave, savereplay, savehighscores,
newhighscore_cms (`0x004666E0`).

So a kept window's enter finds what its last visit left. The windows' base enter (`0x005A5F20`) gives a Menu element
(a menu) its items back (`0x005AECF0`), then on the record's first enter (its `+0x08`, 1 in the records' data)
clears `+0x08` and only sets the help ticker (`0x005AF860`), on a later one highlights the item the base leave
(`0x004C7F70`) kept in the record's `+0x04` at once; and starts every ticker of the window again (`0x005B52C0`). The
records are the game's own data: what they keep outlives the front ends, and the names registered with one record
share it (careerload and garageload; opts_bg, opts_menu, opts_audio, opts_game_pc and the race's opts_ingame_bg,
opts_ingame_menu, opts_ingame_audio, opts_ingame_game, opts_ingame_game_ply2; online_bg and online_ingame_bg;
online_arcade_results and online_career_results). A front end that goes (`0x0046BF70` -> `0x005A6770`: before a race loads, after it
unloads for the loading front end) closes its windows with their states' exits (the stack from the bottom, the window
over the windows, the message windows) before it frees them; the records keep what those exits left. After a race the stock code makes the front
end on the unload's job thread, so its startup window is drawn by that thread while the main thread draws the loading
window (OpenJuice keeps the loading window; at the game's start it draws the three frames and makes the windows at
once).

## The message window (`0x005A80E0`)

A message replaces the one open. It uses the window `msgbox` (`0x005A7170`): every element shown, then by the type
(`+0xC` of the state `0x00722FAC`) hidden:

| type | hidden | waits |
|---|---|---|
| 0 | MSG_CANCEL, MSG_CONTINUE, MSG_CONTINUE_ICON, PROGRESS_BAR, PROGRESS_BDR; names holding MSG_MENU, SEL, BACK_, MSG_BOTTOMPANEL, MSG_ANI, ANIM | no |
| 1, 2 | MSG_CANCEL, PROGRESS_BAR, PROGRESS_BDR; names holding MSG_MENU, SEL_, BACK_, MSG_BOTTOMPANEL, MSG_ANI, ANIM | 1 yes |
| 3, 4 | MSG_CANCEL, MSG_CONTINUE, MSG_CONTINUE_ICON, PROGRESS_BAR, PROGRESS_BDR, SEL_TEXT2, SEL_RECOVERY; names holding ANIM | 3 yes |
| 5 | MSG_CANCEL, MSG_CONTINUE, MSG_CONTINUE_ICON; names holding MSG_MENU, SEL_, BACK_, MSG_BOTTOMPANEL, MSG_ANI, ANIM | no |
| 6 | MSG_CANCEL, MSG_CONTINUE, MSG_CONTINUE_ICON, PROGRESS_BAR, PROGRESS_BDR; names holding MSG_MENU, SEL, BACK_, MSG_BOTTOMPANEL, MSG_ANI | no |

The call (`ecx` the type, `edx` the front end) passes, after its first value and the text, the answer (the state's
`+0x18`), `+0x1C` (a pointer to a progress bar's value, read each frame by type 5), `+0x20` (a note's answer), `+0x24` (the player whose
events it reads, `[front end + 0x88 + 4 n]`; 0 but in the race's pause), `+0x28` (the menu starts on
`no`; in `msgbox_network` SELECT answers by it, not by the item) and the small font. Every
question of the stock code is in the normal font and starts on `no`, but `savereplay`'s (`0x004654E2`) and the
network windows' (`0x0047543E`, `0x0047706E`), which start on `yes`.

MSG_DATA takes the text over several lines, `title` is emptied. In the small font MSG_DATA takes SMALL_FONT's font
when it differs; otherwise ORIGINAL_FONT's when that has one and differs. The state's enter (`0x005A9180`) selects
the MSG_MENU item `yes` (`no` with `+0x28`), MSG_MENU_BACK its first. The update (`0x005A9200`) waits for the
window's transitions; then SELECT: type 3 / 4 writes 1 to the answer when the selected item is `yes`, else 0; type 1
/ 2 writes 1 when there is an answer; sound CLICK and the close flag (in `msgbox_network` BACK answers too).

## DemoFlyby, the career's first sight of the city

State record `0x0071FC14`: enter `0x00435090` (the FADE box black), update `0x004352B0`. The fade `+0x10` falls by
the frame time from 1 to 0; FADE's alpha is the fade (before the narration) or 1 - the fade (after it), x 255 cut.
With the speech object (`[game + 0x4C]`): at fade 0 the narration "narrDemo" starts (`+0x14`); when it has ended
(`0x0040B5A0`) the fade starts again from 1 (`+0x18`), and at its end the way into the career follows (`+0x1C`).
Without the speech object: at fade 0 `+0x14` is set, the next frame (nothing playing) `+0x1C`. SELECT takes the way
in at once. The way in: `08-career.md`, "Into the career"; then `CareerLoad` (`14-career-windows.md`).

## Input

The controls file is `scripts\controls_dx9_<letter>.txt` of the language (`controls_dx9_e.txt` for English, the table
at `0x0040BE84`): action maps (`[BeginActionMap]`, `Name`, `Device`, then `[BeginAction]` blocks with `Name`, `Type
DIGITAL|ANALOGUE`, `OneShot`, `Control "<semantic> '<label>'"`, `Control2`, `Deadzone`, `Saturation`, `Sign NEGATIVE`;
`;` starts a comment; an action may be listed once per control). The game maps the maps of `Device ACTION_MAP`
through DirectInput; a `DIKEYBOARD_<key>` semantic is a key of the keyboard (the names the executable knows are the
letters, F1..F12, NUMPAD0..9, the arrows, RETURN, ESCAPE, SPACE, TAB, PRIOR, NEXT, END, INSERT, DELETE, MINUS, EQUALS,
ADD, LBRACKET, RBRACKET, LSHIFT, RSHIFT, RCONTROL).

The input manager (`0x00591050`, vtable `0x006FC4E0`, `[G+0x18]`) makes its own DirectInput (`CoInitialize`,
`DirectInput8Create`) and lists the attached game controllers (`EnumDevices` with `DI8DEVCLASS_GAMECTRL`, the
callback `0x00590EA0` -> `0x00591650`): each is made (`CreateDevice`) and kept with its instance GUID; one with force
feedback (`DIDC_FORCEFEEDBACK`) is noted ("Found force feedback device!" to the debugger) and kept in a list of its
own; `c_dfDIJoystick2`; the cooperative level `0x14` (`DISCL_FOREGROUND | DISCL_NOWINKEY`: neither exclusive nor not,
so DirectInput refuses it and the default level stays, a stock problem); its objects listed (`0x00590EC0` ->
`0x00591770`): each axis gets the range -1000 .. 1000 (`DIPROP_RANGE` by its ID) on the first controller listed,
whichever controller's objects are listed (a stock problem). Then the keyboard (`GUID_SysKeyboard`,
`c_dfDIKeyboard`, level `0x16`, acquired) and the mouse (`GUID_SysMouse`, `c_dfDIMouse`, level `0x0A`; never
acquired nor read). A device object (`0x00594950`, vtable `0x006FC5BC`, 0x3B8 bytes, named `ACTION_MAP`) is made for
each controller in the list's order, each holding the DirectInput (`+0x38`), its controller (`+0x3C`), the keyboard
(`+0x40`, one object all the devices share) and the mouse (`+0x44`); with no controller one device without one. Then
the force feedback manager (`0x00591DF0`, `[0x0075F5F8]`; below) and its response table (`0x005914D0`). OpenJuice reads
the keyboard from the game window's messages (the scan codes are DirectInput's keyboard codes) and makes the rest
(`Game/Platform/InputDevices`); a test run (`--test`) lists no controller.

Each frame the input manager's update (`0x0040CCF0` -> `0x005918C0` -> `0x0058FE50`, by the frame's time) first takes
the first player's device when it has none (`+0x28` -1, `0x005918E0`: each device with input by its last poll that
is not already the first's or the second's (`+0x2C`) becomes it, the last of them winning; its `+0x0C` its number),
then polls every device (its `+0x1C`, `0x00594C20`), then reads the wheels of the force feedback manager
(`0x00592050`). A device's poll:

- With the window inactive (`[0x0075F780]` 0) or the 3D device lost (`[0x0075F0F8]`): nothing is read, the actions
  do not step and the device reads as not connected (`+0x34` 0).
- Its controller: `Poll`; failing, `Acquire`; that failing too, `Unacquire` (whose result then counts). With an
  action map (`+0x78`) the buffered data (`GetDeviceData`, room for 32) each set the value (`+0x58`) of their
  `uAppData`; raw the state (`GetDeviceState`, `DIJOYSTATE2`, `+0x7C`). The device is connected (`+0x34` 1) while the
  last result is not an error; a device without a controller always.
- The keyboard: raw (`+0x78` 0) its whole state (`GetDeviceState`, `0x00594DB8`) into `+0x19C` and the characters
  looked at, so a key down and up again between two polls is not seen; no key has a value. With an action map one
  buffered event (`GetDeviceData` with room for one, `0x00594D4A`), only of the keys of the map registered last on
  the shared keyboard: a key of a character (`uAppData` with its high bit) sets its state and the characters are
  looked at (`0x00595A80`); a key of the map sets its value to the event's data, kept 0 while an Alt key's state is
  down. So several keys going down in one frame are read over as many frames, a key held through a map's making gives
  nothing until it goes down again, and the keyboard's events while the window is inactive are lost (DirectInput's
  foreground keyboard is unacquired; a key let go then keeps its value).
- The actions step (`0x00590B50`, only while connected).

A map is registered (`0x0058F890` through the manager's `+0x04`) on a device the caller emptied first
(`0x00595890`: with a map, its actions, its DIACTIONs (`+0x48`), values and their flags (`+0x68`) emptied, the
controller, the keyboard and the mouse unacquired and set to their plain formats; a device without a map is left as
it is). The first map of the name whose `Device` is the device's name (`ACTION_MAP`) or none is taken; without one
nothing is made. With "UI" in its name the keys of characters are added as DIACTIONs (`0x00595030`: the digits,
backspace, `-`, the letters, `@`, `.`, space, caps lock, both shift and both Alt keys; `uAppData` the key's code with
the high bit, no value). Each action by the kind of its control's semantic (the table `0x0071E060`, `0x00593D00`; the
control's text is split at its first space, the label between the two quotes is the DIACTION's name, the flags after
them: APPFIXED 0x10, APPMAPPED 2, APPNOMAP 4, FORCEFEEDBACK 1, NORANGE 8; `0x00593D70`):

- A key or a button (kind 0: `DIKEYBOARD_*`, `DIBUTTON_*`): the control and Control2 made (`0x005940B0`, value flag
  1; read by `0x00594030`: 1 while its value is not 0). DIGITAL: on while the first's value is (`0x00590050`,
  made with the value then; `0x005900F0`; a OneShot one only at the poll it comes on; Control2 made but not
  bound). ANALOGUE of one control: up by Attack a second while it is on and down by Decay, within 0 .. 1
  (`0x00590180`); of two (Control towards -1, Control2 towards +1): a value of the other side drops to 0 and it goes
  by Attack a second, neither on it goes toward 0 by Decay a second, within -1 .. 1 (`0x00590360`; the keyboard's
  STEERING has Attack 1.5 and Decay 3).
- An axis (kind 1: `DIAXIS_*`): the control (`0x00594390`, value flag 0; read by `0x00593F70`: its value x 0.01),
  for STEERING with the steering wheel on (the manager's `+0x38`, the save system's `+0x78` as the map is registered)
  through the response table (`0x00594670`, read by `0x00593F90`: (the value + 100) x 5.12 made whole (to the nearest
  with SSE, `[0x0079E5F8] & 2`), at most 1023, the table's entry x 0.0001). DIGITAL: on while the value x the sign
  passes the dead zone (`0x00590130`; a OneShot one made as held, so it gives nothing until let go). ANALOGUE of one
  axis (`0x00590200` / `0x00590260`): the value x the sign past the dead zone (either way) less it, x 1 /
  (saturation - dead zone) (0 when the saturation is not above it), within -1 .. 1; a target of the other side clears
  the value; it moves toward the target by Attack a second when the target lies further on its side, else by Decay.
  Of two axes: the second's value less the first's (`0x0058EA50`).
- Another semantic: the action is passed over.

A semantic already made keeps its value (one DIACTION and one value per semantic: THROTTLE and BRAKE share
`DIAXIS_DRIVINGR_ACCEL_AND_BRAKE`). The device then registers the map (`0x005954D0`): a `DIACTIONFORMAT` of its
DIACTIONs (`uAppData` the value's index), the map's GUID line (`0x00595490`: each field "0x" and hexadecimal digits,
`0x005953D0`), the genre `DIVIRTUAL_DRIVING_RACE`, a buffer of 32, axes -100 .. 100 and the map's name; the controller,
the keyboard and the mouse unacquired; `BuildActionMap` and `SetActionMap` on the controller and the keyboard (DirectInput
assigns the semantics to the device's objects); the device mapped (`+0x78` 1), its controller's and keyboard's states
cleared. So only the map registered last is the device's, and its actions are the device's actions.

An action's value is that of its biggest binding, the first of equal ones (`0x005905E0`); a DIGITAL action is on while
a binding is (`0x00590590`). The controllers (`[G+0xAC]`, one per device) keep the actions' indexes by name (any case,
`0x00590C20` / `0x00590CA0`): the UI map (`0x00407FB0`) the UI actions below and AXIS1_X, AXIS1_Y, AXIS2_X, AXIS2_Y
(`0x00711600`), DRIVER1 (`0x004080D0`; DRIVER2 .. DRIVER4 by the save system's `+0xCC` of the device's number)
PAUSE, SELECT, REAR_VIEW, GEAR_DOWN, GEAR_UP, CHANGE_CAMERA, RESET, NITRO, DRIVER_HORN, NEXTCAR, the six
DRIVER_TACTICS ones and the analogue STEERING, THROTTLE, BRAKE, HANDBRAKE, REVERSE, DRIVER_CAMERA_PAN; every other
slot is -1. A slot of -1 or past the device's actions reads 0 (`0x004015D0`, `0x00401640`). Registering the UI map
(`0x00407FB0`, after the force feedback stops, below) or DRIVER1 (`0x004080D0`, after the response table of channel
0 is made again by the save system's `+0xAC`: 20 not speed sensitive, else -20) sets the manager's `+0x38` from the
save system's `+0x78` first. The response table (`0x00596C00`): 1023 entries trunc(((1 - k) x + k x³) x 10000) for x
= (i - 511) / 511 and k the nonlinearity / 100, the last entry the one before.

Of DRIVER1's keys besides the driving ones: DRIVER_HORN (H) sounds the car's horn while held (`0x0041E2D7`, the car's
`+0x4C0`); DEBUG_LDPAD_LEFT .. DOWN, DEBUG_RDPAD_UP and DEBUG_RTHUMB step the songs of the race's
and the front end's music players (`0x0040B840`, `0x0040B950`, vtables `0x006EEA0C` and `0x006EEA34`, made by
`0x0040D3C0`); the other DEBUG_ actions are read by nothing.

An action without its values takes Deadzone 0.2, Saturation 1, Sign 1, Attack and Decay 1000 (`0x0058F165`). In 1.01
an analogue axis whose Deadzone is 1% (0.009 .. 0.011) takes 0.2 instead (`0x005920F0`, `patch-1.01.md`).

The interface reads the actions of the map `UI` in this order (`0x00711540`, looked up by `0x00407FB0`): LEFT, RIGHT,
UP, DOWN, SCROLL_LEFT, SCROLL_RIGHT, SCROLL_UP, SCROLL_DOWN, SELECT, SELECT2 .. SELECT7, BACK, BACK2, START. Each frame
they make the window event (`0x00408260`):

1. Directions: while none of LEFT, RIGHT, UP, DOWN is on, the repeat timer and the last direction are cleared. A
   running timer counts down by the frame time (kept within 0 .. 1) and no direction is given. Else the first of LEFT
   (3), RIGHT (4), UP (1), DOWN (2) that is on is the event, and the timer is set to 0.1 s when it is the last
   direction again, 0.25 s when it is new.
2. START on: event 13 and the start flag.
3. Then, the first that is on: SELECT or START → 5; BACK or BACK2 → 12; SELECT2 → 6; SELECT3 → 7; SELECT4 → 8; SELECT5
   → 9; SELECT6 → 10; SELECT7 → 11.
4. The scroll value: SCROLL_LEFT 3, SCROLL_RIGHT 4, SCROLL_UP 1, SCROLL_DOWN 2.

With the stock keyboard controls: arrows, RETURN (SELECT), ESCAPE (BACK, BACK2), NEXT / Page Down (SELECT2), TAB
(SELECT3), F2 (SELECT4), F1 (SELECT5). UP and DOWN repeat while held; LEFT and RIGHT are OneShot. The event also
carries the four values of the map's analogue actions AXIS1_X, AXIS1_Y, AXIS2_X, AXIS2_Y (`+0x08 .. +0x14`; read by
no window found) and the device's connection (`+0x18`, its `+0x34`).

Besides the actions the input manager answers two questions. Its `+0x08` (through `+0x48`, `0x00591820`) is true
while a device typed a character at its last poll (`+0x39C`, set by the keyboard's character scan `0x00595A80`: the
keys of letters, digits, `-`, `.`, `@`, space, backspace, RETURN, ESCAPE, caps lock, the shift keys) or has input
(`0x00594E20`: with a map a value of a key or a button (value flag 1) not 0; then a raw controller's button down
(`+0xAC ..`); then RETURN's state `+0x1B8`): `legals`, the attract replay's window and `byebye` ask it. The typed
character and the shift key's state asked are those of the first player's device (`0x00591870`, `0x005917E0`; the
last device without one). The players' devices (`+0x28`, `+0x2C`) are cleared by `0x0058FEA0` (every device
emptied too), the second's by `multi`'s and `instant_manu`'s enter (`0x0046CAA0`, `0x0045E490`).

The windows `disclaimer`, `legals`, `splash`, `juicesplash` and `skippable` call `0x0058FEA0` each frame, `start` as
it comes in (`0x004671C2`), the attract replay's window as it ends (`0x0044DFF0`): the keyboard is raw there, so
RETURN held is its only input and arrows are no press. `start` goes on once the first player's device is taken
(`0x00467365`; the device takes it at the update after the poll that saw the press), so only RETURN or a raw
controller's button leaves the title. The front end's frame (`0x0046BFD0`) makes the first player's window event only
with the first player's device taken and no switch waiting (the window manager's `+0x100` 0): then it registers the UI
map once (its `+0x18`, with `+0x1C` set; both cleared while there is no first player and by `start`'s enter,
`0x0046C051`) and makes the event (`0x00408260`). The race front end makes it each frame (`0x00457180`).

While the front end watches the controllers (its `+0x38`: set as `start` goes on, `0x00467350`; cleared on the way into
the career, `0x0043542B`, and by CMSLoader's enter, set by its leave) the event's `+0x18` (the device's last poll read
it; never with the window inactive) tells of a lost controller: lost with no note up (`+0x1C` set), the note opens
(`0x005A8030`: the window manager's `+0x34`, `msgbox_critical` of type 0, its state `0x00722FD8` with the priority 1
kept in `[0x00722FEC]`; not over a note of a lower priority unless that one is closing) with no text: "To continue,
please reconnect the DUALSHOCK®2 analog controller into controller port " (1632) and "<device + 1>." (the PS2's
text) are made and emptied before it opens (`0x0046C10B`), so the screen fades to black with nothing on it (the
stock game seen windowed, its window losing the focus, 2026-10-21);
opened, `+0x1C` 0, the game's sounds paused (`0x0040D8E0` with 1) and the interface's volume 0; on the network before
the race (stage under 9) our player not ready (`0x004EA370` with 0, 0). Connected with a note up: the UI map registered
again, the note closing (`0x005A7F70` with 1: the window manager's `+0x38`, closed at the next frame), the sounds
resumed, the interface's volume the settings' effects', `+0x1C` and `+0x24` set (with the front end's `+0x30`, set by
nothing found, the network game would end and `menu` be asked). No first player or a switch waiting: the note of
priority 1 or more closes. With the note up it alone takes the events; the message and the windows wait, every element
of the windows draws, and it draws over the front end's black.

The second player's part (`0x0046C18B`) runs whatever the first player's did: with a device of its own (the input
manager's `+0x2C`) its UI map is registered once (`+0x20`; `+0x24` set) and its frame made (`0x00408260`), the window
manager taking both frames (`0x005A6AB0`: the windows' `+0x88` and `+0x8C`). Its controller is watched whatever the
front end's `+0x38`: lost with no note up (`+0x24` set), unless the first player's closed one this frame, the note
opens (priority 1) with 1632 and "<device + 1>." not emptied (Stock problem: the PS2's text), `+0x24` 0 when
opened, nothing paused, nothing sent; connected with its note up,
the note of priority 1 or more closes and `+0x24` and `+0x1C` are set (nothing more). `+0x20` stays as its device goes:
only `split_selcar`'s BACK and a new driver clear it (with `+0x24`), so a device taken later after another way out keeps
the UI map it had.

The race front end (watching from its making, `0x004563E0`, but in `restart_race`, `start_replay` and
`restart_replay`, whose enters clear its `+0x38` and whose leave `0x00455F50` sets it; not in the attract replay, race
mode 4) does the same for the first player (`+0x14`, `+0x18`): lost with no note up, a replay playing at its speed 1
(`[G+0x9C]+0x30` when `+0x828`), the note opened at priority 1 with no text (a stock problem: the second player's has
the text); opened, `+0x14` 0, the window asked dropped, outside a finish, a replay's race and a pause the race paused
(`0x004263E0`) with `hud_pause` asked, else the music's song paused (`[G+0x48]+0x58`); in a replay's race with the
replay not done it stopped (`+0x7C` 0, `+0x30` 1, `+0x820` 2); the UI map registered. Connected with the note up:
DRIVER1 registered (`0x004080D0`), the note closing, `+0x14` and `+0x18` set, the UI map registered (in a replay's
race `hud_replay` asked and the UI map registered again). So leaving the game's window in a race pauses it.

In the race DRIVER1 is the device's map as the race starts (`0x0041DE76`), as it goes on after the
pause (`0x0044F6FE`) and as it starts again (`0x00455B1A`, `0x0041E1E5`); the UI map at the pause (`0x0041F6F4`), at
the finish of a lone player off a replay (`0x0041FBCF`: then the driver's keys and the views' read nothing more), as
the results' windows and `hud_replay` come in, each 4 s of `hud_race_results` and `hud_sprint_results` (`0x004534A4`,
`0x00454E98`), as the sprint's rows are filled (`0x00453BDA`) and after the options' controls (`0x00480612`,
`0x0048062C`).

An event that comes while an element of the window is still in its transition ends all transitions of the window
(`0x005A9E80`); the event is still handled. Events 1 .. 4 move the highlighted button to the neighbour it names
(`UpButton`, `DownButton`, `LeftButton`, `RightButton`; button `+0x154`, `+0x174`, `+0x194`, `+0x1B4`) with the
sound SELECT; event 5 calls the highlighted button's handler, if it has one (`0x005A9F70`).

### Typed characters

The keyboard device keeps one character per frame (`0x00595A80`, cleared before the frame's key data): for every
key change it goes through these keys in order and takes the character of each key that is down and was not
(a flag per character), so the last new key wins: ESCAPE 0x1B, 1 .. 9, 0, BACKSPACE 0x08, MINUS '-', Q W E R T Y U I O
P, RETURN 0x0A, A S D F G H J K L, AT '@', Z X C V B N M, PERIOD '.', SPACE ' ', CAPITAL 0x0E; then either shift key
gives 0x0F. Letters are upper case. The per-character flag tells whether its key is held (asked for 0x0F, shift).

## Buttons

`Begin Button` elements: a box (`Colour`, ...) with a `Caption` in `Font`; the highlighted button draws its caption
in `HighFont`. The button marked `Highlight` is highlighted when the window opens. `Font` sets the element's colour
`+0x11C` and `+0x150`, `HighFont` `+0x12C` (property handlers `0x005AC3A0`, `0x005AC500`).

## bootupcheck

Registered, but no window asks for it. Enter (`0x004673E0`): the save system's state (`[0x00767FE8] ..
[0x00768070]`) cleared and its check started (`[game + 0x60]`'s method `+0x18`); the elements named FRAME_BG hidden
(the `_stricmp` at `0x0046746C` hides on a match; FRAME_BG is the window's only element, so it comes up black);
`driver` asked for (mode 1). Update (`0x004674F0`): while the check has no answer (`[0x00767FE8]`, the method `+0x08`;
`+0x30` 1 sets `+0x20`) it waits, then asks for `driver`. Leave (`0x004674D0`): the check ended.

## driver

`driver` (`0x004678C0` / `0x004679B0`; vtable `0x006F25E0`) works through the save device (`03-saves.md`). Enter:
TITLE "Profile Manager" (2017), the elements whose name holds "DATE" hidden, the records to be read again (their
`+0x1120` 0), the list cleared (`0x00467800`: MENU and backmenu back to "CREATE NEW DRIVER", line 551, value -1, and
its bar `ui_pan5_ora`; the device started again and asked for the list). Each frame MENU and backmenu take events
only with no message up; the elements holding DEL_ show while a driver is highlighted, SEL_TEXT "SELECT" (1560); the
profile device steps (the list a folder a frame); idle with a driver read it goes to `menu` (mode 2), else the list
is asked again. Its flags: the list made (14) fills MENU once (`0x00467620`: after "CREATE NEW DRIVER" the drivers,
the latest saved first, value their place; past 10 the note "Cannot display all the savegames! Displaying the first
10." and the rest left out; the first driver highlighted, or "CREATE NEW DRIVER" with none: `0x004677C4` passes the
list's being non-empty, 1 or 0, as the item to `0x005AF8A0`); a driver read (11) shows "Load Successful" (a note, type 1, that stays up over the main
menu until answered) and turns the saving on; a read failing (15, 18) shows its note, turns the saving off and lists
again; the delete question (20) "Are you sure you want to delete driver <name>?" (type 4, starting on no); deleted
(21) "Delete successful!" (1496) and the list again; not deleted (22) "Could not delete driver." (Stock problem: the
close flag set for the "Deleting..." note, 23, closes it the next frame). SELECT (the list ready, no question or
message): a driver is read; "CREATE NEW DRIVER" with fewer than 10 drivers listed sets up the name window and goes to
`keyboard` (CLICK twice, the career head value, the game object's `+0x7C`, cleared), with 10 or more the note "There
are too many drivers! Please delete an existing driver." (1625). Event 7 on a driver asks to delete it. BACK: sound
CLICK, `start`.

Set up of the name window for a new driver: title = line 472 of the game texts ("NEW DRIVER NAME"), starting text =
line 167 ("Player"), at least 1 and at most 15 characters, name check on, the characters `A`..`Z`, `a`..`z`,
`0`..`9`, BACK → `driver`, DONE → `cmschooselogo`, on DONE a new driver is made (below).

## keyboard (name entry)

The state object `0x00720C88` (vtable `0x006F1CD4`: enter `0x00460960`, update `0x00460B40`, exit `0x00460B00`) is
filled by the state that opens it: title `+0x0C` (`0x00460110`), ticker text `+0x10C`, the text `+0x20C` with the
least and most characters `+0x30C`, `+0x310`, the DONE handler `+0x314`, the name check `+0x5BC` (`0x00461240`), the
allowed characters `+0x320` (`0x00460140`), the characters not allowed first `+0x420`, the symbols page `+0x520`,
the BACK state `+0x524` (`0x00461320`), the DONE state `+0x564` (`0x00461340`). More than 26 characters use the box
STRING_SMALL instead of STRING. When the window closes everything is cleared (`0x00460840`).

Enter: INSTRUCTION shows the title, TICKER the ticker text; without the symbols page SELECTC and SYMBOLS_TEXT are
hidden; letter case upper, then the case is worked out (below), which shows lower case; the text is marked "fresh"
(the first character entered replaces it).

A character may be entered when it is one of the allowed ones, and not one of the not-first ones while the text is
fresh or empty (`0x004601E0`).

Letter case (`0x00460450`): the symbols page when it is on; else upper case while a shift key is held or the SHIFT
button is on, the other way round with caps on. On a change the buttons `BA` .. `BZ` show the letters in that case
and the colours are worked out again.

Colours (`0x00460250`): every button gets font colour `0xFF00B4FF` and highlight colour `0xFFECEC01`; a button of
two letters whose caption character may not be entered, SPACE when a space may not be entered, and DONE while the
text is shorter than the least, get `0xC8800000` for both.

Update:

| event | action |
|---|---|
| 5 on DONE | too short: sound NO; the name check refuses it: message line 1738 ("Invalid name."); else the DONE handler, sound CLICK, the DONE state |
| 5 on BACKSPACE | fresh cleared, the last character removed (sound CLICK), colours |
| 5 on SPACE | a space if it may be entered (else sound NO), colours |
| 5 on SHIFT | SHIFT on, or off again when it was on |
| 5 on a letter button | the button's caption is added if its character may be entered (else sound NO), colours |
| 6 | symbols off; caps turned over unless SHIFT was on; SHIFT off |
| 7 | with the symbols page: symbols turned over, SHIFT off |
| 12 | sound CLICK, the BACK state |

Events 5 turn SHIFT off before the button acts. Adding a character: a fresh text is emptied first; the character is
added while the text is shorter than the most, with sound CLICK. Then the typed character: BACKSPACE empties a fresh
text and removes the last character; 0x09, 0x0A, 0x0E, 0x1B do nothing; 0x0F turns SHIFT off; any other is made lower
case (`| 0x20`) in lower case and added like a button's. Last the letter case is worked out, the STRING box shows the
text (`*` per character for a password; text wider than the name box, 300 pixels without a NAMEBOX element, loses its
start behind "...", `0x005A9060`; the width `0x005B3260` taken as the window opens, `+0x5C4`), its FontModifier record
started again (`+0x108`, none in `keyboard.ui`; `04-interface.md`), and the highlighted button's box turns `0xAF404040`
(the one before back to `0xAF000000`).

The name check (`0x00418A70`) reads `scripts\profanities.txt` and `scripts\nonprofanities.txt` at start (`0x00418940`,
a word per line, CR LF, made lower case). It keeps the name's letters and digits in lower case and refuses it when it
holds a word of the first list and none of the second.

## New driver

DONE of the driver name (`0x004675E0` → `0x0040AB80`, the save system object `+0x64` of the game object):

- the name `+0x20`; the driver folder `P<name>` with spaces as `_` (`0x0040AA20`);
- the id `+0x30`, four `rand()` values r1 .. r4 (`0x0040B0E0`): high = ((r2 ^ r1 << 16) & 0xFFFF0000) | ((r3 ^ r2 <<
  16) >> 16), low = r4 ^ r3 << 16;
- the settings of a new driver (`0x004097F0`, see `03-saves.md`);
- `+0x04` = 1 (not saved yet), `+0x0C` = 0;
- the arcade progress of a new driver (`0x0042A640` from `ArcadeUnlock.txt`, `0x0042A960` from `ArcadeRaces.txt`);
- no help page seen (the flags of the table `0x00715D48` cleared).

Then: `cmschooselogo` (`0x004C3B70` / `0x004C3BD0`) fills its menu with the logos `ui_chooselogo_01` .. `_19` (the
table `0x00718A28`); event 5 stores the chosen logo in the settings (`+0x20`) and goes to `cmsmobilechoose`; event 12
opens the name window again with the name entered. `cmsmobilechoose` (`0x004C3DB0` / `0x004C3DD0`): choosing a phone
pays its price (the table `0x00718F70`) and stores it (`+0x08` of the career object `+0x5C`), then the crew name:
the name window with title line 473 ("NEW CREW NAME"), starting text line 169 ("Crew"), 1 .. 15 characters, letters,
digits and space, the handler `0x004675A0` (the crew name into the settings, `0x00409F30`), BACK →
`cmsmobilechoose`, DONE → `saveprofile`.

`cmsmobilechoose` has two horizontal menus that move together, MOBILENAMES (the phone names, value 0 .. 5) and
PHONEMENU (their pictures). For a new career (the career object `+0x5C` with `+0` = 0) event 5 stores MOBILENAMES'
value as the phone (`+0x08`) with no payment and opens the crew name window; event 12 returns to `cmschooselogo`. In a
running career a different phone costs its price (the table `0x00718F70`) after a question.

`saveprofile` (record `0x00721260`, vtable `0x006F23D4`: enter `0x00463E40`, update `0x00463F30`, exit
`0x00463F00`) shows "Please wait..." (a message of type 0, jettext line 3) and asks the save device to write; once
the message is in it clears the career head value and starts the new career (`0x004A2300`: crews and their names
from `CMSPlayersCrewCollection.txt`, the garage, rivals, the profile's defaults `0x004B7660`, the flyers `0x004B2C20`,
...). The device writes the driver's folder (`03-saves.md`); a folder of that name there asks "Driver already
exists. Overwrite previous Driver?" (type 4, starting on no): yes writes over it, no asks the name again as for a new
driver (`0x0046401B` .. `0x00464086`: `keyboard` titled "NEW DRIVER NAME" with "Player", BACK `driver`, on to
`cmschooselogo`). Written: the saving on, "Save Successful" (a note), then `menu`. A write ending without success
asks "You will not be able to save your progress. Do you want to continue without saving? This will disable
Autosave." (yes: the saving off, "Autosave is disabled. It will be enabled after the next save.", `menu`; no: the
write again).

## Main menu

`menu` (record `0x00721510`, vtable `0x006F26A0`: enter `0x00468490`, update `0x00468850`, exit `0x00468790`). The
window's MENU items carry a name (the third word of `AddItem`): `CMSHub`, `instant`, `custom`, `multi`, `online`,
`options`, `extras`, `quit`. The first time the menu runs with the records not loaded (`[game + 0xA8] + 0x1120`) it
goes to `loadhighscores`. Event 5 by the selected item's name (compared without regard to case): `career` →
`careerload`, `instant` → `Arcade_Championship` (race mode 0), `custom` → `instant_manu` (race mode 1), `multi` →
`link_search` (or a message without a network), `online` → `online_logon_gs` (`online_main` when logged on), `Extras`
→ `Extras`, `options` → `opts_bg`, `CMSHub` → (race mode 2) `DemoFlyby` while `[[game + 0x7C]]` is 0 (and the menu's
exit then makes a new career again and sets up the race session, `0x004A2300`, `0x004B1870`, `0x004B8570`), else
`CMSNewCont`; `quit` asks "Quit to Windows" / "ARE YOU SURE?" (lines 1608, 86; the answer in `+0x24`, `+0x20` set),
and a yes ends the game: the application's method `+0x40` (`0x00513C50`) clears its `+0x14` bit 0, which ends the
game's loop. Event 12 (CLICK) with no message up asks line 1528 ("You will lose all unsaved progress ... quit to the
title screen?", the answer in `+0x28`), else closes the message (the front end's `+0x48`); a yes goes to `start`
(mode 1) and the records are asked for again (`[0x0075EE68]` 0). Both questions start on "no". BACKMENU follows MENU.
`instant` and `custom` also make the race's players (`+0x58`) 1. Each frame and at the enter, off line, every
`ONLINEICON` is hidden and `ONLINE`'s text emptied (`0x00473CD0`). The enter (`0x00468490`) also sets the profile's
`+0x614` 1 and `+0x28`, `+0x2C`, `+0x30` -1.

## The custom race

The cars of the custom race by performance group are the game's `+0x84` object (0x788 bytes, `0x0042A640` at
start from `ArcadeUnlock.txt`): 8 groups of 60 car ids, 0 ending a group; `Level` starts the next group (from -1),
`Car <name>` puts the id of the name (`0x0042A8F0`, 11 when unknown) at the group's next place (the list is one block:
a place past 60 runs into the next group). `+0x780` is the place in the group, `+0x784` the group; `0x0042A800` goes
to the next place (0 past the last car), `0x0042A830` to the one before (from 0 the last place of the group with a
car). A car is locked when the arcade progress's flag of its id (`+0x52F0`) is set. The arcade cars are the save
system's list (`arcadecollection.txt`, `+0x40`); the save system's `+0x50` is the arcade car raced.

`instant_manu` (enter `0x0045E490`, update `0x0045E790`): enter sets `[game + 0x18] + 0x2C` -1, empties CarMenu
(`+0x10` -1), makes Menu take the events; on the first visit (`[0x007253B0]`, cleared then) Menu highlights the first
group holding a car not locked; every arcade car's parts are cleared (`0x004600C0`: `+0x58`, `+0x44` 0, the body
parts 0 but the decal and the neon -1, every catalogue part -1). Update: when Menu's highlight is not the group CarMenu
lists, CarMenu is filled again: group 7 "ALL CARS" (1910), else the display names of the group's cars (`0x0048CC30`)
with each locked one disabled; its middle item highlighted, its text modifier deleted (`0x0045E8FD`: `+0x10C` freed
and 0, so the item does not blink) and CarMenu taking no events (`+0x178` 0: a menu at 0 takes
no window events, `0x005B11A0`). backmenu follows Menu. SELECT on an item (CLICK): the group its value less 1,
`instant_selcar` (a split screen race, mode 3: `instant_setup`); BACK (CLICK): `menu` (`multi`).

`instant_selcar` (enter `0x0045EC40`, update `0x0045F340`, exit `0x0045F100`): enter hides the PROT_ elements and
shows them (`+0x4C`) when every arcade race is completed (`0x0042B9F0`: each race's `+0xD0`, `+0x08` and `+0x04`)
off line; repairs every arcade car (`0x0048EB20`); takes paint 0. An empty place of the group goes to 0, a locked car to
the last place of the group whose car is not locked. The car shown (`+0x20`) is the first arcade car of that model (0
when none). CARORIGIN is hidden, the parts' buttons are on (`+0x48`). The car view (`0x0045A2C0` on FRAME_CAR3D,
CARNAME, CARLOGO and CARORIGIN, zoom 0.7, the mirror image, the folder kept and the shadow) shows the car rated (`0x0048E8A0`),
turned pi/4, not turning, fading; CARFADE (`+0x18`); TARROW shown; MODDING hidden; the options (`0x0045F150`:
OptionsBox, CarSel_Colour_Text and CarSel_ColourArrows shown, Locked and Locked_Text shown for a locked car and hidden
otherwise, the auto modification's four elements shown with `+0x48`); COUNTDOWNBOX hidden; FRAME_CAR3D's view in the
map mode.

Update: the answer to "AUTO-MOD CAR?" (`+0x54`): yes modifies the car (`0x0045A8A0`), no goes on as SELECT does (a
locked car makes NO and keeps the answer). The view's frame and CARFADE's alpha 1 - the view's `+0x4C`; while the view
loads the car again (`+0x40`) MODDING shows and the events wait; COUNTDOWN emptied (`0x00471740`). UP and DOWN take
the next and the previous paint of the CarDefs' list (wrapping) onto the car (`0x0045A640`: the paint, then the dial
colours `+0x30`, `+0x34` of the `[DialColour]` entry rand() % the list's size; the model painted again, `0x0045A6F0`),
CLICK. LEFT and RIGHT take the group's place before and after; the car shown is the arcade car of its model (its parts
cleared without the buttons), SELECT, shown fading, and the options again. SELECT: a car shown whose first four body
parts are 0 (`0x0045A850`), with the buttons and not locked, asks "AUTO-MOD CAR?" (2014, CLICK); a locked car NO;
else CLICK, the race's `+0x54` the automatic box (the settings' `+0x30`; 0 for a sprint) and `instant_seltype`.
Events 6, 7 and 8 (CLICK, with the buttons; 8 also with the prototypes) call `0x0045A8A0` with nothing loading: the
automatic modification of level 2 (`0x0048F2C0`), the parts cleared (`0x004600C0`), a prototype modification (any
level); the model's parts are put again and the car loaded again; the load's last job (`0x0045FED0`, its main-thread
part `0x0045A790`) shows the element's `gen_neon` for a car with a neon (`+0x0D` over -1) and gives the car a neon
colour (`+0x2C`) of `[NeonColour]` at rand() % their count, the `gen_neon` material coloured (`0x0048BEB0`), else
hides it. BACK (CLICK) `instant_manu`. Then the save system's
`+0x50` takes the car's place; with the view not loading BHP is "<rating> " (cut toward zero) and "bhp" (484), DRIVE
the drive (1830 ..), else both hidden.

`instant_seltype` (enter `0x0045B0B0`, update `0x0045B390`): Menu and BackMenu highlight their first item, then the
item of the race's type (3 solo, 4 cruise, 6 sprint, else race). SELECT on an item (CLICK): cruise clears the
profile's `+0x634` and makes type 4, laps 99999, one player, no opponents, weather 1, time of day 1; solo type 3,
laps 99999, one player, no opponents; race type 5, a lap, one player; sprint type 6, `+0x08` 0, `+0x0C` 3, a lap, one
player; then `instant_setup`. BACK (CLICK) `instant_selcar` (mode 3).

`instant_setup` (enter `0x0045BFE0`, update `0x0045DC60`, exit `0x0045C420`) sets the race on the city map in pages
(`+0x28`): 0 the location, 1 the route, 2 the laps, 3 the opponents, 4 the difficulty, 5 the time of day, 6 the
weather. Enter: a cruise off line whose tutorial was not asked (the profile's `+0x634`) asks "VIEW SHOWOFF
TUTORIAL?" (1911) and marks it; the menu shown; the map (`0x0045B6B0`: MAP3D's camera from 4000 to 5 000 000, the
sky, the rig's pitch within -1.5 .. -0.15, the atmosphere `map`); page 0; laps 3 for a race of another type than 0 ..
4 and 6; opponents 0 for the types 0 .. 4, 3 for a sprint, else 5; the difficulty 3.0; the time of day 1 for a cruise,
else rand() % 3 (1 afternoon, 2 evening, else morning); weather 1; LOCKED hidden. Each of these goes into the race and
the settings (the save system's `+0x64`, `+0x68`, `+0x6C`).

Each frame the map's model that comes down (the element's list's entry 1) goes down by 15000 a second within 250 ..
20000, and the view's zoom is 0.6 with its turn 0.3 (`0x0045BC40`). The page fills its menu once:

| page | items | highlighted | the item taken |
|---|---|---|---|
| 0 `0x0045C450` | each city place (`0x00767E50`) with a route open (`0x0042B7B0`; a cruise's of type 3, a sprint's of type 2, else of type 1 or 4): its text, its place, its name | the item kept (`[0x0075ED14]`) | the map looks at the place (`0x0045BE00`: the map mode on the arm (0, 0, -240000), pitch -0.4); the race's track the name; LOCKED hidden |
| 1 `0x0045C6C0` | the track's routes (`0x00531890`, the .tdf) but cruises and sprints, each open one (`0x0042B810`): reversed point-to-point "POINT-TO-POINT Reversed", reversed "ROUTE n Reversed", point-to-point "POINT-TO-POINT" (not in a solo race), else "ROUTE <its place + 1>" | the race's route by its place among the .tdf's routes | UI_<route>_GUI on the map (`0x0045B720`: added after the element's model, the view looking at its `centre` node, the model coming down from 20000); the race's route |
| 2 `0x0045CD10` | "1 LAP", "2 LAPS" .. "6 LAPS" (1940, 175) | the settings' laps | the race's and the settings' laps |
| 3 `0x0045D000` | from 1 (0 with more players) to 6 cars (4 in a sprint, at night or wet) less the players, "<n + 1> CARS" (65) | the settings' opponents (the most when they ask for more) | the race's and the settings' opponents |
| 4 `0x0045D2E0` (the time limit) | "1 MINUTE", "3 MINUTES", "5 MINUTES" (1941, 66), valued 1, 3, 5 | 1.0 the first, 5.0 the third, else the second | the race's `+0x60` and the settings' difficulty |
| 5 `0x0045D5C0` | MORNING .. NIGHT (67 ..) | the settings' `+0xDC` | the race's time of day (1 .. 4) and the settings' |
| 6 `0x0045D870` | CLEAR, WET (71, 72) | the settings' `+0xE0` | the race's weather and the settings' |

SELECT (CLICK) goes on by the race's type: solo location → route → time → weather → the race (from the route at once
in race mode 0); cruise the race at once; sprint location → time → weather → opponents (more players: none) → the
race; another race location → route → laps (a point-to-point route: the time) → time → weather → opponents → the race
(no page leads to the difficulty; from it SELECT goes to the route). The race: a cruise takes the track's first route
named "cruise" (`0x0045DB00`; none: "No Cruise Route Found"), a sprint its "sprint" route (a stock problem: the note
"No Sprint Route Found" comes when that route is the second of the track); more players `split_selcar`; else
`loading` with the race's `+0x54` the automatic box (0 for a sprint). BACK (CLICK) goes back the same way; from the
first page `instant_seltype` (a cruise and a sprint with mode 7; a race of more players `instant_manu`, mode 2). A
"yes" to the tutorial goes to `vid_tutorial`, coming back here.

`split_selcar` (Game/Frontend/CustomRace/SplitSelCar.cpp) picks the two players' cars of a split screen race. The second player has its own
save system object (`[game + 0x68]`, made as the first's with the start driver "Player2", line 168): its arcade cars
(`+0x40`, the second load of `arcadecollection.txt`), its car among them (`+0x50`) and its groups (`[game + 0x88]`, the
second load of `ArcadeUnlock.txt`). The enter (`0x0046CE10`) gives the second player the first one's group; each
player's place starts at 0 and goes on past locked cars (a place without a car back to 0) in the ids of the first
player's group, its car the first of its arcade cars of that model; both cars repaired, their parts cleared and rated;
the views (`CAR3D` / `CAR3D2` with `CARNAME(2)`, `CARLOGO(2)`; the zoom 0.7, the mirror image, the shadow) show them
turned 3 pi / 4, turning 0.4 a second. `Locked1` / `Locked2` hide for a car not locked; every element holding
"MODDING" hides. Each frame (`0x0046DA60`) each player's part (`0x0046D4A0`) takes its own event: while its view loads
`MODDING1` / `MODDING2` comes in and the event is dropped. Not ready: UP / DOWN its paint (`0x0045A640`, the dial
colours of a random `[DialColour]`), LEFT / RIGHT the place before / after (`locked1` / `locked2` shown for a locked
car), 6 / 7 the automatic modification (level 2) or the parts cleared; SELECT a car not locked ready, else NO; BACK
(CLICK) not ready: the first player's asks for `instant_setup`, either lets the second player's device go and clears
the front end's `+0x20` and `+0x24`; ready: not ready. `READY1` / `READY2` show while ready; `Drive1` / `Drive2` the
drive. Both ready: the race's automatic box the settings', `loading`. The cars go to the save system objects' `+0x50`.
Without the second player's device `PLAYER2COVER` and `PLAYER2LOGO` show and a device is taken for it (the input
manager's vt+0x3C with 1: a device with input that is no player's), else they hide. Neither view loading: `BHP1` /
`BHP2` "<rating to the nearest> bhp". The leave (`0x0046D3F0`) frees both views.

An unmapped keyboard has input only by RETURN (the PS2's START), so a keyboard pressed in this window first goes to the
second player when the first one has no device (the stock order: the first player's device is taken by the polls
before, `0x0058FE50`, the second's in the window's frame after the poll).

The race of mode 1 (`0x00495DE0`, off line) uses the career's session (`0x004B9150`): the players and the opponents,
racer i of crew 0 (a player) or i, member 0; then the drivers (`0x004BDCC0`). The player's car is the arcade car (its
grid made, rated); the opponents are new cars of a car id drawn from the chosen group (rand() % 60 until not empty)
with the CarDef of the first arcade car of that model, modified automatically when the player's car has a catalogue
part (as a prototype when it has a prototype part, else of level rand() % 3), painted from the CarDefs' list
(rand()), and drawn again (20 times at most) while their rating passes the player's by more than 1. Opponent k is of
crew and number players + k and named by the short name of the first member of crew k + 1. After the race the front
end starts at `instant_manu`.

## Extras

`extras` (record `0x00721484`, vtable `0x006F2630`: enter `0x0046A920`, update `0x0046AA00`). Enter puts `menu` and
`backmenu` back to their items (`0x005AECF0`: the items put at a place freed), takes `replay` out of `menu`
(`0x005AFAD0`) and `backmenu`'s first item with it (its pointer only), hides every `ONLINEICON` and sets `+0x0C` 4,
`+0x10` -1.

Update: SELECT on `menu`'s highlighted item (CLICK): `Cheats` `CMSCheats`, `credits` `credits`, `videos` `vid_tutorial`
(coming back to `extras`, `[0x0072158C]`), `hiscores` `viewhiscores`, `replay` `loadreplay` (the race's mode,
`[G+0x6C]+0x00`, set to 0 first; the enter took the item out), `save` the question (type 3, small) "Are you sure you
want to save?" (1504) when the save system saves (its method `+0x24`), else "Do you want to enable Autosave and save
your driver?" (1503) into `+0x10`; `done` `menu`. BACK (CLICK) `menu`. `+0x0C` counts down each frame; at 1 the `save`
item's text is "SAVE" (1582) or "ENABLE AUTOSAVE" (1550) by the same test and `+0x0C` is 0. A "yes" sets `[0x0075ED38]`
(read by `autosave`), asks for `autosave` over the window and sets `+0x0C` 4; `+0x10` goes back to -1.

`vid_tutorial` (`0x0046A040` / `0x0046A0D0`): the enter highlights MENU's first item, shows COVER, plays `whatis`
(`whatis_spanish` in Spanish) looped on MOVIE (`0x005AA300`) at the settings' music volume (`+0x58`) and hides COVER;
SELECT on an item plays its movie looped on MOVIE (`what` → `whatis`, or `whatis_spanish` in Spanish; `boot` →
`bootleg`; `180`, `360`, `donut`, `jturn`, `drift`, `boomerang`, `combo` by their names) with COVER hidden, `done`
goes back to the window that asked for it (`[0x0072158C]`, mode 1); then MOVIE's volume the music's and CLICK; BACK
(CLICK) goes back the same way.

`loadreplay` (enter `0x004694B0`, update `0x00469530`, leave `0x00469D60`) is reached by nothing on the PC: `extras`
takes its `replay` item out, and `savereplay` (`0x004653A0`, `0x00465400`, `0x004653D0`) waits for a save item
`hud_replay.ui` does not have. Both follow the replay save device (`[game + 0xA0]`, vtable `0x006F6DC8`: a replay
saved under "R%04d%02d%02d%02d%02d%02d" of the local time with the text "%d/%d/%02d %2d:%02d", `0x004FA600`;
`03-saves.md`). Its replays' object (`+0x0C`, which writes and takes a replay's files: the replay's slots 13 and 14,
`0x004292C0` and `0x00429550`, `24-replay.md`) is given by nothing on the PC, so a write or a read of a replay would
call through address 0 there; OpenJuice gives it the game's replay, and the windows work when reached.

The device lists the folders of SAVE starting with "R" (`0x005026B0`): each one's `USERDATA` holds u32 cars, u32 the
race's type, char[32] track, char[32] route, char[17] the date's text and wchar_t[16] the driver (0x79 bytes, copied
whatever the file's size), sorted as the drivers are, latest first.

The enter of `loadreplay` (`0x004694B0`) clears the record (`0x004693A0`: MENU emptied, every INFO_ and DEL_ element
hidden, the requests cleared, the list asked for, the device started again) and writes "CARS" (65) in LAPSTITLE. Its
update follows the device's flags as the driver window does: "Checking hard disk..." (held at least 1 s), "Loading
data..." and "Deleting..." (type 0), the failures' notes (type 1), "Delete successful!" (1496) and the delete's
question with the item's text. A listing fills MENU (`0x00468FC0`): each replay with cars its date's text, else
"DAMAGED REPLAY" (1665); DEL_ shown with any. The highlighted replay shows its details (`0x004690E0`): INFO_RACETRACK
the city place of its track (the first place when none has the name), INFO_ROUTE its route's name (empty for a
showoff or a sprint), INFO_RACETYPE the type's name (495 + type; 7 as 5), INFO_NUMCARS, INFO_DRIVERNAME and
INFO_TRACK_PIC the place's picture; all empty for a replay without cars. SELECT reads the replay; once the device is
idle with it read (the replay object took it: its race set from its header, the play from the start, `menu` the
window after the race) the loading note is closed and `loading` asked (mode 1); SELECT2 deletes it, BACK (CLICK) goes
to `extras`; the leave starts the device again.

`savereplay` (over `hud_replay`, mode 5) asks the device to write (`0x00465310`) and follows its flags as
`savehighscores` does: the space's note (the flag's line, lines 4 and 5 of jettext with the size and "KB", " to save
this replay." 1491), "Saving data..." while it writes, "Save Successful" and done, the write's and format's failures,
the format question; done, it closes itself (mode 6) and its exit starts the device again. A failure asks the PS2's
"Do you want to save the replay on memory card ..." (434, type 3; a console's text, asked again each frame while
unanswered): no ends, yes writes again. The device's other flags of kind 1 end the stock game (`0x0059AF30`).

## CMSCheats

Record `0x00722070` (vtable `0x006F53C4`): enter `0x004C54D0`, update `0x004C55B0`. Its fields: `+0x0C ..` the
elements `L1` .. `L4` (the first of each name), `+0x1C ..` their four letters, `+0x20` leaving. Enter: the letters
`A`, the letters shown (`0x004C5CB0`), `+0x20` 0.

Update: `+0x20` set asks for `extras`. The highlighted button `L<n>` shows `TOPARROW<n>` and `BOTTOMARROW<n>` and hides
every other arrow. BACK (CLICK) `extras`. UP and DOWN (SELECT) turn the highlighted button's letter on (`Z` then `.`,
`.` then `A`) or back (`A` then `.`, `.` then `Z`); with no `L<n>` highlighted the place is 4 and the byte after the
letters is turned, `+0x20`'s low byte, so the next frame asks for `extras`. Then `0x004C5CB0`: each `L<n>` its letter (`"%c"`); the first of
the eight codes (`0x00718F88`, each letter kept as its code + 0x5A: `PINT`, `1OSH`, `1ESP`, `1ARS`, `1REW`, `1HAR`,
`1IN.`, `1LL.`) the letters make; on SELECT (CLICK) a code sounds WARNING, takes its cheat (`0x004A6E50`,
`08-career.md`), shows its line (type 1; 395 + the cheat: "Arcade / Custom mode Unlocked", "You have more cash",
"Everyone now respects you", "You now have all the cars", "You now have a full crew", "Character test mode active",
"Win all races active", "All cheats applied") and sets `+0x20`; no code shows "Invalid Cheat Code." (1507). The
letters never reach `1`, so only `PINT` can be made.

## Options

`opts_bg` (record `0x00721A6C`): enter `0x0047FF60` asks for `opts_menu` over it (mode 5; from a race's pause, the game
object's `+0x3C` set, `opts_ingame_menu`); update `0x0047FFF0` asks again whenever it is the top window.

`opts_menu` and `opts_ingame_menu` (record `0x00721A78`): enter `0x00480090`. Menu and BackMenu (`+0x10`, `+0x14`) get
`+0x1A0` 1 and their items back (`0x005AECF0`); `control` reads "CONTROLS" (21); from a race's pause the
highlighted `control` moves the highlight to the first item and `control` is taken out (with BackMenu's first item),
and a highlight past the items goes back to the first. Then "STEERING WHEEL" (1677, `wheel`) is put before the last
item (`0x005AF230` with the place) and BackMenu gets an orange bar (`0x005AF480`); `control` reads "CONTROLS" again;
with `[0x0071E718]` & 1 `audio` is taken out (and BackMenu's first item); unless more than one player plays a network
game (`[G+0x6C]+0x58` over 1 and `[[G+0x34]+0x0C]+0x10` 1) "VOICE" (1914, `voice`) goes before the last item with a
bar (its `+0x54` 1). A highlight past the items goes back to the first in both menus; `+0x0C` 0.

Update `0x00480310`: BackMenu follows Menu. `+0x0C` set asks for `menu` (mode 3) and sends the top window's
transitions out (`0x005AA0E0`). The players' controllers are polled (`0x0041DD60`) and the first one's `+0x10C`,
`+0x110` cleared. For each player's event: SELECT on Menu's item: `audio` `opts_audio`, `game` `opts_game_pc`, `graph`
`opts_graphics`, `wheel` `opts_wheel_dx9`, `voice` `opts_voice` (mode 7; from a race's pause `opts_ingame_audio`,
`opts_ingame_game` / `opts_ingame_game_ply2` by the player), `control` the controller's set up (`0x004080D0`,
`0x00407FB0`), each with CLICK; `done` CLICK, then from a race's pause `hud_pause`, else when the save system saves
`autosave` over the window and `+0x0C` set, else `menu` (mode 3) with the transitions out. BACK: the same without the
CLICK when saving (from a race's pause `hud_pause` with mode 3).

`opts_game_pc` (record `0x00721B00`): enter `0x004813E0` keeps the sensitivity (the save system's `+0xC4`) in `+0x58`
and `+0x5C`, finds VIB, VIB2, MAP, AUTO, AUTO2, UNITS, AIDS, DIFF (`+0x0C ..`) and the meter STEERING (`+0x30`: its
value and shown value the sensitivity, its `+0xFC` 1), clears `+0x2C`, keeps the settings `+0x78`, `+0x80`, `+0x84`, `+0x88`,
`+0x94`, `+0x7C`, `+0xA0` (`+0x34`, `+0x3C` .. `+0x50`), writes "STEERING WHEEL" (1677) into VIB_BUT, hides every
element with VIB in its name, makes AUTO_BUT's up button (`+0x154`) and DON_BUT's down button (`+0x174`) each other
(`0x005A99F0` links them again) and clears `+0x54`.

Update `0x00481770`: for the second player of a network game the up and down buttons of DON_BUT and AUTO_BUT are set
again (on events 1 and 2). The meter's `+0xF8` is set while SENSITIVITY_BUT is highlighted. A highlighted button other
than `+0x2C` becomes it and writes the texts (`0x005B19C0`): VIB, AUTO, AUTO2, AIDS "YES" (105) or "NO" (106) by
`+0x78`, `+0x84`, `+0x88`, `+0x7C`; MAP by `+0x80` in 0xFFE86A17, with more than one player "NO" in 0xFF808000;
UNITS "MPH" (1494) or "KPH" (1495) by `+0x94`; DIFF "NORMAL" (179) for `+0xA0` 0, else "HARD" (1975). `+0x54` above 0
counts down by the frame time (within 0 .. 1) and the controller rumbles at 1 (else 0, `0x0041DD60`); without a race's
pause the controller's `+0x10C`, `+0x110` are cleared. BACK gives the kept values back (`+0x88` gets `+0x84`'s kept
value; `+0xA0` is not given back), with STEERING the kept sensitivity, CLICK and `opts_menu` (mode 7). SELECT on
DON_BUT: with STEERING the sensitivity takes its shown value, CLICK and `opts_menu`. LEFT or RIGHT on VIB_BUT, MAP_BUT (NO with
more than one player), UNITS_BUT, AIDS_BUT, DIFF_BUT, AUTO_BUT, AUTO_BUT2 flips its value with CLICK and clears `+0x2C`.

`opts_audio` (record `0x00721A90`; from a race's pause `opts_ingame_audio`): enter `0x00480960` finds the meters
MUSIC, SFX and SPEECH (`+0x0C .. +0x14`) and makes them sliders (`+0xFC` 1) not taking the direction (`+0xF8` 0) at
the settings' music, effects and speech volumes (the save system's `+0x54`, `+0x58`, `+0x5C`: value and shown value),
keeps them and the speaker set (`+0x60`) for BACK (`+0x5C .. +0x68`), shows the speaker set by the ticks
DOLBY/STEREO/MONO TICK and TITLE (`0x0047FD50`; none in the windows) and keeps the volumes heard (`+0x4C .. +0x54`).
Update `0x00480AD0`, for each player (nothing while the race's players, `+0x58`, are 0): a highlighted button other than
`+0x18` becomes it: MUSIC_BUT makes the MUSIC meter take the direction (not with `[0x0071E718]` & 8), SFX_BUT SFX,
SPEECH_BUT SPEECH (not with & 0x10), the speakers' titles, `playlist` and DON_BUT none; with them the music and the
sounds are stopped or played (the music object `[G+0x48]`, the sound objects). Each frame the settings take the
meters' shown values and the speaker set, and the sound objects their volumes; the effects' volume moved over 0.07
plays "Warning", the speech's over 0.01 "SPEECH". BACK: the kept values back, CLICK and `opts_menu` (from a race's pause
`opts_ingame_menu`; mode 7). SELECT on DON_BUT: CLICK and the same; on `playlist` `opts_songlist` (CLICK; NO with
`[0x0071E718]` & 8); on a speaker's title (`0x0047FDB0`) CLICK and that speaker set.

From a race's pause the options run as the race front end's windows (`opts_ingame_bg` with the game object's `+0x3C`
set): `opts_ingame_bg` (the record of `opts_bg`) asks for `opts_ingame_menu` (the record of `opts_menu`, without
`control`), whose items open `opts_ingame_audio`, `opts_ingame_game` (the second player's `opts_ingame_game_ply2`;
the record of `opts_game_pc`), `opts_graphics`, `opts_wheel_dx9` and `opts_voice`; each of them goes back to
`opts_ingame_menu`, and its DONE (CLICK) asks for `hud_pause` (mode 1), BACK for `hud_pause` (mode 3).

`opts_wheel_dx9` (record `0x00721C10`): enter `0x00483D10` finds TICK_STEERINGWHEEL, TICK_SPEED, TICK_FORCE and the
meters SENSITIVITY and STRENGTH (`+0x0C ..`), clears `+0x20` and keeps the wheel (`+0x24`, the save system's `+0x78`),
speed sensitive steering (`+0x28`, `+0xAC` 0), force feedback (`+0x2C`, `+0xB4`), the sensitivity (`+0x30`, `+0xC4`)
and the strength (`+0x34`, `+0xA4`); the ticks show UI_TICKBOX_YES or UI_TICKBOX_NO by them (`0x00483CF0`), the
meters' value and shown value are theirs (`+0xFC` 1, `+0xF8` 0) and DONE_BUT's links are made again (`0x005A99F0`).
Update `0x00483E80`: a new highlighted button (`+0x20`) sets the `+0xF8` of the meter of STRENGTH_BUT or
SENSITIVITY_BUT (and clears the other's). SELECT (CLICK) on STEERINGWHEEL_BUT, SPEEDSENSITIVE_BUT or FORCE_BUT flips its
value and its tick; on DONE_BUT the meters' shown values are taken, the settings written (`+0xAC` 1 when not speed
sensitive, `+0xB4`, `+0x78`, with the meters `+0xC4` and `+0xA4`), CLICK and `opts_menu` (mode 7). BACK (CLICK)
`opts_menu` with the settings as they were.

`opts_graphics` (record `0x00721BC8`): enter `0x00481F00` finds BRIGHT, DETAIL, SHADOWS, REFLECTION, MOBLUR, GLOW,
DOF (`+0x10 ..`); the meters DETAIL and BRIGHT become sliders (`+0xFC` 1) with the detail and the brightness of the
graphics settings (`12-rendering.md`) as value and shown value. Kept for BACK: the brightness (`+0x2C`), the detail
(`+0x30`), the reflections (`+0x34`), the shadows on (`+0x38`: 2; 0 without shaders), the motion blur, glow and depth
of field (`+0x3C .. +0x44`; 0 without shaders or with no shaders on the adapter). RESET_BUT "RESET TO DEFAULTS"
(1609); `+0x0C` 0.

Update `0x004820A0`: a highlighted button other than `+0x0C` becomes it and writes the texts (`0x005B19C0`):
REFLECTION "OFF", "LOW", "MED", "HI" (1486 ..) by the reflections (other values "OFF"); SHADOWS "YES" (105) for 2, else
"NO" (106); MOBLUR, GLOW, DOF "YES" or "NO" by theirs; "-" for each without shaders, and for the three effects also
with no shaders on the adapter. DETAIL's and BRIGHT's `+0xF8` are set while DETAIL_BUT or BRIGHT_BUT is highlighted.
Each frame on BRIGHT_BUT the brightness takes BRIGHT's shown value. BACK: the kept values back (the reflections and
the effects 0 without shaders, the shadows 2 when kept on, else 1), the detail and the detail factor from it, CLICK,
`opts_menu` (from a race's pause `opts_ingame_menu`; mode 7). SELECT on DONE_BUT: the detail DETAIL's shown value and
the factor from it, the brightness BRIGHT's, CLICK and `opts_menu`; on RESET_BUT: CLICK, the defaults by the adapter
(`0x005A26C0`), the meters at the detail and the brightness, `+0x0C` 0. LEFT or RIGHT: SHADOWS_BUT turns the shadows
over (1 or 2; 1 without shaders), MOBLUR_BUT, GLOW_BUT, DOF_BUT their effect (staying 0 without shaders), and
REFLECTION_BUT steps the reflections down (LEFT) or up (RIGHT) round 0 .. 3 (0 without shaders); each with CLICK and
`+0x0C` 0.

`opts_songlist` (record `0x00721B60`, from `PLAYLIST` of `opts_audio`): enter `0x004828A0` sets `[0x0075EF28]`, keeps
SONGMENU (`+0x20`) and TICKMENU (`+0x24`), clears `+0x28` .. `+0x38` and `+0x44`, stops the music's song and empties
both menus (`0x005AEEF0`). With the music bank (`[G+0x48]+0x08`, `[0x0075EF1C]`) the songs from the settings' known
count (`+0x218`) to the bank's get their bit in the settings' song bits (`+0x208`, one per song; past 128 songs the
game aborts with "bitset") and the known count becomes the bank's; the music object takes the bits (`+0x24` ..
`+0x34`). For each song with a name (the bank's display name, `music.cfg`'s `displayname`) SONGMENU gets the name and
TICKMENU a picture item (ui_no, ui_no; ui_yes, ui_yes for a song on), both of the song's number; then SONGMENU "DONE"
(552) and TICKMENU an item without a picture, both of the songs' count and named `done`. Each menu's first item is
selected. Update `0x00483130`: the song's preview (`0x004090D0`, `0x00482E10`); UP and DOWN set the preview to wait
(`+0x2C` 0, `+0x30` 1, `+0x34` 0); SELECT on TICKMENU's item: `done` CLICK and `opts_audio` (mode 7), a song CLICK, its
bit turned over (`0x00408CC0`) and its pictures ui_yes when it came on, else ui_no; SELECT2 previews SONGMENU's song
(`+0x48`); BACK CLICK, `opts_audio` (mode 7) and the music's song back (`0x00408E60`). Exit `0x00482D80`:
`[0x0075EF28]` cleared, both menus emptied, the music's song and volume back, the playlist made again from the bits
(`0x00408D20`, rand()).

`control` of the options' menu (`0x004080D0` with 1, then `0x00407FB0`): the first player's controller is made again
with DirectInput's mapping window (the input manager's method `+0x04`: `IDirectInput8::ConfigureDevices` with
`DICD_EDIT`, `0x00595717`; `[0x0075F770]` set while it runs, the cursor shown for it). Windows has had no such window
since Vista: the call fails and a message box (`MB_ICONHAND | MB_SETFOREGROUND | MB_TOPMOST`) shows the texts 111 and
112 of `scripts\jettext<letter>.txt` ("There is a problem with your DirectInput registry settings." / "Please see the
ReadMe file for further assistance.") under the title 113 ("DirectInput problem detected"). OpenJuice shows the same
message box without calling the mapping window. Either way the devices are made again after it (`0x005957D1`:
`0x00590EE0` for every input manager, `0x00590F40`: each device's controller released and made again from its
instance GUID with `c_dfDIJoystick2` and the cooperative level `0x16` (`0x00596210`; `DISCL_NOWINKEY` is a keyboard's,
so DirectInput refuses it), the keyboard made again (the old one not released) and the force feedback manager made
anew) and the map registered again on the new controller and the keyboard.

## opts_voice

`voice` asks for `opts_voice` (mode 7). Its object `0x0076890C` lies in the zeroed part of `.data`: the static
initialiser `0x006BEF10` (in the `__xc` table at `0x00702D94`) makes it through `0x0047FEF0` (vtable `0x006F3678`);
both lists of windows register and preload it (`0x00484E08`, `0x00484EB2`). Fields: `+0x0C` the enter made the voice
object, `+0x10` and `+0x20` the capture and playback devices' names (0x40 bytes each), `+0x30` and `+0x34` the
ones chosen (-1 none), `+0x38` the time since the lists were made.

Enter `0x00484700` (after the windows' enter): with a voice object already (`[0x0075F044]`, the online game's)
`+0x0C` 0; else the voice object is made (`0x004EECA0` releases any, `0x00565360` makes GameSpy Voice's: COM, the
Speex coder, no device opened; its `+0x1C` `0x004EEB50` for the chat's on/off) and `+0x0C` 1. Both lists emptied,
`+0x38` 0, `+0x30` and `+0x34` -1; the lists made (`0x004842B0`); `+0x30` and `+0x34` the devices named by the
settings' `+0x2D4` and `+0x314` (the save system's `+0x328`, `+0x368`; -1 for "none" or a name not listed,
`0x004841B0`, `0x00484200`: the first name equal case aside); the texts (`0x00484250`): INPUT and OUTPUT "None"
(164) without a device, else its name widened byte by byte with its sign (`0x0049A850`; a stock problem for
names past ASCII).

The lists (`0x004842B0`): the names chosen are kept ("none" for -1); with the voice object its `+0x04` lists the
capture devices, then the playback ones (`0x00565D70` → GameSpy Voice's `0x0057F190` into the object's room for 10:
`DirectSoundCaptureEnumerateA` or `DirectSoundEnumerateA` through `0x0057F0B0`, which passes over the primary
driver's NULL GUID, gives a GUID already listed only its kind and adds any other with its description cut to 63
characters, stopping at the room; the devices of `DSDEVID_DefaultVoiceCapture` and `DSDEVID_DefaultVoicePlayback`
are marked 1 and 2), each name copied into the window's list; the kept names are found again (-1 for a device gone)
and the texts written.

Update `0x00484830`: `+0x38` grows by the game's frame time; past 2 s it goes back to 0 and the lists are made again.
SELECT on DON_BUT: the settings take the names chosen ("none" for -1; `strncpy` of 0x40, the last byte 0), CLICK and
`opts_menu` (mode 7; from a race's pause `opts_ingame_menu`, and with an online player's presence `[0x0075F040]` the
voice object's `+0x10` first opens the devices for the chat). LEFT on INPUT_BUT: with one chosen CLICK and the one
before (from the first: none); OUTPUT_BUT the same; RIGHT: from none the first when there is one, else the next up
to the last, with CLICK; both then write the texts, whatever the button. BACK: CLICK and the options' menu (mode 7),
the settings as they were. Leave `0x004847F0`: the windows' leave (no Menu here) and the voice object released when
the enter made it. OpenJuice: Game/Frontend/Network/VoiceOptions.cpp with the voice object (Core/Network/VoiceChat, Core/Network/GameSpy/GameSpyVoice).

## Options windows nothing asks for

`opts_playlist`, `opts_ctl_port` and `opts_ctl_pc` are registered (`0x00484D88`, `0x00484D98`, `0x00484DD0`; the
first without the preload the others have) and their names are referred to by nothing else. OpenJuice runs them
by `--screen`.

`opts_playlist` (object `0x007688B0`, static initialiser `0x006BEEF0` through `0x0047FE20`; vtable `0x006F3624`).
Enter `0x004833C0` (not the windows' enter): PLMENU (`+0x20`) emptied; `[0x0075EF24]` the playlists' source
`[0x0075F85C]` (made with the sound system by `0x005B83F0`, vtable `0x006FE9C0`); the game's music made again
(`0x00483720`: the name of the sound playing read and dropped, the music object freed with its song, a new one made
on `audio\music` by `0x00408B60` with no song, and with the sound on its playlist from the driver's settings,
shuffled with `rand()`); `[0x0075EF1C]` the new music object; the source's method 0 asked for the list (`+0x38`).
The PC's source answers 0 (`0x0051F960`) and lists nothing; with a list each name from the last to the first would
become a PLMENU item of value 0 for "Juiced" and 1 for any other, the one of the settings' playlist name (the save
system's `+0x21A`) highlighted and kept in `+0x24` (its place) and `+0x28` (its name). Update `0x00483600`: SELECT
with an item: `+0x48` its value cut toward zero, `+0x4C` its text, CLICK and `opts_songlist` (mode 7); BACK: CLICK,
`opts_audio` (mode 7), `+0x48` and `+0x4C` back to `+0x24` and `+0x28` (`opts_songlist`'s BACK does the same).
Leave `0x004835D0`: PLMENU highlights item `+0x24` and is emptied. Without the sound system the source is not made
and the enter reads through null (a stock problem).

`opts_ctl_port` (record `0x00721BAC`, vtable `0x006F36C8`). Enter `0x004838C0` (not the windows' enter): MENU
highlights the item of the first player's device's place (the input manager's `+0x28`). Update `0x004838F0`:
backmenu highlights MENU's item; each of the input manager's devices (`[G+0x18]`'s list) sets MENU's item of its
place on by the manager's method `+0x20` (`0x0042E470`: 1; a device past the items writes through null). SELECT: a
highlight past the items goes to the first; an item on gives CLICK and `[0x0075EF2C]` (read by nothing) its place:
4 (DONE) `opts_menu` (from a race's pause `opts_ingame_menu`), 0 and 1 `opts_ctl_ps2`, 2 and 3 `opts_wheel` with
`[0x0075EF2C]` less 2 (mode 7; neither name is registered on the PC); an item off, or none, NO. BACK: CLICK and the
options' menu (mode 7).

`opts_ctl_pc` (record `0x00721BB8`, vtable `0x006F3688`): no enter; update `0x00483C40`: BACK gives CLICK and the
options' menu (mode 7).

## credits

Record `0x00721558` (vtable `0x006F2600`): enter `0x00469D80` (`+0x0C` and `+0x10` 0), update `0x00469D90`. While the
page's text is not written (`+0x10` 0) CREDITS (when there is one) is set multiline (`+0x128`), drawn in its box
(`+0x13C`), scrolling (`+0x140`) from 0 (`+0x144`), coloured 0xFF9CC4CE (`+0x150`), and given the page's lines, each
followed by a line feed: page 1 the lines 874 .. 1099, page 2 1100 .. 1310, page 3 for each n of 0 .. 173 the line
1311 + n when the line 1100 + n is not empty (the test reads the second page's line), any other page 780 .. 873; then
`+0x10` is set. BACK (CLICK) `extras`; SELECT clears `+0x10`, CLICK, and moves to the next page (past 3 the first).

## viewhiscores

Record `0x00721620` (vtable `0x006F25C0`): enter `0x0046A300`, update `0x0046A520`. Its fields: `+0x0C` M_TRACK,
`+0x10 ..` M_COL1 .. M_COL4, `+0x20` the track shown, `+0x24` the lists made.

Enter: M_TRACK emptied and given the nine tracks (lines 576 .. 584, values 1 .. 9), its first item highlighted (its
`+0x13C` and `+0x148` 0; its text element `+0x184` the item's text with `0x005B52C0`), its `+0x10C` freed; the four
columns emptied (their `+0x10C` freed); `+0x24` 0, `+0x20` the highlighted track's value (cut toward
zero).

Update: a new track highlighted becomes `+0x20` and clears `+0x24`. With `+0x24` 0 the columns are emptied and, for
each route of the track (`0x004265C0`: the entries of the routes' table `0x00711A38` whose `+0x84` is the track, in
order), filled: M_COL1 the route's name (the game text global at the entry's `+0x8C`: lines 585 .. 601, or 1972 for
"Point-to-point (r)"; value the route's number), M_COL2 its record's driver fitted to the column (`0x005A8F50`: the
column's width in pixels, cut toward zero; a text wider than that loses characters at its end until it fits with
"..."), M_COL4 the record's car (line 211 + its id) or "-", M_COL3 for a showoff (type 3) the points (cut toward zero,
thousands marked), for a record of 300 s "--:--:--", else the time (`0x0044FBD0`). Each column then highlights M_COL1's
highlighted place from before (0 past its items; the scroll `+0x148` there; its text element as above) and `+0x24` is
set. BACK (CLICK) `extras`.

## newhighscore_cms and savehighscores

Both windows are empty. `newhighscore_cms` (enter `0x00466420`: with fewer than two players the race type's text,
`[0x007622F0]` + 16 x type, made and dropped; `+0x0C`, `+0x10` 0; update `0x004664C0`; exit `0x004664A0`: the front
end's `+0x110` 0): its first frame, when the save system saves (its method `+0x24`) while its `+0x3C` is 0 or the
records were loaded (`[game + 0xA8] + 0x1120`), asks for `savehighscores` over it (mode 5), CLICK; `+0x0C` 1. The next
frame closes it (mode 6, `+0x10` 1); with `+0x10` set and itself the top window it asks again.

`savehighscores` (enter `0x00465DD0`, update `0x00465E50`, exit `0x00465E20`) writes the routes' records
(SAVE\HS\USERDATA, `03-saves.md`) through the records' device (`[game + 0xA4]`), a step a frame, with its messages
(the space, "Save failed! Data write error", ...; a failure asks "Hi-score has not been saved. Do you want to continue
without saving?", 1485, whose no writes again); written, the message up is closed and the window closes itself
(mode 6).

`loadhighscores` (enter `0x00465A90`, update `0x00465B00`, exit `0x00465E20`) is asked for over the main menu (mode
5) by its update (`0x004689C0`) while the records are not read, once (`[0x0075EE68]`, cleared when the menu goes back
to `start`; the driver window clears the records' `+0x1120`): the records' device reads SAVE\HS (none there: nothing
shown); read, the records' `+0x1120` is set; "Unable to load hi-scores!" for records that do not hold; then the
window closes itself (mode 6).

## autosave

`autosave` (record `0x007212D0`: enter `0x004648C0`, update `0x00464A10`, exit `0x004649D0`), asked for over a window
(mode 5) by the windows that change the driver's progress when the save system saves (its method `+0x24`), and by
extras' SAVE: enter turns the saving on when it is off (its method `+0x28` with 1), hides the PROGRESS elements and,
asked by the driver (`[0x0075ED38]`), sets TEXT "SAVING..." (1611). The profile device writes the driver (flag 9
shows TEXT; a folder there is written over without a question when the save system's driver did not change, its
`+0x04`); written, asked by the driver, "Save Successful" (a note); then the window closes itself (mode 6) and its
exit clears `[0x0075ED38]`. A write ending without success asks the "continue without saving?" question as
`saveprofile`; with the saving turned on by the window its no asks "Are you sure you want to save driver on memory
card (PS2) in MEMORY CARD slot 1?" (1994; a console's text on the PC). OpenJuice keeps in juicey.dat the entries it
does not make (those of mods).

## Music and the draws of the front end

At the end of the game object's start (`0x0040D200`) the music playlist is made (`0x00408B60` / `0x00408D20`): the
songs of the music bank (`audio\music\music.cfg`, 25) the settings allow (a bit per song at `+0x208`), shuffled
(`0x00409460`: song i swaps with song `rand() % (i + 1)`). After the front end is made, with the sound on, its music
bank starts (`0x0046BAC0`, "music1"), which draws one `rand()` value. `start` and loading a driver (`0x004679B0`)
shuffle the playlist again.

## All states

Registered by name (`0x005A8E30`) with a record whose first field is the vtable (enter `+0x00`, update `+0x04`, `+0x08`,
exit `+0x0C`; `-` is the empty method `0x0042E470`). States sharing a record are the same state under two names.

Most enters begin with the windows' enter (`0x005A5F20`): an element named Menu (any case) of the menu kind goes back to
its items (`0x005AECF0`) and selects the item the record keeps (`+0x04`; an index past the end selects 0), or with the record's
`+0x08` set (then cleared) keeps its selection and only its help ticker takes the item's help; every ticker is measured.
Most exits are the windows' exit (`0x004C7F70`) or exits of their own doing the same first: the Menu's selected item
(`+0x13C`) into the record's `+0x04`. The record lives as long as the game, so a window opened again shows the item
selected when it was left (the main menu's, the career menu's, the options' ...).

| state | record | vtable | enter | update | exit |
|---|---|---|---|---|---|
| `Arcade_Championship` | `0x0071FA48` | `0x006EF918` | `0x0042DA40` | `0x0042DD00` | `0x004C7F70` |
| `Arcade_Loading` | `0x0071FA6C` | `0x006EF938` | `0x0042E3F0` | - | `0x004C7F70` |
| `arcade_loading` | `0x00721250` | `0x006F1E30` | `0x00462840` | - | - |
| `Arcade_Race` | `0x0071FA24` | `0x006EF928` | `0x0042C5B0` | `0x0042CB90` | `0x0042CB00` |
| `Arcade_Results` | `0x0071FA60` | `0x006EF908` | `0x0042E150` | `0x0042E330` | `0x004C7F70` |
| `autosave` | `0x007212D0` | `0x006F23C4` | `0x004648C0` | `0x00464A10` | `0x004649D0` |
| `blank` | `0x00720578` | `0x006F14F8` | `0x0044F990` | - | - |
| `blank_split` | `0x00720584` | `0x006F14F8` | `0x0044F990` | - | - |
| `bootupcheck` | `0x00721648` | `0x006F25F0` | `0x004673E0` | `0x004674F0` | `0x004674D0` |
| `buymod` | `0x007203D8` | `0x006F03B4` | `0x00446750` | `0x00446B80` | - |
| `car_rolling` | `0x007228D0` | `0x006F60EC` | `0x004DD490` | `0x004DF260` | `0x004DDF90` |
| `Car_Stats2` | `0x00721DD8` | `0x006F5258` | `0x004C42D0` | `0x004C4770` | `0x004C4710` |
| `Car_Stats2purple` | `0x00721F20` | `0x006F5258` | `0x004C42D0` | `0x004C4770` | `0x004C4710` |
| `Car_StatsLogos` | `0x00722064` | `0x006F5248` | `0x004C5260` | - | `0x004C7F70` |
| `carbuy` | `0x0071FCB0` | `0x006F0374` | `0x00438150` | `0x00438BB0` | `0x00438A70` |
| `careerload` | `0x0071FC60` | `0x006F0444` | `0x004358A0` | `0x00435960` | `0x00435930` |
| `carsel` | `0x0071FC70` | `0x006F0414` | `0x00436380` | `0x004371C0` | `0x00436B90` |
| `closedown` | `0x00720590` | `0x006F14E8` | `0x0044F9C0` | `0x0044FAC0` | `0x0044FAA0` |
| `character` | `0x0071FA78` | `0x006EFAD8` | `0x0042E610` | `0x004325D0` | `0x0042F030` |
| `characterPostRace` | `0x0071FB70` | `0x006EFAC8` | `0x004337D0` | `0x004344D0` | `0x00434380` |
| `CMSAdvice` | `0x00721CC8` | `0x006F4EEC` | `0x004BFB60` | `0x004C0110` | `0x004C7F70` |
| `CMSAtMeeting` | `0x00721CD4` | `0x006F4F38` | `0x004C01E0` | `0x004C06A0` | `0x004C0650` |
| `CMSBet` | `0x007221B0` | `0x006F55C8` | `0x004CA1A0` | `0x004CA210` | `0x004C7F70` |
| `CMSCalendar` | `0x00721CF8` | `0x006F5018` | `0x004C11D0` | `0x004C1320` | `0x004C7F70` |
| `CMSCalendarHelp` | `0x00721D2C` | `0x006F5008` | `0x004C2980` | `0x004C2A60` | `0x004C7F70` |
| `CMSCarCollection2` | `0x00721D38` | `0x006F5154` | `0x004C2AD0` | `0x004C2F10` | `0x004C2E70` |
| `CMSCarDealership` | `0x00721D6C` | `0x006F5154` | `0x004C2AD0` | `0x004C2F10` | `0x004C2E70` |
| `CMSChallengeResults` | `0x007226D0` | `0x006F5E24` | `0x004D9960` | `0x004D9FB0` | `0x004C7F70` |
| `CMSCheats` | `0x00722070` | `0x006F53C4` | `0x004C54D0` | `0x004C55B0` | `0x004C7F70` |
| `CMSChooseLogo` | `0x00721DB8` | `0x006F5194` | `0x004C3B70` | `0x004C3BD0` | `0x004C7F70` |
| `CMSCrew` | `0x00722094` | `0x006F545C` | `0x004C5F60` | `0x004C63D0` | `0x004C7F70` |
| `CMSCrewRaceSetup` | `0x00722688` | `0x006F5DCC` | `0x004D8AC0` | `0x004D90F0` | `0x004C7F70` |
| `CMSCutScene` | `0x007220A8` | `0x006F5560` | `0x004C6B80` | `0x004C7BF0` | `0x004C7030` |
| `CMSGridWalk` | `0x00722148` | `0x006F55A8` | `0x004C7FA0` | `0x004C91B0` | `0x004C9130` |
| `CMSHostMeetingSetup` | `0x007221E0` | `0x006F56C0` | `0x004CA9A0` | `0x004CAAE0` | `0x004CAA90` |
| `CMSHostMeetingSetupRace` | `0x00722240` | `0x006F56B0` | `0x004CB550` | `0x004CB690` | `0x004CB640` |
| `CMSHostMeetingSummary` | `0x0072226C` | `0x006F56D0` | `0x004CC3A0` | `0x004CC500` | `0x004CC4B0` |
| `CMSLoader` | `0x007221CC` | `0x006F55B8` | `0x004CA910` | - | `0x004CA950` |
| `CMSMainMenu` | `0x0072229C` | `0x006F59C4` | `0x004CE160` | `0x004CE1F0` | `0x004C7F70` |
| `CMSMeetingSummary` | `0x007222B0` | `0x006F5A38` | `0x004CEAE0` | `0x004CECD0` | `0x004CEC30` |
| `CMSMobileChoose` | `0x00721DC4` | `0x006F5184` | `0x004C3DB0` | `0x004C3DD0` | `0x004C7F70` |
| `CMSMobilePhone` | `0x00722588` | `0x006F5BC8` | `0x004D17A0` | `0x004D1C10` | `0x004D1AD0` |
| `CMSMobilePhoneContacts` | `0x007225C8` | `0x006F5BA8` | `0x004D2CB0` | `0x004D2D90` | `0x004D2740` |
| `CMSMobilePhoneMenu` | `0x00722598` | `0x006F5BB8` | `0x004D2460` | `0x004D27C0` | `0x004D2740` |
| `CMSNewCont` | `0x00721DA0` | `0x006F51A4` | `0x004C37E0` | `0x004C3910` | `0x004C3830` |
| `CMSOnlineCrewRaceSetup` | `0x00722620` | `0x006F5D4C` | `0x004D6090` | `0x004D6550` | `0x004C7F70` |
| `CMSOnlineGridWalk` | `0x00769A68` | `0x006F5D10` | `0x004D3310` | `0x004D40B0` | `0x004D3850` |
| `CMSOnlineRaceSetup` | `0x007225E4` | `0x006F5D3C` | `0x004D4720` | `0x004D52D0` | `0x004D5270` |
| `CMSRaceResults` | `0x007226B0` | `0x006F5E34` | `0x004DA310` | `0x004DAF80` | `0x004C7F70` |
| `CMSRaceSetup` | `0x00722640` | `0x006F5DBC` | `0x004D69F0` | `0x004D7840` | `0x004D77E0` |
| `CMSRespect` | `0x007226F8` | `0x006F5F1C` | `0x004DB2F0` | `0x004DB740` | `0x004DB4B0` |
| `CMSRespectChanges` | `0x00722308` | `0x006F5A28` | `0x004CF5A0` | `0x004D00F0` | `0x004D0000` |
| `CMSRespectHelp` | `0x00722830` | `0x006F5F2C` | `0x004DCEF0` | `0x004DD290` | `0x004C7F70` |
| `CMSRespectMsg` | `0x007227E8` | `0x006F5EF8` | `0x004DC9E0` | `0x004DCBD0` | `0x004DCB90` |
| `CMSRespectOverview` | `0x00722770` | `0x006F5F0C` | `0x004DC260` | `0x004DC7A0` | `0x004DC760` |
| `CMSStats` | `0x00722954` | `0x006F6318` | `0x004DF750` | `0x004DFC10` | `0x004DFBC0` |
| `CMSStatsCarCollection` | `0x00722998` | `0x006F6328` | `0x004E08D0` | `0x004E0C70` | `0x004C7F70` |
| `CMSStatsMenu` | `0x0072298C` | `0x006F6308` | `0x004E0700` | `0x004E0760` | `0x004C7F70` |
| `colourchoose` | `0x0071FE48` | `0x006F0434` | `0x0043DB90` | `0x0043DD60` | `0x00440320` |
| `colourfocus` | `0x0071FE6C` | `0x006F03E4` | `0x0043E2D0` | `0x0043E410` | `0x00440320` |
| `colourmenu` | `0x0071FDB8` | `0x006F0364` | `0x0043D270` | `0x0043D390` | `0x004C7F70` |
| `credits` | `0x00721558` | `0x006F2600` | `0x00469D80` | `0x00469D90` | - |
| `custom_loader` | `0x00721250` | `0x006F1E30` | `0x00462840` | - | - |
| `custmenu` | `0x0071FCEC` | `0x006F0324` | `0x00439050` | `0x004390A0` | `0x004C7F70` |
| `decalmenu` | `0x0071FD28` | `0x006F0424` | `0x00439AD0` | `0x00439E50` | `0x00440320` |
| `DemoFlyby` | `0x0071FC14` | `0x006F0208` | `0x00435090` | `0x004352B0` | `0x004351C0` |
| `disclaimer` | `0x0072145C` | `0x006F2620` | `0x00466D00` | `0x00466D70` | - |
| `driver` | `0x007214B8` | `0x006F25E0` | `0x004678C0` | `0x004679B0` | `0x00463F00` |
| `enginechip` | `0x0071FF4C` | `0x006F0474` | `0x004407B0` | `0x004408E0` | `0x004C7F70` |
| `exhaustmenu` | `0x0071FF80` | `0x006F03F4` | `0x00440EF0` | `0x004411C0` | `0x00440320` |
| `extras` | `0x00721484` | `0x006F2630` | `0x0046A920` | `0x0046AA00` | - |
| `garageload` | `0x0071FC60` | `0x006F0444` | `0x004358A0` | `0x00435960` | `0x00435930` |
| `gearboxmenu` | `0x0071FFCC` | `0x006F0334` | `0x004425B0` | `0x004426B0` | `0x00440320` |
| `hornmenu` | `0x0071FF08` | `0x006F02C4` | `0x0043FEC0` | `0x00440040` | `0x0043FF90` |
| `hud_challenge_results` | `0x00720AA4` | `0x006F14A8` | `0x00450B60` | `0x00450F80` | - |
| `hud_cruise` | race front end `+0x54` | `0x006F1438` | `0x004476D0` | `0x0044B1C0` | `0x0044A450` |
| `hud_cruise_results` | `0x00720984` | `0x006F1498` | `0x00451270` | `0x00451830` | - |
| `hud_pause` | `0x00720548` | `0x006F1508` | `0x0044E920` | `0x0044EEA0` | - |
| `hud_race` | race front end `+0x54` | `0x006F1438` | `0x004476D0` | `0x0044B1C0` | `0x0044A450` |
| `hud_race_complete` | `0x007209A8` | `0x006F14D8` | `0x0044FD20` | `0x004504A0` | - |
| `hud_race_results` | `0x007205A0` | `0x006F14B8` | `0x00452E60` | `0x00453450` | `0x00453420` |
| `hud_replay` | `0x007209F0` | `0x006F1458` | `0x0044DA80` | `0x0044DE20` | `0x0044DC20` |
| `hud_solo` | race front end `+0x54` | `0x006F1438` | `0x004476D0` | `0x0044B1C0` | `0x0044A450` |
| `hud_spectator` | `0x00720A64` | `0x006F1478` | `0x00455880` | - | - |
| `hud_split` | race front end `+0x54` | `0x006F1438` | `0x004476D0` | `0x0044B1C0` | `0x0044A450` |
| `hud_sprint` | race front end `+0x54` | `0x006F1438` | `0x004476D0` | `0x0044B1C0` | `0x0044A450` |
| `hud_sprint_results` | `0x00720670` | `0x006F1488` | `0x00454760` | `0x00454DB0` | `0x00454D80` |
| `hudcolourmenu` | `0x0071FEA8` | `0x006F0314` | `0x0043ED70` | `0x0043F120` | `0x00440320` |
| `icemenu` | `0x0071FEE8` | `0x006F02E4` | `0x0043F7F0` | `0x0043FB40` | `0x00440320` |
| `instant_manu` | `0x00720C68` | `0x006F19E4` | `0x0045E490` | `0x0045E790` | - |
| `instant_selcar` | `0x00720C10` | `0x006F19F4` | `0x0045EC40` | `0x0045F340` | `0x0045F100` |
| `instant_seltype` | `0x00720ABC` | `0x006F19D4` | `0x0045B0B0` | `0x0045B390` | - |
| `instant_setup` | `0x00720AD0` | `0x006F19B0` | `0x0045BFE0` | `0x0045DC60` | `0x0045C420` |
| `juicesplash` | `0x00721450` | `0x006F2678` | `0x00466C00` | `0x00466C30` | - |
| `keyboard` | `0x00720C88` | `0x006F1CD4` | `0x00460960` | `0x00460B40` | `0x00460B00` |
| `legals` | `0x00721474` | `0x006F2610` | `0x00466DF0` | `0x00466FC0` | - |
| `link_lobby` | `0x00721724` | `0x006F2B9C` | `0x0046EBB0` | `0x0046FB20` | `0x004C7F70` |
| `link_selcar` | `0x00768090` | `0x006F2BEC` | `0x0046FF10` | `0x00471160` | `0x004709B0` |
| `link_search` | `0x0072170C` | `0x006F2BAC` | `0x0046E800` | `0x0046E880` | `0x004C7F70` |
| `loader` | `0x00721250` | `0x006F1E30` | `0x00462840` | - | - |
| `loadhighscores` | `0x00721398` | `0x006F23A4` | `0x00465A90` | `0x00465B00` | `0x00465E20` |
| `loading` | `0x00721548` | `0x006F2668` | - | `0x0046AE50` | - |
| `loadreplay` | `0x007215D0` | `0x006F2640` | `0x004694B0` | `0x00469530` | `0x00469D60` |
| `menu` | `0x00721510` | `0x006F26A0` | `0x00468490` | `0x00468850` | `0x00468790` |
| `modmenu` | `0x0071FD98` | `0x006F02D4` | `0x0043BDE0` | `0x0043C1B0` | `0x00440320` |
| `modshop` | `0x0071FFE8` | `0x006F0484` | `0x00442A80` | `0x00444690` | - |
| `multi` | `0x00721680` | `0x006F2BCC` | `0x0046CAA0` | `0x0046CB10` | `0x004C7F70` |
| `neonmenu` | `0x0071FE88` | `0x006F0344` | `0x0043E690` | `0x0043E9E0` | `0x00440320` |
| `newhighscore_cms` | `0x0072141C` | `0x006F2384` | `0x00466420` | `0x004664C0` | `0x004664A0` |
| `online_arcade_create` | `0x00721880` | `0x006F2EAC` | `0x00474840` | `0x00474A30` | `0x004761B0` |
| `online_arcade_results` | `0x007219DC` | `0x006F2F0C` | `0x00478AD0` | `0x00478C00` | `0x00478B30` |
| `online_arcade_search` | `0x007218D4` | `0x006F2E8C` | `0x00475250` | `0x00475360` | `0x004C7F70` |
| `online_bg` | `0x00721820` | `0x006F2EFC` | `0x004739C0` | - | `0x004C7F70` |
| `online_career_create` | `0x00721908` | `0x006F2E6C` | `0x00475F20` | `0x004761F0` | `0x004761B0` |
| `online_career_results` | `0x007219DC` | `0x006F2F0C` | `0x00478AD0` | `0x00478C00` | `0x00478B30` |
| `online_career_search` | `0x00721980` | `0x006F2E2C` | `0x00476DD0` | `0x00476F90` | `0x004C7F70` |
| `online_connect_gs` | `0x00721800` | `0x006F2E0C` | `0x00472760` | `0x00472A90` | `0x00472030` |
| `online_create_gs` | `0x007217E4` | `0x006F2E3C` | `0x00471D70` | `0x00472070` | `0x00472030` |
| `online_friend_options` | `0x00721760` | `0x006F2E5C` | `0x0047C0B0` | `0x0047C870` | `0x0047C670` |
| `online_friends` | `0x00721A10` | `0x006F2E7C` | `0x0047B4C0` | `0x0047B9D0` | `0x0047B870` |
| `online_ingame_bg` | `0x00721820` | `0x006F2EFC` | `0x004739C0` | - | `0x004C7F70` |
| `online_logon_gs` | `0x007217C8` | `0x006F2E4C` | `0x004719F0` | `0x00471A60` | `0x004C7F70` |
| `online_main` | `0x0072185C` | `0x006F2EBC` | `0x00473EA0` | `0x004740D0` | `0x00472030` |
| `online_mode` | `0x0072184C` | `0x006F2ECC` | `0x00473A10` | `0x00473A30` | `0x004C7F70` |
| `online_player_options` | `0x00721748` | `0x006F2E9C` | `0x0047A380` | `0x0047A5C0` | `0x004C7F70` |
| `online_players` | `0x007219FC` | `0x006F2EDC` | `0x00479CF0` | `0x00479F80` | `0x00479EB0` |
| `online_stats` | `0x00721A30` | `0x006F2EEC` | `0x0047E3F0` | `0x0047F720` | `0x0047E6B0` |
| `online_voice` | `0x007217A4` | `0x006F2E1C` | `0x0047DD50` | `0x0047E020` | `0x0047DEA0` |
| `opts_audio` | `0x00721A90` | `0x006F3698` | `0x00480960` | `0x00480AD0` | `0x004C7F70` |
| `opts_bg` | `0x00721A6C` | `0x006F3668` | `0x0047FF60` | `0x0047FFF0` | `0x004C7F70` |
| `opts_ctl_pc` | `0x00721BB8` | `0x006F3688` | - | `0x00483C40` | - |
| `opts_ctl_port` | `0x00721BAC` | `0x006F36C8` | `0x004838C0` | `0x004838F0` | - |
| `opts_game_pc` | `0x00721B00` | `0x006F3638` | `0x004813E0` | `0x00481770` | `0x004C7F70` |
| `opts_graphics` | `0x00721BC8` | `0x006F36A8` | `0x00481F00` | `0x004820A0` | `0x004C7F70` |
| `opts_ingame_audio` | `0x00721A90` | `0x006F3698` | `0x00480960` | `0x00480AD0` | `0x004C7F70` |
| `opts_ingame_bg` | `0x00721A6C` | `0x006F3668` | `0x0047FF60` | `0x0047FFF0` | `0x004C7F70` |
| `opts_ingame_game` | `0x00721B00` | `0x006F3638` | `0x004813E0` | `0x00481770` | `0x004C7F70` |
| `opts_ingame_game_ply2` | `0x00721B00` | `0x006F3638` | `0x004813E0` | `0x00481770` | `0x004C7F70` |
| `opts_ingame_menu` | `0x00721A78` | `0x006F3658` | `0x00480090` | `0x00480310` | `0x004C7F70` |
| `opts_menu` | `0x00721A78` | `0x006F3658` | `0x00480090` | `0x00480310` | `0x004C7F70` |
| `opts_playlist` | `0x007688B0` | `0x006F3624` | `0x004833C0` | `0x00483600` | `0x004835D0` |
| `opts_songlist` | `0x00721B60` | `0x006F3648` | `0x004828A0` | `0x00483130` | `0x00482D80` |
| `opts_voice` | `0x0076890C` | `0x006F3678` | `0x00484700` | `0x00484830` | `0x004847F0` |
| `opts_wheel_dx9` | `0x00721C10` | `0x006F36B8` | `0x00483D10` | `0x00483E80` | `0x004C7F70` |
| `performance` | `0x007203A0` | `0x006F0354` | `0x004423C0` | `0x00442420` | `0x004C7F70` |
| `performmod` | `0x0071FFB4` | `0x006F0384` | `0x00441D20` | `0x00442020` | `0x00441F90` |
| `performtype` | `0x0071FF9C` | `0x006F03C4` | `0x00441560` | `0x00441800` | `0x004C7F70` |
| `restart_race` | `0x00720A70` | `0x006F14C8` | `0x00455940` | `0x00455A90` | `0x00455F50` |
| `restart_replay` | `0x00720A94` | `0x006F1448` | `0x00455E20` | `0x00456030` | `0x00455F50` |
| `savehighscores` | `0x007213C8` | `0x006F2394` | `0x00465DD0` | `0x00465E50` | `0x00465E20` |
| `saveprofile` | `0x00721260` | `0x006F23D4` | `0x00463E40` | `0x00463F30` | `0x00463F00` |
| `savereplay` | `0x00721338` | `0x006F23B4` | `0x004653A0` | `0x00465400` | `0x004653D0` |
| `schememenu` | `0x0071FDE0` | `0x006F03A4` | `0x0043C600` | `0x0043CAB0` | - |
| `skippable` | `0x00721444` | `0x006F2658` | `0x00466A40` | `0x00466AD0` | `0x00466AB0` |
| `splash` | `0x00721430` | `0x006F2690` | `0x00466840` | `0x004668C0` | - |
| `split_selcar` | `0x00721698` | `0x006F2BBC` | `0x0046CE10` | `0x0046DA60` | `0x0046D3F0` |
| `sponsordecalmenu` | `0x0071FD58` | `0x006F03D4` | `0x0043A520` | `0x0043A590` | - |
| `start` | `0x00721498` | `0x006F25B0` | `0x00467060` | `0x00467350` | - |
| `start_replay` | `0x00720A84` | `0x006F1468` | `0x00455BD0` | `0x00455D60` | `0x00455F50` |
| `stylingmenu` | `0x0071FD7C` | `0x006F02F4` | `0x0043B060` | `0x0043B4A0` | `0x004C7F70` |
| `suppliers` | `0x0071FF74` | `0x006F0454` | `0x00440B80` | `0x00440CC0` | `0x004C7F70` |
| `suspensionmenu` | `0x0071FF38` | `0x006F02B4` | `0x00440560` | `0x00440620` | `0x004C7F70` |
| `tuningmenu` | `0x0071FD68` | `0x006F0394` | `0x0043A780` | `0x0043A8B0` | `0x0043A860` |
| `tintsmenu` | `0x0071FF24` | `0x006F0494` | `0x00440250` | `0x004403B0` | `0x00440320` |
| `vid_tutorial` | `0x00721570` | `0x006F25D0` | `0x0046A040` | `0x0046A0D0` | - |
| `viewhiscores` | `0x00721620` | `0x006F25C0` | `0x0046A300` | `0x0046A520` | - |
| `wheelcolourmenu` | `0x0071FED4` | `0x006F0304` | `0x0043F3F0` | `0x0043F600` | `0x00440320` |
| `wheelsmenu` | `0x0071FD04` | `0x006F0464` | `0x00439300` | `0x00439730` | `0x00439630` |

Two records of a demo build are in the executable but never registered (no code refers to them): a demo menu (record
`0x0071FC3C`, vtable `0x006F01E8`: enter `0x00435620` the windows' enter and `+0x0C` 0; update `0x00435640`: `+0x0C`
1 asks for `byebye`; SELECT (CLICK) on `StartDemo` asks for `DemoFlyby`, on `Options` for `opts_bg`, on `QuitDemo`
"Quit to Windows" / "ARE YOU SURE?" into `+0x0C`; BACK (CLICK) asks for `Start`) and `byebye` (record `0x0071FC4C`,
vtable `0x006F01F8`: enter `0x00435540` `+0x0C` 7.0, PARA1 .. PARA6 multiline, `+0x10` 1; update `0x004355E0` runs
`+0x0C` down by the game's frame time and while `+0x10` is set asks `[game + 0x18]`'s method `+0x08`, clearing
`+0x10` on 0). `demomenu.ui` and `byebye.ui` (the movie `juiced_outro`) are among the game's files. Neither name is
registered: the demo menu's `byebye` (mode 1) brings up `byebye.ui` with no state (see below), not byebye's own
class. `[game + 0x18]` is the input
manager (`0x00591A00`, DirectInput, vtable `0x006FC4E0`): its `+0x08` calls its `+0x48` (`0x00591820`), true while a
controller has input; byebye's `+0x10` goes to 0 once none has. OpenJuice opens the two windows by
`unregistered:demomenu` and `unregistered:byebye`.

## Windows the game never opens

The window registry takes 184 registrations (`0x005A8E30`; the names of the table above). A name is opened by
`0x005A6DB0`: the window of the name's `.ui` is loaded (`0x005A9490`; a missing file gives an empty window) and
pushed with the state the registry finds for the name, or with none: such a window draws but nothing answers its
events, and it cannot be left. The stock asks for these names no record takes (each a stock problem where it can
be reached; none can on the PC):

- `car_maintain` (`0x004374A7`, mode 5): carsel's `maintenance` item, which `carsel.ui` does not have; there is no
  `car_maintain.ui` either.
- `CMSCareerSummary` (`0x004CDF6A`, mode 1) and "" (Cars, mode 1) from the earlier career menu (never registered,
  `14-career-windows.md`); `cmscareersummary.ui` (DATE, CASH, WIN1 .. 6 and LOSE1 .. 3 with their C columns) is among
  the game's files, and no code of the PC executable fills it.
- `byebye` (`0x00435675`, mode 1) from the demo menu.
- `opts_ctl_ps2` and `opts_wheel` (`0x00483B08`, `0x00483B53`, mode 7) from `opts_ctl_port`; `opts_ctl_ps2.ui` is
  among the game's files (the PS2 pad), `opts_wheel.ui` is not.
- `newhighscore` (`0x00466517`) is passed with mode 6, which takes the top window off whatever the name.

Window classes whose records are never registered (no code refers to the records):

- the demo menu and byebye (above); the earlier career menu (record `0x00722284`) and the repair window (record
  `0x007203C4`) of `14-career-windows.md`;
- record `0x0072212C` (vtable `0x006F5598`): enter `0x00473A10` (the windows' enter, nothing more), no update, leave
  the windows' leave;
- records `0x00722138` and `0x007225D8` (vtable `0x006F5D00`): the same enter and leave; update `0x004D31D0`: BACK
  gives CLICK and asks for `CMSMainMenu` (mode 1);
- record `0x0072173C` (vtable `0x006F2BDC`): four empty methods;
- the race HUD's class (vtable `0x006F1438`) has a record of its own at `0x00720738` that nothing uses: the HUD runs
  on the race front end's copy (`+0x54`, `0x0044D9F0`), registered as `hud_solo`, `hud_cruise`, `hud_sprint`,
  `hud_race` and `hud_split`;
- the message windows' records `0x00722FAC`, `0x00722FD8` and `0x00723004` (vtable `0x006FDDA0`) are opened by the
  message functions `0x005A80E0`, `0x005A8030` and `0x005A7F90`, not by name.

The records' names are unknown (only a registration names a window). OpenJuice opens these classes by
`unregistered:CMSMainMenu`, `unregistered:repair`, `unregistered:demomenu`, `unregistered:byebye` and by their
records, `unregistered:0x0072212C`, `unregistered:0x00722138`, `unregistered:0x007225D8`, `unregistered:0x0072173C`
(no `.ui`), with `--screen`.

`.ui` files no code of the PC executable opens: `blank_tactics`, `cmsatmeetingsubmenu`, `cmscareergameover`,
`cmscareersummary`, `cmsdriverdriverbet`, `cmsgarage`, `cmsnewpriv`, `instant`, `instant_selgears`, `legal_car1` ..
`3`, `startup`, `test`, `test2`, `vid_demo`, `vid_extras`, `videos` and the consoles' `demoloader_ps2`,
`demoloader_xbox`, `dnas`, `eula_ps2`, `memcardwarning`, `netcnf`, `online_advanced`, `online_logon_xbox`,
`online_passcode_xbox`, `online_quick`, `opts_audio_ps2`, `opts_ctl_ps2` (above), `opts_ctl_xbox`, `opts_game`,
`opts_live` (`startup` is only compared with the bottom window's registered name, `0x0040BD11` and `0x0046B6FC`;
`eula_ps2` and the two Xbox names with the top window's name in the front end's frame, `0x0046C4CA`, `0x0046C697`). The loaders' `arcade_loader_anim`, `custom_loader_anim` and `loader_anim` are put over
the loaders' windows (`0x00462D90`, `0x004634B0`) and `msgbox`, `msgbox_critical`, `msgbox_network` are the message
windows' files.
