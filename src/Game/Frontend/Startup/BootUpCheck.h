#pragma once
// bootupcheck: the save device's start (no window asks for it).

#include "Game/Frontend/Saves/SaveHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721648 (vtable 0x006F25F0).
class BootUpCheck {
public:
    // 0x004673E0
    void enter(SaveHost& host, ui::Screen& s);
    // 0x004674F0
    void update(SaveHost& host);

private:
    int32_t m_toDriver = 0;  // +0x20: to the driver window
    int32_t m_answer = -1;   // +0x30: an answer (-1 none)
};

}  // namespace oj::frontend
