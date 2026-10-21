#pragma once
// The options' menu (opts_menu; from a race's pause opts_ingame_menu).

#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721A6C.
class OptionsMenu {
public:
    // 0x00480090
    void enter(OptionsHost& host, ui::Screen& s);
    // 0x00480310
    void update(OptionsHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_saving = 0;  // +0x0C (0x00721A78): the autosave asked, the main menu next
};

}  // namespace oj::frontend
