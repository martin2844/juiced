#pragma once
// saveprofile: a new driver written through the save device.

#include "Game/Frontend/Saves/SaveHost.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721260.
class SaveProfile {
public:
    // 0x00463E40
    void enter(SaveHost& host);
    // 0x00463F30
    void update(SaveHost& host);
    // 0x00463F00
    void leave(SaveHost& host);

private:
    // 0x00463DE0: the device asked to write again.
    void again(SaveHost& host);

    int32_t m_answer = -1;      // +0x0C: the "continue without saving?" answer
    int32_t m_ps2Answer = -1;   // +0x10: the PS2 question's answer
    int32_t m_ended = 0;        // +0x14: the write ended without success
    int32_t m_writeAgain = 0;   // +0x1C
    int32_t m_done = 0;         // +0x20
    int32_t m_left = 0;         // +0x24
    int32_t m_turnedOn = 0;     // +0x28: the saving turned on by the window
    int32_t m_current = 0;      // +0x2C: the message up
    // The numbers of the messages shown (out of reach by the enter).
    int32_t m_continueAsked = 0;   // +0x30: "continue without saving?"
    int32_t m_formatAsked = 0;     // +0x34: the format question (flag 6)
    int32_t m_overwriteAsked = 0;  // +0x38: the questions of flags 7 and 20
    int32_t m_writingNote = 0;     // +0x40: the note of flag 10
    int32_t m_deviceNote = 0;      // +0x44: the note of a flag of kind 0
    int32_t m_waitNote = 0;        // +0x48: "Please wait..."
    int32_t m_spaceNote = 0;       // +0x4C: the space's note (flag 4)
    int32_t m_errorNote = 0;       // +0x50: "Save failed! Data write error" (flag 16)
    int32_t m_ps2Asked = 0;        // +0x54: the PS2 question
    std::array<int32_t, 2> m_unusedNotes{};  // +0x3C, +0x58: set by the enter, read by nothing
    uint32_t m_frames = 0;      // +0x5C: a frame count
    float m_closeTime = 1.0f;   // +0x60: 0 by the enter, read by nothing here (autosave's close)
    int32_t m_career = 0;       // +0x68: the career to make
};

}  // namespace oj::frontend
