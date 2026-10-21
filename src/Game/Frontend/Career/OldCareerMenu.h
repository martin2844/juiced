#pragma once
// An earlier career menu (vtable 0x006F58C8, table 0x00722284; on cmsmainmenu.ui):
// the stock code has it but never registers it (its factory entry is never handed to 0x005A8E30); OpenJuice opens
// it by "unregistered:CMSMainMenu" (--screen).

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>

namespace oj::frontend {

class OldCareerMenu {
public:
    // 0x004CDBF0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004CDC50
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_leaving = 0;          // +0x0C
    bool m_hasImage = false;        // +0x10: SELECTIONIMAGE
    std::string m_highlighted;      // +0x14: the menu item last highlighted
};

}  // namespace oj::frontend
