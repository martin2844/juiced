// Integration tests: the fixed build's MSAA with the stock's effects (IRenderDevice::multisampleTarget, the scene's
// targets of Game/Scene/PostEffects) on a Direct3D 9 device: the scene's target drawn with the frame's samples and
// resolved into its texture, so its edges are smoothed as the back buffer's.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Render/RenderDevice.h"

#include <windows.h>
#include <d3d9.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using oj::test::check;

// The adapter takes 4x MSAA in a window (the fixed build's setting MSAA 4).
bool fourSamples() {
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3d == nullptr) return false;
    DWORD levels = 0;
    const bool ok = SUCCEEDED(d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE,
                                                              D3DMULTISAMPLE_4_SAMPLES, &levels)) &&
                    levels != 0;
    d3d->Release();
    return ok;
}

// The pixels part white (neither 0 nor full) of a 64 x 64 target, black with a white triangle over its diagonal,
// drawn with MSAA 4 into a target multisampled (`samples`) or not; -1 without a device or the target's pixels.
int smoothed(bool samples) {
    oj::test::HiddenWindow window(64, 64);
    oj::render::DeviceDesc desc = oj::test::realDesc(window.get());
    desc.msaa = 4;
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, desc, &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return -1;
    const oj::render::TextureId target = device->createRenderTarget(64, 64, false);
    if (samples) device->multisampleTarget(target);
    std::vector<uint8_t> pixels;
    uint32_t pitch = 0;
    if (device->beginFrame()) {
        device->setRenderTarget(target, 0);
        device->clear(0x3, 0xFF000000u, 1.0f, 0);
        const oj::render::DebugVertex triangle[3] = {{0.0f, 0.0f, 0xFFFFFFFFu}, {64.0f, 0.0f, 0xFFFFFFFFu},
                                                     {0.0f, 64.0f, 0xFFFFFFFFu}};
        device->drawDebugTriangles(triangle, 3);
        device->setRenderTarget(0, 0);
        device->endFrame();
    }
    if (!device->readTarget(target, pixels, &pitch) || pitch < 64 * 4) return -1;
    int partial = 0;
    for (uint32_t y = 0; y < 64; ++y) {
        for (uint32_t x = 0; x < 64; ++x) {
            const uint8_t green = pixels[static_cast<size_t>(y) * pitch + x * 4 + 1];
            if (green > 16 && green < 240) ++partial;
        }
    }
    device->destroyTexture(target);
    return partial;
}

}  // namespace

// MSAA 4: the scene's target's diagonal edge part covered in its texture; a target not multisampled hard (the stock's
// effects lost the MSAA).
OJ_INTEGRATION_TEST(SceneTargetMsaa, EdgesSmoothed) {
    if (!fourSamples()) return;
    const int plain = smoothed(false);
    const int samples = smoothed(true);
    if (plain < 0 || samples < 0) return;
    check(plain == 0, "a target not multisampled: a hard edge (" + std::to_string(plain) + " pixels part covered)");
    check(samples >= 32, "a multisampled target: its edge smoothed (" + std::to_string(samples) + " pixels)");
}
