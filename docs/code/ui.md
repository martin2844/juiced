# The interface (`src/Game/UI`)

How a window of the interface is read, kept, run and drawn. The windows themselves (what the menus, the garage, the
HUD do) are the front ends': `frontend.md` and `race-frontend.md`. What the stock interface does is
`docs/spec/04-interface.md` and `docs/spec/02-frontend.md`.

## The pieces

- **`UiFile`** (`UiFile.h`) reads a window's file, `ui\windows\<name>.ui` ("Jet UI Window File"): its elements
  (`Element`: a kind, a name, a rectangle, its properties) in their order. The kinds are the stock's
  (`ElementKind`): Box, Text, Button, Menu, Meter, Ticker, Movie, 3d, Dial, Slider, ColourChart, Line.
- **`Screen`** (`UiScreen.h`) is one window on screen: its file, its texts in the game's language
  (`ui\windows\<language>\<name>.<ext>`), its textures (`ui\textures\<name>.ddt`, or the language's), its fonts and
  the state of each element (hidden, highlighted, its transition, a menu's items, a meter's value, ..). A window's
  code works on it by the elements' names: `setText`, `show`, `hideContaining`, `setCurrentButton`, `selectMenuItem`,
  `setMeter`, .. Each frame `update` steps the transitions and the moving elements, `draw` draws them in order.
- **`WindowManager`** (`WindowManager.h`) opens windows by name as the stock manager does: one it keeps (`keep`,
  made once and reused with its transitions started again and the rest as its last visit left it) or a new one made
  from its file. A front end owns one.
- **`Components/`**: an element kind's own state and drawing (`Menu`, `Meter`, `Slider`, `Dial`, `Ticker`,
  `ColourChart`, `Movie`, `Text` and its fonts, `Transition`, `TextModifier`; `Box.cpp` the boxes and lines). The
  draws are `Screen`'s methods (`drawMenu` in `Menu.cpp`, ..), the state the component's class.
- **`MessageWindow`** (`MessageWindow.h`): the three message windows (`msgbox`, `msgbox_network`,
  `msgbox_critical`), their types (`MessageType`: a note, CONTINUE, yes / no, a progress bar) and what SELECT and BACK
  do in them, shared by the two front ends.
- **`UiSound`** (`UiSound.h`): the interface's sounds, a player of the bank `audio\ui\<bank>` (`SELECT`, `BACK`, ..),
  a set played again within 0.05 s left out, as the stock does.

## A window's life

1. A front end asks its manager to `open("carsel")`. The manager's `prepare` gives the window its sound and the
   factory of its 3D views before the file is read; its `fileOf` names the file when the window's name is not it.
2. `Screen::load` reads the `.ui`, the texts, the textures and the fonts. The fonts are chosen by the screen's size
   class (`resolutionClass`: 640, 800, 1024, 1280, 1600; `ui\fonts\<font>_<class>.d8f`); the elements' rectangles are
   fractions of the screen.
3. The window's code (`enter`) sets texts, shows and hides elements, fills menus, picks the first button.
4. Each frame the front end passes the interface's event (`input::UiFrame`: the direction, SELECT, BACK, .. made by the
   controls' UI map) to the window's code, which moves the highlight (`moveHighlight`), steps a menu
   (`selectMenuItem`) or answers; then `update` and `draw`.
5. Closed, the window is freed unless the manager keeps it.

A Begin3d element shows a 3D view (`Components/View3d.h`): the front end's factory makes it from the element
(`Game/Scene/Scene3d.h`, a car turning, the city's map, ..) and the screen draws it in the element's rectangle after
its box. A Movie element plays a Bink movie (`audio.md`).

## Following the stock

- The fields the stock code writes on an element are kept with their offsets in the comments (`+0x120` highlighted,
  `+0xEC` a meter's shown value, ..); a window's code reading them by offset finds them under their names.
- The text fitting (`fitText`, `fitTextEnd`), the pixel widths (`pixelWidth`, `pixelWidthNearest`) and the tickers'
  measures round as the stock code does: the windows' layouts depend on it.
- A window under a message draws only its `FRAME_` elements, or none (`Screen::Under`), as the stock manager does.

## Testing

`tests/Integration/Frontend` opens windows from the game's own files over a render device that draws nothing
(`tests/Framework/Fakes/NullRenderDevice.h`); a window's test reads what its screen shows (`textOf`, `hidden`,
`currentButton`). `tests.md` and `frontend.md` show one.
