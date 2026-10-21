#pragma once
// The custom race's window instant_seltype (docs/spec/02-frontend.md, "The custom race").

#include "Game/Frontend/CustomRace/CustomRaceHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// instant_seltype; the window keeps no state of its own.
class InstantSelType {
public:
    // 0x0045B0B0
    void enter(CustomRaceHost& host, ui::Screen& s);
    // 0x0045B390
    void update(CustomRaceHost& host, ui::Screen& s, uint32_t event);
};

}  // namespace oj::frontend
