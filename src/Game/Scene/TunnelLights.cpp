#include "Game/Scene/TunnelLights.h"

#include <algorithm>

namespace oj::scene {

void TunnelLights::step(bool inTunnel, float dt) {
    if (inTunnel) {
        on = true;
        outside = 0.0f;
    } else if (on) {
        outside += dt;
    }
    if (!on) return;
    if (outside < kTunnelLightsDelay) {
        level = std::min(1.0f, level + dt / kTunnelLightsOn);
        return;
    }
    level = std::max(0.0f, level - dt / kTunnelLightsOff);
    if (level <= 0.0f) {
        on = false;
        outside = 0.0f;
    }
}

}  // namespace oj::scene
