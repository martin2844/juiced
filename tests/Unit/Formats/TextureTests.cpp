// Unit tests: Texture.

#include "Framework/Test.h"

#include "Core/Formats/Ddt.h"

namespace {

using oj::test::check;

}  // namespace

// A DXT1 block: the two ends and the colours a third of the way; a DXT4 2 x 2 texture whose 1 x 1
// level averages the colours divided by alpha, multiplied by the average alpha.
OJ_UNIT_TEST(Texture, DxtDecode) {
    const uint8_t dxt1[8] = {0x00, 0xF8, 0x1F, 0x00, 0xE4, 0, 0, 0};
    const auto p = oj::decodeLevel(oj::Ddt::kDxt1, 4, 4, dxt1);
    const bool colours = p[0] == 0xFFFF0000 && p[1] == 0xFF0000FF && p[2] == 0xFFAA0055 &&
                         p[3] == 0xFF5500AA && p[4] == 0xFFFF0000;
    oj::Ddt t;
    t.format = oj::Ddt::kDxt4;
    t.width = t.height = 2;
    t.mipLevels = 1;
    // Alpha ends 255 and 0: block pixels 0 and 5 index 0, 1 and 4 index 1; every colour white.
    t.pixels = {0xFF, 0x00, 0x08, 0x10, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0};
    uint32_t levels = 0;
    const auto chain = oj::fullMipChain(t, levels);
    // Pixels 0 and 3 opaque white, 1 and 2 transparent: alpha 0.5, white divided back = 1, times 0.5.
    const bool premultiplied = levels == 2 && chain.size() == 5 && chain[0] == 0xFFFFFFFF &&
                               chain[1] == 0x00FFFFFF && chain[4] == 0x80808080;
    check(colours && premultiplied, "texture decode: DXT1 colours, a DXT4 mip level");
}
