#pragma once
// CMSStatsMenu: the career's stats' menu.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window keeps no state of its own.
class StatsMenu {
public:
    // 0x004E0700
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004E0760
    void update(CareerHost& host, ui::Screen& s, uint32_t event);
};

}  // namespace oj::frontend
