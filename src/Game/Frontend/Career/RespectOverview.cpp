// CMSRespectOverview and CMSRespectHelp: the crews' respect and its help.

#include "Game/Frontend/Career/RespectOverview.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

float g_respectBlink = 0.0f;  // [0x0079E864]

}  // namespace

// CMSRespectOverview (0x004DC260): help_text "HELP" (1623); for crews 1 .. 8 meter<n> the respect / 1500 (at most 1)
// and val<n> the respect (thousands marked); for each of its challenges whether the player has a car for it
// (0x004BC1D0, kept at +0x18 ..) and chall<n>-<k> ui_yes when done, else ui_no; progress the career's completion
// (0x004BCA70) and progressval its percent ("%.0f%% ") and "Completed" (338).
void RespectOverview::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    s.setText("help_text", texts.at(kTextHelp));
    for (size_t n = 1; n < 9; ++n) {
        const Crew& crew = career.crews[n];
        s.setMeter(std::format("meter{}", n), std::min(1.0f, static_cast<float>(crew.respect) * 0.00066666666f));
        s.setText(std::format("val{}", n), toWide(groupThousands(crew.respect, host.language().index)));
        for (int k = 0; k < 3; ++k) {
            m_cars[(n - 1) * 3 + static_cast<size_t>(k)] = challengeCar(career, crew, k, nullptr);
            const bool done = crew.challenges[static_cast<size_t>(k)].done != 0;
            s.setTexture(std::format("chall{}-{}", n, k + 1), done ? "ui_yes" : "ui_no");
        }
    }
    const float completion = careerCompletion(career);
    s.setMeter("progress", completion);
    s.setText("progressval", toWide(std::format("{:.0f}% ", completion * 100.0f)) + texts.at(kTextCompleted));
}

// 0x004DC7A0: the blinking (the game's frame time, past 1 back to 0); the phone's frame (0x004B60F0);
// BACK (CLICK) CMSMainMenu; PGDN (CLICK) CMSRespectHelp. A crew with its +0x2D8 blinks its meter, and each challenge
// the player has a car for blinks its chall<n>-<k> (hidden over 0.75).
void RespectOverview::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    g_respectBlink = g_respectBlink + host.gameFrameTime();
    if (g_respectBlink > 1.0f) g_respectBlink = 0.0f;
    host.phoneFrame(s);
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("CMSMainMenu", SwitchMode::Replace);
    } else if (event == input::kSelect2) {
        host.sound("CLICK");
        host.request("CMSRespectHelp", SwitchMode::Replace);
    }
    for (size_t n = 1; n < 9; ++n) {
        if (career.crews[n].meterBlinks != 0) s.setHidden(std::format("meter{}", n), g_respectBlink > 0.75f);
        for (int k = 0; k < 3; ++k) {
            const bool car = m_cars[(n - 1) * 3 + static_cast<size_t>(k)];
            s.setHidden(std::format("chall{}-{}", n, k + 1), car && g_respectBlink > 0.75f);
        }
    }
}

// CMSRespectHelp (0x004DCEF0): for crews 1 .. 8 RespectTxt<n> what the crew respects most (297 + 0x004BC110);
// RESPECTTXT1 .. 8 over several lines; HELP over several lines with line 1969; LEADERTXT3 "SUE YEN".
void RespectHelp::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    const Career& career = *data->career;
    for (size_t n = 1; n < 9; ++n) {
        s.setText(std::format("RespectTxt{}", n),
                  texts.at(297 + static_cast<size_t>(respectedMost(career.crews[n]))));
    }
    for (int n = 1; n <= 8; ++n) s.setMultiline(std::format("RESPECTTXT{}", n), true);
    s.setMultiline("HELP", true);
    s.setText("HELP", texts.at(kTextCrewChallengesHelp));
    s.setText("LEADERTXT3", L"SUE YEN");
}

// 0x004DD290: BACK (CLICK) CMSRespectOverview.
void RespectHelp::update(CareerHost& host, ui::Screen& /*s*/, uint32_t event) {
    if (event != input::kBack) return;
    host.sound("CLICK");
    host.request("CMSRespectOverview", SwitchMode::Replace);
}

}  // namespace oj::frontend
