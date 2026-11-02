// Integration tests: WORLD LIGHT ULTRA's lights reflected in the road on a Direct3D 9 device (Render/D3D9,
// JuicedConfigExt's Shared/LightReflectionPass): the pass runs, and over a view without road (no pixel of the depth
// marked) it adds nothing, a bright view and a street lamp's streak alike.

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

}  // namespace

OJ_INTEGRATION_TEST(LightReflections, OnARealDevice) {
    if (!shaders30()) return;
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    // A depth of a wall half the range away (no road marked) and a street lamp's streak down the middle.
    const oj::render::TextureId depth = device->createFloatTarget(64, 64);
    check(depth != 0, "the depth's float target");
    if (depth == 0) return;
    oj::render::LightReflectionView view;
    view.range = 1000.0f;
    view.reach = 2000.0f;
    view.streakCount = 1;
    view.streaks[0] = {0.5f, 0.6f, 0.05f, 0.2f, 0.5f, 0.5f, 0.5f, 0.3f};
    const auto run = [&](uint32_t depthColour, bool& ran) {
        device->setRenderTarget(depth, 0);
        device->clear(oj::render::ClearFlags::Target, depthColour, 1.0f, 0);
        device->setRenderTarget(0, 0);
        return oj::test::captureFrame(*device, [&] {
            device->clear(0x3, 0xFF808080u, 1.0f, 0);
            ran = device->lightReflections(depth, view);
        });
    };
    bool ran = false;
    const oj::test::Bitmap wall = run(0xFF800000u, ran);
    check(ran, "the pass ran");
    if (wall.width != 128) return;
    int changed = 0;
    for (int32_t y = 0; y < 128; ++y) {
        for (int32_t x = 0; x < 128; ++x) {
            if ((wall.pixel(x, y) & 0xFFFFFFu) != 0x808080u) ++changed;
        }
    }
    check(changed == 0, "no road in the depth: nothing added (" + std::to_string(changed) + " pixels changed)");
}
