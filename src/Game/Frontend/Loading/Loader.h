#pragma once
// The loading phase's windows (arcade_loading, loader, custom_loader).

#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721250 (arcade_loading, loader, custom_loader).
class Loader {
public:
    // 0x00462840
    void enter(WindowHost& host, ui::Screen& s);
    // The record's +0x0C: the tips shown, set by the race's load (0x00462D90) and cleared after the race (0x004634B0).
    void setTips(bool shown) { m_tips = shown; }

private:
    bool m_tips = false;  // the record's +0x0C: the tips shown
};

}  // namespace oj::frontend
