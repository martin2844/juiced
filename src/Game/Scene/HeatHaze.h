#pragma once
// The fixed build's heat haze (docs/fixes.md Added): JuicedConfigExt's HeatHaze. The game makes a Heat_Haze effect for
// every car (scripts\effectsdefs.txt: particles rising from the car, material heathaze01, whose texture is a
// distortion map) but never draws their list (ParticleSystem::drawFramebuffer, called by nothing). After the particles
// of a camera view the frame is copied, the haze's particles add the shift of their distortion map into a target of
// their own (the more the closer, swaying with time, hidden where the scene is in front of them) and the copy is drawn
// over the view moved by the sum (Render/D3D9's heat haze pass, JuicedConfigExt's Shared/Shaders/HeatHaze.hlsl). Of
// the plugin's own: the haze comes from the exhausts (both pipes of a double exhaust; the game puts its emitter over
// the bonnet), a standing car's at a quarter of its strength up to the full one at 18 km/h, on the throttle it spreads
// into a cone and shifts more, its quads lie along the plume, its particles emitted by the time.
//
// What the haze decides apart from the device and the game's objects: the plume's shape, the sizes and fades.

#include "Core/Base/Matrix.h"

#include <array>
#include <cstdint>
#include <vector>

namespace oj::scene::haze {

using Vec3 = std::array<float, 3>;

// The shift of a particle at a depth of 1 world unit (cm) at 100 %; nearer than kNearest it grows no more; farther than
// kFarthest it falls off as 1 / sqrt(depth). The sway: the shift turns with time by up to this part of its length.
constexpr float kShift = 6.0f;
constexpr float kNearest = 50.0f;
constexpr float kFarthest = 600.0f;
constexpr float kSway = 0.5f;
constexpr float kNeutral = 0.03f;  // the zone around 0.5 of the map that shifts nothing (DXT1 grey: 132 / 130)
constexpr float kLargest = 0.03f;  // the largest shift the 8-bit target stores, in texture units (3 % of the frame)

// The haze of a standing car is at a quarter of its strength, at full strength from kMovingSpeed (cm/s, 18 km/h) on;
// the strength follows the speed within about kMotionTime, the throttle within kThrottleTime (s).
constexpr float kStanding = 0.25f;
constexpr float kMovingSpeed = 500.0f;
constexpr double kMotionTime = 0.5;
constexpr double kMotionStep = 0.03;  // views of the same frame are not new frames
constexpr double kThrottleTime = 0.3;

// An effect's motion as followed from frame to frame.
struct Motion {
    Vec3 position{};
    double time = -1.0;  // < 0: not followed yet
    float strength = kStanding;
    float speed = 0.0f;  // cm/s
    Vec3 velocity{};     // cm/s
    float throttle = 0.0f;
};

// The motion after the node was at `position` and the car at `throttle` at `now` (s).
void follow(Motion& m, const Vec3& position, float throttle, double now);
// The time the motions are followed by: the performance counter's seconds.
double clock();

// The game emits a particle each EMIT_INTERVAL of a timer it counts down by 0.02 a pass, not by the time: the interval
// from the frame's time to kEmitInterval seconds; a car's own from kStandingInterval standing to kEmitInterval from
// kMovingSpeed on, the life of its particles their ring of them.
constexpr float kTimerStep = 0.02f;
constexpr float kEmitInterval = 0.2f;
constexpr float kStandingInterval = 0.1f;
// The emit interval the game's timer step takes for the frame's time `step` (s); 0 for a step out of (1e-4, 1).
float emitInterval(float step);
// The car's own interval at `speed` (cm/s).
float ownInterval(float speed);

// At an exhaust a particle is drawn at kExhaustSize of its size at the pipe, growing with its distance from the emitter
// up to kExhaustSpreadSize: over about kCoastSpread (cm) off the throttle, kThrottleSpread on it. The shift grows with
// the throttle up to kThrottleBoost times and with the speed up to kSpeedBoost times at kFastSpeed (cm/s, 150 km/h).
float exhaustScale(float distance, float throttle);
float boost(float throttle, float speed);
// Farther than kFarSize the particles at an exhaust are drawn larger, as sqrt(depth / kFarSize), up to kMaxFarScale.
constexpr float kFarSize = 800.0f;
constexpr float kMaxFarScale = 1.8f;

// Where the game puts the emitter (over the bonnet, in the car's root axes); the end of a pipe kPipeEnd along the -z of
// its node (where the game starts its exhaust flame).
constexpr Vec3 kGameEmitter = {0.0f, 70.0f, 180.0f};
constexpr float kPipeEnd = 35.0f;

// `d` (from the emitter to a particle of a car at `speed`, cm/s) as drawn: its rise along `up` kept within about 80 cm
// above and 15 below, the rest left behind as by a car of at most 150 cm/s + 5 % of its speed and within about 80 + 3 %
// of its speed: a plume at the back of a slow car, a trail behind a fast one.
Vec3 plume(const Vec3& d, const Vec3& up, float speed);

// The axes in the camera plane (`a`, `b`: unit) of a quad along `dir`: `along` (its long axis), `across`; the share of
// `dir` seen (1 across the view .. 0 along it), where little is seen the axes the game's (along = b).
float orient(const Vec3& dir, const Vec3& a, const Vec3& b, Vec3& along, Vec3& across);

// The mip bias of the 64-texel distortion map for a quad `pixels` wide: +2 for large ones, down to 8 texels across.
float mipBias(float pixels);

// How much of a quad shows by its half size on the screen (a part of it): 1 up to 0.35, 0 from 0.5 on (MAX_VIS_SIZE of
// Heat_Haze, which the PC version keeps but does not use).
float visible(float screenHalf);

// At an exhaust: quads kStretch times longer along the plume on a moving car; the game's fade as (..)^kFade; up to
// kMaxBetween quads between two neighbouring particles where they are apart.
constexpr float kStretch = 2.0f;
constexpr float kFade = 0.75f;
constexpr int kMaxBetween = 3;
constexpr size_t kMaxParticles = 256;  // more in an effect: its quads drawn as the game made them

// A car's exhaust as its haze's emitter takes it each frame: none (the game's place over the bonnet), or the end of its
// pipe, or the middle between the two of a double exhaust (`half` from it to the other pipe); the world's up (hot air
// rises whatever the car's pitch), the way out of its pipes, the car's throttle.
struct Exhaust {
    bool valid = false;
    bool twin = false;
    Vec3 half{};
    Vec3 emitter{};
    Vec3 up{0.0f, 1.0f, 0.0f};
    Vec3 axis{};
    float throttle = 0.0f;  // 0 .. 1
};

// The exhaust of a car whose model's root is at `root` (the node the haze follows) and whose pipes' nodes are at `pipe`
// and `other` (none: nullptr), and the emitter's place in the root's axes (`local`); invalid without a pipe.
Exhaust placeAtExhaust(const Mat34& root, const Mat34* pipe, const Mat34* other, float throttle, Vec3& local);

// The particles' vertex (the effect manager's declaration).
struct Vertex {
    float x, y, z;
    uint32_t colour;
    float u, v;
};

// The camera's axes the game's quads are made on (the particle system's billboard: side, up, view toward the camera).
struct Axes {
    Vec3 side{}, up{}, view{};
};

// The quads of an effect's frame (the game's, 4 vertices a quad) as the haze draws them: each put back where its
// particle is (+ view x its size: the game moves it toward the camera); at an exhaust in the plume, smaller near the
// pipe, larger far away, along the plume and stretched on a moving car (`speed`), twice at a double exhaust, with
// quads between neighbours that are apart; each faded by the car's `strength` and by its size on the screen
// (`toClip`: the world-to-clip matrix as the constants c18 .. c21 hold it), the game's fade at an exhaust as
// (..)^kFade, its mip bias in the colour's green (`viewWidth` the view's pixels). Empty when the effect has more than
// kMaxParticles quads.
std::vector<Vertex> hazeQuads(const std::vector<Vertex>& game, const Axes& axes, const Exhaust& exhaust, float strength,
                              float speed, float throttle, const float (&toClip)[16], float viewWidth);

}  // namespace oj::scene::haze
