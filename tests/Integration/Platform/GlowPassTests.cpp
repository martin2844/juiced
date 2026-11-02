// Integration tests: the game's glow (GLOW of the graphics options, "Light flares"; Game/Scene/PostEffects glow) on a
// Direct3D 9 device with the game's effects: a grey square on black, the glow on and off.

#include "Framework/Fixtures.h"
#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Core/Saves/GraphicsConfig.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"
#include "Render/Camera.h"

#include <string>

namespace {

using oj::test::check;

// The red byte of the middle of a grey (0x60) 64 x 64 square in the middle of a 256 x 256 frame after six frames with
// the glow setting `glow` (the atmospheres' LightBlur 0.5), and the mean red byte of the ring 1 .. 3 pixels around it;
// -1 when the device or the frame failed.
struct Glow {
    double middle = -1.0, ring = -1.0;
};

Glow glowOfSquare(int32_t glow) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return {};
    oj::test::HiddenWindow window(256, 256);
    if (window.get() == nullptr) return {};
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    if (device == nullptr) return {};
    oj::GraphicsConfig& c = oj::graphicsConfig();
    const oj::GraphicsConfig kept = c;
    c.noShaders = 0;
    c.shaders = 1;
    c.sceneWidth = 256;
    c.sceneHeight = 256;
    c.wideBlurWidth = 0x200;  // the settings' sizes of the blur targets
    c.wideBlurHeight = 0x100;
    c.blurWidth = 0x100;
    c.blurHeight = 0x100;
    c.motionBlur = 0;
    c.depthOfField = 0;
    c.glow = glow;
    Glow result;
    {
        oj::scene::Engine engine(*device, oj::test::sharedGameData().tables, 3, false);
        engine.renderer().lightBlur = 0.5f;
        oj::scene::PostEffects& post = engine.postEffects();
        oj::render::Camera camera;
        oj::scene::PostEffects::View view;
        const auto frame = [&] {
            engine.frameParameters();
            post.begin(camera, view, 0, 0, 256, 256, false);
            device->setViewport({96, 96, 64, 64, 0.0f, 1.0f});
            device->clear(0x1, 0xFF606060u, 1.0f, 0);
            device->setViewport({0, 0, 256, 256, 0.0f, 1.0f});
            post.end(camera, view, 0.0f);
        };
        bool drawn = true;
        for (int i = 0; i < 5 && drawn; ++i) {
            drawn = device->beginFrame();
            if (drawn) {
                frame();
                device->endFrame();
            }
        }
        const oj::test::Bitmap bmp = drawn ? oj::test::captureFrame(*device, frame) : oj::test::Bitmap{};
        if (bmp.width == 256) {
            double sum = 0.0;
            int count = 0;
            for (int y = 93; y < 163; ++y) {
                for (int x = 93; x < 163; ++x) {
                    if (x >= 96 && x < 160 && y >= 96 && y < 160) continue;
                    sum += static_cast<double>((bmp.pixel(x, y) >> 16) & 0xFF);
                    ++count;
                }
            }
            result.middle = static_cast<double>((bmp.pixel(128, 128) >> 16) & 0xFF);
            result.ring = count > 0 ? sum / count : -1.0;
        }
    }
    c = kept;
    return result;
}

}  // namespace

// Off: the square as drawn (0x60), nothing around it. On (1, or any other value but 0, as a percent the earlier fixed
// build kept): as the stock's (Light flares as the retail game), the glow's blur goes into the exposure's measure
// (0x005213B5: the target taken last still the back buffer), so nothing shows around the square (1.02.5 drew the blur
// onto the screen: a ring of it); the square follows the exposure, whose measure is one for the process (its history
// differs run to run).
OJ_INTEGRATION_TEST(GlowPass, AsTheStock) {
    const Glow off = glowOfSquare(0);
    const Glow on = glowOfSquare(1);
    const Glow kept = glowOfSquare(50);
    check(off.middle >= 0.0 && on.middle >= 0.0 && kept.middle >= 0.0, "a Direct3D 9 device and the game's effects");
    check(off.middle == 96.0 && off.ring == 0.0, "off: the square as drawn (" + std::to_string(off.middle) + ")");
    check(on.ring == 0.0 && kept.ring == 0.0, "on, or a kept percent: no blur around the square, as the stock (" +
                                                  std::to_string(on.ring) + ", " + std::to_string(kept.ring) + ")");
}
