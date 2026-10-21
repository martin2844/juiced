#pragma once
// The driver window (driver): the drivers of the save device listed, one read, one deleted, a new one started.

#include "Game/Frontend/Saves/SaveHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record of vtable 0x006F25E0.
class Driver {
public:
    // 0x004678C0
    void enter(SaveHost& host, ui::Screen& s);
    // 0x004679B0
    void update(SaveHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x00467800: the state cleared and the list asked for again.
    void clearList(SaveHost& host, ui::Screen& s);
    // 0x00467620: MENU filled with the drivers listed.
    void fillList(SaveHost& host, ui::Screen& s);

    int32_t m_filled = 0;               // +0x14: the menu filled
    int32_t m_refill = 1;               // +0x18: the list to be filled again
    int32_t m_loaded = 0;               // +0x1C: a driver read (to the menu once the device is idle)
    int32_t m_busy = 0;                 // +0x24: a question or a delete
    int32_t m_ready = 0;                // +0x28: the list ready for SELECT
    int32_t m_leaving = 0;              // +0x2C
    int32_t m_note = 0;                 // +0x30 ([0x0075EE64]): the note of the device's state 2 shown
    // The numbers of the messages shown (+0x34 .. +0x48; out of reach by the enter).
    int32_t m_deleteAsked = 0x98967F;   // +0x34: the delete question
    int32_t m_loading = 0x98967E;       // +0x38: loading (shown by nothing: its number stays)
    int32_t m_checking = 0x98967C;      // +0x3C
    int32_t m_deleting = 0x98967D;      // +0x40
    int32_t m_current = 0;              // +0x44: the message up
    int32_t m_stateNote = 0x98967B;     // +0x48: the note of the device's state
    uint32_t m_count = 0;               // +0x4C: the drivers listed
    float m_wait = 0.0f;                // +0x50: a wait before the list (s)
    float m_afterFailure = 0.0f;        // +0x54: 0.5 after a failure
};

}  // namespace oj::frontend
