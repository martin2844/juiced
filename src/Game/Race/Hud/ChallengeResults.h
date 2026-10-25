#pragma once
// A challenge's results' window.

#include "Core/Data/Language.h"
#include "Game/Race/Hud/HudRace.h"
#include "Game/Race/Hud/RaceHud.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <cstdint>
#include <memory>

namespace oj {

// The window hud_challenge_results (factory 0x00720AA4: enter 0x00450B60, frame 0x00450F80): the challenge's outcome
// in MSG1 and MSG2, the menu RESTART / REPLAY / QUIT: QUIT leads to closedown at once (the fixed build's: asks
// first), RESTART asks and leads to restart_race, REPLAY to start_replay.
class ChallengeResults {
public:
    ChallengeResults(ui::WindowManager& windows, const TextTable& texts);
    bool enter(const HudRace& race, RaceHud* hud);
    // 0x00450F80: the window's frame with the first player's event.
    RaceWindowFrame frame(const HudRace& race, uint32_t uiEvent);
    void draw(float seconds, float dt);

private:
    ui::WindowManager& m_windows;
    const TextTable& m_texts;
    std::shared_ptr<ui::Screen> m_screen;
    int32_t m_quit = 0;     // +0x0C
    int32_t m_restart = 0;  // +0x10
    int32_t m_answer = 0;   // +0x14
};

}  // namespace oj
