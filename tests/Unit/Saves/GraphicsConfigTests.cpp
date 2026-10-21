// Unit tests: GraphicsConfig.

#include "Framework/Test.h"

#include "Core/Saves/GraphicsConfig.h"
#include "Core/Saves/Save.h"

namespace {

using oj::test::check;

}  // namespace

OJ_UNIT_TEST(GraphicsConfig, GraphicsClass) {
    check(oj::graphicsClass(0x1002, 0x4E50) == 4 && oj::graphicsClass(0x8086, 0) == 3 &&
              oj::graphicsClass(0x10DE, 0x0040) == 4 && oj::graphicsClass(0x10DE, 0x0020) == 0 &&
              oj::graphicsClass(0x10DE, 0x0301) == 2 && oj::graphicsClass(0x10DE, 0x0330) == 3 &&
              oj::graphicsClass(0x10DE, 0x2204) == 3 && oj::graphicsClass(0x4E50, 0) == 2 &&
              oj::graphicsClass(0x556A, 0) == 4 && oj::graphicsClass(0x5D44, 0) == 0,
          "graphics class of an adapter");
}
OJ_UNIT_TEST(GraphicsConfig, Settings) {
    oj::GraphicsConfig c;
    oj::graphicsDefaults(c);
    oj::graphicsStart(c);
    oj::graphicsAdapter(c, 0x10DE, 0x0040, true);
    const oj::save::RegCf1 reg = oj::graphicsRegCf1(c);
    oj::GraphicsConfig back;
    oj::graphicsFromRegCf1(back, reg);
    check(c.shadows == 2 && c.reflections == 2 && c.motionBlur == 0 && c.brightness == 0.5f &&
              c.lodBias > 1.5f && back.detail == c.detail && back.shadows == 2,
          "graphics settings: defaults, start, adapter and reg.cf1");
}
