#pragma once
// DemoFlyby: the city's fly-by before a new career.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record of DemoFlyby.
class DemoFlyby {
public:
    // 0x0045B6B0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004351C0
    void leave(CareerHost& host);
    // 0x004352B0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    float m_fade = 0.0f;              // +0x10
    bool m_narrationStarted = false;  // +0x14
    bool m_fadeOut = false;           // +0x18: the narration ended, the box going black
    bool m_done = false;              // +0x1C
};

}  // namespace oj::frontend
