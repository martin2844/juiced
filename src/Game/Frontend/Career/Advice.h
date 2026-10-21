#pragma once
// CMSAdvice: the advisor's help over a career window.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window keeps no state of its own.
class Advice {
public:
    // 0x004BFB60
    void enter(CareerHost& host, ui::Screen& screen);
    // 0x004C0110
    void update(CareerHost& host, ui::Screen& screen, uint32_t event);
};

}  // namespace oj::frontend
