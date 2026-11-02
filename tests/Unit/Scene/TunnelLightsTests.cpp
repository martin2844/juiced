// Unit tests: the fixed build's headlights in the tunnels of a dry day (Game/Scene/TunnelLights.h): on as the car
// enters, the light coming up; on past the tunnel for the delay, then going out and off.

#include "Framework/Test.h"

#include "Game/Scene/TunnelLights.h"

#include <cmath>

namespace {

using oj::test::check;

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

}  // namespace

OJ_UNIT_TEST(TunnelLights, OnInATunnel) {
    oj::scene::TunnelLights t;
    t.step(false, 0.1f);
    check(!t.on && t.level == 0.0f, "outside a tunnel: off");
    t.step(true, oj::scene::kTunnelLightsOn * 0.5f);
    check(t.on && near(t.level, 0.5f), "in a tunnel: on, the light half way up");
    t.step(true, oj::scene::kTunnelLightsOn);
    check(t.on && t.level == 1.0f, "then all of it");
}

OJ_UNIT_TEST(TunnelLights, OffPastTheTunnel) {
    oj::scene::TunnelLights t;
    t.step(true, 1.0f);
    t.step(false, oj::scene::kTunnelLightsDelay * 0.5f);
    check(t.on && t.level == 1.0f, "past the tunnel within the delay: still on");
    t.step(true, 0.1f);
    t.step(false, oj::scene::kTunnelLightsDelay * 0.9f);
    check(t.on && t.level == 1.0f, "back in a tunnel: the delay starts again");
    t.step(false, oj::scene::kTunnelLightsDelay * 0.1f);
    t.step(false, oj::scene::kTunnelLightsOff * 0.5f);
    check(t.on && t.level < 1.0f && t.level > 0.0f, "past the delay: going out");
    t.step(false, oj::scene::kTunnelLightsOff);
    check(!t.on && t.level == 0.0f && t.outside == 0.0f, "then off");
}
