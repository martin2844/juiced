#pragma once
// A Text's, a Button's or a Menu item's text and its font (0x005B17E1, 0x005AFCA0).

#include <cstdint>
#include <string>

namespace oj::ui {

struct TextState {
    bool set = false;
    std::wstring value;
    bool multiline = false;
    bool clip = false;               // +0x13C
    bool autoScroll = false;             // +0x140
    float scroll = 0.0f;                 // +0x144
    float scrollSpeed = 30.0f;           // +0x148
    float scrollEnd = 0.0f;              // +0x14C: the box's height and the lines'
    bool fontColours = false;
    uint32_t font = 0;
    uint32_t high = 0;
    float alpha = 1.0f;             // +0x130
    std::string fontName;     // a font set by the code
};

}  // namespace oj::ui
