#pragma once
// The race's results in the career (CMSRaceResults).

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x007226B0.
class CareerRaceResults {
public:
    // 0x004DA310
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004DAF80
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    float m_won = 0.0f;    // +0x0C: the race's money left to count
    float m_step = 0.0f;   // +0x10: 0.05 of it, counted each frame
    float m_after = 0.0f;  // +0x14: the money when counted
    float m_shown = 0.0f;  // +0x18: the money shown
    float m_delay = 0.0f;  // +0x1C: before the count
};

}  // namespace oj::frontend
