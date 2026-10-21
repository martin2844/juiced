# Interface windows of the stock game

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

## Files

A window is `ui\windows\<name>.ui` (text, `;;` comments): `NextStringID n`, then `[BeginUI]` ... `[EndUI]` holding
elements:

```
Begin<Kind> <NAME> x y w h        fractions of the screen
    <Property> values...
End<Kind>
```

Its texts are `ui\windows\<language>\<name>.<ext>` (UTF-16, a line per `Caption` number, ending with the line `END`);
when the file of the game's language does not exist the English one is read (`english\<name>.eng`, `0x005AA660`).
Names of elements and words of properties are compared without regard to case.

Kinds (`Begin*` handlers in the table `0x00724B00`, element kind at `+0x14`, class vtables with destroy / draw /
update):

| kind | number | object size | vtable | draw |
|---|---|---|---|---|
| Line | 1 | 0x6C | `0x006FE604` | `0x005B3340` |
| Box | 2 | 0xC0 | `0x006FE5EC` | `0x005B2BE0` |
| Text | 3 | 0x154 | `0x006FE5DC` | `0x005B1CB0` |
| Button | 4 | 0x1D8 | `0x006FE648` | `0x005B1CB0` |
| 3d | 5 | 0x1B4 | `0x006FE610` | `0x005B4870` |
| Menu | 6 | 0x1B4 | `0x006FE5A4` | `0x005AFB80` (update `0x005B11A0`) |
| Slider | 7 | 0xE0 | `0x006FE658` | `0x005B60E0` |
| Ticker | 8 | 0x168 | `0x006FE61C` | `0x005B53C0` |
| Movie | 9 | 0x118 | `0x006FDED0` | `0x005AA4C0` |
| Dial | 10 | 0xF0 | `0x006FE62C` | `0x005B5B10` |
| Meter | 11 | 0x100 | `0x006FE674` | `0x005B6650` |
| ColourChart | 12 | 0xEC | `0x006FE680` | `0x005B7460` |

Every kind draws its box first (`0x005B2BE0`).

## Properties

| property | handler | meaning |
|---|---|---|
| `Colour a r g b` | `0x005AC100` | the four corner colours (`+0x10 .. +0x1C`); each corner's alpha is also its most (`+0x20 .. +0x2C`) |
| `VertexColour TOPLEFT\|TOPRIGHT\|BOTLEFT\|BOTRIGHT a r g b` | `0x005AC7A0` | one corner |
| `Texture <name>` | `0x005AC240` | `ui\textures\<name>.ddt` |
| `UVs u0 u1 v0 v1` | `0x005AC270` | texture coordinates (`+0x5C .. +0x68`) |
| `UVScroll du dv` | `0x005AC330` | the texture moves (`+0x6C`, `+0x70`, flag `+0x74`) |
| `Animated <columns> <rows> <rate>` | `0x005ACA10` | frames of a texture sheet by time: `+0x78` 1, columns `+0x7C` and rows `+0x80` (at least 1), frames a second `+0x84` (0 when missing; the box's constructor `0x005B2A80` sets 25) |
| `Additive` | `0x005ACAB0` | additive blending |
| `Outline` | `0x005AC1A0` | an outline rectangle (`+0xAC`, colour `+0xB0`) |
| `InactivePos x y` | `0x005ACAD0` | read and dropped |
| `Transition <type> <duration> <delay> [LOOP]` | `0x005AE180` | see Transitions |
| `Caption n` | `0x005AC670` | line n of the window's texts |
| `Font <name> a r g b` | `0x005AC3A0` | text font and colour (`+0x11C`, `+0x150`; menus: their own fields) |
| `HighFont <name> a r g b` | `0x005AC500` | font and colour while highlighted (`+0x12C`) |
| `Align LEFT\|RIGHT\|CENTRE` | `0x005AC6F0` | 1, 2, 0 (`+0x138`, menus `+0x158`); another word gives LEFT; without the property CENTRE |
| `Highlight` | `0x005AC780` | the button highlighted when the window opens |
| `Shadowed` | `0x005AD700` | a black copy of the text 3 pixels down and right (`+0x124`, menus `+0x154`) |
| `Multiline`, `Autoscroll`, `TickerSpeed` | `0x005ACB20`, `0x005ACB40`, `0x005ACB70` | text breaking and scrolling |
| `AddItem <caption> <value> <command> [<help>]` | `0x005AD3A0` | a menu item |
| `AddTexturedItem <texture> <highlight> <caption> <value> <command> [<help>] [ADDITIVE]` | `0x005AD4B0` | a menu item with pictures; `""` is no picture, caption -1 no text |
| `ItemHeight h`, `HighlightItemHeight h` | `0x005AD620`, `0x005AD670` | menu item height (default 0.5), extra space around the selection |
| `MenuType HORIZONTAL\|VERTICAL` | `0x005AD800` | 0 / 1 (default 1) |
| `OptionCentered`, `ClipMenu`, `AutoArrows` | `0x005AD7E0`, `0x005AD860`, `0x005AD880` | menu layout flags (`AutoArrows` `+0x190`) |
| `TextXOffset n`, `UnselectedOffset f`, `SelectItem n` | `0x005AD740`, `0x005AD790`, `0x005AD6C0` | |
| `Movie <name>`, `Looped`, `PlayFast` | `0x005ADBB0` .. | movie elements: `movies\<name>.bik` |

`AlignedTop`, found in some window files, is no property of the table and does nothing.

## Coordinates

An element's box in pixels is `x * width`, `y * height`, `(x + w) * width`, `(y + h) * height` of the back buffer
(the window's offset and scale are 0 and 1). The element's place is `+0xA4`, `+0xA8` and its size `+0x94`, `+0x98`
(the .ui line's, or what the code sets): the box draw places it at `+0xA4 + (+0x9C - +0xA4) * f * f` with the factor
`f` (`+0xBC`) moving toward 0 while `+0xB8` is set and toward 1 by the frame time when not; the constructor
(`0x005B2A80`) sets `+0xB8`, `+0xBC` 0 and both points to the place, and nothing clears `+0xB8`, so an element stays at
`+0xA4`, `+0xA8`. `InactivePos` is read and dropped.

An alpha from 0 .. 1 becomes a byte as x 255 cut toward zero (`0x0042E560`). `0x00434D20` sets the four corner
colours and each corner's most alpha (`+0x20 .. +0x2C`, alpha / 255); `0x0042E4F0` sets each corner's alpha byte from
a value held within 0 .. 1.

The window's list (`+0x5C`) is the order of drawing and of the searches by name (`0x005A9A90`: the first element of
the name, case aside); `0x005A9CA0` takes an element out and puts it at the end.

Quads are drawn at these positions; a coordinate within 2 pixels of the screen's left or top edge becomes -0.5, one
within 2 pixels of the right or bottom edge becomes the screen's width or height (`0x00517730`, `0x005177C0`). A
picture's quad (`0x00517EC0` and the like) is drawn with the material TexShape2D (class `0x10002`, `0x0052CF20`): stage
0 clamped, then the texture's own sampler states (its `.ddt` header, `0x0051EF30`) unless the stage has them already
(`[0x0075B298]`, the renderer's cache; then the clamp stays). Of the interface's 733 textures 637 are point sampled, 94
linear, all but two wrapping. A font's glyphs go through the font material (`0x0052B230`: the page's own states, not
clamped; OpenJuice samples the pages linear and wrapping).

## Transitions

`0x005B2970` makes the transition object (0x3C bytes): type `+0`, loop `+4`, out `+8`, duration `+0x0C` and the time
left `+0x10`, delay `+0x14` and the delay left `+0x18`, the screen's width and height `+0x1C`, `+0x20`, progress
`+0x24`, grow `+0x28`, alpha `+0x2C`, offsets `+0x30`, `+0x34`, flash colour `+0x38`; it is stepped once with 0.001 s.

Types: FADEIN 9 (also any unknown word), SLIDE_LR 1, SLIDE_RL 2, SLIDE_TB 3, SLIDE_BT 4, SLIDE_LRBT 5, SLIDE_RLBT 6,
SLIDE_LRTB 7, SLIDE_RLTB 8, GROW 10, GROW_HORI 11, GROW_VERT 12, GROW_UP 13, GROW_DOWN 14, GROW_LEFT 15, GROW_RIGHT
16, FALSH 17.

A step (`0x005B56B0`, from the box draw, by the frame time): while time is left, progress = 1 - left / duration
(within 0 .. 1), then by type, with p = progress (going out: the other way):

| type | effect |
|---|---|
| SLIDE_LR | x offset = -(1 - p) * width |
| SLIDE_RL | x offset = (1 - p) * width |
| SLIDE_TB | y offset = -(1 - p) * height |
| SLIDE_BT | y offset = (1 - p) * height |
| FADEIN | alpha = p |
| GROW | grow = p, alpha = p |
| FALSH | alpha = 1 - p, a white flash of that alpha |
| the others | nothing |

Then the delay counts down first, the time left after it. A finished transition: when looping it starts again,
else progress 1, alpha 1 and no offsets (going out: alpha 0 and offsets -1000).

The box draw adds the offsets; for FADEIN and GROW each corner's alpha becomes the transition's, but not above the
corner's own (`0x0042E560` sets the alpha byte); GROW shrinks the box towards its centre by (1 - grow)² of its size;
FALSH on a Box draws a white quad of the flash alpha over it (not over an Animated or a UVScroll box, whose draws end
there).

The texture of a box is drawn by the first of: Animated (`0x005184B0`: the cell of the game's clock x the rate, cut
toward zero, modulo columns x rows, counted along the rows; its texture coordinates column / columns .. (column + 1)
/ columns and the same for the row), UVScroll (`0x005185D0`), the UVs. Eleven windows use Animated: the loaders'
and the message's spinners (`Animated 4 2 12`) and the arrows of `cmscheats` (4 2 10), `colourfocus`, `enginechip`,
`gearboxmenu`, `suspensionmenu`, `tintsmenu` and `wheelcolourmenu` (4 2 12).

An event that comes while an element of the window has not finished its transition ends all transitions of the
window (`0x005AA160`: time left 0, progress 1; an element going out is hidden).

Setting an element's text (`0x005B1980` from a narrow text, `0x005B19C0` from a wide one, and the same lines written
out in place at many call sites) starts the text's modifier again (`+0x108`, 0x2C bytes; the text's constructor makes
the menus' BOUNCE record, `0x005B17E1`, and a `FontModifier` line, `0x005AC8C0` / `0x005B1B20`: `ONCE`, `LOOP` or
`BOUNCE`, the start, the growth, the speed and the reach, makes it anew): `+0x08` = `+0x04`, `+0x18` the integer 1,
`+0x1C`, `+0x20` 0, `+0x24` 1.5; the element's transition (`+0x34`) is left as it is. Emptying the text (`0x005B1A00`)
leaves it too. A text highlighted in a HighFont (`0x005B1E79`) steps its modifier (`0x005B14C0`, see Menu) by the frame
time with the text's length and draws through it (the shadow too): a swelling, fading place sweeps along the
highlighted button's text. No window has a `FontModifier` line; on a Menu it makes the menu's record anew
(`0x005AF0B0`), on another element it is written at the element's `+0x10C` (a stock problem). Neither `SelectItem n`
(`0x005AD6C0`: the item n of those made so far highlighted, past the last the first) nor `HighlightButton <name>`
(`0x005AE150`: the button of the name made so far highlighted, else none) is in any window, nor a `BeginLine <name>
x0 y0 x1 y1` element (`0x005AACB0`, `0x005B32B0`; drawn by `0x005B3340` while shown: a line between the two points
of the screen moved by its transition's offsets in its Colour, `0x00517A60`; a FADEIN or GROW writes the transition's
alpha into that colour, where it stays).

## Fonts

`ui\fonts\<name>_<class>.d8f`. The resolution class is 640, raised to 800 / 1024 / 1280 / 1600 once the back buffer
reaches 800 x 600, 1024 x 768, 1280 x 960, 1600 x 1200. Glyphs are drawn at their own pixel size (no scaling to
the screen).

```
u8  palette[256][4]          B, G, R, A
u32 glyphCount, default glyph (glyphCount - 1), spaceAdvance
glyph[glyphCount], 48 bytes: u16 code (UTF-16), u16 0xFFFF, u32 width, height, i32 xOffset, yOffset (from the pen,
    negative above the baseline), u32 advance, u32 0, f32 u0, v0, u1, v1, u32 page
u32 pageCount
page[pageCount]: u32 width, height, u8 pixels[width * height] (palette indices)
```

The font object (vtable `0x006F845C`, made by `0x0051E070`): the ascent is the most `-yOffset`, the descent the
most `height + yOffset` over all glyphs; the line height is their sum (`0x0051E710`). The glyphs 0x152 and 0x153
are also found as 0x8C and 0x9C. Characters the font lacks use the default glyph.

Width of a text (`0x0051E690`): the space adds `spaceAdvance`, `_` adds nothing, other characters their glyph's
advance, or the font's fixed advance (`+0x14`) while its `+0x10` is set (the font's vtable `+0x10`, `0x0051DB90`).

Drawing a text (`0x005186C0`, `0x0051E740`): the pen position is cut to whole pixels (toward 0); each glyph is a
quad at pen + offsets of its size; the space advances by `spaceAdvance`, a glyph by the fixed advance while it is
set, else by its advance (the last character by its width).

## Text and Button

`0x005B1CB0`: the box, then the text (`+0x10C`, the caption or the text the code set). A highlighted text (`+0x120`,
`0x005B2930`: set or cleared with the alpha back to 1) uses the HighFont (`+0xC4`) and its colour (`+0x12C`), else the
Font (`+0xC0`) and its colour (`+0x11C`). The colour's alpha byte is replaced by the text's alpha `+0x130` (1 from the
constructor; a highlighted text's below 0.2 becomes 1 first): without a transition the Font line's alpha is not used.
With a transition its offsets move the text; while a FADEIN or GROW runs (progress below 1) the alpha is the
transition's; then, the transition running or done, the alpha is held to the colour's own.

One line (the default): x = box left + 2 (LEFT), box right - 2 - width (RIGHT), or box centre - width / 2 (CENTRE);
the baseline is the box's middle + 0.325 x the line height. With `Shadowed` a black copy (the colour's alpha) is
drawn first at +3, +3. The transition's offsets move the text. Lines with a baseline at -50 or below, or at the
screen's height + 50 or more, are not drawn.

Multiline (`+0x128`, set by code: disclaimer BOX01..03; legals OWNERS, NOKIA, BINK, DEVELOPED, JUICEDTHQ, ONLINE,
DOLBY, LOGITECH): the first baseline is the box's top + the line height, each further line a line height lower.
Characters are added to the line; CR is dropped, LF ends the line; once the line is wider than the box (whole
pixels) it is cut at its last space.

`Multiline` in the file sets `+0x128` (`0x005ACB20`); `Autoscroll` (`0x005ACB40`) sets `+0x140` and `+0x13C` and
clears `+0x144`. With `+0x13C` the multiline text is drawn inside its box only. With `+0x140` it scrolls: the first
baseline is the box's bottom + the line height, less `+0x144`, which grows each draw by the front end's frame time x
`+0x148` (30 from the constructor) while the front end's `+0x34` window is not up; after the lines `+0x14C` takes the
box's height and the lines' heights, and when that is below `+0x144` the scroll starts again from 0. The text's
drawn colour is `+0x150` (the Font line's at first, also set by code).

## Ticker

A ticker (kind 8, `0x005B51B0`: a text element of 0x168 bytes) moves its one line of text from right to left
through its box. Fields: width `+0x154` (0 from the constructor: to be measured), moved `+0x158` (0), `TickerSpeed`
`+0x15C` (0.25; `0x005ACB70` reads the number, 0 without one), ramp `+0x160`, fade `+0x164`.

Measure (`0x005B52C0`): the width is the text's (the font's `+0x08`; none without a font) + the box's width in pixels;
the text starts half of it in, at most 0.95 of the box and within 0 .. 550; the ramp and the fade 0. The windows'
enter (`0x005A5F20`) measures every ticker of the window; code that sets a ticker's text calls it (else the old width
stays).

Draw (`0x005B53C0`): the box; with a text and a font the drawing is cut to the box (held to the screen). The text's
right end is 2 pixels in from the box's right side, less `+0x158`; its baseline the box's middle + 0.3 x the font's
height. Below a fade of 1 the fade grows by the window's step (to 1); then the ramp grows by it to 1 and `+0x158` by
step x speed x ramp x 400, back to 0 once past the width; a width of 0 sets the text again (`0x005B5280`, the effect
held) and measures it. With a transition its offsets move the text and while it runs the fade is 0. The colour is
the Font's with the fade x 255 (cut) as its alpha.

On PC every ticker of the window files is placed at y 1.25, below the screen (they are drawn into a clip of the
screen's last row and show nothing), but for `performtype`'s HELP; the menus' help tickers (below) are on the
screen.

## Showing and hiding elements

An element's `+0x58` hides it. `0x005A9BB0` shows a hidden element of a name (without regard to case);
`0x005A9C20` the hidden elements whose name holds a part (case counts); with "animate" their transition starts again
(time and delay left = their lengths, progress and alpha 0, not going out), else it rests at its end (progress 1).
`0x005A9AD0` hides the elements whose name holds a part: with "animate" one with a transition goes out (time left =
the duration, progress 0, no delay) and is hidden by the box draw once the progress reaches 1; without, at once.
`0x005AA0E0` sends every transition of the window out that is not going out. `0x005AA120`: every transition of the
window has reached its end.

## Dial

The Dial (`0x005B5960`): the needle texture `+0xD0` (`ui_gauge_needle`), its colour `+0xD4` (white), the range `+0xC0`,
`+0xC4` (-2.3561945, 2.3561945), the angle shown `+0xC8` (2.3561945) and the angle `+0xCC` (-2.3561945), the length
`+0xD8` (0.45), the half width `+0xDC` (0.04), the tail `+0xE0` (0.08), the pivot `+0xE4`, `+0xE8` (0.5, 0.5) and the
ease `+0xEC` (0.25). `NeedleTexture <name> a r g b` (`0x005ADC50`) and `NeedleData min max pivotX pivotY length width
tail` (`0x005ADD30`) set them; the shown angle and the angle keep the defaults.

`0x005B5B10` after the box, while shown and with its texture: the shown angle moves the ease of the way to the angle;
with H the box's height in pixels and the pivot at (pivot x x w + x, pivot y x h + y) of the screen, the corners
(-H x width, -H x length), (H x width, -H x length), (-H x width, H x tail), (H x width, H x tail) are turned by the
shown angle (`0x005B5AC0`: x cos - y sin, x sin + y cos) about the pivot and moved by the transition's offsets (a
fading transition sets the box's first corner's alpha, not the needle's). The needle texture's quad (`0x005181B0`,
texture coordinates (0, 0), (1, 0), (0, 1), (1, 1) in that corner order) is drawn black 2 pixels right and 2 up, then
in the needle's colour; `0x005181B0` moves onto the screen's edges, within 2 pixels, the first corner's x and y and
the second corner's y (as an x) and x (as a y).

## Meter

`0x005B6650` after the box: nothing without a MeterTexture (`+0xC0`). The shown value `+0xEC` moves to the value
`+0xF0` by `+0xF4` (0.15) of the way at each draw; within 0.01 of 0 nothing is drawn. MeterType VERTICAL (`+0xE4` 1,
the default) fills from the bottom, HORIZONTAL from the left, MeterCentered (`+0xE8`) from the middle (left for a
negative value). The part of the box is drawn with the same part of the picture; the colours (MeterVertexColour,
`+0xC4 .. +0xD0`, white by default) take the box's fading transition, and the far corners move from the near corners'
colours to their own by the value (each byte rounded).

A slider meter (`+0xFC`, set by its window) that its window marks (`+0xF8`) takes the direction held each frame
(`0x005B6580`, the window's `+0x90`): a horizontal one (`+0xE4` 0) LEFT down and RIGHT up, a vertical one UP down and
DOWN up. Up: while the shown value is under 1 it takes the frame time x 0.5; down: while it is over 0 it gives that.
The shown value is then held within 0 .. 1 and the value takes it.

## Slider

A Slider (`0x005B5EE0`) starts at 0.5 (`+0xC0`), horizontal (`+0xCC` 0), active (`+0xD4` 1), its knob white (`+0xC8`)
with the picture `ui_sliderhandle` (`+0xDC`). `SliderType HORIZONTAL | VERTICAL` (`0x005ADA10`), `SliderColour a r g
b` (`0x005ADA70`), `SliderTexture` (`+0xD8`, not drawn) and `KnobTexture` set them; the box's own Texture is the bar.

An active slider takes the direction held each frame (`0x005B6020`, the window's `+0x90`): a horizontal one LEFT
down and RIGHT up, a vertical one UP down and DOWN up, by the frame time x 0.5 while not at that end; then held within
0 .. 1.

The draw (`0x005B60E0`) after the box: the knob is a square of 0.8 of the box's height (a vertical one: of its width)
whose centre runs from 0.08 of the box's width in from its start to as far in from its end by the value, across the
box's middle. The box's transition moves the run but not the middle; a FADEIN or GROW sets the knob colour's alpha
(and keeps it there). The knob is drawn at its colour, dark grey (0xFF323232) when the slider is not active; without
a picture a plain square.

## ColourChart

A ColourChart (`0x005B6E70`) finds the colour under its cursor on the chart its window shows as a picture
(`ui_colourbar`). Its fields: the cursor's column and row (`+0xC0`, `+0xC4`), the half rows (`+0xC8`, 12), the cells
of a hue (`+0xCC`, 7), the cursor's wait (`+0xD0`), the colour looked for (`+0xD4` set, `+0xDC` the colour), the
colour under the cursor (`+0xD8`), the cursor picture (`+0xE0`, `ui_colourselect`), its blink (`+0xE4`) and the greys
(`+0xE8`, 1).

The update (`0x005B6FE0`): the wait less the frame time, the blink up by twice the frame time (0 again past 1), the
wait held within 0 .. 0.03. With no wait and the chart shown the direction held moves the cursor: UP and DOWN a row
(0 .. (`+0xC8` - 1) x 2, to the nearest), LEFT and RIGHT a column (0 .. 7 x `+0xCC` - 1; without the greys 7 x
(`+0xCC` - 1) - 1); the wait 0.03.

The draw (`0x005B7460`) after the box: the box holds 7 x `+0xCC` cells across (7 x (`+0xCC` - 1) without the greys)
and (`+0xC8` - 0.5) x 2 rows. Row r (from `+0xC8` - 1 at the top to 1 - `+0xC8`) has the brightness r / (`+0xC8` - 1);
its cells run red to yellow, yellow to green, green to cyan, cyan to blue, blue to magenta, magenta to red (`+0xCC`
each, 255 / `+0xCC` a step), each shaded (`0x005B7270`: a negative brightness b scales red, green and blue by 1 + b,
another adds b x b x 255; held within 0 .. 255, cut, opaque), then `+0xCC` greys of 255 / (`+0xC8` - 1) x r x 0.5 +
128 with the alpha of the element's colour. While looking, the colour looked for first goes up by a third (held
within 0 .. 255, cut); the first cell within 8 of it in red, green and blue (`0x005B7200`, `[0x00725840]`) takes the
cursor and the looking ends after the chart. The cell under the cursor gives `+0xD8`. The cursor picture (white)
covers that cell grown by the blink x its width on each side and the blink x its height (10 at most) x 4 above and
below. Then `+0xD8`'s red, green and blue become 0.75 of themselves (to the nearest).

## Movie

A Movie element (0x118 bytes, `0x005AA290`): `+0xC0` the movie's name, `+0x100` looped, `+0x104` fast, `+0x108`
muted, `+0x10C` the draws before a muted movie starts again, `+0x114` the volume (1). `Movie <name>`
(`0x005ADBB0`) gives the interface's movie back, keeps the name, sets `+0x100` 1 and the volume 1, then `+0x100` 0
whatever came before; `Looped` (`0x005ADC10`) sets `+0x100`, `PlayFast` (`0x005ADC30`) `+0x104`. `0x005AA300` (the
code's request: a name, looped or not) gives the interface's movie back and sets the name, `+0x100` and the volume 1.

The draw (`0x005AA4C0`), after the box, while shown: `[0x0075F840]` 1; a movie of another name than the one playing
(`[0x0075DBD8]`, case aside) is opened unless the frame holds the movies (`[0x0075F064]`, then nothing more is done):
`0x005AA3A0` gives the movie back and, unless the name is "(null)", takes the player of the name (`0x0055A9B0`: one
kept by the texture manager, else a new one, played once unless looped) for the element's rectangle (its place
`+0xA4`, `+0xA8` and size `+0x94`, `+0x98` on the screen, not where a transition or InactivePos moves it); a movie
named "frontend" sets `+0x104`. The player exists also for a file that is not there, and then draws nothing. The
draw sets the player's volume (`+0x114`); while a critical or network message is up (the window manager's `+0x34`,
`+0x3C`) the movie is muted and after 4 draws held and started again (`0x00559A60`) at each draw; once none is up and
no message of the front end's own (`+0x44` with `+0x108`) either, the volume comes back and the hold goes. Unless the
frame holds the movies the player steps; it is drawn and its fast flag becomes `+0x104`. The players
(`25-movies.md`): `+0x24` holds one (paused, and at the end of a movie played once); `+0x08` of the vtable
(`0x0055A530`) tells it has finished: at its last frame, or without a Bink.

## Menu

Fields: items `+0x11C`, selected `+0x13C`, item height `+0x140`, highlight item height `+0x144`, position `+0x148`
(follows the selection), widest item `+0x150`, align `+0x158`, text x offset `+0x15C`, centred `+0x160`, type
`+0x164`, clip `+0x168`, unselected offset `+0x174`, active `+0x178`, reveal `+0x17C`, first shown `+0x1A8`,
scrolled `+0x1B0`.

An item (0x58 bytes, `0x005B15D0`): enabled `+0` (1), text `+4`, help `+0x14`, value `+0x24`, command `+0x28`,
texture `+0x48`, highlight texture `+0x4C`, colour `+0x50` (0xFFFFFFFF).

A menu keeps two lists of item pointers: the items shown (`+0x11C`) and every item added at the end (`+0x12C`).
`0x005AF230` (a text item; AddItem) and `0x005AF480` (a textured item; AddTexturedItem) add an item: at the end
(place -1) to both lists, at a place to the shown ones only (`0x005B1420`). Each widens `+0x150`: a text item with a
text in the HighFont (`+0x108`, else the Font `+0x104`) to its width + 20 when wider; a textured item with a text and
a font to its width when wider, else to the item height x the screen height (`[+0x30]+0x58 x [+0x30]+0x50 x
+0x140`). `0x005AFAD0` (by name) and `0x005AFAA0` (by place) take an item out of the shown list only. `0x005AECF0`
gives the shown list every item added at the end again (the items put at a place freed; what the code changed in the
others stays) and starts the items' coming in again (`+0x17C` 0, `+0x180` 1). `0x005AEEF0` empties both lists (the
items added at the end freed) and sets the highlight `+0x13C`, the widest `+0x150` and the first shown `+0x1A8` to 0;
the coming in stays as it was. So a menu of a kept window (02-frontend.md) whose enter adds at the end and gives the
items back grows at each visit: opts_menu's BackMenu gets two more orange bars each time (only the highlighted one
has a picture). `0x005AF8A0` selects an item (an index past the end selects 0) and, unless asked to glide, puts the
position there.
An item's help is the window's text of AddItem's fourth word (AddTexturedItem's sixth; none for -1).

`HelpText x y w h <font> a r g b` (`0x005AD8A0`, `0x005AF6A0`) gives the menu a help ticker (`+0x184`, named "help",
not in the window's list): its box takes the menu's corner colours and outline, its font the colour, the menu's
transition is made again for it without its loop (`0x005B2970`). Selecting an item (`0x005AF8A0`, `0x005AF920`), a
change of the selection in the update and the windows' enter for the menu named Menu (`0x005AF860`) set the ticker's
text to the item's help (`0x005B5240`: the effect held) and measure it. The draw hides it while the menu's
transition goes out and draws it after the menu (`0x005B1172`).

Update (`0x005B11A0`, active and shown menus): the items that fit = height / (item height x screen height + 3)
- 1 cut toward zero, at least 1. VERTICAL: UP selects the previous enabled item, DOWN the next (no wrap); going above the first
shown item makes it the first shown, going past the first shown + the items that fit makes the first shown the
selection - that number; either sets "scrolled". HORIZONTAL: LEFT / RIGHT one item, no wrap. A change plays SELECT.

Draw (`0x005AFB80`): the box; with ClipMenu drawing is cut to the box. The position moves toward the selection by
(selection - position) x frame time x 10; the reveal rises by 8 x frame time up to the item count. Per item, with
the selected item in HighFont and its colour, the others in Font:

- x: LEFT: box left + 10; RIGHT: box right - 10 - text width - item count x TextXOffset; CENTRE: box centre - text
  width / 2. y: the box's middle.
- VERTICAL and OptionCentered: y += (index - position) x (item height x screen height + 2); items other than the
  selected one move by UnselectedOffset in x, and those before / after it by highlight item height x screen height
  + 2 up / down.
- VERTICAL list: step = item height x screen height + 3; y = top - first shown x step (and - (position - selected) x
  step while "scrolled") + index x step + step / 2; while reveal - index < 1 the item is (1 - (reveal - index)) x
  the widest item to the left.
- HORIZONTAL and OptionCentered: x += (index - position) x step, step = widest x 1.2 when the widest item is wider
  than item height x screen width, else that + 10; other items move by UnselectedOffset in y.
- HORIZONTAL: x -= item count / 2 x item height x screen width, then + index x (item height x screen width + 3) +
  item height / 2 x screen width.
- The transition's offsets move the item; FADEIN and GROW set the colours' alpha.
- Pictures: VERTICAL across the box, the item height high around y; HORIZONTAL down the box, the item height x
  screen width wide around x. The selected item draws its highlight texture (else its texture), inset by
  |selected - position| x 15% on each side; in the window `driver` of languages 5 .. 7 a highlight texture holding
  `ui_pan5_` is lifted by 4 pixels. Other items draw their texture inset by 15%, moved by (1 - |selected - position|)
  x UnselectedOffset (VERTICAL in x, HORIZONTAL in y), at no more than half alpha.
- The text: baseline y + 0.3 x the line height; Shadowed: a black copy at +3, +3; a disabled item is drawn in
  0xFF294552.

The menu's text modifier (`+0x10C`, 0x2C bytes from the constructor `0x005AE8D0`: type 2 BOUNCE, start `+0x04` and
place `+0x08` -8, growth `+0x0C` 10, speed `+0x10` 30, reach `+0x14` 8, forward `+0x18` 1, spread `+0x1C` 0, alpha
`+0x20` 0, time `+0x24` 1, pulse `+0x28` 1). For the selected item the draw steps it (`0x005B14C0`, at the item's turn,
with the window manager's frame time held within 0 .. 0.1 and the text's length): the time runs down by the frame
time and the pulse by 3 x it (back to 1 below -1); the time below 0 goes back to 1.5 and the spread to 0; below 1
the spread is (1 - time²) x the length x 7 and the alpha the time x 0.5; the place moves by the speed backward or
forward, a BOUNCE turning past the reach before the text or after its end. A change of the selection in the update
starts it again (`0x005B14A0`: place = start, forward, spread and alpha 0, time 1.5; also `+0x170` 1 and `+0x118` 1).
The selected item's shadow is drawn with the modifier; then the modifier's start and place are set to 0, its growth
to (1 - time) x 10 and its reach to 200 and two copies of the text go the spread to the right and to the left, their
alpha no more than the modifier's alpha; then the text itself at the least of its alpha and |pulse| + 0.25 (within
0.25 .. 1): the selected item blinks. A window whose list must not blink deletes the menu's modifier (`0x005AF170`, or
the same inline: `+0x10C` freed and 0): instant_manu's CarMenu, viewhiscores' M_TRACK and M_COL1 .. M_COL4,
online_arcade_create's CARMENU, the online statistics' lists; its selected item is then drawn in HighFont, steady.
The font's draw with a modifier (`0x0051E740`) counts the characters (the spaces too) from 0: a glyph nearer the
modifier's place than its reach grows by the growth x the nearness (1 - distance / reach) on each side and takes the
alpha 1 - the nearness (at least 0, no more than the colour's).
`+0x118` (1, down by the frame time, x 12 while a switch waits, back to 1 below 0.2) only becomes the alpha of the
selected picture's colour after that picture is drawn.

AutoArrows (`0x005B0D87`, after the items and the end of the clip, before the help ticker), on a HORIZONTAL menu
whose widest item is not 0: the arrows' distance from the menu's middle (`+0x198`) moves toward its target (`+0x19C`)
by (target - distance) x the front end's frame time x 10; the target is (the selected item's text width in the
HighFont, else the Font, + 15) x 0.5 + 25, or the widest item without a text or a font. The arrows are the menu's
`ui_arrow_left` and `ui_arrow_right` (`+0x188`, `+0x18C`, loaded by the constructor), half their size (`+0x194`, 24)
wide and the size high around the menu's middle in height: the left one from the distance left of the middle, the
right one ending the distance right of it; the left one only past the first item, the right one only before the
last; each a cell of a 4 x 2 sheet at 12 a second (as Animated) drawn adding, in the last item's colour with alpha
255, no more than a fading transition's.
