#pragma once
// CMSOnlineCrewRaceSetup: the online career's crew race's drivers.

#include "Game/Frontend/Network/NetworkHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record of CMSOnlineCrewRaceSetup (0x00722620, vtable 0x006F5D4C).
class OnlineCrewRaceSetup {
public:
    // 0x004D6090
    void enter(NetworkHost& host, ui::Screen& s);
    // 0x004D6550
    void update(NetworkHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x004D6930: a full team "NO"; else the member in the first free slot (+0x18 up) and its selection<n> shown.
    void take(NetworkHost& host, ui::Screen& s, int32_t member);

    std::array<int32_t, 3> m_members{-1, -1, -1};  // +0x0C: the member slots chosen
    int32_t m_chosen = 0;                          // +0x18
};

}  // namespace oj::frontend
