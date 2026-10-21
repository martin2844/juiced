#pragma once
// CMSCalendarHelp: the calendar's help over it.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window keeps no state of its own.
class CalendarHelp {
public:
    // 0x004C2980
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004C2A60
    void update(CareerHost& host, uint32_t event);
};

}  // namespace oj::frontend
