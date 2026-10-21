#pragma once
// An item of a Menu (AddItem, AddTexturedItem).

#include <string>
#include <cstdint>

namespace oj::ui {

struct MenuItem {
    std::wstring text;
    float value = 0.0f;
    std::string command;      // +0x28, the item's name (AddItem's third word)
    std::wstring help;        // +0x14: the window's text of AddItem's fourth word (none for -1)
    std::string texture;      // +0x48
    std::string highTexture;  // +0x4C, the selected item's picture
    uint32_t colour = 0xFFFFFFFF;
    bool additive = false;
    bool enabled = true;
};

}  // namespace oj::ui
