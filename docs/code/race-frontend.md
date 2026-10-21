# The race front end's code (`src/Game/Race/FrontEnd`, `src/Game/Race/Hud`)

How the windows over a race are put together. What they do is in `docs/spec/22-race-hud.md`; this page is about where
that lives and how the parts talk. The game's front end (the menus, the garage, the career) is `frontend.md`.

## The pieces

- **`RaceFrontEnd`** (`FrontEnd/RaceFrontEnd.h`) is the stock's race front end (the game object's +0x3C, made by
  0x004563E0): its window manager, the window shown and the one asked for, the window objects, the messages
  (`message`, `netMessage`, the controller's note `critical`), the save windows, its sounds (`raceUi`) and its black
  (`raceFade`). `load` makes it as the race loads, `closeWindows` closes it as the race ends.
- **`RaceFrontEndHost`** is what its windows ask of the race: what the HUD reads (`hudRace`), the race's state, the
  replay, the pause and the sounds' holds, the restart and the replay's start, and what a window's frame asks of the
  race (`followWindow`, `followReplayWindow`). `TrackView` gives it (`TrackView::FrontEndHost`).
- **A window** is a class in `Hud/`, one per stock record: `RaceHud` (the HUD of the race's type), `RaceComplete`,
  `RaceResults`, `ChallengeResults`, `SprintResults`, `CruiseResults`, `RacePause`, `HudReplay`, `RaceScreenWindow`
  (closedown, the restart's, the spectator's, blank_split), `RaceMessage`; the save windows `SaveHighScoresWindow` and
  `SaveReplayWindow` (`RaceSaves.h`, behind `RaceSaveHost`, which `RaceFrontEnd` gives). A window's frame returns a
  `RaceWindowFrame`: what it asks of the race (the window to ask for, a question, the race going on, ..).
- **`RaceWindowId`** (`Hud/RaceWindowId.h`) names the windows the window manager knows by name; the front end switches
  on it (`windowFrame`, `drawWindow`, `leaveWindow`, `enterWindow`). `RaceWindowShown` keeps the window shown's name
  (the manager's and the logs') with its id.

## A frame

`TrackView::frontEndFrame` runs the stock's order: the network's watch before the windows (`netBeforeWindows`), the
controllers' watch (`controllerWatch`; a lost controller can end the frame there), then `RaceFrontEnd::windowsFrame`
(the sounds, the note closing, the window asked for coming in, then whichever takes the events: the note, the
network's message, the message, the save windows, else the window shown), the network's watch after the windows
(`netAfterWindows`) and the players' messages. The watches are the race's (`TrackViewControllers.cpp`,
`TrackViewNetwork.cpp`): they pause the race and stop the replay, and reach the front end as `frontEnd`.

The race asks for a window with `frontEnd.askWindow(RaceWindowId::..)` (or a name a window returned); it comes in at
the next frame's `windowsFrame`: the window shown leaves, the new one enters.

`TrackView::draw` draws the views, the network's overlay and the racers' labels, then `RaceFrontEnd::draw`: the window
shown, the message, the black, the network's message, the note, the invitation's icon.

## Adding a race window

1. A class in `Hud/`: the record's fields, its `enter` / `frame` (returning a `RaceWindowFrame`) / `draw` / `leave`.
2. Its name in `RaceWindowId` (and `kNames` in `RaceWindowId.cpp`), its object in `RaceFrontEnd`, kept by `load` when
   the stock's manager keeps it.
3. Its cases in `enterWindow`, `windowFrame`, `drawWindow` and `leaveWindow`.
4. What it needs of the race: a method on `RaceFrontEndHost`, given by `TrackView::FrontEndHost`.
