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
// The fixed build's glow as a percent: 0 off, 2 .. 100 the percent, the stock's on (1) and other values 50 %; a
// strength kept as its percent, 1 % as 2; reg.cf1 keeps it.
OJ_UNIT_TEST(GraphicsConfig, GlowStrength) {
    check(oj::glowStrength(0) == 0.0f && oj::glowStrength(1) == 0.5f && oj::glowStrength(2) == 0.02f &&
              oj::glowStrength(70) == 0.7f && oj::glowStrength(100) == 1.0f && oj::glowStrength(101) == 0.5f &&
              oj::glowStrength(-1) == 0.5f,
          "glow strength of the kept glow");
    check(oj::glowSetting(0.0f) == 0 && oj::glowSetting(0.004f) == 0 && oj::glowSetting(0.01f) == 2 &&
              oj::glowSetting(0.5f) == 50 && oj::glowSetting(0.8f) == 80 && oj::glowSetting(1.0f) == 100 &&
              oj::glowSetting(1.5f) == 100 && oj::glowSetting(-0.5f) == 0,
          "kept glow of a strength");
    oj::GraphicsConfig c;
    oj::graphicsDefaults(c);
    c.glow = 37;
    oj::GraphicsConfig back;
    oj::graphicsFromRegCf1(back, oj::graphicsRegCf1(c));
    check(back.glow == 37, "the glow's percent through reg.cf1");
}
