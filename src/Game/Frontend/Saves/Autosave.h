#pragma once
// autosave: the driver written through the save device over a window.

#include "Game/Frontend/Saves/SaveHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x007212D0.
class Autosave {
public:
    // 0x004648C0
    void enter(SaveHost& host, ui::Screen& s);
    // 0x00464A10
    void update(SaveHost& host, ui::Screen& s);
    // 0x004649D0
    void leave(SaveHost& host);

private:
    // 0x00464830: the device asked to write again.
    void again(SaveHost& host);

    int32_t m_answer = -1;      // +0x0C: the "continue without saving?" answer
    int32_t m_ps2Answer = -1;   // +0x10: the PS2 question's answer
    int32_t m_ended = 0;        // +0x14: the write ended without success
    int32_t m_writeAgain = 0;   // +0x1C
    int32_t m_done = 0;         // +0x20
    int32_t m_left = 0;         // +0x24
    int32_t m_turnedOn = 0;     // +0x28: the saving turned on by the window
    int32_t m_current = 0;      // +0x34: the message up
    int32_t m_checkStateFlag = 0;  // +0x2C: the first step's check of the device's state flag 3
    int32_t m_checkFreeSpace = 0;  // +0x30: the first step's check of the free size under the size needed
    // The numbers of the messages shown (out of reach by the enter and again).
    int32_t m_questionAsked = 0;   // +0x38: the format, overwrite and continue questions
    int32_t m_writingNote = 0;     // +0x40: the note of flag 10
    int32_t m_deviceNote = 0;      // +0x44: the note of a flag of kind 0
    int32_t m_errorNote = 0;       // +0x4C: "Save failed! Data write error" (flag 16)
    int32_t m_ps2Asked = 0;        // +0x50: the PS2 question
    std::array<int32_t, 4> m_unusedNotes{};  // +0x3C, +0x48, +0x54, +0x58: set, read by nothing
    uint32_t m_frames = 0;      // +0x5C: a frame count
    float m_closeTime = 1.0f;   // +0x60: the time before the window closes
};

}  // namespace oj::frontend
