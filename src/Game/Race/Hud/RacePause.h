#pragma once
// The window hud_pause: the race paused and its menu.

#include "Core/Data/Language.h"
#include "Game/Race/Hud/HudRace.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <cstdint>
#include <memory>

namespace oj {

// 0x0044EF7D, a restart from the pause: a sprint starts again from its first heat, the points (+0x138) of the session's
// racers' cars cleared and the heat -1 (restart_race counts it). Stock problem: their heats' time (+0x134) is kept and
// decides the new sprint's ties (equal points: the less time first); the fixed build clears it too.
void restartSprint(const HudRace& race);

// The window hud_pause (factory 0x00720548: enter 0x0044E920, frame 0x0044EEA0): the race's pause menu CONTINUE /
// FRIENDS / PLAYERS / RESTART / OPTIONS / QUIT. CONTINUE and BACK go on with the race; QUIT and RESTART ask first and
// lead to closedown and restart_race; OPTIONS opts_ingame_bg, FRIENDS and PLAYERS online_ingame_bg.
class RacePause {
public:
    RacePause(ui::WindowManager& windows, const TextTable& texts);
    bool enter(const HudRace& race);
    // 0x0044EEA0: the window's frame with the players' events (none in the frame the window came in).
    RaceWindowFrame frame(const HudRace& race, uint32_t uiEvent, uint32_t secondEvent = 0);
    // SELECT of a player on Menu's item.
    void selected(const HudRace& race, RaceWindowFrame& out, int32_t player);
    void draw(float seconds, float dt);

private:
    ui::WindowManager& m_windows;
    const TextTable& m_texts;
    std::shared_ptr<ui::Screen> m_screen;
    int32_t m_quitAsked = 0;     // +0x0C
    int32_t m_restartAsked = 0;  // +0x10
    int32_t m_restart = 0;       // +0x18
    int32_t m_settled = 1;       // +0x1C: the lap's records looked at
    int32_t m_quit = 0;          // +0x20
    int32_t m_friends = 0;       // +0x24: FRIENDS there (the friends' object)
    int32_t m_restartPlayer = 0;  // +0x28: the player who asked for the restart (read by nothing)
    int32_t m_answer = 0;        // +0x2C
};

}  // namespace oj
