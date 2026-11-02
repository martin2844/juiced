#include "Render/Display.h"

#include <algorithm>
#include <cmath>

namespace oj::render {

namespace {

AspectMode g_mode = AspectMode::Stock;

constexpr float kFrameAspect = 4.0f / 3.0f;
constexpr float kZoneAspect = 21.0f / 9.0f;
constexpr float kMenuAspect = 16.0f / 9.0f;

}  // namespace

AspectMode aspectModeOf(uint32_t setting) {
    return setting <= static_cast<uint32_t>(AspectMode::Ratio32x9) ? static_cast<AspectMode>(setting)
                                                                     : AspectMode::Auto;
}

Display displayOf(AspectMode mode, uint32_t width, uint32_t height) {
    Display d;
    d.width = static_cast<float>(width);
    d.height = static_cast<float>(height);
    d.frameW = d.width;
    d.frameH = d.height;
    d.zoneW = d.width;
    d.menuW = d.width;
    if (mode == AspectMode::Stock || width == 0 || height == 0) return d;
    d.stock = false;
    switch (mode) {
    case AspectMode::Ratio5x4: d.aspect = 5.0f / 4.0f; break;
    case AspectMode::Ratio16x10: d.aspect = 16.0f / 10.0f; break;
    case AspectMode::Ratio16x9: d.aspect = 16.0f / 9.0f; break;
    case AspectMode::Ratio21x9: d.aspect = 21.0f / 9.0f; break;
    case AspectMode::Ratio32x9: d.aspect = 32.0f / 9.0f; break;
    default: d.aspect = d.width / d.height; break;
    }
    // A pixel's width over its height as the screen shows it (1 for square pixels).
    const float pixel = d.aspect * d.height / d.width;
    if (d.aspect >= kFrameAspect) {
        d.frameH = d.height;
        d.frameW = d.height * kFrameAspect / pixel;
    } else {
        d.frameW = d.width;
        d.frameH = d.width * pixel / kFrameAspect;
    }
    d.frameX = (d.width - d.frameW) * 0.5f;
    d.frameY = (d.height - d.frameH) * 0.5f;
    d.zoneW = std::min(d.width, d.height * kZoneAspect / pixel);
    d.zoneX = (d.width - d.zoneW) * 0.5f;
    d.menuW = std::max(d.frameW, std::min(d.width, d.height * kMenuAspect / pixel));
    d.menuX = (d.width - d.menuW) * 0.5f;
    d.widen = std::max(d.aspect, kFrameAspect) / kFrameAspect;
    d.widenY = kFrameAspect / std::min(d.aspect, kFrameAspect);
    return d;
}

namespace {

// The tangent of half the race camera's angle across at rest on a 4:3 picture (the stock's 1.1623551, 0x006FFBC8).
float restTangent() {
    return static_cast<float>(std::tan(1.1623551 * 0.5));
}

constexpr double kDegrees = 57.29577951308232;

}  // namespace

float restFovDegrees(const Display& d, float scale) {
    return static_cast<float>(std::atan(restTangent() * d.widen * scale) * 2.0 * kDegrees);
}

float fovScaleOf(const Display& d, float degrees) {
    return static_cast<float>(std::tan(degrees / kDegrees * 0.5) / (restTangent() * d.widen));
}

void setAspectMode(AspectMode mode) {
    g_mode = mode;
}

AspectMode aspectMode() {
    return g_mode;
}

Display display(uint32_t width, uint32_t height) {
    return displayOf(g_mode, width, height);
}

}  // namespace oj::render
