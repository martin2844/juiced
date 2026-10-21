#pragma once
// opts_voice: the voice chat's devices.

#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>
#include <vector>

namespace oj::frontend {

// opts_voice (object 0x0076890C, made by the static initialiser 0x006BEF10 through 0x0047FEF0;
// vtable 0x006F3678), kept while the game runs.
class VoiceOptions {
public:
    // 0x00484700
    void enter(OptionsHost& host, ui::Screen& s);
    // 0x00484830
    void update(OptionsHost& host, ui::Screen& s, uint32_t event);
    // 0x004847F0
    void leave();

private:
    // 0x004842B0: the names chosen kept ("none" for -1); with the voice object the capture devices listed again into
    // +0x10 and the playback ones into +0x20; +0x30 and +0x34 the kept names' devices again (-1 for "none" or a device
    // gone); INPUT and OUTPUT written as 0x00484250 does.
    void makeLists(OptionsHost& host, ui::Screen& s);
    // 0x00484250: INPUT "None" (164) without a device, else its name (0x005AA1F0 through 0x0049A850); OUTPUT the same.
    void showTexts(OptionsHost& host, ui::Screen& s);

    int32_t m_madeVoice = 0;              // +0x0C: the enter made the voice object
    std::vector<std::string> m_inputs;    // +0x10: the capture devices' names (0x40 bytes each)
    std::vector<std::string> m_outputs;   // +0x20: the playback devices'
    int32_t m_input = -1;                 // +0x30: the input chosen (-1 none)
    int32_t m_output = -1;                // +0x34: the output chosen
    float m_relist = 0.0f;                // +0x38: the time since the lists were made
};

}  // namespace oj::frontend
