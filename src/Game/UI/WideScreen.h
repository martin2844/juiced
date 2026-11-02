#pragma once
// The fixed build's wide screens in the interface (Render/Display, docs/fixes.md Added): a menu's window is laid out
// across its zone (the screen's width, at most 16:9's) and down the picture's 4:3 frame, its texts in the font of the
// frame's size and its pictures (icons, logos, portraits, movies, dials) in their own shape, the frame's size, while
// its bars, panels and 3D views stretch across; its backgrounds reach the screen's edges (a full screen texture or
// movie scaled to cover it, cut at two edges, a splash or loading picture and a movie played once no wider than
// 16:9), and the race HUD's elements keep the frame's layout, held to the edges of its zone.

#include "Game/UI/UiFile.h"

#include <string>
#include <vector>

namespace oj::ui {

enum class Anchor { Left, Centre, Right };

// The race HUD's windows, their elements anchored: hud_race, hud_solo, hud_sprint, hud_cruise, hud_split and
// hud_spectator (the results, the pause and the replay's windows are laid out as the menus).
bool hudWindow(const std::string& window);
// The menus' windows laid out in the 4:3 frame as the HUD's (their pictures and texts one piece): hudcolourmenu (the
// HUD's dial and its texts), cmsmobilephone and cmsmobilephonemenu (the phone and the texts on its screen).
bool framedWindow(const std::string& window);
// An element of a HUD window: by the middle of its .ui box, left of 0.3 to the zone's left, right of 0.7 to its
// right, else the frame's centre; across the window's height the same on a picture narrower than 4:3. A few by name:
// hud_cruise's bonus panel (DIGITAL_BG_BONUS, REGIONMESSAGE, BONUS_TEXT) left, hud_spectator's RP_NEXTCARTEXT left
// with its button, the crew's orders' DPADCONTROL01 .. 03 (on the line, 0.306 .. 0.312) the centre with their texts.
Anchor hudAnchor(const std::string& window, const Element& e, bool down);
// A box of a full screen picture covering the screen no wider than 16:9 (cut above and below, black beyond it): the
// splashes (UI_SPLASH), the loading screens (ui_background_01, ui_background_02, UI_ARCADE_LOADING, garagedoor).
bool pictureElement(const std::string& window, const Element& e);
// An element of a menu keeping its shape (the frame's size, its middle where the layout puts it): a movie, a dial, a
// box with a picture (texture, its own or the one the code gave it) other than the bars, panels and patterns
// (ui_test_pan*, ui_topsweep, don_ui_*, the meters' tracks, ...), which stretch across with the layout.
bool keepsShape(const Element& e, const std::string& texture);
// Per element of a window, the picture keeping its shape that holds it (-1 none): a picture at most 0.6 of the window
// across and 0.8 down with the element's middle within its box and the element's box within it grown by 0.05 (the
// largest such; of two of a size the first): an event flyer and its texts and pictures, a crew's slot and its
// portrait, border, plate and name, a character's movie and the fade over it, a hint's icon and its arrows. The
// element goes with it (the frame's size, its place by the picture's middle).
std::vector<int> pictureHolders(const Window& window);
// Per element of a window, whether it heads a column: a right-aligned text across a left-aligned menu (the same place
// and width within 0.01: viewhiscores' CAR over the cars). It keeps the column's 4:3 width from the column's start, so
// it stays as far from the column's texts as on the stock's 4:3 screen.
std::vector<bool> columnHeads(const Window& window);

}  // namespace oj::ui
