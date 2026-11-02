// Unit tests: the fixed build's wide screens (Render/Display.h) and the camera's widening (Render/Camera.h).

#include "Framework/Test.h"

#include "Render/Camera.h"
#include "Render/Display.h"

#include <cmath>

namespace {

using oj::test::check;
using oj::render::AspectMode;

bool near(float a, float b) {
    return std::fabs(a - b) < 0.01f;
}

}  // namespace

// The stock's mode: the whole screen the frame and the zone, no widening; a settings' value past the list is the
// screen's own shape.
OJ_UNIT_TEST(Display, Stock) {
    const oj::render::Display d = oj::render::displayOf(AspectMode::Stock, 1920, 1080);
    check(d.stock && d.frameX == 0.0f && d.frameW == 1920.0f && d.frameH == 1080.0f && d.zoneW == 1920.0f &&
              d.widen == 1.0f && d.widenY == 1.0f,
          "stock: the whole screen, no widening");
    check(oj::render::aspectModeOf(1) == AspectMode::Auto && oj::render::aspectModeOf(4) == AspectMode::Ratio16x9 &&
              oj::render::aspectModeOf(99) == AspectMode::Auto,
          "1 auto, 4 16:9, 99 auto");
}

// The screen's own shape: 16:9 (the 4:3 frame in the middle, Hor+ 4/3), 21:9 (2560 x 1080 wider than 21:9: the zone
// 2520 wide), 32:9 (the zone 2520 of 3840), 5:4 (the frame 960 high in the middle, Vert+ 16/15).
OJ_UNIT_TEST(Display, OwnShape) {
    const oj::render::Display hd = oj::render::displayOf(AspectMode::Auto, 1920, 1080);
    check(near(hd.frameX, 240.0f) && near(hd.frameW, 1440.0f) && near(hd.frameH, 1080.0f) && near(hd.zoneW, 1920.0f) &&
              near(hd.widen, 4.0f / 3.0f) && hd.widenY == 1.0f,
          "16:9: the frame 1440 x 1080 at 240, the zone the screen, widen 4/3");
    const oj::render::Display uw = oj::render::displayOf(AspectMode::Auto, 2560, 1080);
    check(near(uw.zoneW, 2520.0f) && near(uw.zoneX, 20.0f) && near(uw.frameW, 1440.0f), "2560 x 1080: the zone 2520");
    const oj::render::Display sw = oj::render::displayOf(AspectMode::Auto, 3840, 1080);
    check(near(sw.zoneW, 2520.0f) && near(sw.zoneX, 660.0f) && near(sw.widen, 2.6667f), "32:9: the zone 2520 at 660");
    const oj::render::Display sx = oj::render::displayOf(AspectMode::Auto, 1280, 1024);
    check(near(sx.frameW, 1280.0f) && near(sx.frameH, 960.0f) && near(sx.frameY, 32.0f) && sx.widen == 1.0f &&
              near(sx.widenY, 16.0f / 15.0f),
          "5:4: the frame 1280 x 960 at 32 down, Vert+ 16/15");
}

// The menus' zone: 16:9 and 16:10 the screen, 21:9 (2560 x 1080) and 32:9 (3840 x 1080) a 16:9 one in the middle
// (1920 at 320 and 960), 5:4 the frame (the screen's width).
OJ_UNIT_TEST(Display, MenuZone) {
    const oj::render::Display hd = oj::render::displayOf(AspectMode::Auto, 1920, 1080);
    const oj::render::Display wxga = oj::render::displayOf(AspectMode::Auto, 1680, 1050);
    const oj::render::Display uw = oj::render::displayOf(AspectMode::Auto, 2560, 1080);
    const oj::render::Display sw = oj::render::displayOf(AspectMode::Auto, 3840, 1080);
    const oj::render::Display sx = oj::render::displayOf(AspectMode::Auto, 1280, 1024);
    const oj::render::Display stock = oj::render::displayOf(AspectMode::Stock, 1920, 1080);
    check(near(hd.menuX, 0.0f) && near(hd.menuW, 1920.0f) && near(wxga.menuW, 1680.0f), "16:9 and 16:10: the screen");
    check(near(uw.menuX, 320.0f) && near(uw.menuW, 1920.0f) && near(sw.menuX, 960.0f) && near(sw.menuW, 1920.0f),
          "21:9 and 32:9: 16:9 in the middle");
    check(near(sx.menuW, 1280.0f) && near(stock.menuW, 1920.0f), "5:4 the frame, the stock's the screen");
}

// A shape asked for on another mode's pixels: 16:9 shown from 1024 x 768 (its pixels 4/3 wide): the frame 768 pixels
// wide in the middle, widen 4/3.
OJ_UNIT_TEST(Display, AskedShape) {
    const oj::render::Display d = oj::render::displayOf(AspectMode::Ratio16x9, 1024, 768);
    check(near(d.frameW, 768.0f) && near(d.frameX, 128.0f) && near(d.frameH, 768.0f) && near(d.widen, 4.0f / 3.0f),
          "16:9 on 1024 x 768: the frame 768 wide at 128, widen 4/3");
}

// The camera's widening: the projection's across by the tangent times 4/3, its down and the parts' distances as
// without; 1 and 1 the same projection as never widened.
OJ_UNIT_TEST(Display, CameraWiden) {
    oj::render::Camera plain;
    plain.setAspect(0.75f);
    plain.setFov(1.2f, 0.0f, false);
    oj::render::Camera wide;
    wide.setAspect(0.75f);
    wide.setWiden(4.0f / 3.0f, 1.0f);
    wide.setFov(1.2f, 0.0f, false);
    const float across = plain.projection()[0] / wide.projection()[0];
    check(near(across, 4.0f / 3.0f) && plain.projection()[5] == wide.projection()[5] &&
              plain.lodScale() == wide.lodScale() && plain.fovSin() == wide.fovSin(),
          "Hor+: across x 4/3, down, the parts' distances and fovSin as before");
    wide.setWiden(1.0f, 1.0f);
    check(plain.projection()[0] == wide.projection()[0], "1 and 1: the stock's projection");
}

// The race camera's angle across at rest in degrees: 66.6 on 4:3 (the stock's 1.1623551), 82.4 on 16:9 (Hor+), 99.6
// on 4:3 with the most view (1.8); the scale of an angle the other way.
OJ_UNIT_TEST(Display, RestFov) {
    const oj::render::Display square = oj::render::displayOf(AspectMode::Stock, 640, 480);
    const oj::render::Display hd = oj::render::displayOf(AspectMode::Auto, 1920, 1080);
    check(near(oj::render::restFovDegrees(square, 1.0f), 66.598f) &&
              near(oj::render::restFovDegrees(hd, 1.0f), 82.424f) &&
              near(oj::render::restFovDegrees(square, 1.8f), 99.552f),
          "the angle across: 66.6 on 4:3, 82.4 on 16:9, 99.6 with the view 1.8");
    check(near(oj::render::fovScaleOf(hd, oj::render::restFovDegrees(hd, 1.3f)), 1.3f), "the scale back from degrees");
}
