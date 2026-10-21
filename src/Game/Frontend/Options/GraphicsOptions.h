#pragma once
// The graphics options (opts_graphics).

#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>

namespace oj::frontend {

// The window of the record 0x00721BC8 (vtable 0x006F36A8): the graphics settings' values at enter, for BACK.
class GraphicsOptions {
public:
    // 0x00481F00
    void enter(OptionsHost& host, ui::Screen& s);
    // 0x004820A0
    void update(OptionsHost& host, ui::Screen& s, uint32_t event);

private:
    // Leaving with DONE or BACK: the frame's renderer takes the detail factor ([0x0079E670]).
    void leave(OptionsHost& host, float detail);

    std::string m_shownButton;   // +0x0C: the highlighted button the texts were written for
    float m_brightness = 0.0f;   // +0x2C
    float m_detail = 0.0f;       // +0x30
    int32_t m_reflections = 0;   // +0x34
    int32_t m_shadows = 0;       // +0x38: on
    int32_t m_motionBlur = 0;    // +0x3C
    int32_t m_glow = 0;          // +0x40
    int32_t m_depthOfField = 0;  // +0x44
};

}  // namespace oj::frontend
