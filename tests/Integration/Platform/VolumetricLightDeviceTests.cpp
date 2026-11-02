// Integration tests: WORLD LIGHT ULTRA's light in the night's haze on a Direct3D 9 device (Render/D3D9,
// JuicedConfigExt's Shared/VolumetricLightPass): a lamp's cone across the middle of the view lightens it, the view's
// corners outside the cone little; with no light the pass does not run.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Game/Scene/VolumetricLight.h"
#include "Render/Camera.h"
#include "Render/RenderDevice.h"

#include <windows.h>
#include <d3d9.h>

#include <cmath>
#include <string>
#include <vector>

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

OJ_INTEGRATION_TEST(VolumetricLight, OnARealDevice) {
    if (!shaders30()) return;
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    const oj::render::TextureId depth = device->createFloatTarget(64, 64);
    check(depth != 0, "the depth's float target");
    if (depth == 0) return;
    // The view: a camera at the world's origin looking along its z at a wall 5 m away (the depth half the range); a
    // lamp 4 m ahead and 3 m up looking down, its cone across the middle of the view.
    oj::render::Camera camera;
    camera.setFov(static_cast<float>(std::atan(0.5) * 2.0), static_cast<float>(std::atan(0.5) * 2.0), false);
    camera.resetMatrices();
    camera.invalidate();
    camera.update();
    const std::vector<oj::render::VolumetricLight> lights{oj::scene::lampCone({0.0f, 300.0f, 400.0f, 0.0f}, 1.0f, 8)};
    oj::render::VolumetricLightView view = oj::scene::volumetricView(camera, lights, 1.0f);
    view.range = 1000.0f;
    view.density = 1.0f;
    device->setRenderTarget(depth, 0);
    device->clear(oj::render::ClearFlags::Target, 0xFF800000u, 1.0f, 0);
    device->setRenderTarget(0, 0);
    bool ran = false;
    const oj::test::Bitmap frame = oj::test::captureFrame(*device, [&] {
        device->clear(0x3, 0xFF202020u, 1.0f, 0);
        ran = device->volumetricLight(depth, view);
    });
    check(ran, "the pass ran");
    if (frame.width != 128) return;
    const uint32_t middle = frame.pixel(64, 64) & 0xFFu, corner = frame.pixel(2, 2) & 0xFFu;
    check(middle > 0x80 && corner < 0x30, "the cone lightens the middle (" + std::to_string(middle) +
                                              "), a corner outside it little (" + std::to_string(corner) + ")");
    oj::render::VolumetricLightView none = view;
    none.count = 0;
    check(!device->volumetricLight(depth, none), "no light: the pass does not run");
}
