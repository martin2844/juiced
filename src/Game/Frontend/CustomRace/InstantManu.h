#pragma once
// The custom race's window instant_manu (docs/spec/02-frontend.md, "The custom race").

#include "Game/Frontend/CustomRace/CustomRaceHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record of instant_manu (0x00720C68, vtable 0x006F19E4).
class InstantManu {
public:
    // 0x0045E490
    void enter(CustomRaceHost& host, ui::Screen& s);
    // 0x0045E790
    void update(CustomRaceHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_shown = -1;  // +0x10: the group CarMenu lists
};

}  // namespace oj::frontend
