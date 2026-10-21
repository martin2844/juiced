#pragma once
// The routes' records (viewhiscores).

#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721620 (vtable 0x006F25C0).
class HiScores {
public:
    // 0x0046A300
    void enter(WindowHost& host, ui::Screen& s);
    // 0x0046A520
    void update(WindowHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_track = 0;  // +0x20: the track shown
    int32_t m_built = 0;  // +0x24: the lists made for it
};

}  // namespace oj::frontend
