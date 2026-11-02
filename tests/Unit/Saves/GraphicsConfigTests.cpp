// Unit tests: GraphicsConfig.

#include "Framework/Test.h"

#include "Core/Saves/GraphicsConfig.h"
#include "Core/Saves/Save.h"

namespace {

using oj::test::check;

}  // namespace

OJ_UNIT_TEST(GraphicsConfig, GraphicsClass) {
    // The fixed build: an ATI card by its device id (unlisted 4), another vendor of a listed number 3.
    check(oj::graphicsClass(0x1002, 0x4E50) == 2 && oj::graphicsClass(0x1002, 0x556A) == 4 &&
              oj::graphicsClass(0x1002, 0x5D44) == 0 && oj::graphicsClass(0x1002, 0x73BF) == 4 &&
              oj::graphicsClass(0x8086, 0) == 3 && oj::graphicsClass(0x10DE, 0x0040) == 4 &&
              oj::graphicsClass(0x10DE, 0x0020) == 0 && oj::graphicsClass(0x10DE, 0x0301) == 2 &&
              oj::graphicsClass(0x10DE, 0x0330) == 3 && oj::graphicsClass(0x10DE, 0x2204) == 3 &&
              oj::graphicsClass(0x4E50, 0) == 3 && oj::graphicsClass(0x556A, 0) == 3,
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

// The fixed build's field of view: reg.cf1's eighth value, 0 (a stock file's) the stock's view, else the permille held
// within 600 .. 1800; reg.cf1 keeps it.
OJ_UNIT_TEST(GraphicsConfig, FieldOfView) {
    check(oj::fovScale(0) == 1.0f && oj::fovScale(1000) == 1.0f && oj::fovScale(1250) == 1.25f &&
              oj::fovScale(100) == oj::kFovLeast && oj::fovScale(5000) == oj::kFovMost,
          "the view's scale of the kept field of view");
    check(oj::fovSetting(1.0f) == 1000 && oj::fovSetting(1.3333f) == 1333 && oj::fovSetting(9.0f) == 1800,
          "the kept field of view of a scale");
    oj::GraphicsConfig c;
    oj::graphicsDefaults(c);
    c.fov = 1234;
    oj::GraphicsConfig back;
    oj::graphicsFromRegCf1(back, oj::graphicsRegCf1(c));
    check(back.fov == 1234 && oj::graphicsRegCf1(c).values[7] == 1234u, "the field of view through reg.cf1");
}
