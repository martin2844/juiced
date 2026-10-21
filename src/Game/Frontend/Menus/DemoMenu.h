#pragma once
// The demo build's menu and goodbye: two windows never registered (record 0x0071FC3C, vtable 0x006F01E8, demomenu.ui;
// record 0x0071FC4C, vtable 0x006F01F8, byebye.ui), opened here by "unregistered:demomenu" and "unregistered:byebye".

#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

class DemoMenu {
public:
    // 0x00435620
    void enter();
    // 0x00435640
    void update(WindowHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_quit = 0;  // +0x0C: the answer to "Quit to Windows"
};

class ByeBye {
public:
    // 0x00435540
    void enter(ui::Screen& s);
    // 0x004355E0; anyKeyDown: a key down (the input manager's +0x08).
    void update(WindowHost& host, bool anyKeyDown);

private:
    float m_time = 0.0f;  // +0x0C: 7 s by the enter, down by the game's frame time
    int32_t m_held = 0;   // +0x10: a key still down since the enter
};

}  // namespace oj::frontend
