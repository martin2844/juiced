// Integration tests: the fixed build's heat haze pass on a Direct3D 9 device (Render/D3D9/D3D9HeatHaze, JuicedConfigExt's
// HeatHaze shaders): a quad of haze over the edge of two colours moves the frame there, nowhere else; off, no pass; the
// same over a scene target (the stock's effects draw the scene there), with MSAA and without.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

using oj::test::check;

struct HazeVertex {
    float x, y, z;
    uint32_t colour;
    float u, v;
};

// A 256 x 256 frame, red on the left, green on the right, with a quad of haze over its middle (the clip matrix the
// identity: the quad -0.5 .. 0.5; no distortion map, so the haze shifts by its most); the changed pixels in the quad and
// outside it, -1 when the device or the pass failed.
struct Changed {
    int inside = -1, outside = -1;
};

// sceneTarget: the frame drawn into a scene target (IRenderDevice::multisampleTarget) and read from its texture;
// msaa: the device's MSAA (the fixed build's setting).
Changed hazeOverEdge(bool enabled, bool sceneTarget = false, uint32_t msaa = 0) {
    oj::test::HiddenWindow window(256, 256);
    if (window.get() == nullptr) return {};
    oj::render::DeviceDesc desc = oj::test::realDesc(window.get());
    desc.heatHaze = enabled;
    desc.msaa = msaa;
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, desc, &error);
    if (device == nullptr) return {};
    const oj::render::VertexElement elements[] = {
        {0, 0, 2, 0, 0, 0}, {0, 12, 4, 0, 10, 0}, {0, 16, 1, 0, 5, 0}, {0xFF, 0, 17, 0, 0, 0}};
    const auto declaration = device->createDeclaration(elements);
    const auto edge = [&] {
        device->setViewport({0, 0, 128, 256, 0.0f, 1.0f});
        device->clear(0x3, 0xFFFF0000, 1.0f, 0);
        device->setViewport({128, 0, 128, 256, 0.0f, 1.0f});
        device->clear(0x3, 0xFF00FF00, 1.0f, 0);
        device->setViewport({0, 0, 256, 256, 0.0f, 1.0f});
    };
    bool begun = true;
    const auto haze = [&] {
        const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        device->setVertexShaderConstantF(18, identity, 4);
        device->setDeclaration(declaration);
        begun = device->beginHeatHaze() == 256;
        const HazeVertex quad[4] = {{-0.5f, 0.5f, 0.5f, 0xFF808080u, 0.0f, 0.0f},
                                    {0.5f, 0.5f, 0.5f, 0xFF808080u, 1.0f, 0.0f},
                                    {-0.5f, -0.5f, 0.5f, 0xFF808080u, 0.0f, 1.0f},
                                    {0.5f, -0.5f, 0.5f, 0xFF808080u, 1.0f, 1.0f}};
        device->drawHeatHaze(quad, 1, 1.0f);
        device->endHeatHaze();
    };
    if (sceneTarget) {
        const oj::render::TextureId target = device->createRenderTarget(256, 256, false);
        device->multisampleTarget(target);
        const auto frame = [&](bool withHaze, std::vector<uint8_t>& pixels) {
            uint32_t pitch = 0;
            if (!device->beginFrame()) return false;
            device->setRenderTarget(target, 0);
            edge();
            if (withHaze) haze();
            device->setRenderTarget(0, 0);
            device->endFrame();
            return device->readTarget(target, pixels, &pitch) && pitch == 256 * 4;
        };
        std::vector<uint8_t> before, after;
        if (!frame(false, before) || !frame(true, after) || !begun) return {};
        Changed c{0, 0};
        for (int y = 0; y < 256; ++y) {
            for (int x = 0; x < 256; ++x) {
                const size_t at = (static_cast<size_t>(y) * 256 + static_cast<size_t>(x)) * 4;
                if (std::memcmp(&before[at], &after[at], 3) == 0) continue;
                const bool in = x >= 63 && x < 193 && y >= 63 && y < 193;
                ++(in ? c.inside : c.outside);
            }
        }
        return c;
    }
    const oj::test::Bitmap before = oj::test::captureFrame(*device, edge);
    const oj::test::Bitmap after = oj::test::captureFrame(*device, [&] {
        edge();
        const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        device->setVertexShaderConstantF(18, identity, 4);
        device->setDeclaration(declaration);
        begun = device->beginHeatHaze() == 256;
        const HazeVertex quad[4] = {{-0.5f, 0.5f, 0.5f, 0xFF808080u, 0.0f, 0.0f},
                                    {0.5f, 0.5f, 0.5f, 0xFF808080u, 1.0f, 0.0f},
                                    {-0.5f, -0.5f, 0.5f, 0xFF808080u, 0.0f, 1.0f},
                                    {0.5f, -0.5f, 0.5f, 0xFF808080u, 1.0f, 1.0f}};
        device->drawHeatHaze(quad, 1, 1.0f);
        device->endHeatHaze();
    });
    if (before.width != 256 || after.width != 256 || (enabled && !begun) || (!enabled && begun)) return {};
    Changed c{0, 0};
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            if (before.pixel(x, y) == after.pixel(x, y)) continue;
            const bool in = x >= 63 && x < 193 && y >= 63 && y < 193;  // the quad: 64 .. 192
            ++(in ? c.inside : c.outside);
        }
    }
    return c;
}

}  // namespace

OJ_INTEGRATION_TEST(HeatHazePass, OverEdge) {
    const Changed on = hazeOverEdge(true);
    check(on.inside >= 0, "a Direct3D 9 device and the pass begun over the 256 pixels of the view");
    check(on.inside > 100 && on.outside == 0, "the edge under the haze moved (" + std::to_string(on.inside) +
                                                  " pixels), nothing outside it (" + std::to_string(on.outside) + ")");
    const Changed off = hazeOverEdge(false);
    check(off.inside == 0 && off.outside == 0, "off: no pass, the frame as it was");
}

// The stock's motion blur, glow or depth of field draw the scene into a scene target: the haze there too, with the
// target's samples (MSAA 4) and without.
OJ_INTEGRATION_TEST(HeatHazePass, OverASceneTarget) {
    const Changed plain = hazeOverEdge(true, true, 0);
    check(plain.inside > 100 && plain.outside == 0, "a scene target: the edge under the haze moved (" +
                                                        std::to_string(plain.inside) + " pixels, " +
                                                        std::to_string(plain.outside) + " outside)");
    const Changed samples = hazeOverEdge(true, true, 4);
    check(samples.inside > 100 && samples.outside == 0, "a scene target with MSAA 4: the edge moved (" +
                                                            std::to_string(samples.inside) + " pixels, " +
                                                            std::to_string(samples.outside) + " outside)");
}
