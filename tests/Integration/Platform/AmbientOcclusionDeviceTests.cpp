// Integration tests: WORLD LIGHT HI's ambient occlusion on a Direct3D 9 device (Render/D3D9, JuicedConfigExt's
// Shared/AmbientOcclusionPass): a depth of a flat floor seen square on darkens nothing; a step darkens the far side
// along it and nothing else.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Render/RenderDevice.h"

#include <windows.h>
#include <d3d9.h>

#include <string>

namespace {

using oj::test::check;

bool shaders30() {
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3d == nullptr) return false;
    D3DCAPS9 caps{};
    const bool ok = SUCCEEDED(d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)) &&
                    caps.PixelShaderVersion >= D3DPS_VERSION(3, 0) && caps.VertexShaderVersion >= D3DVS_VERSION(3, 0);
    d3d->Release();
    return ok;
}

// The darkened pixels of a white 128 x 128 view: near the middle column (within 12 pixels of it) left and right of it,
// and far from it; -1 without a frame.
struct Darkened {
    int left = -1, right = -1, elsewhere = -1;
};

Darkened occluded(bool step) {
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return {};
    const oj::render::TextureId depth = device->createFloatTarget(64, 64);
    check(depth != 0, "the depth's float target");
    if (depth == 0) return {};
    // The depth 128 / 255 of the range everywhere, with the step its left half 118 / 255 (39 nearer: past the half
    // sphere's points over the far side, within its radius).
    device->setRenderTarget(depth, 0);
    device->clear(oj::render::ClearFlags::Target, 0xFF800000u, 1.0f, 0);
    if (step) {
        device->setViewport({0, 0, 32, 64, 0.0f, 1.0f});
        device->clear(oj::render::ClearFlags::Target, 0xFF760000u, 1.0f, 0);
    }
    device->setRenderTarget(0, 0);
    oj::render::AmbientOcclusionView view;
    view.tanX = 1.0f;
    view.tanY = 1.0f;
    view.range = 1000.0f;
    view.radius = 60.0f;
    view.strength = 1.0f;
    view.bias = 1.0f;
    view.fadeFrom = 100000.0f;
    bool ran = false;
    const oj::test::Bitmap frame = oj::test::captureFrame(*device, [&] {
        device->clear(0x3, 0xFFFFFFFFu, 1.0f, 0);
        ran = device->ambientOcclusion(depth, view);
    });
    check(ran, "the pass ran");
    if (frame.width != 128) return {};
    Darkened d{0, 0, 0};
    for (int32_t y = 8; y < 120; ++y) {
        for (int32_t x = 0; x < 128; ++x) {
            if (((frame.pixel(x, y) >> 8) & 0xFF) > 240) continue;
            if (x >= 52 && x < 64) ++d.left;
            else if (x >= 64 && x < 76) ++d.right;
            else ++d.elsewhere;
        }
    }
    return d;
}

}  // namespace

OJ_INTEGRATION_TEST(AmbientOcclusion, OnARealDevice) {
    if (!shaders30()) return;
    const Darkened flat = occluded(false);
    const Darkened step = occluded(true);
    if (flat.elsewhere < 0 || step.elsewhere < 0) return;
    check(flat.left == 0 && flat.right == 0 && flat.elsewhere == 0, "a flat floor: nothing darkened");
    check(step.right > 100 && step.left == 0 && step.elsewhere == 0,
          "a step: its far side darkened along it (" + std::to_string(step.right) + " pixels), its near side and " +
              "the rest not (" + std::to_string(step.left) + ", " + std::to_string(step.elsewhere) + ")");
}
