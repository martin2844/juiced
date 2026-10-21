#pragma once
// The front end's message windows.

#include "Game/UI/MessageWindow.h"
#include "Game/UI/UiScreen.h"

#include <memory>

namespace oj::frontend {

// A message window of the front end's manager (Game/UI/MessageWindow.h): msgbox (0x005A80E0, its state 0x00722FAC;
// the manager's +0x44), msgbox_network (0x005A7F90, 0x00723004; +0x3C) or msgbox_critical (0x005A8030, 0x00722FD8;
// +0x34).
struct Message {
    std::shared_ptr<ui::Screen> screen;
    ui::MessageState state;  // the state's record: the type, the answer, the start on "no"
    bool modal = false;      // msgbox: the manager's +0x4C (types 1 and 3), the windows under it wait
    bool close = false;      // closed at the next frame: msgbox's +0x48, msgbox_network's +0x40
};

}  // namespace oj::frontend
