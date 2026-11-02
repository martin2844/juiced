// Integration tests: the fixed build's window without a frame on a Direct3D 9 device (Render/D3D9, DeviceDesc's
// picture): the game draws into a surface of its resolution, its screen that size, the screenshot the picture alone,
// a reset (the window's client changed) keeping it; with MSAA too.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include <string>

namespace {

using oj::test::check;

// A 160 x 200 picture in a 320 x 200 window: its screen, a red frame's screenshot, the same after the window's client
// changed to 400 x 300 (a reset).
void pictureSurface(uint32_t msaa) {
    oj::test::HiddenWindow window(320, 200);
    check(window.get() != nullptr, "a hidden window");
    if (window.get() == nullptr) return;
    oj::render::DeviceDesc desc = oj::test::realDesc(window.get());
    desc.msaa = msaa;
    desc.pictureWidth = 160;
    desc.pictureHeight = 200;
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, desc, &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    check(device->width() == 160 && device->height() == 200 && device->targetWidth(0) == 160,
          "the game's screen: the picture's 160 x 200");
    check(device->description().find("the picture 160x200") != std::string::npos, "the picture's surface made");

    const auto shot = [&](uint32_t argb) {
        return oj::test::captureFrame(*device, [&] { device->clear(0x1 /* D3DCLEAR_TARGET */, argb, 1.0f, 0); });
    };
    const oj::test::Bitmap red = shot(0xFFFF0000);
    check(red.width == 160 && red.height == 200 && red.pixel(0, 0) == 0xFF0000,
          "the screenshot: the picture alone, 160 x 200, red");

    const uint32_t resets = device->resets();
    device->resize(400, 300);
    const oj::test::Bitmap green = shot(0xFF00FF00);
    check(device->resets() == resets + 1, "the window's new client: a reset");
    check(device->width() == 160 && device->height() == 200 && green.width == 160 && green.height == 200 &&
              green.pixel(0, 0) == 0x00FF00,
          "after the reset: the picture's surface again, 160 x 200, green");
}

}  // namespace

OJ_INTEGRATION_TEST(ScreenModes, PictureSurface) {
    pictureSurface(0);
}

OJ_INTEGRATION_TEST(ScreenModes, PictureSurfaceMsaa) {
    pictureSurface(4);
}
