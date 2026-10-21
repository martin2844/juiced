#pragma once
// A Ticker (kind 8, 0x005B51B0): its text moving across its box.

namespace oj::ui {

struct TickerState {
    // A ticker (kind 8, 0x005B51B0): the text's width and the box's (+0x154; 0 to be measured), how far the text
    // has moved (+0x158), TickerSpeed (+0x15C, 0.25 by default), the speed's ramp (+0x160) and the fade in
    // (+0x164).
    float width = 0.0f;
    float scroll = 0.0f;
    float speed = 0.25f;
    float ramp = 0.0f;
    float fade = 0.0f;
};

}  // namespace oj::ui
