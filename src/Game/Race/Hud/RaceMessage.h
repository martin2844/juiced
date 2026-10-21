#pragma once
// The race front end's message windows (msgbox, msgbox_network, msgbox_critical).

#include "Game/Race/Hud/HudRace.h"
#include "Game/UI/MessageWindow.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <cstdint>
#include <memory>
#include <string>

namespace oj {

// A message window of the race front end's manager (Game/UI/MessageWindow.h): msgbox (the manager's +0x44,
// 0x005A80E0: opened, its state entered and its font), msgbox_network (+0x3C, 0x005A7F90) or msgbox_critical (+0x34,
// 0x005A8030), those two opened only.
class RaceMessage {
public:
    explicit RaceMessage(ui::WindowManager& windows);
    bool open(ui::MessageType type, const std::wstring& text, int32_t* answer, bool startNo = false,
              const char* window = "msgbox");
    // The manager's event for it (0x005AA010: an event ends its transitions, its menus take it, MSG_MENU the second
    // player's too) and its state's (0x005A9200): what SELECT or BACK closes.
    ui::MessageClose event(uint32_t uiEvent, uint32_t secondEvent);
    void draw(float seconds, float dt);

private:
    ui::WindowManager& m_windows;
    std::shared_ptr<ui::Screen> m_screen;
    ui::MessageState m_state;
    bool m_network = false;  // msgbox_network
    bool m_entered = false;  // msgbox: its state entered (0x005A9180)
};

}  // namespace oj
