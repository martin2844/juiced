// Integration tests: the frame limiter (Render/FrameOptions waitFrame) on a Direct3D 9 device presenting at once (VSync
// off): 30 frames at a limit of 120 take a quarter of a second, the device's present as quick as it may be (a
// wrapper's, DXVK's, returns at once too).

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Render/FrameOptions.h"

#include <windows.h>

#include <string>

OJ_INTEGRATION_TEST(FrameLimit, HoldsTheLimit) {
    oj::test::HiddenWindow window(160, 120);
    oj::render::DeviceDesc desc = oj::test::realDesc(window.get());
    desc.vsync = false;
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, desc, &error);
    oj::test::check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    LARGE_INTEGER frequency{}, start{}, end{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    int64_t last = start.QuadPart;
    const double interval = oj::render::frameInterval(120, false);
    for (int i = 0; i < 30; ++i) {
        if (device->beginFrame()) {
            device->clear(0x1, 0xFF000000u + static_cast<uint32_t>(i), 1.0f, 0);
            device->endFrame();
        }
        last = oj::render::waitFrame(last, interval);
    }
    QueryPerformanceCounter(&end);
    const double seconds = static_cast<double>(end.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);
    oj::test::check(seconds > 0.24 && seconds < 0.30,
                    "30 frames at 120 a second: a quarter of a second (" + std::to_string(seconds) + " s)");
}
