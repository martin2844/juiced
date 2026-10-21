#pragma once
// loadhighscores: the routes' records read through the records' device over the main menu.

#include "Game/Frontend/Saves/SaveHost.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721398.
class LoadHighScores {
public:
    // 0x00465A90
    void enter(SaveHost& host);
    // 0x00465B00
    void update(SaveHost& host);
    // 0x00465E20 (loadhighscores' and savehighscores' exit)
    void leave(SaveHost& host);

private:
    int32_t m_read = 0;      // +0x0C
    int32_t m_closing = 0;   // +0x10
    int32_t m_current = 0;   // +0x20: the message up
    float m_wait = -1.0f;    // +0x28: the wait to close
};

}  // namespace oj::frontend
