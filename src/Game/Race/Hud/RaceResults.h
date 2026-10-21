#pragma once
// The race's results' windows.

#include "Core/Data/Language.h"
#include "Game/Race/Hud/HudRace.h"
#include "Game/Race/Hud/RaceHud.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <array>
#include <cstdint>
#include <memory>

namespace oj {

// The window hud_race_results (factory 0x007205A0: enter 0x00452E60, frame 0x00453450): the finishers in up to six
// rows, the menu RESTART / REPLAY / QUIT: QUIT leads to closedown (asking first in a team round the player's crew has
// not finished), RESTART asks and leads to restart_race, REPLAY to start_replay. The career's respect (counter,
// Respect_Icon) is hidden as outside the career.
class RaceResults {
public:
    RaceResults(ui::WindowManager& windows, const TextTable& texts);
    bool enter(const HudRace& race, RaceHud* hud);
    // 0x00453450: the window's frame with the first player's event; the window manager first ends the transitions
    // and gives the menu LEFT / RIGHT (0x005A9E80, 0x005B11A0).
    RaceWindowFrame frame(const HudRace& race, float dt, uint32_t uiEvent);
    void draw(float seconds, float dt);

private:
    void fill(const HudRace& race);       // 0x00451C90
    void fillCrews(const HudRace& race);  // 0x004529E0
    void fillNet(const HudRace& race);    // 0x00452030
    void fillOnlineTeams(const HudRace& race);  // 0x00452330

    ui::WindowManager& m_windows;
    std::shared_ptr<ui::Screen> m_screen;
    const TextTable& m_texts;
    float m_time = 0.0f;                  // +0x0C: the controller let go each 4 s
    std::array<int, 6> m_pos{};           // +0x18
    std::array<int, 6> m_name{};          // +0x30
    std::array<int, 6> m_team{};          // +0x48
    std::array<int, 6> m_raceTime{};      // +0x60
    std::array<int, 6> m_car{};           // +0x78
    std::array<int, 6> m_bar{};           // +0x90
    int32_t m_filled = 0;                 // +0xB8: the finishers shown
    std::array<int32_t, 3> m_placeGiven{};  // +0xBC .. +0xC4: the online teams' places whose points were given
    static inline float m_refresh = 0.0f;               // [0x007A2EB0]
    int32_t m_started = 0;                // +0x10: the lap's records looked at
    int32_t m_notSaving = 0;              // +0x14: the save system does not save (read by nothing)
    int32_t m_quit = 0;                   // +0xA8
    int32_t m_restart = 0;                // +0xAC
    int32_t m_answer = 0;                 // +0xB4
};

}  // namespace oj
