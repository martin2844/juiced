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
    // DONE: the detail and the brightness of their meters kept, then leave.
    void done(OptionsHost& host, ui::Screen& s);
    // The fixed build's ADVANCED page of the effects (Game/UI/WindowFixes' MORE_BUT and FX rows): the page's elements
    // shown or the stock's, MORE_BUT's text and the neighbours of MORE_BUT and DONE_BUT for it.
    void setPage(OptionsHost& host, ui::Screen& s, bool effects);
    // The page turned to: the meters at the values, the highlight on its first row or on MORE_BUT; leaving the
    // ADVANCED page, the programs made again when the shaders' settings changed on it.
    void showPage(OptionsHost& host, ui::Screen& s, bool effects);
    void showEffects(OptionsHost& host, ui::Screen& s);
    void updateEffects(OptionsHost& host, ui::Screen& s, uint32_t event);
    // The effects `e` in effect (OptionsHost::applyEffects), the programs made again when they were made with others.
    void applyEffects(OptionsHost& host, const FixedEffects& e);

    std::string m_shownButton;   // +0x0C: the highlighted button the texts were written for
    float m_brightness = 0.0f;   // +0x2C
    float m_detail = 0.0f;       // +0x30
    int32_t m_reflections = 0;   // +0x34
    int32_t m_motionBlur = 0;    // +0x3C
    int32_t m_glow = 0;          // +0x40
    FixedEffects m_effects;      // the fixed build's (GraphicsConfig::effects) at enter
    FixedEffects m_madeWith;     // those the programs were made with last
    render::QualityCaps m_caps;  // the anti-aliasing and texture filter the adapter offers
    bool m_effectsPage = false;
    bool m_paged = false;        // the window has the ADVANCED page (MORE_BUT)
};

}  // namespace oj::frontend
