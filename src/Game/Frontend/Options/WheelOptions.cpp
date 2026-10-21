// The steering wheel's options (opts_wheel_dx9).

#include "Game/Frontend/Options/WheelOptions.h"

#include "Core/Input/Input.h"

#include <bit>
#include <cstring>
#include <utility>

namespace oj::frontend {

namespace {

void tick(ui::Screen& s, const char* element, uint32_t on) {
    if (const int e = s.find(element); e >= 0) s.setTexture(e, on != 0 ? "UI_TICKBOX_YES" : "UI_TICKBOX_NO");
}

}  // namespace

// 0x00483D10: TICK_STEERINGWHEEL, TICK_SPEED, TICK_FORCE, the meters SENSITIVITY and STRENGTH (+0x0C ..) found; +0x20
// 0; the settings kept: the wheel (+0x78), speed sensitive (+0xAC 0), force (+0xB4), the sensitivity (+0xC4), the
// strength (+0xA4); the ticks UI_TICKBOX_YES or UI_TICKBOX_NO by them; the meters' value and shown value theirs (their
// +0xFC 1, +0xF8 0); DONE_BUT's links made again (0x005A99F0).
void WheelOptions::enter(OptionsHost& host, ui::Screen& s) {
    const save::Settings& set = host.optionSettings();
    m_shownButton.clear();
    m_wheel = set.u32(save::Settings::kSteeringWheel);
    m_speed = set.u32(save::Settings::kWheelSpeed) == 0 ? 1 : 0;
    m_force = set.u32(save::Settings::kWheelForce);
    m_sensitivity = std::bit_cast<float>(set.u32(save::Settings::kSensitivity));
    m_strength = std::bit_cast<float>(set.u32(save::Settings::kWheelStrength));
    tick(s, "TICK_STEERINGWHEEL", m_wheel);
    tick(s, "TICK_SPEED", m_speed);
    tick(s, "TICK_FORCE", m_force);
    for (const auto& [meter, value] : {std::pair{"SENSITIVITY", m_sensitivity}, std::pair{"STRENGTH", m_strength}}) {
        if (s.find(meter) < 0) continue;
        s.setMeter(meter, value, true);
        s.setMeterSlider(meter, true);
        s.setMeterActive(meter, false);
    }
}

// 0x00483E80: a new highlighted button (+0x20) marks the meter of STRENGTH_BUT or SENSITIVITY_BUT (their +0xF8; a
// marked meter slides, 0x005B6580). SELECT (CLICK): STEERINGWHEEL_BUT, SPEEDSENSITIVE_BUT, FORCE_BUT flip their value
// and tick; DONE_BUT takes the meters' shown values and writes the settings (+0xAC 1 for not speed sensitive, +0xB4,
// +0x78, with the meters +0xC4 and +0xA4), then opts_menu (mode 7). BACK (CLICK) opts_menu, the settings as they were.
void WheelOptions::update(OptionsHost& host, ui::Screen& s, uint32_t event) {
    const std::string current = s.currentButton();
    const auto is1 = [&](const char* name) { return _stricmp(current.c_str(), name) == 0; };
    if (m_shownButton != current) {
        m_shownButton = current;
        s.setMeterActive("SENSITIVITY", is1("SENSITIVITY_BUT"));
        s.setMeterActive("STRENGTH", is1("STRENGTH_BUT"));
    }
    if (event == input::kSelect) {
        if (is1("STEERINGWHEEL_BUT")) {
            m_wheel = m_wheel == 0 ? 1 : 0;
            tick(s, "TICK_STEERINGWHEEL", m_wheel);
        } else if (is1("SPEEDSENSITIVE_BUT")) {
            m_speed = m_speed == 0 ? 1 : 0;
            tick(s, "TICK_SPEED", m_speed);
        } else if (is1("FORCE_BUT")) {
            m_force = m_force == 0 ? 1 : 0;
            tick(s, "TICK_FORCE", m_force);
        } else if (is1("DONE_BUT")) {
            save::Settings& set = host.optionSettings();
            if (s.find("SENSITIVITY") >= 0) m_sensitivity = s.meterShown("SENSITIVITY");
            if (s.find("STRENGTH") >= 0) m_strength = s.meterShown("STRENGTH");
            set.setU32(save::Settings::kWheelSpeed, m_speed == 0 ? 1 : 0);
            set.setU32(save::Settings::kWheelForce, m_force);
            set.setU32(save::Settings::kSteeringWheel, m_wheel);
            if (s.find("SENSITIVITY") >= 0) {
                set.setU32(save::Settings::kSensitivity, std::bit_cast<uint32_t>(m_sensitivity));
            }
            if (s.find("STRENGTH") >= 0) {
                set.setU32(save::Settings::kWheelStrength, std::bit_cast<uint32_t>(m_strength));
            }
            host.sound("CLICK");
            host.request(host.optionsMenu(), SwitchMode::Swap);
            return;
        } else {
            return;
        }
        host.sound("CLICK");
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request(host.optionsMenu(), SwitchMode::Swap);
    }
}

}  // namespace oj::frontend
