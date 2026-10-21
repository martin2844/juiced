#pragma once
// A Menu (0x005AE8D0): its items, the selection and how it shows them.

#include "Game/UI/Components/MenuItem.h"
#include "Game/UI/Components/TextModifier.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace oj::ui {

struct MenuState {
    bool active = true;  // +0x178
    // +0x1A0: with no event of the window's first player (+0x88) the second's (+0x8C) moves it (0x005B1211); set for
    // the message windows' MSG_MENU only (0x005A9196).
    bool secondPlayer = false;
    int align = -1;      // +0x158 set by the code (-1: the Align property's)
    bool clipped = false;  // +0x16C
    bool useModifier = true;  // +0x10C there
    int helpTicker = -1;  // a menu's HelpText ticker (+0x184): its element, after the window's own
    // Menus: the items shown (+0x11C) and every item added at the end (+0x12C); the same item may be in both.
    std::vector<std::shared_ptr<MenuItem>> items;
    std::vector<std::shared_ptr<MenuItem>> all;
    size_t selected = 0;      // +0x13C
    float position = 0.0f;    // +0x148, follows the selection
    int top = 0;              // +0x1A8, the first item shown of a scrolled list
    bool scrolled = false;    // +0x1B0
    float reveal = 0.0f;      // +0x17C, items slide in one after another
    float revealSound = 1.0f;  // +0x180: the next item whose coming in plays SWOOSH
    float widest = 0.0f;      // +0x150, the widest item text
    // AutoArrows (+0x190, 0x005AD880): the arrows' distance from the menu's middle (+0x198) going to +0x19C.
    float arrowGap = 0.0f;
    float arrowTarget = 0.0f;
};

}  // namespace oj::ui
