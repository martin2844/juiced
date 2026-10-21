#pragma once
// A sprint's results' window between its heats.

#include "Core/Data/Language.h"
#include "Game/Race/Hud/HudRace.h"
#include "Game/Race/Hud/RaceHud.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <array>
#include <cstdint>
#include <memory>

namespace oj {

// The window hud_sprint_results (factory 0x00720670: enter 0x00454760, frame 0x00454DB0, exit 0x00454D80): the heat's
// finishers in four rows with their points and times; once all have finished SELECT shows the leaderboard (the
// points of the heats so far), then the next heat (restart_race) or, after the last, the sprint's order to the career
// and closedown.
class SprintResults {
public:
    SprintResults(ui::WindowManager& windows, const TextTable& texts);
    bool enter(const HudRace& race, RaceHud* hud);
    // 0x00454DB0: the window's frame with the first player's event; dt the game's frame time.
    RaceWindowFrame frame(const HudRace& race, float dt, uint32_t uiEvent);
    void draw(float seconds, float dt);

private:
    void fill(const HudRace& race);         // 0x00453BA0
    void leaderboard(const HudRace& race);  // 0x004542D0

    ui::WindowManager& m_windows;
    const TextTable& m_texts;
    std::shared_ptr<ui::Screen> m_screen;
    float m_time = 0.0f;          // +0x0C: the controller let go each 4 s
    float m_master = 0.0f;        // +0x2C: the 3D world's player's master as the window came
    int32_t m_recordsDone = 0;    // +0x10: the lap's records looked at
    int32_t m_settled = 0;        // +0x14: the sprint's end taken
    int32_t m_closing = 0;        // +0x18
    int32_t m_notSaving = 0;      // +0x1C
    float m_allFinished = 0.0f;   // +0x20: 0.1 once every competitor finished
    int32_t m_board = 0;          // +0x24: the leaderboard shown
    int32_t m_last = -1;          // +0x28: 1 for the last heat's end
    std::array<int, 4> m_pos{};   // +0x30
    std::array<int, 4> m_name{};  // +0x48
    std::array<int, 4> m_heatTime{};  // +0x60
    std::array<int, 4> m_team{};  // +0x78
    std::array<int, 4> m_car{};   // +0x90
    std::array<int, 4> m_bar{};   // +0xA8
    int32_t m_filled = 0;         // +0xC0: the rows of finishers filled
};

}  // namespace oj
