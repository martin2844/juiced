#pragma once
// A showoff's (cruise's) results' window.

#include "Core/Data/Language.h"
#include "Game/Race/Hud/HudRace.h"
#include "Game/Race/Hud/RaceHud.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace oj {

// The window hud_cruise_results (factory 0x00720984: enter 0x00451270, frame 0x00451830): the cruise's score, the
// menu RESTART / REPLAY / QUIT as the challenge's results (QUIT at once, RESTART asked).
class CruiseResults {
public:
    CruiseResults(ui::WindowManager& windows, const TextTable& texts);
    bool enter(const HudRace& race, RaceHud* hud);
    void netBoard(const std::array<std::wstring, 3>& lines, const std::array<bool, 3>& shown);  // 0x004B0A50
    RaceWindowFrame frame(const HudRace& race, uint32_t uiEvent);
    void draw(float seconds, float dt);

private:
    ui::WindowManager& m_windows;
    const TextTable& m_texts;
    std::shared_ptr<ui::Screen> m_screen;
    int32_t m_notSaving = 0;   // +0x0C
    int32_t m_recordsDone = 0;  // +0x10: the records looked at (set at once in the career before its race cars)
    int32_t m_quit = 0;        // +0x14
    int32_t m_restart = 0;     // +0x18
    int32_t m_answer = 0;      // +0x1C
};

}  // namespace oj
