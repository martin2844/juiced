// The graphics options (opts_graphics).

#include "Game/Frontend/Options/GraphicsOptions.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Game/UI/WindowFixes.h"

#include <cstring>
#include <utility>

namespace oj::frontend {

// 0x00481F00: BRIGHT, DETAIL, SHADOWS, REFLECTION, MOBLUR, GLOW, DOF (+0x10 ..) found; the meters DETAIL and BRIGHT
// sliders (+0xFC) at the detail ([0x0079E650]) and the brightness ([0x0079E660]), value and shown value; kept for
// BACK: the brightness, the detail, the reflections, the shadows on (2; none without shaders), the motion blur, glow
// and depth of field (none without shaders or with no shaders on the adapter); RESET_BUT "RESET TO DEFAULTS" (1609);
// +0x0C 0.
void GraphicsOptions::enter(OptionsHost& host, ui::Screen& s) {
    const GraphicsConfig& c = graphicsConfig();
    for (const auto& [meter, value] : {std::pair{"DETAIL", c.detail}, std::pair{"BRIGHT", c.brightness}}) {
        if (s.find(meter) < 0) continue;
        s.setMeterSlider(meter, true);
        s.setMeter(meter, value, true);
    }
    const bool effects = c.noShaders == 0 && c.shaders != 0;
    // The fixed build's glow meter (Game/UI/WindowFixes.h): the glow's strength, 0 without the effects.
    if (s.find(ui::kGlowMeter) >= 0) {
        s.setMeterSlider(ui::kGlowMeter, true);
        s.setMeter(ui::kGlowMeter, effects ? glowStrength(c.glow) : 0.0f, true);
    }
    m_brightness = c.brightness;
    m_detail = c.detail;
    m_reflections = c.reflections;
    m_shadows = c.noShaders == 0 && c.shadows == 2 ? 1 : 0;
    m_motionBlur = effects ? c.motionBlur : 0;
    m_glow = effects ? c.glow : 0;
    m_depthOfField = effects ? c.depthOfField : 0;
    s.setText("RESET_BUT", host.gameTexts().at(kTextResetDefaults));
    m_shownButton.clear();
}

void GraphicsOptions::leave(OptionsHost& host, float detail) {
    GraphicsConfig& c = graphicsConfig();
    c.detail = detail;
    c.lodBias = detailFactor(detail);
    host.setDetailFactor(c.lodBias);
    host.sound("CLICK");
    // From a race's pause ([game + 0x3C]) opts_ingame_menu.
    host.request(host.optionsMenu(), SwitchMode::Swap);
}

// 0x004820A0. A new highlighted button (+0x0C): REFLECTION "OFF", "LOW", "MED", "HI" (1486 ..) by the reflections
// (another value "OFF"), SHADOWS "YES" for 2, else "NO", MOBLUR, GLOW, DOF "YES" or "NO" by theirs; "-" for each
// without shaders (and the three effects with no shaders on the adapter). The meters DETAIL and BRIGHT take the held
// direction while their buttons are highlighted (+0xF8). On BRIGHT_BUT the brightness follows BRIGHT's shown value.
// BACK: the kept values back (the reflections and the effects 0 without shaders, the shadows 2 or 1), the detail factor
// from the detail, CLICK, opts_menu (mode 7). SELECT on DONE_BUT: the detail DETAIL's shown value and the factor from
// it, the brightness BRIGHT's, CLICK, opts_menu; on RESET_BUT: CLICK, the adapter's defaults (0x005A26C0), the meters
// at the detail and the brightness, +0x0C 0. LEFT or RIGHT: SHADOWS_BUT turns the shadows over (1 or 2), MOBLUR_BUT,
// GLOW_BUT, DOF_BUT their effect (stays off without shaders), REFLECTION_BUT the reflections one down (LEFT) or up
// (RIGHT) round 0 .. 3 (0 without shaders); each CLICK and +0x0C 0.
// The fixed build's glow meter in place of GLOW's YES and NO (Game/UI/WindowFixes.h): GLOW empty, the meter and its
// boxes shown with the effects (else hidden and GLOW "-"), taking the held direction while GLOW_BUT is highlighted,
// the glow following its shown value (glowSetting); RESET puts it at the defaults' glow.
void GraphicsOptions::update(OptionsHost& host, ui::Screen& s, uint32_t event) {
    GraphicsConfig& c = graphicsConfig();
    const std::string current = s.currentButton();
    const auto is1 = [&](const char* name) { return _stricmp(current.c_str(), name) == 0; };
    const bool shaderless = c.noShaders != 0;
    const bool effects = c.noShaders == 0 && c.shaders != 0;
    const bool glowMeter = s.find(ui::kGlowMeter) >= 0;
    if (m_shownButton != current) {
        m_shownButton = current;
        const auto yesNo = [&](bool on) { return host.gameTexts().at(on ? kTextYes : kTextNo); };
        if (shaderless) {
            s.setText("REFLECTION", L"-");
            s.setText("SHADOWS", L"-");
        } else {
            const size_t level = c.reflections >= 1 && c.reflections <= 3 ? static_cast<size_t>(c.reflections) : 0;
            s.setText("REFLECTION", host.gameTexts().at(kTextOffLowMedHi + level));
            s.setText("SHADOWS", yesNo(c.shadows == 2));
        }
        if (effects) {
            s.setText("MOBLUR", yesNo(c.motionBlur != 0));
            s.setText("GLOW", glowMeter ? std::wstring() : yesNo(c.glow != 0));
            s.setText("DOF", yesNo(c.depthOfField != 0));
        } else {
            for (const char* name : {"MOBLUR", "GLOW", "DOF"}) s.setText(name, L"-");
        }
        s.setMeterActive("DETAIL", is1("DETAIL_BUT"));
        s.setMeterActive("BRIGHT", is1("BRIGHT_BUT"));
        for (const char* name : {ui::kGlowMeter, ui::kGlowMinus, ui::kGlowPlus}) s.setHidden(name, !effects);
        s.setMeterActive(ui::kGlowMeter, effects && is1("GLOW_BUT"));
    }
    if (s.find("BRIGHT") >= 0 && is1("BRIGHT_BUT")) c.brightness = s.meterShown("BRIGHT");
    if (glowMeter && effects && is1("GLOW_BUT")) c.glow = glowSetting(s.meterShown(ui::kGlowMeter));
    if (event == input::kBack) {
        c.brightness = m_brightness;
        c.reflections = shaderless ? 0 : m_reflections;
        c.shadows = (shaderless ? 0 : m_shadows) != 0 ? 2 : 1;
        c.motionBlur = effects ? m_motionBlur : 0;
        c.glow = effects ? m_glow : 0;
        c.depthOfField = effects ? m_depthOfField : 0;
        leave(host, m_detail);
        return;
    }
    if (event == input::kSelect) {
        if (is1("DONE_BUT")) {
            const float detail = s.meterShown("DETAIL");
            c.brightness = s.meterShown("BRIGHT");
            leave(host, detail);
        } else if (is1("RESET_BUT")) {
            host.sound("CLICK");
            graphicsDefaults(c);
            if (s.find("DETAIL") >= 0) s.setMeter("DETAIL", c.detail, true);
            if (s.find("BRIGHT") >= 0) s.setMeter("BRIGHT", c.brightness, true);
            if (glowMeter) s.setMeter(ui::kGlowMeter, effects ? glowStrength(c.glow) : 0.0f, true);
            m_shownButton.clear();
        }
        return;
    }
    if (event != input::kLeft && event != input::kRight) return;
    const auto changed = [&] {
        host.sound("CLICK");
        m_shownButton.clear();
    };
    if (is1("SHADOWS_BUT")) {
        const bool on = !shaderless && c.shadows == 2;
        c.shadows = !on && !shaderless ? 2 : 1;
        changed();
    } else if (is1("MOBLUR_BUT") || (is1("GLOW_BUT") && !glowMeter) || is1("DOF_BUT")) {
        int32_t& value = is1("MOBLUR_BUT") ? c.motionBlur : is1("GLOW_BUT") ? c.glow : c.depthOfField;
        const bool on = effects && value != 0;
        value = !on && effects ? 1 : 0;
        changed();
    } else if (is1("REFLECTION_BUT")) {
        int32_t r = c.reflections + (event == input::kLeft ? -1 : 1);
        if (r < 0) {
            r = 3;
        } else if (r > 3) {
            r = 0;
        }
        c.reflections = shaderless ? 0 : r;
        changed();
    }
}

}  // namespace oj::frontend
