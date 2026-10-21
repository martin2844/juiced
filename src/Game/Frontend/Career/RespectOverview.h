#pragma once
// CMSRespectOverview and CMSRespectHelp: the crews' respect and its help.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00722770 (vtable 0x006F5F0C): +0x18, per crew
// 1 .. 8 and challenge, the player has a car for it (0x004BC1D0).
class RespectOverview {
public:
    // 0x004DC260
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004DC7A0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    std::array<bool, 24> m_cars{};
};

// The window keeps no state of its own.
class RespectHelp {
public:
    // 0x004DCEF0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004DD290
    void update(CareerHost& host, ui::Screen& s, uint32_t event);
};

}  // namespace oj::frontend
