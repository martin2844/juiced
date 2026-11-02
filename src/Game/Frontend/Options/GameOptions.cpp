// The game options (opts_game_pc).

#include "Game/Frontend/Options/GameOptions.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Game/UI/WindowFixes.h"
#include "Render/Display.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <string>

namespace oj::frontend {

namespace {

// The fixed build's field of view (GraphicsConfig::fov) on the FOV row (Game/UI/WindowFixes): its meter over the race
// camera's angle across at rest on this picture from the least view to the most (Render/Display), the degrees shown.
struct FovRange {
    render::Display shape;
    float least = 0.0f;
    float most = 0.0f;
};

FovRange fovRange(OptionsHost& host) {
    FovRange r;
    r.shape = render::display(host.device().width(), host.device().height());
    r.least = render::restFovDegrees(r.shape, kFovLeast);
    r.most = render::restFovDegrees(r.shape, kFovMost);
    return r;
}

float fovMeter(const FovRange& r, int32_t fov) {
    return (render::restFovDegrees(r.shape, fovScale(fov)) - r.least) / (r.most - r.least);
}

void showFov(OptionsHost& host, ui::Screen& s, int32_t fov) {
    const FovRange r = fovRange(host);
    const auto degrees = std::lround(render::restFovDegrees(r.shape, fovScale(fov)));
    s.setText(ui::kFovValue, std::to_wstring(degrees));
}

}  // namespace

// 0x004813E0: +0x58 and +0x5C the sensitivity (+0xC4); VIB, VIB2, MAP, AUTO, AUTO2, UNITS, AIDS, DIFF (+0x0C ..) and
// the meter STEERING (+0x30: its value and shown value the sensitivity, its +0xFC 1) found; +0x2C 0; the settings kept
// for BACK; VIB_BUT reads "STEERING WHEEL" (1677) and every element with VIB in its name is hidden; AUTO_BUT's up
// button and DON_BUT's down button each other (over the hidden VIB_BUT); +0x54 0.
void GameOptions::enter(OptionsHost& host, ui::Screen& s) {
    const save::Settings& set = host.optionSettings();
    m_sensitivity = std::bit_cast<float>(set.u32(save::Settings::kSensitivity));
    if (s.find("STEERING") >= 0) {
        s.setMeter("STEERING", m_sensitivity, true);
        s.setMeterSlider("STEERING", true);
    }
    m_shownButton.clear();
    m_wheel = set.u32(save::Settings::kSteeringWheel);
    m_map = set.u32(save::Settings::kMap);
    m_automatic = set.u32(save::Settings::kAutomatic);
    m_automatic2 = set.u32(save::Settings::kAutomatic + 4);
    m_units = set.u32(save::Settings::kMph);
    m_aids = set.u32(save::Settings::kAids);
    m_difficulty = set.u32(save::Settings::kDifficulty);
    s.setText("VIB_BUT", host.gameTexts().at(kTextSteeringWheel));
    s.hideContaining("VIB");
    s.setNeighbour("AUTO_BUT", input::kUp, "DON_BUT");
    s.setNeighbour("DON_BUT", input::kDown, "AUTO_BUT");
    m_rumble = 0.0f;
    // The fixed build's field of view: its row between SENSITIVITY's and DONE, its meter at the kept view.
    m_fov = graphicsConfig().fov;
    if (s.find(ui::kFovMeter) >= 0) {
        s.setText(ui::kFovButton, L"FIELD OF VIEW");
        s.setNeighbour("SENSITIVITY_BUT", input::kDown, ui::kFovButton);
        s.setNeighbour(ui::kFovButton, input::kUp, "SENSITIVITY_BUT");
        s.setNeighbour(ui::kFovButton, input::kDown, "DON_BUT");
        s.setNeighbour("DON_BUT", input::kUp, ui::kFovButton);
        s.setMeter(ui::kFovMeter, fovMeter(fovRange(host), m_fov), true);
        s.setMeterSlider(ui::kFovMeter, true);
        showFov(host, s, m_fov);
    }
}

// 0x00481770 (the second player's pause of a race of more players: its event and its UP / DOWN moving the highlight):
// the meter's +0xF8 on the highlighted SENSITIVITY_BUT (it slides, 0x005B6580). A new highlighted button (+0x2C) writes
// the texts: VIB, AUTO, AUTO2, AIDS "YES" (105) or "NO" (106) by +0x78, +0x84, +0x88, +0x7C; MAP the same by +0x80 in
// orange 0xFFE86A17 (with more than one player "NO" in 0xFF808000); UNITS "MPH" or "KPH" (1494, 1495) by +0x94; DIFF
// "NORMAL" (179) for +0xA0 0, else "HARD" (1975). +0x54 counts down by the frame time (held to 0 .. 1) and the
// controller rumbles while it is over 0 (0x0041DD60; not followed). BACK: the kept values back (stock problem: +0x88
// takes +0x84's, and +0xA0 is not given back; the fixed build gives each its own and the difficulty back), the
// sensitivity with STEERING, CLICK and opts_menu (mode 7). SELECT on DON_BUT: the sensitivity STEERING's shown value,
// CLICK and opts_menu. LEFT or RIGHT on a button flips its value (CLICK; MAP with more than one player NO) and +0x2C 0.
void GameOptions::update(OptionsHost& host, ui::Screen& s, uint32_t event) {
    // 0x004817C4: the second player's pause of a race of more players takes the second player's event (+0x8C) and its
    // UP and DOWN move the highlight here (to the button's neighbours +0x154 / +0x174, SELECT): the window manager
    // moves it by the first player's.
    if (host.secondPlayerPaused()) {
        event = host.secondPlayerEvent();
        if (!s.currentButton().empty() && (event == input::kUp || event == input::kDown)) s.moveHighlight(event);
    }
    save::Settings& set = host.optionSettings();
    const GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const std::string current = s.currentButton();
    const auto is1 = [&](const char* name) { return _stricmp(current.c_str(), name) == 0; };
    const auto yesNo = [&](uint32_t v) { return texts.at(v != 0 ? kTextYes : kTextNo); };
    s.setMeterActive("STEERING", is1("SENSITIVITY_BUT"));
    // The fixed build's field of view follows its meter's shown value while FOV_BUT is highlighted (the race's views
    // take it at once).
    if (s.find(ui::kFovMeter) >= 0) {
        s.setMeterActive(ui::kFovMeter, is1(ui::kFovButton));
        if (is1(ui::kFovButton)) {
            const FovRange r = fovRange(host);
            const float degrees = r.least + s.meterShown(ui::kFovMeter) * (r.most - r.least);
            graphicsConfig().fov = fovSetting(render::fovScaleOf(r.shape, degrees));
            showFov(host, s, graphicsConfig().fov);
        }
    }
    if (m_shownButton != current) {
        m_shownButton = current;
        if (const int e = s.find("VIB"); e >= 0) s.setText(e, yesNo(set.u32(save::Settings::kSteeringWheel)));
        if (const int e = s.find("MAP"); e >= 0) {
            // 0x004818AB: with more than one player "NO" in 0xFF808000.
            if (data != nullptr && data->race.players > 1) {
                s.setText(e, texts.at(kTextNo));
                s.setFontColour(e, 0xFF808000);
            } else {
                s.setFontColour(e, 0xFFE86A17);
                s.setText(e, yesNo(set.u32(save::Settings::kMap)));
            }
        }
        if (const int e = s.find("AUTO"); e >= 0) s.setText(e, yesNo(set.u32(save::Settings::kAutomatic)));
        if (const int e = s.find("AUTO2"); e >= 0) s.setText(e, yesNo(set.u32(save::Settings::kAutomatic + 4)));
        if (const int e = s.find("UNITS"); e >= 0) {
            s.setText(e, texts.at(kTextMph + (set.u32(save::Settings::kMph) != 0 ? 0 : 1)));
        }
        if (const int e = s.find("AIDS"); e >= 0) s.setText(e, yesNo(set.u32(save::Settings::kAids)));
        if (const int e = s.find("DIFF"); e >= 0) {
            s.setText(e, texts.at(set.u32(save::Settings::kDifficulty) == 0 ? kTextNormal : kTextHard));
        }
    }
    if (m_rumble > 0.0f) {
        m_rumble = std::clamp(m_rumble - host.frameTime(), 0.0f, 1.0f);
    }
    if (event == input::kBack) {
        set.setU32(save::Settings::kSteeringWheel, m_wheel);
        set.setU32(save::Settings::kMap, m_map);
        if (s.find("AUTO") >= 0) set.setU32(save::Settings::kAutomatic, m_automatic);
        if (s.find("AUTO2") >= 0) set.setU32(save::Settings::kAutomatic + 4, m_automatic2);
        set.setU32(save::Settings::kMph, m_units);
        set.setU32(save::Settings::kAids, m_aids);
        set.setU32(save::Settings::kDifficulty, m_difficulty);
        if (s.find("STEERING") >= 0) set.setU32(save::Settings::kSensitivity, std::bit_cast<uint32_t>(m_sensitivity));
        graphicsConfig().fov = m_fov;
        host.sound("CLICK");
        host.request(host.optionsMenu(), SwitchMode::Swap);
    } else if (event == input::kSelect) {
        if (!is1("DON_BUT")) return;
        if (s.find("STEERING") >= 0) {
            set.setU32(save::Settings::kSensitivity, std::bit_cast<uint32_t>(s.meterShown("STEERING")));
        }
        host.sound("CLICK");
        host.request(host.optionsMenu(), SwitchMode::Swap);
    } else if (event == input::kLeft || event == input::kRight) {
        const auto flip = [&](uint32_t at) {
            set.setU32(at, 1 - set.u32(at));
            host.sound("CLICK");
            m_shownButton.clear();
        };
        if (is1("VIB_BUT")) flip(save::Settings::kSteeringWheel);
        if (is1("MAP_BUT")) {
            // With more than one player ([G+0x6C]+0x58) the map stays and NO is played.
            if (data == nullptr || data->race.players < 2) {
                flip(save::Settings::kMap);
            } else {
                host.sound("NO");
                m_shownButton.clear();
            }
        }
        if (is1("UNITS_BUT")) flip(save::Settings::kMph);
        if (is1("AIDS_BUT")) flip(save::Settings::kAids);
        if (is1("DIFF_BUT")) {
            set.setU32(save::Settings::kDifficulty, set.u32(save::Settings::kDifficulty) == 0 ? 1 : 0);
            host.sound("CLICK");
            m_shownButton.clear();
        }
        if (is1("AUTO_BUT")) flip(save::Settings::kAutomatic);
        if (is1("AUTO_BUT2")) flip(save::Settings::kAutomatic + 4);
    }
}

}  // namespace oj::frontend
