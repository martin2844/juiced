#pragma once
// .d8f, a bitmap font of the interface: ui\fonts\<name>_<640|800|1024|1280|1600>.d8f, one file per resolution class.
//
//   u8  palette[256][4]      B, G, R, A
//   u32 glyphCount, glyphCount - 1, spaceAdvance
//   glyph[glyphCount], 48 bytes each:
//       u16 code (UTF-16), u16 0xFFFF
//       u32 width, height     pixels of the glyph's picture
//       i32 xOffset, yOffset  of the picture from the pen (y up is negative: yOffset of a capital is -height)
//       u32 advance           pen step
//       u32 0
//       f32 u0, v0, u1, v1    picture in its page
//       u32 page
//   u32 pageCount
//   page[pageCount]: u32 width, height; u8 pixels[width * height] (palette indices)
//
// The glyph of code 0xFFFF stands for characters the font lacks.

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace oj {

struct D8f {
    struct Glyph {
        uint16_t code = 0;
        uint32_t width = 0, height = 0;
        int32_t xOffset = 0, yOffset = 0;
        uint32_t advance = 0;
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
        uint32_t page = 0;
    };
    struct Page {
        uint32_t width = 0, height = 0;
        std::vector<uint8_t> indices;
    };

    std::array<uint32_t, 256> palette{};  // 0xAARRGGBB
    uint32_t spaceAdvance = 0;
    std::vector<Glyph> glyphs;
    std::unordered_map<uint16_t, size_t> byCode;
    std::vector<Page> pages;

    bool parse(const std::vector<std::byte>& file);
    // The glyph of a character, or the 0xFFFF glyph, or nullptr.
    const Glyph* find(wchar_t c) const;
};

}  // namespace oj
