#pragma once
// The cheats' letters (CMSCheats).

#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00722070 (vtable 0x006F53C4).
class Cheats {
public:
    // 0x004C54D0
    void enter(WindowHost& host, ui::Screen& s);
    // 0x004C55B0
    void update(WindowHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x004C5CB0: the letters shown and a code taken on SELECT.
    void letters(WindowHost& host, ui::Screen& s, uint32_t event);

    std::array<int, 4> m_letterElements{-1, -1, -1, -1};  // +0x0C: L1 .. L4
    std::array<char, 4> m_letters{'A', 'A', 'A', 'A'};     // +0x1C
    int32_t m_leaving = 0;                                // +0x20: a cheat taken, back to extras
};

}  // namespace oj::frontend
