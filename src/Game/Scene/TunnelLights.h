#pragma once

// The fixed build's headlights by a dry day (the user, 2026-10-28: by night and in the rain the cars' lights are on, by
// a dry day never, but on in a tunnel and off past its end): a car's lights come on as it enters one of the scene's
// tunnel* zones (SceneObjects::inTunnel, 0x00422D20) and go off a little after it left, the light fading in and out.

namespace oj::scene {

constexpr float kTunnelLightsOn = 0.2f;     // seconds the light takes to come on
constexpr float kTunnelLightsDelay = 1.0f;  // seconds past the tunnel before the lights go off
constexpr float kTunnelLightsOff = 0.4f;    // seconds the light takes to go out

// A car's lights in the tunnels of a dry day.
struct TunnelLights {
    bool on = false;       // the lights shown (their flares and lit lamps)
    float level = 0.0f;    // the light's share of its strength, 0 .. 1
    float outside = 0.0f;  // seconds since the car left the tunnel

    // A step of `dt` seconds with the car in a tunnel or not.
    void step(bool inTunnel, float dt);
};

}  // namespace oj::scene
