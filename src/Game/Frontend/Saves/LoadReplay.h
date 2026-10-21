#pragma once
// loadreplay: the replays of the save device listed, one read or deleted (no way leads to it on the PC: extras takes
// its replay item out).

#include "Game/Frontend/Saves/SaveHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x007215D0.
class LoadReplay {
public:
    // 0x004694B0
    void enter(SaveHost& host, ui::Screen& s);
    // 0x00469530
    void update(SaveHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x004693A0: the record cleared and the list asked for again.
    void clear(SaveHost& host, ui::Screen& s);
    // 0x00468FC0: MENU filled with the replays listed.
    void fill(SaveHost& host, ui::Screen& s);
    // 0x004690E0: a replay's details.
    void showInfo(SaveHost& host, ui::Screen& s, const save::ReplaySlot& replay);

    int32_t m_cleared = 1;        // +0x0C: 1 by the clear, 0 by the fill (read by nothing)
    int32_t m_refill = 1;         // +0x10: the menu to be filled
    int32_t m_loaded = 0;         // +0x14: a replay read
    // The numbers of the messages shown (+0x18 .. +0x28).
    int32_t m_loading = 999999;
    int32_t m_checking = 999998;
    int32_t m_deleteAsked = 999997;
    int32_t m_deleting = 999996;
    int32_t m_networkNote = 999995;
    int32_t m_busy = 0;           // +0x2C
    int32_t m_ready = 0;          // +0x30: the list ready
    int32_t m_leaving = 0;        // +0x34
    int32_t m_noteShown = 0;      // +0x38: the network's note shown
    int32_t m_infoHidden = 1;     // +0x3C: the INFO_ elements to be hidden
    int32_t m_infoState = 1;      // +0x40: as they are
    int32_t m_shown = -1;         // +0x44: the replay shown
    float m_wait = 0.0f;          // +0x48: the checking's least time
    float m_afterFailure = 0.0f;  // +0x4C: 0.5 after a failure (read by nothing)
};

}  // namespace oj::frontend
