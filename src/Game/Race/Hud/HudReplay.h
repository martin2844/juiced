#pragma once
// The window hud_replay: the replay's controls.

#include "Core/Data/Language.h"
#include "Game/Race/Hud/HudRace.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

namespace oj {

// The window hud_replay (factory 0x007209F0, vtable 0x006F1458: enter 0x0044DA80, frame 0x0044DE20, leave
// 0x0044DC20): the replay's controls RP_MENU (hide, change view, restart, slow motion, play, pause, fast forward,
// exit), its meter, the state's text, the showoff's moves of the frames played and the fade to black (BACKGROUND).
class HudReplay {
public:
    HudReplay(ui::WindowManager& windows, const TextTable& texts);
    bool enter(const HudRace& race, Replay& replay);
    // 0x0044DE20: the window's frame with the first player's event; `anyInput` the controller's (the attract's end).
    ReplayWindowFrame frame(const HudRace& race, Replay& replay, float dt, uint32_t uiEvent, bool anyInput);
    void leave(Replay& replay);
    void draw(float seconds, float dt);

private:
    void eject(Replay& replay, ReplayWindowFrame& out);  // 0x0044DD00
    void showControls(const HudRace& race, const Replay& replay);  // 0x0044DCA0

    ui::WindowManager& m_windows;
    const TextTable& m_texts;
    std::shared_ptr<ui::Screen> m_screen;
    float m_progress = 0.0f;   // +0x10
    float m_idle = 5.0f;       // +0x14: since the last event, from 5 s down
    size_t m_selected = 0;     // +0x18: RP_MENU's item last frame
    int32_t m_info = 3;        // +0x1C: RP_INFO's text (504 + it: PLAYING, FAST FORWARD, SLOW-MOTION, PAUSED)
    int32_t m_answer = -1;     // +0x20
    int32_t m_saveAsked = 0;   // +0x24 (set nowhere: the save is not reached; the fixed build's after the eject's yes)
    int32_t m_ejectAsked = 0;  // +0x28
    int32_t m_quit = 0;        // +0x2C
    int32_t m_hideAsked = 0;   // +0x30
    int32_t m_hidden = 0;      // +0x34
    float m_moveTime = 0.0f;   // +0x40: the showoff's move shown
    std::deque<std::wstring> m_moves;  // +0x44: the moves waiting
};

}  // namespace oj
