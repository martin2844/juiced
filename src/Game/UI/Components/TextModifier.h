#pragma once
// A text modifier: the glyphs of a text grown and faded about a place moving along it (0x005B14C0).

#include <cstdint>

namespace oj::ui {

// A text modifier record (0x2C bytes; the menus' BOUNCE one: type 2, +0x04 and +0x08 -8, +0x0C 10, +0x10 30,
// +0x14 8, +0x18 1, +0x24 and +0x28 1). The font's draw (0x0051E740) takes each character at its place from +0x08
// (counting the spaces): within +0x14 of it the glyph grows by +0x0C x the nearness and its alpha is the farness,
// no more than the colour's.
// How a text modifier's place moves (its +0x00; the .ui's LOOP and BOUNCE words): not at all, around, back and forth.
enum class ModifierType : int32_t { None = 0, Loop = 1, Bounce = 2 };

struct TextModifier {
    ModifierType type = ModifierType::Bounce;  // +0x00
    float start = -8.0f;     // +0x04
    float place = -8.0f;     // +0x08
    float grow = 10.0f;      // +0x0C
    float speed = 30.0f;     // +0x10
    float reach = 8.0f;      // +0x14
    int32_t forward = 1;     // +0x18
    float spread = 0.0f;     // +0x1C
    float alpha = 0.0f;      // +0x20
    float time = 1.0f;       // +0x24
    float pulse = 1.0f;      // +0x28

    void step(float dt, float length);  // 0x005B14C0
    void restart();                     // 0x005B14A0
};

}  // namespace oj::ui
