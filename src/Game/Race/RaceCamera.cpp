#include "Game/Race/RaceCamera.h"

#include "Core/Physics/Dynamics.h"
#include "Core/Base/Log.h"
#include "Core/Race/Racer.h"
#include "Core/Base/Random.h"
#include "Core/Tracks/Route.h"
#include "Game/Platform/Platform.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Race/RaceIntro.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace oj::scene {

namespace {

const float kRandScale = std::bit_cast<float>(0x38000100u);    // 1 / 32767 (0x006FF7EC)
const float kShakeChance = std::bit_cast<float>(0x364CCE67u);  // 0x006FFE3C
const float kHalfPi = std::bit_cast<float>(0x3FC90FDBu);
const float kPi = std::bit_cast<float>(0x40490FDBu);
const float kTwoPi = std::bit_cast<float>(0x40C90FDBu);
const float kFovScale = std::bit_cast<float>(0x3F94C80Du);     // 1.1623551 (0x006FFBC8)
const float kSpeedFov = std::bit_cast<float>(0x3C98B8CBu);     // 0x006FFBD4

// The debug camera (0x00721C60, 0x68 bytes of the game's data; its +0x50 1): with +0x60 set it takes the race's
// camera over each frame (0x0049F910, from 0x00406547). Nothing sets +0x60, nor +0x54.
struct DebugCamera {
    float x = 0.0f, y = 0.0f, z = 0.0f;  // +0x00 .. +0x08: the place it puts the camera at
    render::Camera* camera = nullptr;    // +0x20: the camera it took last
    int32_t unused = 1;                  // +0x50: read by nothing
    int32_t step = 0;                    // +0x54: the first player's camera stepped instead
    int32_t on = 0;                      // +0x60
};
DebugCamera g_debugCamera;

// The chase views' offsets from the car (centimetres; x right, y up, z forward), for one player and for several
// (0x0071F914 ..): the near view, the far view, the farther view, and the near view's first frame after a reset.
struct Offset {
    float x, y, z;
};
constexpr Offset kOnePlayer[3] = {{0.0f, 180.0f, -530.0f}, {0.0f, 210.0f, -630.0f}, {0.0f, 270.0f, -830.0f}};
constexpr Offset kPlayers[3] = {{0.0f, 170.0f, -700.0f}, {0.0f, 200.0f, -830.0f}, {0.0f, 220.0f, -980.0f}};
constexpr Offset kResetOffset = {0.0f, 140.0f, -200.0f};
// The views on the car (0x0071F9A4 ..): the bumper, the bonnet; looking back.
constexpr Offset kBumperOffset = {0.0f, 40.0f, 130.0f};
constexpr Offset kBonnetOffset = {0.0f, 90.0f, 90.0f};
constexpr Offset kBumperBack = {0.0f, 40.0f, -85.0f};
constexpr Offset kBonnetBack = {0.0f, 90.0f, -85.0f};
// The views on the car's sides ([0x0071F984], [0x0071F994]) and their look back ([0x0071F9E4]).
constexpr Offset kRightSideOffset = {105.0f, 70.0f, -10.0f};
constexpr Offset kLeftSideOffset = {-105.0f, 70.0f, -10.0f};
constexpr Offset kSideBack = {0.0f, 60.0f, -85.0f};

float draw() {
    return static_cast<float>(gameRandom().next());
}

// A draw of rand() as 0 .. 1 (x 1 / 32767, 0x006FF7EC).
float unit() {
    return draw() * kRandScale;
}

// 0x00407910: an angle brought into -pi .. pi by whole turns.
float wrap(float a) {
    if (a > kPi) {
        do {
            a = a - kTwoPi;
        } while (a > kPi);
    }
    if (a < -kPi) {
        do {
            a = a + kTwoPi;
        } while (a < -kPi);
    }
    return a;
}

// 0x00407960 / 0x00407A30: sine and cosine of a float, the x87 result rounded once.
float sinOf(float a) {
    return static_cast<float>(std::sin(static_cast<double>(a)));
}

// 0x0049C590: the point `distance` ahead along a line (3: the competitor's own) from where the competitor is
// (the point before the one it heads for and its fraction), with the curve's direction there. A point's length is
// the way from the point before it.
void pointAhead(const Competitor& c, int line, float distance, Vec4& point, Vec4& direction) {
    const Route& route = *c.route;
    const int l = line == 3 ? c.line : line;
    const std::vector<RoutePoint>& points = route.line(l);
    int32_t i = c.next - 1;
    if (i < 0) i = route.count() - 1;
    float length = points[static_cast<size_t>(i)].length;
    float covered = (1.0f - c.fraction) * length;
    float t = 1.0f - (covered - distance) / length;
    while (covered < distance) {
        i = i + 1 > route.count() - 1 ? 0 : i + 1;
        length = points[static_cast<size_t>(i)].length;
        t = (distance - covered) / length;
        covered = covered + length;
    }
    route.curve(l, i, t, point, direction);
}

// 0x0049C670: the point `distance` behind.
void pointBehind(const Competitor& c, int line, float distance, Vec4& point, Vec4& direction) {
    const Route& route = *c.route;
    const int l = line == 3 ? c.line : line;
    const std::vector<RoutePoint>& points = route.line(l);
    int32_t i = c.next - 1;
    if (i < 0) i = route.count() - 1;
    float length = points[static_cast<size_t>(i)].length;
    float covered = c.fraction * length;
    float t = (covered - distance) / length;
    while (covered < distance) {
        i = i - 1 < 0 ? route.count() - 1 : i - 1;
        length = points[static_cast<size_t>(i)].length;
        t = 1.0f - (distance - covered) / length;
        covered = covered + length;
    }
    route.curve(l, i, t, point, direction);
}

// The views the director cuts to (0x00711648).
constexpr int kDirectorViews[18] = {RaceCamera::kPassing,   RaceCamera::kTrackside, RaceCamera::kTrackside,
                                    RaceCamera::kTrackside, RaceCamera::kTrackside, RaceCamera::kTrackside,
                                    RaceCamera::kTrackside, RaceCamera::kBumper,    RaceCamera::kBumper,
                                    RaceCamera::kOnGround,  RaceCamera::kOnGround,  RaceCamera::kPassing,
                                    RaceCamera::kPassing,   RaceCamera::kPassing,   RaceCamera::kTrackside,
                                    RaceCamera::kTrackside, RaceCamera::kTrackside, RaceCamera::kTrackside};
const float kPeriodScale = std::bit_cast<float>(0x39200140u);  // 5 / 32767 (0x006FFECC)

}  // namespace

void CameraDirector::start(RaceCamera& camera, int mode, float length, const CameraTarget& target,
                           const CameraRace& race, render::Camera& node) {
    active = true;
    camera.cut(mode);
    camera.update(target, race, 0.02f, node);
    time = 0.0f;
    period = length;
    if (sounds) sounds(true);
}

void CameraDirector::update(RaceCamera& camera, float dt, bool paused, const CameraTarget& target,
                            const CameraRace& race, render::Camera& node) {
    if (!active) return;
    if (paused) dt = 0.0f;
    ++seedCount();
    const auto seed = static_cast<int32_t>(time * -10000.0f);
    gameRandom().seed(static_cast<uint32_t>(static_cast<int32_t>(seedCount()) - seed));
    gameRandom().next();
    time = dt + time;
    // 0x004088A1: a transition running grows by 2 x the step ([0x00725270]); below 1 it is the post effect's amount,
    // else the effect stops.
    if (transition < 1.0f) {
        transition = 2.0f * dt + transition;
        if (transition < 1.0f) {
            blendAmount = transition;
        } else {
            stopBlend();
        }
    }
    if (!(time > period)) return;
    if (cutDue) {
        int mode = RaceCamera::kChase;
        do {
            mode = kDirectorViews[static_cast<uint32_t>(gameRandom().next()) % 18u];
        } while (mode == camera.mode() && mode != RaceCamera::kTrackside);
        // The replay's race type ([G+0x9C]+0x816, the race's type as the race starts): in a cruise the views on the
        // car and the ground's become the passing view. A replay playing fast forward takes the bonnet.
        if (race.raceType == RaceType::Cruise &&
            (mode == RaceCamera::kOnGround || mode == RaceCamera::kRightSide || mode == RaceCamera::kLeftSide ||
             mode == RaceCamera::kBumper || mode == RaceCamera::kBonnet))
            mode = RaceCamera::kPassing;
        if (race.replayFast) mode = RaceCamera::kBonnet;
        camera.take(mode, true, target, race, node);
        // The bumper's view of a car with another in its slipstream looks back at it.
        if (mode == RaceCamera::kBumper && target.follower) camera.forceLookBack();
        log::info("race: the director cuts to view {}", mode);
        time = 0.0f;
        period = draw() * kPeriodScale + 3.0f;
        if (mode == RaceCamera::kOnGround) {
            if (period > 3.0f) period = 3.0f;
        } else if (mode == RaceCamera::kTrackside) {
            period = 9999.0f;
        }
        cutDue = false;
        started = false;
    } else {
        // 0x004089F9: the transition of the cut starts (one still running stopped first).
        cutDue = true;
        if (transition < 1.0f) stopBlend();
        blendOn = true;
        transition = 0.0f;
    }
}

void RaceCamera::cut(int mode) {
    m_mode = mode;
    m_reset = true;
    m_viewChanged = true;
    m_nearCar = false;
    m_lookBackForced = false;
}

// 0x00403800: the point the views look at: the middle of the car and the cars of its slipstream (the one it follows,
// the one following it), 50 cm up. The offset from the middle to the car eases in over 2 s by (1 + sin(blend pi +
// pi / 2)) / 2 of the old offset, the blend from 1 again when the cars taken change or the offset moves over 1 m in a
// frame.
void RaceCamera::lookPoint(const CameraTarget& target) {
    const Mat34& m = target.matrix;
    const Vec4 own{m[3], m[7], m[11], 0.0f};
    Vec4 sum = own;
    float cars = 1.0f;
    if (target.followed) {
        for (size_t i = 0; i < 3; ++i) sum[i] = sum[i] + target.followedOrigin[i];
        cars = 2.0f;
    }
    if (target.follower) {
        for (size_t i = 0; i < 3; ++i) sum[i] = sum[i] + target.followerOrigin[i];
        cars = cars + 1.0f;
    }
    const float share = 1.0f / cars;
    const Vec4 offset{own[0] - sum[0] * share, own[1] - sum[1] * share, own[2] - sum[2] * share, m_look[3]};
    const float jx = offset[0] - m_middleOffset[0];
    const float jy = offset[1] - m_middleOffset[1];
    const float jz = offset[2] - m_middleOffset[2];
    m_middleOffset = offset;
    if (cars != m_lookCars || std::sqrt((jz * jz + jy * jy) + jx * jx) > 100.0f) {
        m_lookCars = cars;
        m_lookBlend = 1.0f;
    }
    const float wave = static_cast<float>(std::sin(static_cast<double>(m_lookBlend * 3.1415927f + 1.5707964f)));
    const float keep = 1.0f - (wave + 1.0f) * 0.5f;
    const float take = 1.0f - keep;
    for (size_t i = 0; i < 3; ++i) m_lookOffset[i] = offset[i] * take + m_lookOffset[i] * keep;
    m_lookOffset[3] = offset[3];
    if (m_lookBlend > 0.0f) {
        m_lookBlend = m_lookBlend - 0.5f * m_dt;
        if (m_lookBlend < 0.0f) m_lookBlend = 0.0f;
    }
    m_look = {own[0] - m_lookOffset[0], own[1] - m_lookOffset[1], own[2] - m_lookOffset[2], offset[3]};
    m_look[1] = m_look[1] + 50.0f;
}

void RaceCamera::take(int mode, bool reset, const CameraTarget& target, const CameraRace& race,
                      render::Camera& camera) {
    if (m_mode != mode || m_mode == kTrackside || m_mode == kPassing || m_mode == kOnGround || reset) {
        m_mode = mode;
        m_reset = reset;
        m_viewChanged = true;
    }
    m_nearCar = false;
    m_lookBackForced = false;
    update(target, race, 0.02f, camera);
}

void RaceCamera::update(const CameraTarget& target, const CameraRace& race, float dt, render::Camera& camera) {
    // 0x00406319: the camera's time (+0xD0), 0 while the race is paused.
    m_dt = race.paused ? 0.0f : dt;
    // 0x0040632D: the scene camera's post effects cleared before the view sets its own.
    camera.depthOfField = false;
    camera.depthOfFieldMode = 0;
    camera.blurMiddle = 0.0f;
    camera.blurEdge = 0.0f;
    // 0x0040635E: the depth of field's point 1 m over the car's origin (0x00407B20; the w of the body's place, read
    // by nothing). The stock branch for a car without a body (30 m ahead of the camera's node) is never taken.
    {
        const Mat34& m = target.matrix;
        camera.setFocus({m[3], m[7] + 100.0f, m[11], 0.0f});
    }
    const Vec4& v = target.localVelocity;
    m_speed = std::sqrt((v[2] * v[2] + v[1] * v[1]) + v[0] * v[0]);
    lookPoint(target);
    // The ground under where the camera's node is (its world matrix of the last frame).
    if (race.collision != nullptr) {
        const Mat34& w = camera.world();
        race.collision->groundBelow(w[3], w[11], m_ground);
    }
    // 0x00406496: the view's step (the jump table 0x00406950 for 3 .. 15, the view behind the car for the rest).
    if (m_mode == kBumper || m_mode == kBonnet) {
        onCar(target, camera, race.wideScreen);
    } else if (m_mode == kRightSide || m_mode == kLeftSide) {
        onSide(target, race, camera);
    } else if (m_mode == kBeforeStart) {
        beforeStart(target, race, camera);
    } else if (m_mode == kTrackside) {
        trackside(target, race, camera);
    } else if (m_mode == kPassing) {
        passing(target, race, camera);
    } else if (m_mode == kOnGround) {
        onGround(target, race, camera);
    } else if (m_mode == kNextCar) {
        nextCarView(target, race, camera);
    } else if (m_mode == kFree) {
        // 0x004064F7 / 0x0040650A: the free views take the frame's time again, paused or not.
        m_dt = dt;
        freeView(camera, race.wideScreen);
    } else if (m_mode == kFreeStill) {
        m_dt = dt;
        freeStillView(camera, race.wideScreen);
    } else {
        chase(target, race, camera);
    }
    camera.local[3] = m_position[0];
    camera.local[7] = m_position[1];
    camera.local[11] = m_position[2];
    camera.invalidate();
    // 0x00406547 → 0x0049F910: the debug camera, when on (never), takes the camera's node over: with +0x54 the first
    // player's camera (the race's only one here) goes back to the view behind the car, reset, its view changed, not
    // near a car nor looking back (and its view's +0x18 set to 1, not modelled), and is stepped with 0.02 s. Stock
    // problem: that step (0x00406300) calls the override again, without end. Else the node at the identity, turned
    // down by -pi / 2 about X, at the debug camera's place.
    if (g_debugCamera.on != 0) {
        if (g_debugCamera.step != 0) {
            m_mode = kChase;
            m_reset = true;
            m_viewChanged = true;
            m_nearCar = false;
            m_lookBackForced = false;
            update(target, race, std::bit_cast<float>(0x3CA3D70Au), camera);
            g_debugCamera.camera = &camera;
        } else {
            camera.resetMatrices();
            rotateX(camera.local, std::bit_cast<float>(0xBFC90FDBu));
            camera.local[3] = g_debugCamera.x;
            camera.local[7] = g_debugCamera.y;
            camera.local[11] = g_debugCamera.z;
            camera.invalidate();
            g_debugCamera.camera = &camera;
        }
    }
    // 0x00406607: +0x198 (set to 1 at the frame's start) puts the screen's smears on (+0x234) once, clearing the
    // camera's rain blobs (+0x194, slot 4); 0x00406638: a changed view (+0xB0, then cleared) clears them again.
    if (!camera.smears) {
        camera.smears = true;
        m_clearBlobs = true;
    }
    if (m_viewChanged) {
        m_clearBlobs = true;
        m_viewChanged = false;
        // With the sound on every emitter of the 3D world starts its frame anew (its manager's +0x2C).
        m_soundsReset = true;
    }
    speedBlur(target, race, camera);
    // 0x004068F8: the free view's mouse hook on with view 14, off with any other.
    hookFreeCameraMouse(m_mode == kFree);
}

namespace {

// The free views' state, globals of the stock game ([0x0079E734] .. [0x0079E77C]) and their tuning ([0x00711458] ..
// [0x00711468]). The actions they read from the player's controls (0x00401640 with the slots +0xEC .. +0x108 of the
// controls' action table, 0x004080D0, which never fills them: -1) are all 0 here as there.
struct FreeViewState {
    float zoom = 1.0f;          // [0x00711468]
    float yaw = 0.0f;           // [0x0079E770]
    float pitch = 0.0f;         // [0x0079E774]
    float roll = 0.0f;          // [0x0079E76C]
    float turnA = 0.0f, turnB = 0.0f;      // [0x0079E768], [0x0079E764]
    float turnASum = 0.0f, turnBSum = 0.0f;  // [0x0079E77C], [0x0079E778]
    float ahead2 = 0.0f, side2 = 0.0f;     // [0x0079E758], [0x0079E754]
    float ahead = 0.0f, side = 0.0f;       // [0x0079E760], [0x0079E75C]
    // The still view's: [0x0079E748], [0x0079E744], [0x0079E750], [0x0079E74C], [0x0079E738], [0x0079E734],
    // [0x0079E740], [0x0079E73C].
    float sTurnA = 0.0f, sTurnB = 0.0f, sTurnASum = 0.0f, sTurnBSum = 0.0f;
    float sAhead2 = 0.0f, sSide2 = 0.0f, sAhead = 0.0f, sSide = 0.0f;
};
FreeViewState g_free;
constexpr float kFreeSpeed = 1.0f;      // [0x00711460]
constexpr float kFreeDamping = 0.7f;    // [0x00711464]
constexpr float kStillSpeed = 1.0f;     // [0x00711458]
constexpr float kStillDamping = 0.7f;   // [0x0071145C]
constexpr float kAction = 0.0f;         // an action of the slots -1 (0x00401640)

// A turn's speed eased in: 0 stops it at once, else the first frame takes it whole, the next ones 0.05 of it more.
float easeIn(float value, float& state) {
    if (value == 0.0f) {
        state = 0.0f;
    } else {
        if (state != 0.0f) value = value * 0.05f + state;
        state = value;
    }
    return state;
}

float signOf(float v) {
    return v < 0.0f ? -1.0f : v > 0.0f ? 1.0f : 0.0f;
}

}  // namespace

// 0x00406DA0, the free view (its time not stopped by the pause): each frame the window of the views asked for
// (0x004584C0 with 0); with the reset the zoom 1, the turns and the roll 0. The camera's node starts again from no
// turn at no place (its place of the last frame kept). The zoom moves by its two actions x 0.01 within 0.05 .. 1.5
// (far 2 000 000, the field the zoom x 1.1623551). Two turns move by their actions (speed eased in) and are damped by
// 0.7 (read by nothing). The yaw and the pitch are the mouse's place from 400 x -0.011780973 (x the yaw, y the
// pitch); ahead: the left button 1, the right -1 (else its action), its square eased in, with its sign x the frame
// time x 2200 into a speed damped by 0.7; aside the same from its action (x 350). The camera goes from the node's
// last place by them along the turned node's z and x axes, up by its action x the frame time x 7500, and rolls by its
// action x 0.1 within -pi / 2 .. pi / 2.
void RaceCamera::freeView(render::Camera& camera, bool wideScreen) {
    FreeViewState& f = g_free;
    m_windowAsks.push_back(0);
    if (m_reset) {
        f.zoom = 1.0f;
        f.pitch = 0.0f;
        f.roll = 0.0f;
        f.yaw = 0.0f;
        m_reset = false;
    }
    const Vec4 from{camera.local[3], camera.local[7], camera.local[11], 0.0f};
    camera.local = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    float zoom = kAction * 0.01f + (f.zoom - kAction * 0.01f);
    zoom = std::clamp(zoom, 0.05f, 1.5f);
    f.zoom = zoom;
    camera.setFar(2000000.0f);
    camera.setFov(f.zoom * kFovScale, 0.0f, wideScreen);
    f.turnASum = f.turnASum + easeIn(kAction * m_dt * -0.2375f, f.turnA);
    easeIn(kAction * m_dt * -0.2375f, f.turnB);
    f.turnASum = kFreeDamping * f.turnASum;
    f.turnBSum = (f.turnBSum + f.turnB) * kFreeDamping;
    const FreeCameraMouse& mouse = freeCameraMouse();
    f.pitch = (static_cast<float>(static_cast<uint32_t>(mouse.y)) - 400.0f) * -0.011780973f;
    f.yaw = (static_cast<float>(static_cast<uint32_t>(mouse.x)) - 400.0f) * -0.011780973f;
    rotateY(camera.local, f.yaw);
    rotateX(camera.local, f.pitch);
    float ahead = kAction;
    if (mouse.forward != 0) ahead = kFreeSpeed;
    if (mouse.back != 0) ahead = -kFreeSpeed;
    easeIn(ahead * ahead, f.ahead2);
    f.ahead = (f.ahead2 * signOf(ahead) * m_dt * 2200.0f + f.ahead) * kFreeDamping;
    const float aside = kAction;
    easeIn(aside * aside, f.side2);
    f.side = (signOf(aside) * f.side2 * m_dt * 350.0f + f.side) * kFreeDamping;
    const Mat34& m = camera.local;
    const float x = (f.ahead * m[2] + f.side * m[0]) + m[3];
    float y = (f.side * m[4] + f.ahead * m[6]) + m[7];
    const float z = (f.side * m[8] + f.ahead * m[10]) + m[11];
    y = kAction * m_dt * 7500.0f + y;
    f.roll = std::clamp(kAction * 0.1f + f.roll, -kHalfPi, kHalfPi);
    rotateZ(camera.local, f.roll);
    m_position[0] = from[0] + x;
    m_position[1] = from[1] + y;
    m_position[2] = from[2] + z;
}

// 0x00407310, the still view (its time not stopped by the pause): with +0x1E8 0 (always) the camera stays where its
// node is, the zoom (+0x1E4) within 0.05 .. 1.5 (far 2 000 000, the field the zoom x 1.1623551). The moving branch
// (+0x1E8 set): the views' window asked for, the node from no turn at no place (0x00534D80, its last place kept), the
// zoom by its two actions, the turns' speeds eased in and damped by 0.7 into +0x1D4 (within -pi / 2 .. pi / 2) and
// +0x1D8 (past pi to -pi and back), turned by them, ahead and aside as the free view's (the buttons, x 2200 and x 350,
// damped by 0.7) from the node's last place along its axes.
void RaceCamera::freeStillView(render::Camera& camera, bool wideScreen) {
    FreeViewState& f = g_free;
    if (m_freeMoving == 0) {
        m_position[0] = camera.local[3];
        m_position[1] = camera.local[7];
        m_position[2] = camera.local[11];
        m_freeZoom = std::clamp(m_freeZoom, 0.05f, 1.5f);
        camera.setFar(2000000.0f);
        camera.setFov(m_freeZoom * kFovScale, 0.0f, wideScreen);
        return;
    }
    m_windowAsks.push_back(0);
    const Vec4 from{camera.local[3], camera.local[7], camera.local[11], 0.0f};
    camera.local = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    m_freeZoom = m_freeZoom - kAction * 0.01f;
    m_freeZoom = kAction * 0.01f + m_freeZoom;
    m_freeZoom = std::clamp(m_freeZoom, 0.05f, 1.5f);
    camera.setFar(2000000.0f);
    camera.setFov(m_freeZoom * kFovScale, 0.0f, wideScreen);
    f.sTurnASum = f.sTurnASum + easeIn(kAction * m_dt * -0.2375f, f.sTurnA);
    easeIn(kAction * m_dt * -0.2375f, f.sTurnB);
    f.sTurnASum = kStillDamping * f.sTurnASum;
    f.sTurnBSum = (f.sTurnBSum + f.sTurnB) * kStillDamping;
    m_freePitch = f.sTurnASum + m_freePitch;
    m_freeYaw = f.sTurnBSum + m_freeYaw;
    if (m_freeYaw < -kPi) m_freeYaw = kPi;
    if (kPi < m_freeYaw) m_freeYaw = -kPi;
    m_freePitch = std::clamp(m_freePitch, -kHalfPi, kHalfPi);
    rotateY(camera.local, m_freeYaw);
    rotateX(camera.local, m_freePitch);
    const FreeCameraMouse& mouse = freeCameraMouse();
    float ahead = kAction;
    if (mouse.forward != 0) ahead = kStillSpeed;
    if (mouse.back != 0) ahead = -kStillSpeed;
    easeIn(ahead * ahead, f.sAhead2);
    f.sAhead = (signOf(ahead) * f.sAhead2 * m_dt * 2200.0f + f.sAhead) * kStillDamping;
    const float aside = kAction;
    easeIn(aside * aside, f.sSide2);
    f.sSide = (signOf(aside) * f.sSide2 * m_dt * 350.0f + f.sSide) * kStillDamping;
    const Mat34& m = camera.local;
    m_position[0] = ((f.sAhead * m[2] + f.sSide * m[0]) + m[3]) + from[0];
    m_position[1] = from[1] + ((f.sSide * m[4] + f.sAhead * m[6]) + m[7]);
    m_position[2] = from[2] + ((f.sSide * m[8] + f.sAhead * m[10]) + m[11]);
}

void RaceCamera::controls(bool changeView, bool rearView, float pan, const CameraTarget& target,
                          const CameraRace& race, render::Camera& camera) {
    if (changeView) {
        if (m_mode == kChase) {
            take(kChaseFar, false, target, race, camera);
        } else if (m_mode == kChaseFar) {
            take(kBonnet, true, target, race, camera);
        } else {
            take(kChase, true, target, race, camera);
        }
    }
    const bool turns =
        m_mode == kChase || m_mode == kChaseFar || m_mode == kChaseFarther || m_mode == kBumper || m_mode == kBonnet;
    m_lookBack = turns && (rearView || m_lookBackForced);
    if (m_lookedBack != m_lookBack) m_viewChanged = true;
    // 0x00406CDB: DRIVER_CAMERA_PAN turned round into +0xCC; the look to the side (+0xC8) goes toward it at 3 x the
    // camera's time, and with no pan back toward 0 by 3 a second of it without passing 0.
    m_pan = -pan;
    m_sideAngle = 3.0f * m_dt * (m_pan - m_sideAngle) + m_sideAngle;
    if (m_pan == 0.0f) {
        const float back = 3.0f * m_dt;
        if (m_sideAngle <= 0.0f) {
            m_sideAngle = back + m_sideAngle;
            if (0.0f < m_sideAngle) m_sideAngle = 0.0f;
        } else {
            m_sideAngle = m_sideAngle - back;
            if (m_sideAngle < 0.0f) m_sideAngle = 0.0f;
        }
    }
}

bool RaceCamera::changeView(const CameraTarget& target, const CameraRace& race, render::Camera& camera) {
    if (m_director != nullptr && m_director->active) {
        cut(kChase);
        update(target, race, 0.02f, camera);
        m_reset = true;
        m_director->stop();
        return true;
    }
    cut(m_mode == kChaseFar ? kBumper : m_mode == kBumper ? kChase : kChaseFar);
    update(target, race, 0.02f, camera);
    return false;
}

void RaceCamera::clearIntro() {
    if (m_intro != nullptr) m_intro->clear();
}

void RaceCamera::release(render::Camera& camera) {
    if (!m_attached) return;
    // 0x00401F9D: the exposure 1 and its speed 0.
    resetExposure();
    camera.detach();
    m_attached = false;
}

void RaceCamera::chase(const CameraTarget& target, const CameraRace& race, render::Camera& camera) {
    TrackCollision* walls = race.collision;
    const bool wideScreen = race.wideScreen;
    const int players = race.players;
    // The offset of the view in the car's frame; the farther the wider the field of view is, turned round when
    // looking back.
    // 0x00401F90: after a view on the car the camera leaves its node.
    if (m_reset) {
        m_windowAsks.push_back(1);
        clearIntro();  // 0x0040206E
        release(camera);
    }
    const Offset* table = players == 1 ? kOnePlayer : kPlayers;
    Offset o = m_mode == kChaseFar ? table[1] : m_mode == kChaseFarther ? table[2] : m_reset ? kResetOffset : table[0];
    float f = m_fov;
    if (m_lookBack && !(f < 1.3f)) f = 1.3f;
    o.z = ((2.0f + f) / (f * 3.0f)) * o.z;
    if (wideScreen) o.z = o.z + (m_lookBack ? 50.0f : 70.0f);
    if (m_lookBack != m_lookedBack) {
        m_lookedBack = m_lookBack;
        m_reset = true;
    }
    if (m_lookBack) o.z = -o.z;

    // Into the world, kept at least 0.7 of its height above the car.
    const Mat34& m = target.matrix;
    const float wx = (m[1] * o.y + m[2] * o.z) + m[0] * o.x;
    float wy = (m[4] * o.x + m[5] * o.y) + m[6] * o.z;
    const float wz = (m[8] * o.x + m[9] * o.y) + m[10] * o.z;
    const float lowest = o.y * 0.7f;
    if (wy < lowest) wy = lowest;

    // The pace of the easing: from half at a stand to whole at 10 m/s.
    float pace = 1.0f;
    if (m_speed < 10.0f) {
        pace = m_speed * 0.1f;
        if (pace < 0.5f) {
            pace = 0.5f;
        } else if (pace > 1.0f) {
            pace = 1.0f;
        }
    }

    if (m_reset) {
        m_offset = {wx, wy, wz, m_offset[3]};
    } else {
        // The height eased, the offset turned towards the car's heading (with a pad's look to the side), its
        // length eased.
        m_offset[1] = (wy - m_offset[1]) * (pace * m_dt + pace * m_dt) + m_offset[1];
        const double heading = std::atan2(static_cast<double>(wx), static_cast<double>(wz));
        const auto aim = static_cast<float>(m_lookBack ? heading + m_sideAngle : heading - m_sideAngle);
        auto turn = static_cast<float>(
            std::atan2(static_cast<double>(m_offset[0]), static_cast<double>(m_offset[2])) - aim);
        if (turn > kPi) {
            turn = turn - kTwoPi;
        } else if (turn < -kPi) {
            turn = turn + kTwoPi;
        }
        const float angle = ((turn * pace) * m_dt) * 2.4f;
        const double c = std::cos(static_cast<double>(angle));
        const double s = std::sin(static_cast<double>(angle));
        const float x = m_offset[0], z = m_offset[2];
        m_offset[0] = productToFloat(c, x) - productToFloat(s, z);
        m_offset[2] = productToFloat(s, x) + productToFloat(c, z);
        const float now = std::sqrt((m_offset[0] * m_offset[0] + m_offset[1] * m_offset[1]) +
                                    m_offset[2] * m_offset[2]);
        const float aimed = std::sqrt((wz * wz + wx * wx) + wy * wy);
        const float length = (((aimed - now) * pace) * m_dt) * 2.4f + now;
        const float unit = 1.0f / (now + 1.1920929e-7f);  // 0x00407C00
        m_offset[0] = unit * m_offset[0];
        m_offset[1] = unit * m_offset[1];
        m_offset[2] = unit * m_offset[2];
        m_offset[0] = length * m_offset[0];
        m_offset[1] = length * m_offset[1];
        m_offset[2] = length * m_offset[2];
    }

    camera.setFar(2000000.0f);
    m_yaw = static_cast<float>(std::atan2(static_cast<double>(m_offset[0]), -static_cast<double>(m_offset[2])));
    m_pitch = players > 1 ? -0.1f : -0.12f;
    m_roll = 0.0f;

    // A wall between the car and a tenth beyond the camera (0x005046E0) brings the camera in and up.
    const Vec4 from{m[3], m[7], m[11], 0.0f};
    const Vec4 to{m_offset[0] * 1.1f + m[3], m_offset[1] * 1.1f + m[7], m_offset[2] * 1.1f + m[11], 0.0f};
    Vec4 hit{};
    float x = m_offset[0], y = m_offset[1], z = m_offset[2];
    if (walls && walls->firstWall(from, to, hit)) {
        const float shorter = 1.0f / 1.1f;
        const float hx = (hit[0] - m[3]) * shorter, hy = (hit[1] - m[7]) * shorter, hz = (hit[2] - m[11]) * shorter;
        const float full = std::sqrt((wz * wz + wx * wx) + wy * wy);
        const float kept = std::sqrt((hz * hz + hy * hy) + hx * hx);
        const float lost = 1.0f - kept / full;
        x = hx;
        z = hz;
        y = lost * 150.0f + m_offset[1];
        m_pitch = lost * -0.25f + m_pitch;
    }
    m_position = {x + m[3], y + m[7], z + m[11], m_position[3]};

    shakeFor(target, target.nitro ? 0.5f : 0.001f * m_speed + 0.2f, 2.0f, camera);
    impactShake(target, 1.0f);  // 0x00402785
    updateFov(target, camera, wideScreen);
    m_reset = false;
}

void RaceCamera::beforeStart(const CameraTarget& target, const CameraRace& race, render::Camera& camera) {
    if (m_reset) {
        // 0x0040533C: the stock's camera without a car (+0x08) only clears its fly-by here, asking no window and
        // setting no sweep; this camera is never stepped without one (TrackView::cameraTarget).
        m_windowAsks.push_back(0);
        clearIntro();  // 0x00405317
        // Across the front of the grid: 400 cm ahead of the farthest place along the first place's heading, from
        // one side of the places to the other, 65 cm above the ground.
        release(camera);
        static const std::vector<Vec4> kNone;
        const std::vector<Vec4>& places = race.places ? *race.places : kNone;
        const float heading = places.empty() ? 0.0f : places[0][3];
        const auto c = static_cast<float>(std::cos(static_cast<double>(heading)));  // 0x004078D0
        const auto s = static_cast<float>(std::sin(static_cast<double>(heading)));  // 0x00407960
        const float nc = c * -1.0f;
        float cx = 0.0f, cy = 0.0f, cz = 0.0f;
        if (!places.empty()) {
            const float share = 1.0f / static_cast<float>(places.size());
            for (const Vec4& p : places) {
                cx = cx + share * p[0];
                cy = share * p[1] + cy;
                cz = share * p[2] + cz;
            }
        }
        float across = 0.0f, along = 0.0f;
        for (const Vec4& p : places) {
            const float dx = cx - p[0], dy = cy - p[1], dz = cz - p[2];
            const float a = std::fabs((dx * nc + dz * s) + dy * 0.0f);
            const float b = std::fabs((dz * c + dx * s) + dy * 0.0f);
            if (a > across) across = a;
            if (b > along) along = b;
        }
        along = along + 400.0f;
        const float ax = nc * across, ay = across * 0.0f, az = across * s;
        const float baseX = cx + s * along, baseY = 0.0f * along + cy, baseZ = along * c + cz;
        m_sweepFrom = {baseX + ax, baseY + ay, baseZ + az, m_sweepFrom[3]};
        m_sweepTo = {baseX - ax, baseY - ay, baseZ - az, m_sweepTo[3]};
        if (race.collision) {
            race.collision->groundBelow(m_sweepFrom[0], m_sweepFrom[2], m_sweepFrom[1]);
            race.collision->groundBelow(m_sweepTo[0], m_sweepTo[2], m_sweepTo[1]);
        }
        m_sweepFrom[1] = 65.0f + m_sweepFrom[1];
        m_sweepTo[1] = 65.0f + m_sweepTo[1];
        m_sweep = 0.0f;
        m_position = m_sweepFrom;
    }
    // Along the sweep at 0.3 a second, a narrow view on the point above the player's car.
    const float t = m_sweep;
    m_position = {m_sweepFrom[0] + (m_sweepTo[0] - m_sweepFrom[0]) * t,
                  m_sweepFrom[1] + (m_sweepTo[1] - m_sweepFrom[1]) * t,
                  m_sweepFrom[2] + (m_sweepTo[2] - m_sweepFrom[2]) * t, m_sweepFrom[3]};
    m_fov = 0.5f;
    m_sweep = 0.3f * m_dt + m_sweep;
    camera.setFar(2000000.0f);
    camera.setFov(m_fov * kFovScale, 0.0f, race.wideScreen);
    lookAt(m_look, camera);
    m_reset = false;
    // The player's view at the sweep's end, or when the count goes below 4.
    if (m_sweep > 1.0f) take(race.playerMode, true, target, race, camera);
    // 0x00405300: under 4 s of the count a network game only watched takes the view behind the car (0).
    if (race.count < 4.0f) take(race.netSpectator ? kChase : race.playerMode, true, target, race, camera);
}

void RaceCamera::lookAt(const Vec4& point, render::Camera& camera) {
    const float dx = point[0] - m_position[0];
    const float dy = point[1] - m_position[1];
    const float dz = point[2] - m_position[2];
    const float flat = std::sqrt(dz * dz + dx * dx);
    float up = dy;
    if (flat > 0.0f) {
        // Farther than 20 m the point is raised, by up to 120 cm.
        float lift = 1.0f - 2000.0f / flat;
        if (lift < 0.0f) {
            lift = 0.0f;
        } else if (lift > 1.0f) {
            lift = 1.0f;
        }
        up = lift * 120.0f + dy;
    }
    m_roll = 0.0f;
    m_pitch = static_cast<float>(-std::atan2(-static_cast<double>(up), static_cast<double>(flat)));
    m_yaw = static_cast<float>(-std::atan2(static_cast<double>(dx), static_cast<double>(dz)));
    place(camera);
}

void RaceCamera::place(render::Camera& camera) {
    camera.local = {1.0f, 0.0f, 0.0f, m_position[0], 0.0f, 1.0f, 0.0f, m_position[1],
                    0.0f, 0.0f, 1.0f, m_position[2]};
    rotateY(camera.local, m_yaw);
    rotateX(camera.local, m_pitch);
    rotateZ(camera.local, m_roll);
    camera.invalidate();
}

void RaceCamera::onCar(const CameraTarget& target, render::Camera& camera, bool wideScreen) {
    const Offset& ahead = m_mode == kBumper ? kBumperOffset : kBonnetOffset;
    const Offset& back = m_mode == kBumper ? kBumperBack : kBonnetBack;
    if (m_reset) {
        m_windowAsks.push_back(1);
        clearIntro();  // 0x00403421
        // The camera's node hangs from the car's, at the view's place, straight ahead.
        m_lookedBack = false;
        if (!m_attached && target.node != nullptr) {
            target.node->attach(&camera);
            m_attached = true;
        }
        m_pitch = 0.0f;
        m_yaw = 0.0f;
        m_roll = 0.0f;
        m_position = {ahead.x, ahead.y, ahead.z, m_position[3]};
    }
    updateFov(target, camera, wideScreen);
    m_pitch = 0.0f;
    m_roll = 0.0f;
    // 0x0040350B: not looking back, the pan looks a quarter turn to its side (+0xCC above 0 a quarter turn, below 0
    // the other), else straight ahead; a change is a changed view.
    if (!m_lookBack) {
        const float yaw = m_pan > 0.0f ? kHalfPi : m_pan < 0.0f ? -kHalfPi : 0.0f;
        if (m_yaw != yaw) {
            m_yaw = yaw;
            m_viewChanged = true;
        }
    }
    // The view pitches with the car's forward acceleration and rolls with its turn, eased at 3 a second.
    float pitch = 0.005f * target.forwardAcceleration;
    if (pitch < -0.08f) {
        pitch = -0.08f;
    } else if (pitch > 0.12f) {
        pitch = 0.12f;
    }
    float roll = target.localAngularVelocity[2] * 0.1f;
    if (roll < -0.08f) {
        roll = -0.08f;
    } else if (roll > 0.12f) {
        roll = 0.12f;
    }
    m_tilt = lerp(m_tilt, {pitch, 0.0f, roll, target.localAngularVelocity[3]}, 3.0f * m_dt);
    m_pitch = m_tilt[0] + m_pitch;
    m_yaw = m_tilt[1] + m_yaw;
    m_roll = m_tilt[2] + m_roll;
    if (m_lookBack != m_lookedBack) {
        m_lookedBack = m_lookBack;
        m_pitch = 0.0f;
        m_yaw = 0.0f;
        m_roll = 0.0f;
        const Offset& at = m_lookBack ? back : ahead;
        m_position = {at.x, at.y, at.z, m_position[3]};
        if (m_lookBack) m_yaw = kPi;
    }
    shakeFor(target, 0.35f, 1.2f, camera);
    impactShake(target, 1.6f);  // 0x0040372C
    m_reset = false;
}

// 0x004027B0, the views on the car's sides: the scene camera's motion blur (+0x224, +0x228) 0.5 each (the speed's
// blur after the view sets them again). With the reset the camera's node hangs from the car's (once), the turn 0, the
// race front end's window choice 0, the place the side's and the field of view's factor 1. The field of view
// 1.1623551 x 1 (the factor not used). A look back changed: the turn 0 and the place the side's, or (0, 60, -85)
// turned half round. The view cuts within 2 s when another car's origin is within 0.75 of its chassis's radius of the
// camera, or when a wall stands between the car and the camera; the shake by 0.005 x the speed at the pace 1.
void RaceCamera::onSide(const CameraTarget& target, const CameraRace& race, render::Camera& camera) {
    camera.blurMiddle = 0.5f;
    camera.blurEdge = 0.5f;
    const Offset& side = m_mode == kLeftSide ? kLeftSideOffset : kRightSideOffset;
    if (m_reset) {
        if (!m_attached && target.node != nullptr) {
            target.node->attach(&camera);
            m_attached = true;
        }
        m_pitch = 0.0f;
        m_yaw = 0.0f;
        m_roll = 0.0f;
        m_windowAsks.push_back(0);
        m_position = {side.x, side.y, side.z, 0.0f};
        m_fov = 1.0f;
    }
    camera.setFov(kFovScale, 0.0f, race.wideScreen);
    if (m_lookBack != m_lookedBack) {
        m_lookedBack = m_lookBack;
        m_pitch = 0.0f;
        m_yaw = 0.0f;
        m_roll = 0.0f;
        if (!m_lookBack) {
            m_position = {side.x, side.y, side.z, 0.0f};
        } else {
            m_position = {kSideBack.x, kSideBack.y, kSideBack.z, 0.0f};
            m_yaw = kPi;
        }
    }
    m_nearCar = false;
    const Mat34& m = target.matrix;
    const Vec4 car{m[3], m[7], m[11], 0.0f};
    const Mat34& w = camera.world();
    const Vec4 at{w[3], w[7], w[11], 0.0f};
    const auto cutSoon = [&] {
        if (m_director == nullptr) return;
        m_director->period = m_director->time >= 2.0f ? 0.0f : 2.0f - m_director->time;
    };
    if (race.cars != nullptr) {
        for (size_t i = 0; i < race.cars->size(); ++i) {
            if (static_cast<int>(i) == race.self) continue;
            const CameraRace::Car& other = (*race.cars)[i];
            const float dx = other.origin[0] - at[0];
            const float dy = other.origin[1] - at[1];
            const float dz = other.origin[2] - at[2];
            if (std::sqrt((dz * dz + dy * dy) + dx * dx) <= 0.75f * other.radius) {
                cutSoon();
                break;
            }
        }
    }
    Vec4 hit{};
    if (race.collision != nullptr && race.collision->firstWall(car, at, hit)) cutSoon();
    shakeFor(target, 0.005f * m_speed, 1.0f, camera);
    m_reset = false;
}

void RaceCamera::shakeFor(const CameraTarget& target, float amount, float pace, render::Camera& camera) {
    const float s = m_speed * 0.01f;
    const float slowing = 1.0f / (0.01f + s);
    float base = 0.2f;
    if (target.nitro) {
        amount = amount * target.nitroAmount;
        base = 0.02f;
    }
    pace = (base * slowing) * pace;
    const Vec4& e = target.wheelEffects;
    amount = ((((std::fabs(e[1]) + std::fabs(e[2])) + std::fabs(e[0])) + (target.nitro ? 1.0f : 0.0f) * 1.4f) +
              s * 1.5f) *
             amount;
    const float calm = 1.0f / (s + 1.0f);
    const float r = draw();
    if (amount * m_dt > r * kShakeChance && !target.replayRace) {
        shake(0.1f, amount * 0.01f, amount * 0.02f, pace * 6.0f, pace * 8.0f, camera);
    } else {
        const float period = calm * pace;
        shake(0.1f, amount * 0.001f, amount * 0.004f, period * 8.0f, 10.0f * period, camera);
    }
}

void RaceCamera::shake(float yawScale, float minAmount, float maxAmount, float minPeriod, float maxPeriod,
                       render::Camera& camera) {
    if (m_dt < 0.0f || m_dt == 0.0f) return;
    m_shakeTime = m_shakeTime + m_dt;
    if (m_shakeTime >= m_shakePeriod) newAim(yawScale, minAmount, maxAmount, minPeriod, maxPeriod);
    // Eased from the last aim to the new one along half a sine wave.
    easeShake(camera);
}

void RaceCamera::newAim(float yawScale, float minAmount, float maxAmount, float minPeriod, float maxPeriod) {
    {
        // A new aim: a direction tilted from straight ahead by up to the amount (x a quarter turn), turned about the
        // view at random (0x00407A90, 0x00407A30); its pitch, its yaw x yawScale, a random roll.
        m_shakeTime = 0.0f;
        float r = draw();
        m_shakePeriod = ((maxPeriod - minPeriod) * r) * kRandScale + minPeriod;
        r = draw();
        const float tilt = (((maxAmount - minAmount) * r) * kRandScale + minAmount) * kHalfPi;
        const auto tiltSin = static_cast<float>(std::sin(static_cast<double>(tilt)));
        const auto tiltCos = static_cast<float>(std::cos(static_cast<double>(tilt)));
        r = draw();
        const float spin = ((r * kRandScale) - 0.5f) * kTwoPi;
        const auto spinSin = static_cast<float>(std::sin(static_cast<double>(spin)));
        const auto spinCos = static_cast<float>(std::cos(static_cast<double>(spin)));
        const float x = spinSin * tiltSin;
        const float y = -(spinCos * tiltSin);
        const float z = tiltCos;
        m_shakeFrom = m_shakeTo;
        const float across = std::sqrt(x * x + z * z);
        m_shakeTo[0] = static_cast<float>(std::atan2(static_cast<double>(y), static_cast<double>(across)));
        m_shakeTo[1] = productToFloat(std::atan2(static_cast<double>(x), static_cast<double>(z)), yawScale);
        r = draw();
        m_shakeTo[2] = ((((r * kRandScale) - 0.5f) * kTwoPi) * maxAmount) * 0.25f;
        // The period stretched by the larger turn (0x00407DE0).
        const float pitchTurn = std::fabs(m_shakeTo[0] - m_shakeFrom[0]);
        const float yawTurn = std::fabs(m_shakeTo[1] - m_shakeFrom[1]);
        m_shakePeriod = (pitchTurn > yawTurn ? pitchTurn : yawTurn) * m_shakePeriod;
    }
}

// 0x00403320: the car's last hit (+0x174) not 0 asks a new aim at once (0x00403130: the time 0) of 0.01 x the hit x
// the scale (at most 0.01 x the scale) .. a quarter more, over 0.04 x the hit x the scale (at most 0.1 x the scale) ..
// a quarter more, the yaw x 0.1.
void RaceCamera::impactShake(const CameraTarget& target, float scale) {
    if (target.impact == 0.0f) return;
    float amount = 0.01f * target.impact * scale;  // [0x007114EC]
    float period = 0.04f * target.impact * scale;  // [0x007114E8]
    if (0.1f * scale <= period) period = 0.1f * scale;  // [0x007114F4]
    if (0.01f * scale <= amount) amount = 0.01f * scale;  // [0x007114F0]
    newAim(0.1f, amount, amount * 0.25f + amount, period, period * 0.25f + period);
}

void RaceCamera::easeShake(render::Camera& camera) {
    float pitch = m_shakeTo[0] - m_shakeFrom[0];
    float yaw = m_shakeTo[1] - m_shakeFrom[1];
    if (m_shakePeriod > 0.0f) {
        const float phase = (m_shakeTime / m_shakePeriod) * kPi - kHalfPi;
        const float ease = static_cast<float>(std::sin(static_cast<double>(phase)) + 1.0) * 0.5f;
        pitch = pitch * ease;
        yaw = yaw * ease;
    }
    pitch = m_shakeFrom[0] + pitch;
    yaw = m_shakeFrom[1] + yaw;

    // The camera's node: at the position, turned by the yaw, the pitch and the roll (0x00534960, 0x00534A10,
    // 0x00534AC0); the shake's roll is not used.
    camera.local = {1.0f, 0.0f, 0.0f, m_position[0], 0.0f, 1.0f, 0.0f, m_position[1],
                    0.0f, 0.0f, 1.0f, m_position[2]};
    rotateY(camera.local, yaw + m_yaw);
    rotateX(camera.local, pitch + m_pitch);
    rotateZ(camera.local, m_roll);
    camera.invalidate();
}

// 0x00403F30, the TV view beside the line ahead: the scene camera's depth of field on (+0x22C), +0x230 2, the motion
// blur 0.5 and 0.5; placed with the reset; its zoom (kind 2); a view lost (the car
// beyond its farthest or behind a wall) cuts within 2 s; the height swings once from the base by the range (a
// quarter turn of a sine over pi / 2 s), 250 cm for the whole range above the height, which rises as cars come near;
// the first frame looks at the point above the car, the next ones follow it.
void RaceCamera::trackside(const CameraTarget& target, const CameraRace& race, render::Camera& camera) {
    camera.depthOfField = true;
    camera.depthOfFieldMode = 2;
    camera.blurMiddle = 0.5f;
    camera.blurEdge = 0.5f;
    const Mat34& m = target.matrix;
    const Vec4 car{m[3], m[7], m[11], 0.0f};
    bool placed = false;
    if (m_reset) {
        m_windowAsks.push_back(0);
        clearIntro();  // 0x00403F81
        release(camera);
        placeTrackside(target, race);
        placed = true;
    }
    tvZoom(target, 2);
    Vec4 hit{};
    const bool blocked = race.collision != nullptr && race.collision->firstWall(car, m_position, hit);
    const float dx = m_position[0] - car[0];
    const float dy = m_position[1] - car[1];
    const float dz = m_position[2] - car[2];
    const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
    if ((distance > m_maxDistance || blocked) && m_director != nullptr) {
        m_director->period = m_director->time < 2.0f ? 2.0f - m_director->time : 0.0f;
    }
    const float swing = (static_cast<float>(std::sin(static_cast<double>(m_tvPhase)) + 1.0) * 0.5f) * m_tvRange +
                        m_tvBase;
    m_tvHeight = rise(race) + m_tvHeight;
    m_position[1] = swing * 250.0f + m_tvHeight;
    m_tvPhase = m_dt + m_tvPhase;
    if (m_tvPhase > kHalfPi) m_tvPhase = kHalfPi;
    if (placed) {
        lookAt(m_look, camera);
    } else {
        follow(camera);
    }
    camera.setFar(2000000.0f);
    camera.setFov(m_fov * kFovScale, 0.0f, race.wideScreen);
}

// 0x00404150: the TV view's place. With the car's forward speed held within 10 .. 50 m/s and a share k from 1, the
// point 800 x speed x k cm along the car's line ahead (behind while going back), and those at half and 1.5 times
// that; the height 100 cm (200 in a cruise) above the point; the base and the range of the swing drawn; the place
// drawn within 10 m of the point in x and z (brought back to 0.9 of the way to a wall between), at the base's height;
// raised to the point's height + the height + 250 with a range of 0.1 when the line ahead climbs over 2 m above the
// middle of the car's and the place's heights. A place the car cannot be seen from is drawn again with k less 0.1,
// down to 0; the farthest the view may lose the car is 1.01 of the distance; the zoom drawn within 0.1 .. 0.8.
void RaceCamera::placeTrackside(const CameraTarget& target, const CameraRace& race) {
    const Mat34& m = target.matrix;
    const Vec4 car{m[3], m[7], m[11], 0.0f};
    float share = 1.0f;
    Vec4 place{};
    for (;;) {
        Vec4 point = car;
        Vec4 nearer = car, farther = car, direction{};
        if (target.competitor != nullptr && target.competitor->route != nullptr) {
            const Competitor& c = *target.competitor;
            const float speed = std::clamp(target.localVelocity[2], 10.0f, 50.0f);
            const float distance = speed * share * 800.0f;
            if (c.goingBack) {
                pointBehind(c, 3, distance, point, direction);
                pointBehind(c, 3, distance * 0.5f, nearer, direction);
                pointBehind(c, 3, distance * 1.5f, farther, direction);
            } else {
                pointAhead(c, 3, distance, point, direction);
                pointAhead(c, 3, distance * 0.5f, nearer, direction);
                pointAhead(c, 3, distance * 1.5f, farther, direction);
            }
        }
        m_tvHeight = race.raceType == RaceType::Cruise ? 200.0f : 100.0f;
        m_tvBase = unit();
        m_tvRange = unit() > 0.5f ? 1.0f - m_tvBase : -m_tvBase;
        m_tvPhase = -kHalfPi;
        m_tvHeight = point[1] + m_tvHeight;
        place[0] = (unit() - 0.5f) * 2000.0f + point[0];
        place[1] = m_tvBase * 250.0f + m_tvHeight;
        place[2] = (unit() - 0.5f) * 2000.0f + point[2];
        place[3] = point[3];
        Vec4 hit{};
        if (race.collision != nullptr && race.collision->firstWall(point, place, hit)) {
            place[0] = (hit[0] - point[0]) * 0.9f + point[0];
            place[1] = (hit[1] - point[1]) * 0.9f + point[1];
            place[2] = (hit[2] - point[2]) * 0.9f + point[2];
        }
        const float middle = (car[1] - place[1]) * 0.5f + place[1];
        if (nearer[1] - middle > 200.0f || farther[1] - middle > 200.0f) {
            m_tvRange = 0.1f;
            place[1] = point[1] + m_tvHeight + 250.0f;
        }
        const float dx = place[0] - car[0];
        const float dy = place[1] - car[1];
        const float dz = place[2] - car[2];
        const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
        m_maxDistance = 1.01f * distance;
        bool seen = true;
        if (distance > m_maxDistance) {
            seen = false;
        } else if (race.collision != nullptr && race.collision->firstWall(car, place, hit)) {
            seen = false;
        }
        if (seen) break;
        share = share - 0.1f;
        if (!(share > 0.0f)) break;
    }
    m_position = place;
    m_fov = std::clamp(unit(), 0.1f, 0.8f);
    m_viewChanged = true;
    m_reset = false;
}

// 0x00404660: how far the view rises this frame: 500 x dt x the most of 1 - distance / radius over the vehicles
// whose chassis' sphere holds the camera.
float RaceCamera::rise(const CameraRace& race) const {
    float most = 0.0f;
    if (race.cars != nullptr) {
        for (const CameraRace::Car& c : *race.cars) {
            const float dx = c.origin[0] - m_position[0];
            const float dy = c.origin[1] - m_position[1];
            const float dz = c.origin[2] - m_position[2];
            const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
            if (distance > c.radius) continue;
            const float near = 1.0f - distance / c.radius;
            if (near > most) most = near;
        }
    }
    return 500.0f * m_dt * most;
}

// 0x00405DB0: the zoom's aim by the car's distance. Kind 0 (view 8's, from its unset +0x1C): 0.01 beyond 2000 m, 1 at
// a distance below 0, else 200000 / ((500 x distance + 200000) x +0x20), held within 0.01 .. 1 (with +0x20 0 the
// division gives +inf, held at 1), eased at 0.5. Kind 1: 1 / +0x20, eased at 1. Any other (2, the TV view): the widest
// (1 - 0.1 x the cars the look takes) within 10 m, narrowing by 0.001 a centimetre from there, 0.1 beyond 20 m, held
// within 0.1 .. the widest; eased at 0.05.
void RaceCamera::tvZoom(const CameraTarget& target, int kind) {
    const Mat34& m = target.matrix;
    const float dx = m_position[0] - m[3];
    const float dy = m_position[1] - m[7];
    const float dz = m_position[2] - m[11];
    const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
    if (kind == 0) {
        if (distance > 200000.0f) {
            m_fovTarget = 0.01f;
        } else if (distance < 0.0f) {
            m_fovTarget = 1.0f;
        } else {
            m_fovTarget = 200000.0f / ((distance * 500.0f + 200000.0f) * m_zoomDivisor);
        }
        m_fovTarget = std::clamp(m_fovTarget, 0.01f, 1.0f);
        easeFov(0.5f);
        return;
    }
    if (kind == 1) {
        m_fovTarget = 1.0f / m_zoomDivisor;
        easeFov(1.0f);
        return;
    }
    const float widest = 1.0f - m_lookCars * 0.1f;
    if (distance > 2000.0f) {
        m_fovTarget = 0.1f;
    } else if (distance >= 1000.0f) {
        m_fovTarget = 1.0f - (distance - 1000.0f) * 0.001f;
    } else {
        m_fovTarget = widest;
    }
    if (m_fovTarget < 0.1f) {
        m_fovTarget = 0.1f;
    } else if (m_fovTarget > widest) {
        m_fovTarget = widest;
    }
    easeFov(0.05f);
}

// 0x00403E40, view 8 (the replay's next car, 0x00408AC0: the director started with it for 0 s, so it lasts until the
// director's next cut, a frame or two; else only a view kept in the settings): the depth of field on (the scene
// camera's +0x22C 1, +0x230 2, the motion blur +0x224 and +0x228 0.5, put at 0 by the speed's blur after the view);
// with the reset the object at +0x1D0 cleared (0x00403E7C, as the chase views), the views' window asked for
// (0x004584C0 with 0), the car's node left (0x00401F90) and the view put looking at +0x12C (0x00403740); the zoom of
// the kind +0x1C (0x00405DB0; never set, kind 0 here), the turn (0x00403B00), far 2 000 000, the field the factor
// (+0xB8) x 1.1623551; the reset off.
void RaceCamera::nextCarView(const CameraTarget& target, const CameraRace& race, render::Camera& camera) {
    camera.depthOfField = true;
    camera.depthOfFieldMode = 2;
    camera.blurMiddle = 0.5f;
    camera.blurEdge = 0.5f;
    if (m_reset) {
        m_windowAsks.push_back(0);
        clearIntro();  // 0x00403E7C
        release(camera);
        lookAt(m_look, camera);
    }
    tvZoom(target, m_zoomKind);
    follow(camera);
    camera.setFar(2000000.0f);
    camera.setFov(m_fov * kFovScale, 0.0f, race.wideScreen);
    m_reset = false;
}

// 0x00403B00: the view turns toward the point above the car with turn rates that grow by |sin| of the error (0.36
// a second, held within ±0.008; a pitch error below 0 takes it off all three rates) and by the error itself (6.67,
// 10 and 20 a second for pitch, yaw and roll).
void RaceCamera::follow(render::Camera& camera) {
    const float dx = m_look[0] - m_position[0];
    const float dy = m_look[1] - m_position[1];
    const float dz = m_look[2] - m_position[2];
    const auto yawAim = static_cast<float>(-std::atan2(static_cast<double>(dx), static_cast<double>(dz)));
    const float flat = std::sqrt(dz * dz + dx * dx);
    float up = dy;
    if (flat > 0.0f) up = std::clamp(1.0f - 2000.0f / flat, 0.0f, 1.0f) * 120.0f + dy;
    const auto pitchAim = static_cast<float>(std::atan2(static_cast<double>(up), static_cast<double>(flat)));
    const float pitchError = wrap(pitchAim - m_pitch);
    const float yawError = wrap(yawAim - m_yaw);
    const float rollError = -m_roll;
    const float rate = std::bit_cast<float>(0x3EB8521Cu);  // 0.36000144 (0x006FFE30)
    float a = std::fabs(sinOf(pitchError));
    if (pitchError < 0.0f) {
        if (m_spin[0] > -0.008f) {
            const float change = a * m_dt * rate;
            m_spin[0] = m_spin[0] - change;
            m_spin[1] = m_spin[1] - change;
            m_spin[2] = m_spin[2] - change;
        }
    } else if (m_spin[0] < 0.008f) {
        m_spin[0] = a * m_dt * rate + m_spin[0];
    }
    const float frames = m_dt * 60.0f;
    const float pitch = frames * m_spin[0] + m_pitch;
    m_pitch = pitch;
    a = std::fabs(sinOf(yawError));
    if (yawError < 0.0f) {
        if (m_spin[1] > -0.008f) m_spin[1] = m_spin[1] - a * m_dt * rate;
    } else if (m_spin[1] < 0.008f) {
        m_spin[1] = a * m_dt * rate + m_spin[1];
    }
    const float yaw = frames * m_spin[1] + m_yaw;
    m_yaw = yaw;
    a = std::fabs(sinOf(rollError));
    if (rollError < 0.0f) {
        if (m_spin[2] > -0.008f) m_spin[2] = m_spin[2] - a * m_dt * rate;
    } else if (rollError > 0.0f) {
        if (m_spin[2] < 0.008f) m_spin[2] = a * m_dt * rate + m_spin[2];
    }
    m_pitch = wrap(pitch);
    m_yaw = wrap(yaw);
    m_roll = wrap(frames * m_spin[2] + m_roll);
    m_pitch = wrap(pitchError * std::bit_cast<float>(0x40D55555u) * m_dt + m_pitch);
    m_yaw = wrap(yawError * 10.0f * m_dt + m_yaw);
    m_roll = wrap(rollError * 20.0f * m_dt + m_roll);
    place(camera);
}

// 0x004047C0, a view passing along the line (the route's first): with the reset the game's rand() is seeded from the
// car's x and the director's counter, one draw passed over, and the start and end distances drawn among 10 m and 30 m
// ahead or behind. Each frame the distance goes from the start to the end on a half turn of a sine (pi s); the point
// that far along the line (behind for a negative distance) is taken at that distance straight from the car, led by
// the car's velocity x 20 near the car (fully within 10 m, none beyond 30 m), raised by 200 x the sine's top and
// 200 / (distance + 1) (at most 200) + 85, kept 85 cm over the ground, and swung 200 cm to the side of the end's sign
// by the same top. The camera eases there keeping 0.85 a frame (all of it while paused, none with the reset), rises
// as cars come near (on all three axes), looks at the point above the car with the reset and follows it after, with
// a fixed zoom of 0.7.
void RaceCamera::passing(const CameraTarget& target, const CameraRace& race, render::Camera& camera) {
    // 0x004047F9: +0x230 2, the motion blur 0.5 and 0.5.
    camera.depthOfFieldMode = 2;
    camera.blurMiddle = 0.5f;
    camera.blurEdge = 0.5f;
    const Mat34& m = target.matrix;
    const Vec4 car{m[3], m[7], m[11], 0.0f};
    float keep = 0.85f;
    bool placed = false;
    const auto distanceOf = [](float r) {
        if (r < 0.25f) return 1000.0f;
        if (r < 0.5f) return -1000.0f;
        if (r < 0.75f) return 3000.0f;
        return -3000.0f;
    };
    if (m_reset) {
        m_windowAsks.push_back(0);
        release(camera);
        m_viewChanged = true;
        ++seedCount();
        gameRandom().seed(static_cast<uint32_t>(static_cast<int32_t>(car[0]) + static_cast<int32_t>(seedCount())));
        gameRandom().next();
        m_passFrom = distanceOf(unit());
        m_passTo = distanceOf(unit());
        m_reset = false;
        m_passPhase = -kHalfPi;
        keep = 0.0f;
        placed = true;
    }
    const float top = (1.0f - sinOf(m_passPhase)) * 0.5f;
    const float along = (1.0f - top) * m_passTo + top * m_passFrom;
    m_passPhase = m_passPhase + m_dt;
    if (m_passPhase > kHalfPi) m_passPhase = kHalfPi;
    Vec4 point = car;
    Vec4 direction{};
    if (target.competitor != nullptr && target.competitor->route != nullptr) {
        if (along < 0.0f) {
            pointBehind(*target.competitor, 0, -along, point, direction);
        } else {
            pointAhead(*target.competitor, 0, along, point, direction);
        }
    }
    const float dx = point[0] - car[0];
    const float dy = point[1] - car[1];
    const float dz = point[2] - car[2];
    const float inverse = 1.0f / (std::sqrt((dz * dz + dy * dy) + dx * dx) + std::bit_cast<float>(0x34000000u));
    const float reach = std::fabs(along);
    Vec4 p{dx * inverse * reach + car[0], dy * inverse * reach + car[1], dz * inverse * reach + car[2], 0.0f};
    const float lead = 1.0f - std::clamp((reach - 1000.0f) / 2000.0f, 0.0f, 1.0f);
    p[0] = target.velocity[0] * lead * 20.0f + p[0];
    p[2] = target.velocity[2] * lead * 20.0f + p[2];
    const float lowest = m_ground + 85.0f;
    const float lift = std::clamp(200.0f / (reach + 1.0f), 0.0f, 1.0f);
    const float swing = sinOf(m_passPhase + kHalfPi);
    p[1] = ((swing * 200.0f + lift * 200.0f) + p[1]) + 85.0f;
    if (p[1] < lowest) p[1] = lowest;
    const float side = m_passTo < 0.0f ? -1.0f : m_passTo > 0.0f ? 1.0f : 0.0f;
    p[0] = direction[2] * 200.0f * swing * side + p[0];
    p[2] = -direction[0] * 200.0f * swing * side + p[2];
    if (m_dt == 0.0f) keep = 1.0f;
    const float move = 1.0f - keep;
    m_position = {m_position[0] * keep + p[0] * move, m_position[1] * keep + move * p[1],
                  m_position[2] * keep + p[2] * move, m_position[3]};
    const float up = rise(race);
    m_position[0] = up + m_position[0];
    m_position[1] = up + m_position[1];
    m_position[2] = up + m_position[2];
    if (placed) {
        lookAt(m_look, camera);
    } else {
        follow(camera);
    }
    tvZoom(target, 2);
    camera.setFar(2000000.0f);
    m_fov = 0.7f;
    camera.setFov(std::bit_cast<float>(0x3F504B45u), 0.0f, race.wideScreen);  // 0.7 x 1.1623551
}

// 0x00404E40, a view on the ground ahead: with the reset the camera is put 10 cm over the ground (over the car's
// height where none is found) at the point 30 m ahead along the line (the route's first) and looks along the line:
// back toward the car right after the finish, else either way by a draw. Each frame the first of the race's vehicles
// whose chassis sphere holds the camera shakes it: speed x 0.01 x (1 - distance / radius) when above the shake left
// (held within 0 .. 1, the fade set to 1). The shake's phase runs at 70 x the fade a second, the fade at 1 a second
// down; the roll is sin(phase) and the pitch 0.3 + sin(0.7 x phase) / 4, both x 0.05 x the shake x the fade. A
// fixed zoom of 1.2.
void RaceCamera::onGround(const CameraTarget& target, const CameraRace& race, render::Camera& camera) {
    // 0x00404E5A: +0x230 2, the motion blur 0.5 and 0.5.
    camera.depthOfFieldMode = 2;
    camera.blurMiddle = 0.5f;
    camera.blurEdge = 0.5f;
    const Mat34& m = target.matrix;
    const Vec4 car{m[3], m[7], m[11], 0.0f};
    if (m_reset) {
        m_windowAsks.push_back(0);
        clearIntro();  // 0x00404E89
        release(camera);
        m_viewChanged = true;
        m_reset = false;
        Vec4 point = car;
        Vec4 direction{};
        if (target.competitor != nullptr && target.competitor->route != nullptr) {
            pointAhead(*target.competitor, 0, 3000.0f, point, direction);
        }
        float ground = 0.0f;
        const bool found = race.collision != nullptr && race.collision->groundBelow(point[0], point[2], ground);
        point[1] = (found ? ground : car[1]) + 10.0f;
        m_position = point;
        Vec4 look{};
        if ((m_director != nullptr && m_director->started) || unit() < 0.5f) {
            look = {point[0] - direction[0], point[1] - direction[1], point[2] - direction[2], point[3] - direction[3]};
        } else {
            look = {point[0] + direction[0], point[1] + direction[1], point[2] + direction[2], point[3] + direction[3]};
        }
        lookAt(look, camera);
        m_shudderPhase = 0.0f;
        m_shudderFade = 0.0f;
        m_shudder = 0.0f;
    }
    m_nearCar = false;
    if (race.cars != nullptr) {
        for (const CameraRace::Car& c : *race.cars) {
            const float dx = c.origin[0] - m_position[0];
            const float dy = c.origin[1] - m_position[1];
            const float dz = c.origin[2] - m_position[2];
            const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
            if (!(distance <= c.radius)) continue;
            if (!(0.3f * c.radius < distance)) m_nearCar = true;
            const float hit = c.speed * 0.01f * (1.0f - distance / c.radius);
            if (hit > m_shudder * m_shudderFade) {
                m_shudder = std::clamp(hit, 0.0f, 1.0f);
                m_shudderFade = 1.0f;
            }
            break;
        }
    }
    const float roll = sinOf(m_shudderPhase);
    const float pitch = sinOf(m_shudderPhase * 0.7f) * 0.25f;
    m_shudderPhase = m_shudderFade * m_dt * 70.0f + m_shudderPhase;
    m_shudderFade = std::clamp((1.0f - m_dt) * m_shudderFade, 0.0f, 1.0f);
    const float amount = m_shudder * m_shudderFade * 0.05f;
    m_roll = roll * amount;
    m_pitch = pitch * amount + 0.3f;
    place(camera);
    camera.setFar(2000000.0f);
    m_fov = 1.2f;
    camera.setFov(std::bit_cast<float>(0x3FB289AAu), 0.0f, race.wideScreen);  // 1.2 x 1.1623551
}

void RaceCamera::speedBlur(const CameraTarget& target, const CameraRace& race, render::Camera& camera) {
    m_blurAim = target.nitro ? target.nitroAmount : m_speed * 0.0125f;
    m_blurAim = std::clamp(m_blurAim, 0.0f, 1.0f);
    m_blur = (m_blurAim - m_blur) * 0.03f + m_blur;
    timerBlurOn = m_blur > 0.01f;
    timerBlur = timerBlurOn ? m_blur : 0.0f;
    camera.blurEdge = m_blur;
    camera.blurMiddle = target.nitro ? m_blur * 0.5f : m_blur * 0.0f;
    if ((m_mode >= kNextCar && m_mode <= kPassing) || m_mode == kFree || race.replayFastSet) {
        camera.blurEdge = 0.0f;
        camera.blurMiddle = 0.0f;
        timerBlur = 0.0f;
        timerBlurOn = false;
    }
}

// 0x004062A0: the field of view's factor eased toward its aim at the rate (x 60 a second), reaching it when the
// step would pass it.
void RaceCamera::easeFov(float rate) {
    const float gap = m_fovTarget - m_fov;
    const float step = ((gap * m_dt) * rate) * 60.0f;
    if (std::fabs(gap) < std::fabs(step)) {
        m_fov = m_fovTarget;
    } else {
        m_fov = step + m_fov;
    }
}

void RaceCamera::updateFov(const CameraTarget& target, render::Camera& camera, bool wideScreen) {
    // The field of view widens with the nitro, or with the car's acceleration and forward speed.
    float rate = 0.01f;
    if (target.nitro) {
        m_fovTarget = target.nitroAmount * 0.8f + 1.0f;
        rate = 0.02f;
    } else {
        m_fovTarget = 1.0f;
        if (target.forwardAcceleration > 10.0f) m_fovTarget = target.forwardAcceleration * 0.8f + 1.0f;
        if (m_fovTarget > 1.35f) m_fovTarget = 1.35f;
        const float forward = target.localVelocity[2];
        if (forward > 0.0f) m_fovTarget = (forward * kSpeedFov) * 0.5f + m_fovTarget;
        if (m_fovTarget > 1.5f) m_fovTarget = 1.5f;
    }
    easeFov(rate);
    if (wideScreen && m_fov > 1.0f) m_fov = (m_fov - 1.0f) * 0.975f + 1.0f;
    float fov = m_fov;
    if (m_lookBack && !(fov < 1.3f)) fov = 1.3f;
    camera.setFov(fov * kFovScale, 0.0f, wideScreen);
}

}  // namespace oj::scene
