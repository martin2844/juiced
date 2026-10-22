#pragma once
// The race's camera of a player (0x00406300 each frame; docs/spec/17-race-camera.md): the views behind the car
// (0x00402040) and on it (0x004033E0), the sweep along the grid before the start (0x00405300), the field of view and
// the shake.

#include "Core/Base/Matrix.h"
#include "Core/Physics/TrackCollision.h"
#include "Core/Race/RaceKinds.h"
#include "Render/Camera.h"

#include <functional>
#include <utility>
#include <vector>

namespace oj {
struct Competitor;
struct Spl;
}

namespace oj::scene {

// What the camera reads of the car it follows (the race car; +0x168 its body).
struct CameraTarget {
    render::Node* node = nullptr;      // the car's node (+0x0C), the views on the car hang from it
    Mat34 matrix = identity34();       // its matrix (centimetres)
    Vec4 velocity{};                   // body +0x178 (m/s)
    Vec4 localVelocity{};              // body +0x198
    Vec4 localAngularVelocity{};       // body +0x1A8
    float forwardAcceleration = 0.0f;  // body +0x2B0: the local acceleration's z
    bool nitro = false;                // +0x190
    float nitroAmount = 0.0f;          // +0x194
    Vec4 wheelEffects{};               // +0x41C .. +0x424: the wheels' effects of the frame (0x00487FE0)
    float impact = 0.0f;               // +0x174
    bool replayRace = false;           // [G+0x6C]+0x68: the strong shake (0x00402F90) not in a replay's race
    const Competitor* competitor = nullptr;  // +0x494: the line and the progress the TV views place by
    // The cars of the slipstream (0x00487D50): the one this car follows (+0x440) and the one following it (+0x444),
    // their model origins (centimetres).
    bool followed = false;
    Vec4 followedOrigin{};
    bool follower = false;
    Vec4 followerOrigin{};
};

// What the camera reads of the race.
struct CameraRace {
    TrackCollision* collision = nullptr;     // [0x0075F0C8]: the walls the chase views keep off, the ground
    bool wideScreen = false;                 // [0x0075F0FC]
    int players = 1;                         // the race's players ([G+0x6C]+0x58)
    float count = 0.0f;                      // the count before the start (the race state's +0x38)
    int playerMode = 0;                      // the player's view (settings +0x8C per player)
    bool netSpectator = false;               // a network game only watched ([0x0075EF8C], [0x0075EF9C])
    const std::vector<Vec4>* places = nullptr;  // the route's grid points (race +0x30: +0x4C, +0xAC; w the heading)
    RaceType raceType = RaceType::TimedLap;     // [G+0x6C]+4
    bool paused = false;                     // [G+0x34]+0x20: the race paused, the camera's time 0 (0x00406322)
    bool replayFast = false;                 // [G+0x9C]: a replay playing (+0x828) fast forward (+0x7C)
    bool replayFastSet = false;              // [G+0x9C]+0x7C alone (the speed's blur, 0x00405FA0)
    // The race manager's vehicles: their model origins (centimetres) and their chassis nodes' sphere radii
    // (`+0x10`, `+0x74`), for the TV view's rise (0x00404660).
    struct Car {
        Vec4 origin{};
        float radius = 0.0f;
        float speed = 0.0f;  // of its body (m/s, +0x168: +0x178)
    };
    const std::vector<Car>* cars = nullptr;
    int self = -1;  // the camera's car among them
};

// The route's grid points for CameraRace::places (its GRIDPOINT lines, 0x004A1410: the route's +0x4C, their number
// +0xAC; w the heading): all of them whatever the cars on the grid, so the sweep before the start runs across the
// grid's width even with one car.
std::vector<Vec4> gridPlaces(const Spl& lines);

class RaceCamera;
class RaceIntro;

// The camera director of a player (`P+0x10`, 0x004085A0): after the finish it cuts between views at random.
class CameraDirector {
public:
    // 0x00408670: active, the camera's mode set with a reset (+0xA4, +0xB0) and a frame of 0.02 s, the time 0 and the
    // period.
    void start(RaceCamera& camera, int mode, float period, const CameraTarget& target, const CameraRace& race,
               render::Camera& node);
    // 0x00408830, each player frame with the frame's step: the game's rand() seeded again from a counter and the time,
    // the cut's transition, then at the period's end a cut (first the transition, a frame later a view of the table
    // 0x00711648 and a period of 3 .. 8 s).
    void update(RaceCamera& camera, float dt, bool paused, const CameraTarget& target, const CameraRace& race,
                render::Camera& node);
    // 0x00408750: no longer active, its time and period 0, a transition still running stopped (the sounds' places not
    // followed).
    void stop() {
        active = false;
        time = 0.0f;
        period = 0.0f;
        if (transition < 1.0f) stopBlend();
        if (sounds) sounds(false);
    }
    // The sounds' part of 0x00408670 (true) and 0x00408750 (false): the game's (the volume groups, the camera's car's
    // sounds turned to it or away).
    std::function<void(bool)> sounds;

    bool active = false;      // +0x00
    float time = 0.0f;        // +0x0C
    float period = 0.0f;      // +0x10
    bool cutDue = false;      // +0x1C
    bool started = false;     // +0x20
    float transition = 1.0f;  // +0x28: the cut's transition
    // The transition's object (+0x24, 0x00516150, vtable 0x006F7898): the post effect's switch [0x0075ABC8] (start
    // 0x00516110) and amount [0x0075ABC4] (0x00516120); both 0 as it is made, stopped (0x00516130) and destroyed.
    bool blendOn = false;
    float blendAmount = 0.0f;
    void stopBlend() {
        blendOn = false;
        blendAmount = 0.0f;
    }
};

class RaceCamera {
public:
    // The views: 0 .. 2 behind the car (near, far, farther), 3 and 4 on its right and left sides, 5 and 6 on it (the
    // bumper, the bonnet), 8 the replay's next car (the director's frame, 0x00408AC0), 10 the sweep before the start,
    // 11 a TV view beside the line ahead, 12 a view passing along the line, 13 a view on the ground ahead, 14 and 15
    // the free views (the mouse's, a still one). 3, 4, 14 and 15 (and 8 besides the replay) only come from a view kept
    // in the settings (+0x8C); CHANGE VIEW and the director never take them. 7 and the views past 15 are the view
    // behind the car (0x0040651D); 9 is the fly-by (0x00405BE0, 15-tracks.md), played apart from this camera here.
    enum Mode {
        kChase = 0, kChaseFar = 1, kChaseFarther = 2, kRightSide = 3, kLeftSide = 4, kBumper = 5, kBonnet = 6, kNextCar = 8,
        kFlyBy = 9, kBeforeStart = 10, kTrackside = 11, kPassing = 12, kOnGround = 13, kFree = 14, kFreeStill = 15
    };

    // 0x00406990: the view taken (with the reset: the camera put back to the view's start), the view on the ground's
    // nearness and the forced look back cleared, and a frame of 0.02 s.
    void take(int mode, bool reset, const CameraTarget& target, const CameraRace& race, render::Camera& camera);

    // 0x00406300: dt the race's step time over the frame's physics steps, taken as 0 while the race is paused
    // (race.paused) but by the free views 14 and 15, which move on.
    void update(const CameraTarget& target, const CameraRace& race, float dt, render::Camera& camera);

    // 0x00406AB0 after the frame's view: CHANGE_CAMERA going down takes the next view (0 to 1, 1 to 6, else 0);
    // REAR_VIEW held turns the views round; DRIVER_CAMERA_PAN (pan) turns them to a side.
    void controls(bool changeView, bool rearView, float pan, const CameraTarget& target, const CameraRace& race,
                  render::Camera& camera);

    // 0x004069F0 (a replay's CHANGE VIEW): with the director active the view behind the car and the director stopped
    // (0x00408750); else the next view: 1 after 0 and the others, 5 after 1, 0 after 5. Each with a reset and a frame
    // of 0.02 s. True when the director was stopped.
    bool changeView(const CameraTarget& target, const CameraRace& race, render::Camera& camera);

    // A changed view's frame (+0xB0 cleared, 0x00406678): the 3D world's emitters to be reset; taken once.
    bool takeSoundsReset() {
        const bool r = m_soundsReset;
        m_soundsReset = false;
        return r;
    }

    // 0x00401DB0 with another car: the view reset (+0xA4, +0xB0).
    void newCar() {
        m_reset = true;
        m_viewChanged = true;
    }

    // 0x00401F20: the view reset (+0xA4, +0xB0), not near a car (+0x164); the director's cut due within 2 s of its
    // last one (at once after that).
    void resetView() {
        m_reset = true;
        m_viewChanged = true;
        m_nearCar = false;
        if (m_director != nullptr) {
            m_director->period = m_director->time < 2.0f ? 2.0f - m_director->time : 0.0f;
            m_director->cutDue = true;
        }
    }

    // The camera leaves the car's node (0x00401F90; the race's end).
    void release(render::Camera& camera);

    int mode() const { return m_mode; }
    bool lookingBack() const { return m_lookBack; }  // +0xA8
    float pan() const { return m_pan; }              // +0xCC
    float ground() const { return m_ground; }  // +0x168
    // 0x004206B1 .. 0x00420771: the listener's offset from the camera (cm, across and along): modes 3, 4, 6 100 back,
    // 5 200; 5 and 6 neither panned nor looking back 100 to the side of the look and 100 back again.
    void listenerOffset(float& x, float& z) const {
        x = 0.0f;
        z = 0.0f;
        if (m_mode >= kRightSide && m_mode <= kBonnet) z = m_mode == kBumper ? -200.0f : -100.0f;
        if ((m_mode == kBumper || m_mode == kBonnet) && m_pan == 0.0f && !m_lookBack) {
            const float side = m_sideAngle == 0.0f ? 0.0f : (m_sideAngle < 0.0f ? -1.0f : 1.0f);
            x = side * -100.0f;
            z = z - -100.0f;
        }
    }
    // 0x00408670 on the camera: the mode set as it is, with a reset and the views' flags.
    void cut(int mode);
    void forceLookBack() { m_lookBackForced = true; }  // +0xF4
    void setDirector(CameraDirector* director) { m_director = director; }
    // The fly-by (+0x1D0, made with the camera by 0x0041C350), cleared as some views reset (RaceIntro::clear).
    void setIntro(RaceIntro* intro) { m_intro = intro; }
    // The race front end's window choice (0x004584C0) each view asks for as it resets: 1 from the views behind and
    // on the car, 0 from the sweep before the start and the TV views; taken in order.
    std::vector<int> takeWindowAsks() { return std::exchange(m_windowAsks, {}); }
    // The camera's rain blobs to be cleared since the last ask (0x00406629, 0x00406654).
    bool takeBlobClear() { return std::exchange(m_clearBlobs, false); }
    const Vec4& position() const { return m_position; }
    float fov() const { return m_fov; }

private:
    void chase(const CameraTarget& target, const CameraRace& race, render::Camera& camera);  // 0x00402040
    void onCar(const CameraTarget& target, render::Camera& camera, bool wideScreen);         // 0x004033E0
    void onSide(const CameraTarget& target, const CameraRace& race, render::Camera& camera);  // 0x004027B0
    void beforeStart(const CameraTarget& target, const CameraRace& race, render::Camera& camera);  // 0x00405300
    void lookAt(const Vec4& point, render::Camera& camera);  // 0x00403740
    void place(render::Camera& camera);                      // 0x00402B50
    void newAim(float yawScale, float minAmount, float maxAmount, float minPeriod, float maxPeriod);
    void impactShake(const CameraTarget& target, float scale);
    void easeShake(render::Camera& camera);
    void shakeFor(const CameraTarget& target, float amount, float pace, render::Camera& camera);  // 0x00402F90
    void shake(float yawScale, float minAmount, float maxAmount, float minPeriod, float maxPeriod,
               render::Camera& camera);  // 0x00402C00
    void updateFov(const CameraTarget& target, render::Camera& camera, bool wideScreen);  // 0x004060F0
    void easeFov(float rate);                                                             // 0x004062A0
    void trackside(const CameraTarget& target, const CameraRace& race, render::Camera& camera);  // 0x00403F30
    void placeTrackside(const CameraTarget& target, const CameraRace& race);                     // 0x00404150
    float rise(const CameraRace& race) const;                                                    // 0x00404660
    void tvZoom(const CameraTarget& target, int kind);                                           // 0x00405DB0
    void follow(render::Camera& camera);                                                         // 0x00403B00
    void passing(const CameraTarget& target, const CameraRace& race, render::Camera& camera);    // 0x004047C0
    void onGround(const CameraTarget& target, const CameraRace& race, render::Camera& camera);   // 0x00404E40

    int m_mode = kChase;           // +0x18
    Vec4 m_position{};             // +0x24
    float m_pitch = 0.0f;          // +0x34
    float m_yaw = 0.0f;            // +0x38
    float m_roll = 0.0f;           // +0x3C
    Vec4 m_tilt{};                 // +0x84: the views on the car's pitch and roll with the car, eased
    Vec4 m_offset{0.0f, 210.0f, -630.0f, 0.0f};  // +0x94: from the car to the camera, eased
    bool m_reset = true;           // +0xA4: the next frame puts the camera back to the view's start
    bool m_lookBack = false;       // +0xA8
    bool m_lookedBack = false;     // +0xAC
    bool m_viewChanged = true;     // +0xB0
    bool m_soundsReset = false;
    bool m_clearBlobs = false;     // the rain blobs to clear (the caller holds them)
    bool m_attached = false;       // +0xB4: the camera's node hangs from the car's
    float m_fov = 1.0f;            // +0xB8: the field of view's factor, eased
    float m_fovTarget = 0.0f;      // +0xBC
    float m_sideAngle = 0.0f;      // +0xC8: the look to the side of the views behind the car
    float m_pan = 0.0f;            // +0xCC: DRIVER_CAMERA_PAN's value, turned round
    float m_maxDistance = 10000.0f;  // +0xE0: the TV view's farthest from the car
    float m_tvBase = 0.0f;         // +0xE4
    float m_tvHeight = 0.0f;       // +0xE8
    float m_tvPhase = 0.0f;        // +0xEC
    float m_tvRange = 0.0f;        // +0xF0
    bool m_lookBackForced = false; // +0xF4
    Vec4 m_lookOffset{};           // +0x13C: from the cars' middle to the car, eased (0x00403800)
    Vec4 m_middleOffset{};         // +0x14C: from the cars' middle to the car, of the last frame
    float m_lookCars = 0.0f;       // +0x15C: the cars the look takes (1 .. 3)
    float m_lookBlend = 0.0f;      // +0x160: 1 .. 0 at 0.5 a second, the ease of the look's offset
    // The passing view (its own use of +0xE4 .. +0xEC): along the line from the first distance to the second as the
    // phase goes from -pi / 2 to pi / 2.
    float m_passFrom = 0.0f;       // +0xE4
    float m_passTo = 0.0f;         // +0xE8
    float m_passPhase = 0.0f;      // +0xEC
    // The view on the ground (+0xE8 .. +0xF0): its shake as a car passes over.
    float m_shudderPhase = 0.0f;   // +0xE8
    float m_shudderFade = 0.0f;    // +0xEC
    float m_shudder = 0.0f;        // +0xF0
    bool m_nearCar = false;        // +0x164: the view on the ground within 0.3 of a car's chassis sphere
    float m_ground = 0.0f;         // +0x168: the ground's height under the camera, kept when none is found
    Vec4 m_spin{};                 // +0x54 .. +0x5C: the TV view's turn rates (never set before; 0)
    CameraDirector* m_director = nullptr;  // +0x1EC
    RaceIntro* m_intro = nullptr;          // +0x1D0
    void clearIntro();
    std::vector<int> m_windowAsks;
    // The free views (14 and 15 only from a view kept in the settings, +0x8C): 15's turn (+0x1D4 pitch, +0x1D8 yaw),
    // zoom (+0x1E4) and the flag of its moving branch (+0x1E8, 0 from the constructor and never set).
    // +0x1C, the kind of the zoom of view 8 (0x00405DB0), and +0x20, the divisor of its kinds 0 and 1: neither the
    // constructor (0x00401760) nor any other code sets them, and the camera's 0x1F0 bytes come from the game's heap
    // uncleared (0x0041DE30 → 0x0059BA60, HeapAlloc with no flags). Stock problem: whatever the block held; 0 and 0
    // here, as fresh memory: kind 0, whose aim 200000 / (... x 0) is held at 1 (0.01 beyond 2 km).
    int32_t m_zoomKind = 0;
    float m_zoomDivisor = 0.0f;
    void nextCarView(const CameraTarget& target, const CameraRace& race, render::Camera& camera);  // 0x00403E40
    float m_freePitch = 0.0f;
    float m_freeYaw = 0.0f;
    float m_freeZoom = 1.0f;
    int32_t m_freeMoving = 0;
    void freeView(render::Camera& camera, bool wideScreen);       // 0x00406DA0
    void freeStillView(render::Camera& camera, bool wideScreen);  // 0x00407310
    float m_dt = 0.0f;             // +0xD0
    Vec4 m_sweepFrom{};            // +0x108: the sweep before the start
    Vec4 m_sweepTo{};              // +0x118
    float m_sweep = 0.0f;          // +0x128: 0 .. 1 along it
    Vec4 m_look{};                 // +0x12C: the point the sweep looks at (0x00403800)
    void lookPoint(const CameraTarget& target);  // 0x00403800
    float m_speed = 0.0f;          // +0x19C: of the car (m/s)
    // 0x00405FA0, after the frame's view: the speed's blur (+0xC0) eased by 0.03 a frame toward its aim (+0xC4):
    // under nitro the nitro's amount, else the car's speed x 0.0125, within 0 .. 1. The scene camera's motion blur
    // edge (+0x228) takes it, its middle (+0x224) half of it under nitro, else 0; views 8 .. 12 and 14 and a replay
    // fast forward (+0x7C) put both at 0.
    void speedBlur(const CameraTarget& target, const CameraRace& race, render::Camera& camera);
    float m_blur = 0.0f;           // +0xC0
    float m_blurAim = 0.0f;        // +0xC4

public:
    // The game's timer ([G+4]) takes the speed's blur (+0x20, above 0.01; else 0) and a flag (+0x24, 1 with it); the
    // pause and the replay's stop clear them. Their reader is not found in the race's code.
    float timerBlur = 0.0f;
    bool timerBlurOn = false;

private:
    float m_shakeTime = 0.0f;      // +0x1A8
    float m_shakePeriod = 0.0f;    // +0x1AC
    Vec4 m_shakeFrom{};            // +0x1B0: pitch, yaw, roll
    Vec4 m_shakeTo{};              // +0x1C0
};

}  // namespace oj::scene
