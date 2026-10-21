#pragma once
// CMSChallengeResults: a challenge's prize counted.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x007226D0 (vtable 0x006F5E24).
class CareerChallengeResults {
public:
    // 0x004D9960
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004D9FB0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    float m_won = 0.0f;       // +0x0C: the prize left to count
    float m_step = 0.0f;      // +0x10
    float m_after = 0.0f;     // +0x14: the money after
    float m_shown = 0.0f;     // +0x18
    float m_delay = 0.0f;     // +0x1C
    int32_t m_finished = 0;   // +0x20
    int32_t m_autosave = 0;   // +0x24
};

}  // namespace oj::frontend
