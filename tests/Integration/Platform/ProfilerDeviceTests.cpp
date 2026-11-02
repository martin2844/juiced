// Integration tests: the frame's layers timed on a Direct3D 9 device (the debug builds' --profile,
// Render/D3D9/D3D9Profiler): each marked layer reported with its draws, the frames counted; off, nothing timed.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Render/RenderDevice.h"

#include <string>

namespace {

using oj::test::check;

}  // namespace

OJ_INTEGRATION_TEST(Profiler, LayersTimed) {
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    check(device->profileReport() == "profile: no frame timed", "off: nothing timed");
    if (!device->setProfiling(true)) return;  // an adapter without timestamps
    const oj::render::QuadVertex quad[4] = {{0, 0, 0, 0, 0xFFFFFFFFu}, {64, 0, 1, 0, 0xFFFFFFFFu},
                                            {0, 64, 0, 1, 0xFFFFFFFFu}, {64, 64, 1, 1, 0xFFFFFFFFu}};
    for (int frame = 0; frame < 10; ++frame) {
        if (!device->beginFrame()) continue;
        device->profileMark("first layer");
        device->clear(oj::render::ClearFlags::Target, 0xFF202020u, 1.0f, 0);
        device->profileMark("second layer");
        device->drawQuad(quad, 0, oj::render::Blend::Alpha, false);
        device->drawQuad(quad, 0, oj::render::Blend::Alpha, false);
        device->endFrame();
    }
    const std::string report = device->profileReport();
    check(report.starts_with("profile: 10 frames"), "the frames counted: " + report.substr(0, 40));
    check(report.find("first layer") != std::string::npos && report.find("second layer") != std::string::npos &&
              report.find("present") != std::string::npos,
          "each layer and the present in the report");
    check(device->setProfiling(false) == false && device->profileReport() == "profile: no frame timed",
          "off again: the sums gone");
}
