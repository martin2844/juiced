#pragma once
// The main menu (menu).

#include "Game/Frontend/Menus/MainMenuHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721510 (vtable 0x006F26A0).
class MainMenu {
public:
    // 0x00468490
    void enter(MainMenuHost& host, ui::Screen& s);
    // 0x00468850
    void update(MainMenuHost& host, ui::Screen& s, uint32_t event);
    // 0x00468790
    void leave(MainMenuHost& host);

private:
    int32_t m_career = 0;     // +0x0C: CMSHub went to DemoFlyby: the career made again at the leave
    int32_t m_quitAsked = 0;  // +0x20: "Quit to Windows" asked
    int m_quitAnswer = 0;     // +0x24
    int m_backAnswer = -1;    // +0x28: BACK's question (-1 by the enter)
};

}  // namespace oj::frontend
