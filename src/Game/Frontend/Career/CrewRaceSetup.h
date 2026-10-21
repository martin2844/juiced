#pragma once
// The crew race's setup (CMSCrewRaceSetup).

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00722688 (vtable 0x006F5DCC).
class CrewRaceSetup {
public:
    // 0x004D8AC0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004D90F0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_respectAnswer = 0;  // +0x0C
    int32_t m_leaving = 0;        // +0x10
    int32_t m_leaveAnswer = 0;    // +0x14
    std::array<int32_t, 3> m_members{-1, -1, -1};  // +0x18: the member slots chosen
    int32_t m_chosen = 0;         // +0x24
};

}  // namespace oj::frontend
