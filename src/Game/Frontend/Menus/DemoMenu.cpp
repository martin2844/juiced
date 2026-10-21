// The demo's menu and its goodbye (demomenu, byebye).

#include "Game/Frontend/Menus/DemoMenu.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// The demo build's menu (0x00435620 / 0x00435640): the windows' enter and +0x0C 0. +0x0C 1 asks for byebye (mode 1;
// a name no record takes: byebye.ui comes up with nothing to run it, its own class never registered). SELECT (CLICK)
// on Menu's StartDemo asks for DemoFlyby, on Options for opts_bg, on QuitDemo asks "Quit to Windows" / "ARE YOU
// SURE?" (type 3, on NO) into +0x0C; BACK (CLICK) asks for Start.
void DemoMenu::enter() {
    m_quit = 0;
}

void DemoMenu::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    if (m_quit == 1) {
        host.request("byebye", SwitchMode::Replace);
        return;
    }
    if (event == input::kSelect) {
        const std::string item = s.find("Menu") >= 0 ? s.menuItemCommand("Menu", s.selectedMenuItem("Menu")) : "";
        host.sound("CLICK");
        if (_stricmp(item.c_str(), "StartDemo") == 0) {
            host.request("DemoFlyby", SwitchMode::Replace);
        } else if (_stricmp(item.c_str(), "Options") == 0) {
            host.request("opts_bg", SwitchMode::Replace);
        } else if (_stricmp(item.c_str(), "QuitDemo") == 0) {
            host.message(MessageType::YesNo, texts.at(kTextQuitToWindows) + L"\n" + texts.at(kTextAreYouSure), false,
                         &m_quit, true);
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("Start", SwitchMode::Replace);
    }
}

// The demo build's goodbye (0x00435540 / 0x004355E0, the movie juiced_outro of byebye.ui): +0x0C 7.0, PARA1 .. PARA6
// over several lines, +0x10 1. The update runs +0x0C down by the game's frame time and, while +0x10 is set, clears it
// once no key is down (the input manager's +0x08); nothing reads them.
void ByeBye::enter(ui::Screen& s) {
    m_time = 7.0f;
    for (const char* name : {"PARA1", "PARA2", "PARA3", "PARA4", "PARA5", "PARA6"}) {
        if (s.find(name) >= 0) s.setMultiline(name, true);
    }
    m_held = 1;
}

void ByeBye::update(WindowHost& host, bool anyKeyDown) {
    m_time = m_time - host.gameFrameTime();
    if (m_held != 0 && !anyKeyDown) m_held = 0;
}

}  // namespace oj::frontend
