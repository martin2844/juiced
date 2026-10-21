#pragma once
// The audio options (opts_audio; from a race's pause opts_ingame_audio).

#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>

namespace oj::frontend {

// The window of the record 0x00721A90 (vtable 0x006F3698).
class AudioOptions {
public:
    // 0x00480960
    void enter(OptionsHost& host, ui::Screen& s);
    // 0x00480AD0: each player's event in turn (none with the race's players at 0).
    void update(OptionsHost& host, ui::Screen& s, uint32_t event);

private:
    // One player's event; true when a window was asked for (the frame ends).
    bool updatePlayer(OptionsHost& host, ui::Screen& s, uint32_t event);

    std::string m_shownButton;  // +0x18: the highlighted button
    float m_sfxHeard = 0.0f;     // +0x4C: the effects' volume last heard ("Warning")
    float m_speechHeard = 0.0f;  // +0x50
    float m_musicHeard = 0.0f;   // +0x54
    uint32_t m_speaker = 0;      // +0x58: the speaker set
    float m_music = 0.0f;        // +0x60: the values at enter, for BACK
    float m_sfx = 0.0f;          // +0x5C
    float m_speech = 0.0f;       // +0x64
    uint32_t m_speakerKept = 0;  // +0x68
};

}  // namespace oj::frontend
