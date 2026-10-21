#pragma once
// CMSNewCont: a new career or the one going on.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721DA0 (vtable 0x006F51A4).
class NewCont {
public:
    // 0x004C37E0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004C3910
    void update(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004C3830
    void leave(CareerHost& host);

private:
    int32_t m_leaving = 0;   // +0x0C: BACK to the main menu
    int32_t m_answer = -1;   // +0x10: "CURRENT GAME WILL BE LOST"
    int32_t m_asked = 0;     // +0x14
};

}  // namespace oj::frontend
