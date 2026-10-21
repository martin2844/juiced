#pragma once
// The tutorial movies (vid_tutorial).

#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window keeps no state of its own: MOVIE (+0x10) and MENU (+0x0C) are its elements.
class VidTutorial {
public:
    // 0x0046A040
    void enter(WindowHost& host, ui::Screen& s);
    // 0x0046A0D0
    void update(WindowHost& host, ui::Screen& s, uint32_t event);
};

}  // namespace oj::frontend
