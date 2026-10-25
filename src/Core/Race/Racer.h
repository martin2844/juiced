#pragma once
// The racers of a race (docs/spec/18-racers.md): each car's competitor (0x184 bytes, 0x0049AFF0) follows the
// route's lines, counts its laps, sectors and place and, for the AI's cars, steers and drives; the race's state
// (0x004A0220) counts down to the start, runs the competitors and sorts their places.

#include "Core/Cars/CarEffects.h"
#include "Core/Network/SessionStage.h"
#include "Core/Race/RaceKinds.h"
#include "Core/Base/Matrix.h"
#include "Core/Base/Random.h"
#include "Core/Tracks/Route.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace oj {

namespace save {
struct Settings;
}
struct Car;
struct Career;
class DamageGrid;
struct DamageZone;

class CarBody;
struct Competitor;
struct ContactPools;
struct RaceSession;
class RaceState;
class Arcade;

// A car's pattern of throttle on the grid (0x20 bytes at the car's +0x50C; 0x0049F500, stepped by 0x0049F600): two
// throttles, each held for its time, the second taken again by chance.
struct RevPattern {
    std::array<float, 4> entries{};  // +0x00: throttle 0, time 0, throttle 1, time 1
    int32_t index = 1;               // +0x10
    float timer = 0.0f;              // +0x14
    float chance = 0.0f;             // +0x1C: of the second throttle when the time is up

    float throttle() const { return entries[static_cast<size_t>(index) * 2]; }  // through +0x18

    // 0x0049F500 / the end of 0x0049B5D0: four values drawn (rand() % 10 each), the second throttle first.
    void make(CrtRandom& random, float frameTime);
    // 0x0049F600: the time runs down by the frame's time; when it is up, a value drawn chooses the next throttle.
    void step(CrtRandom& random, float frameTime);
};

// What a wheel of a car shows (0x9C bytes from the car's +0x200; 0x00487FE0): its tyre's touch, the skid marks, the
// smoke and the slide (what the AI reads).
struct WheelLook {
    bool down = false;               // +0x200: the tyre touches the route, facing up
    float compression = 0.0f;        // +0x204
    float lastCompression = 0.0f;    // +0x208
    Vec4 point{};                    // +0x210: the touch (centimetres), less the zone's offset in y
    float skid = 0.0f;               // +0x220: smoothed, 0 .. 1
    float smoke = 0.0f;              // +0x224: smoothed, 0 .. 1
    float slide = 0.0f;              // +0x228: the tyre's sideways slip against its speed, 0 .. 1
    int32_t lastSurface = 0;         // +0x22C
    int32_t surface = 0;             // +0x230: the zone's (0 tarmac, 1 sand, 2 grass)
    bool surfaceChanged = false;     // +0x234
    float poseSpeed = 0.0f;          // +0x1D8: the wheel node's speed (the wheels' frame 0x00487670)
};

// What the car's effects object (the vehicle's +0x64, 0x0048FA40) keeps of its motion for the impacts: the model's
// origin each frame (centimetres) and the change of its velocity (0x004924A0).
struct CarJolt {
    float change = 0.0f;              // +0x0C: the length of the velocity's change over the frame
    Vec4 velocity{};                  // +0x10: of the origin (centimetres a second); w the origin's
    Vec4 lastVelocity{};              // +0x20
    Vec4 origin{};                    // +0x30
    Vec4 delta{};                     // +0x40: the velocity's change; w the origin's
};

struct SplinePathPoint;

// Who drives a race car (the vehicle's +0x008): a player, the AI, another machine (the network).
enum class CarKind : int32_t { Player = 0, Ai = 1, Remote = 2 };

// The car of a competitor as the race sees it (the game's vehicle, 0x510 bytes): its body, its node, what its wheels
// and contacts say, its crew.
struct RaceCar {
    CarKind kind = CarKind::Ai;       // +0x008
    Mat34 node = identity34();        // +0x00C: the node's matrix (centimetres), the body's after the physics
    std::array<bool, 2> exhausts{};   // +0x14, +0x18: the exhaust's nodes (EXHAUST_C, else EXHAUST_S and EXHAUST_O)
    CarJolt jolt;                     // [+0x64]
    CarEffects effects;               // [+0x64]
    CarBody* body = nullptr;          // +0x168
    float lowest = 0.0f;              // +0x16C: the lowest touch of the route (centimetres)
    int32_t contactSide = 0;          // +0x170: 1 the contacts ahead of the centre, 2 behind, 0 none (0x004916F0)
    float impact = 0.0f;              // +0x174: what is left of the last hit's damage
    bool landed = false;              // +0x178: more wheels down than the frame before
    bool bodyContact = false;         // +0x17C: a contact with another body
    bool hasTurbo = false;            // +0x180: a turbo part fitted, or the model's own
    bool hasNitro = false;            // +0x184: a nitro part fitted
    int32_t nitroFlash = 0;           // +0x188: frames left of the nitro's start
    bool nitroStarted = false;        // +0x18C
    bool nitroAsked = false;          // +0x190
    float nitroLook = 0.0f;           // +0x194
    int32_t nitroWarning = 0;         // +0x198: the HUD's nitro messages given (1 low, 2 empty)
    bool onStand = false;             // +0x19C: on car_rolling's stand (its tyres do not wear, 0x0048EF70)
    uint8_t modelParts = 0;           // the model's (+0x430) +0x170: its parts knocked off, a bit each
    std::array<WheelLook, 4> wheels{};  // +0x200
    float wheelSkid = 0.0f;           // +0x410: a quarter of the wheels' skid
    float wheelSmoke = 0.0f;          // +0x414: a quarter of the wheels' smoke
    float wheelSlide = 0.0f;          // +0x418: a quarter of the wheels' slide
    float maxSkid = 0.0f;             // +0x41C
    float maxSmoke = 0.0f;            // +0x420
    float maxSlide = 0.0f;            // +0x424
    int32_t wheelsDown = 4;           // +0x428
    bool tipped = false;              // +0x42C: two wheels down, on one side
    uint32_t lastGear = 1;            // +0x438: the gear the engine's sound (+0x498: gearUp, gearDown) was told of
    int32_t gearChange = 0;           // +0x43C: 2 after a change up with that sound, down by 1 a frame; read by nothing
    // The sounds' side of the replay (0x00428010, 0x00428180) for the game's sound objects: as recorded the engine's
    // load (+0x498's +0x30; 0.5 without it) and the horn sounding (+0x4C0's +0x48); as played back the frame's engine
    // value (+0x508) and its horn (-1: no frame applied since the game took it).
    float engineLoad = 0.5f;
    bool hornSounding = false;
    float replayEngineSound = 0.5f;   // +0x508
    int32_t replayHorn = -1;
    RaceCar* followed = nullptr;      // +0x440: the car whose slipstream this one is in (0x00487D50)
    RaceCar* follower = nullptr;      // +0x444: a car in this one's slipstream
    float followGap = 0.0f;           // +0x448: from the point 6 m behind the followed car, by 447 cm
    bool rearLightsOut = false;       // +0x44C: the rear bumper's damage put out the rear lights
    bool headlightsOut = false;       // +0x450: the front bumper's damage put out the headlights
    bool rearPartOut = false;         // +0x454: the rear bumper's damage past the model's threshold
    float rumbleShare = 0.0f;         // +0x45C: the chance of a jolt on a rough surface
    float rearThreshold = 0.5f;       // +0x460: 0.1 for the models 5, 7, 11, 19 and 35, else 0.5
    bool jolted = false;              // +0x464
    float rumble = 0.0f;              // +0x468: the drag of a rough surface, falling to its table value
    Vec4 hitPoint{};                  // +0x46C: the direction of the last hit's point from the centre
    int32_t crew = 0;                 // +0x47C
    int32_t record = 0;               // +0x480: the driver's racer in the race session
    Competitor* competitor = nullptr;  // +0x494
    int32_t model = 0;                // [+0x4E4]: the car's model number (its drive, 0x0048CE20)
    int32_t drive = 0;                // 0x0048CE20 of the model: 0 front, 1 rear, 2 all wheels
    // [+0x4E4]: the car itself (its body parts and the damage it keeps, steeringPulls ..) when the race has its record.
    Car* car = nullptr;
    const Car* carRecord = nullptr;   // +0x4E4: the car the race started with (its paint, its HUD colours)
    float ghostTime = 0.0f;           // +0x4D8: after a respawn the car passes through others
    float upsideTime = 0.0f;          // +0x4E0: with fewer than two wheels down
    bool parked = false;              // +0x4E8: out of the world (set by nothing but 0, as the race makes the car)
    bool netPaused = false;           // +0x4EC: another machine's car whose race is paused (bit 5 of its state)
    // +0x4D4: the car's tracker on the race's centre line (0x00419E10, none without one): the line's point it heads
    // for (+0x08, -1 until known) and its share of the line (+0x18, 0 .. 1), read by nothing found (the crowd's look
    // takes only the tracker's node, the chassis).
    const std::vector<SplinePathPoint>* centreLine = nullptr;
    int32_t lineIndex = -1;
    float lineShare = 0.0f;
    // The damage's events of the frame (0x00488B50): the steering began to pull (+0x4F4, its side +0x4F8), the turbo
    // was lost (+0x4FC), the nitro began to leak (+0x500), the neon to flicker (+0x504).
    bool steeringFault = false;
    int32_t pullSide = 0;
    bool turboLost = false;
    bool nitroLost = false;
    bool neonDamaged = false;
    RevPattern revs;                  // [+0x50C]
    DamageGrid* damage = nullptr;     // [+0x4E4]+0xB4: the car's damage grid
    // The model's zones of the damage grid by its parts' table (+0x5C ..): FBUMP, RBUMP, BONNET, (SPOILER), SSKIRT_L,
    // SSKIRT_R, (the three exhausts), DECAL, and in the last place the sponsors' node.
    std::array<DamageZone*, 11> zones{};
};

// What a competitor knows of another (0x28 bytes, the competitor's +0x150 list; 0x0049AC50).
struct Neighbour {
    Competitor* other = nullptr;  // +0x00
    Vec4 local{};                 // +0x04: where it is in this car's frame (centimetres)
    float gap = 0.0f;             // +0x14: how far it is ahead along the lap (negative behind)
    float closing = 0.0f;         // +0x18: how fast the gap closes
    float contactTime = 99999.0f;  // +0x1C: the time until it is reached at the speed they close at
    float order = 0.0f;           // +0x20: the gap, far behind counted as far ahead (the sort's key)
    bool sameCrew = false;        // +0x24
};

// A competitor (the game's racer, 0x184 bytes; made by 0x0049AFF0, reset by 0x0049B5D0).
struct Competitor {
    int32_t index = 0;               // +0x000: in the race's list
    RaceState* race = nullptr;       // +0x004
    RaceCar* car = nullptr;          // +0x008 (the node +0x00C is the car's)
    // +0x010, +0x024, +0x028: 1, 0 and 30 by the reset (0x0049B5D0), read by nothing found.
    std::array<float, 3> unusedResets{1.0f, 0.0f, 30.0f};
    int32_t level = 0;               // +0x02C: of the driver (0x004BEAF0)
    float skill = 1.0f;              // +0x030: 0 .. 1 (0x0049E6A0)
    float throttle = 0.0f;           // +0x034: the AI's controls
    float steering = 0.0f;           // +0x038
    float brake = 0.0f;              // +0x03C
    float handbrake = 0.0f;          // +0x040
    float nitroTimer = 0.0f;         // +0x044
    bool nitro = false;              // +0x048
    float throttleFactor = 1.0f;     // +0x04C: 0.3 .. 1
    float revTimer = 0.0f;           // +0x050 (0x0049D1C0)
    float revThrottle = 0.0f;        // +0x054
    bool facingAway = false;         // +0x058: the car points away from the line
    float awayTime = 0.0f;           // +0x05C
    bool wrongWay = false;           // +0x060: away for over 1.5 s
    bool goingBack = false;          // +0x064: the lap's distance went down
    float rollingStart = 1500.0f;    // +0x068: the distance of a rolling start left
    float rollingDelay = 0.0f;       // +0x06C
    bool gridSkid = true;            // +0x070
    float finishRoll = 0.0f;         // +0x074: the distance rolled after a point-to-point finish
    const Route* route = nullptr;    // +0x078
    int32_t line = 0;                // +0x07C: FAST, ATTACK or CENTRE
    int32_t next = 0;                // +0x080: the point of the line the car heads for
    float along = 0.0f;              // +0x084: how far the car is behind the next point along its direction
    float fraction = 0.0f;           // +0x088: of the way from the point before to the next
    float offset = 0.0f;             // +0x08C: across the line, last frame
    float lapDistance = 0.0f;        // +0x090: along the fast line in this lap
    float lastLapDistance = 0.0f;    // +0x094
    float raceDistance = 0.0f;       // +0x098: laps done x the lap + the lap's distance
    float angle = 0.0f;              // +0x09C: between the car's heading and the line ahead
    bool spun = false;               // +0x0A0
    float bend = 0.0f;               // +0x0A4: how much the road ahead turns, 0 .. 1 (0x0049D320)
    float progressTimer = 10.0f;     // +0x0A8
    float progress = 0.0f;           // +0x0AC: the distance made since the timer last ran out
    float stuckTime = 0.0f;          // +0x0B0
    float reverseTime = 0.0f;        // +0x0B4
    bool respawnAsked = false;       // +0x0B8
    float lineHold = 0.0f;           // +0x0BC: how long a change of line is kept
    float magnet = 0.0f;             // +0x0C0: 0 .. 1, how far the car may be moved onto the line
    int32_t sector = 0;              // +0x0C4: the next sector to enter
    float gapAhead = 0.0f;           // +0x0C8: the player's, to the car ahead at the last sector
    float gapBehind = 0.0f;          // +0x0CC: the player's, set by the car behind
    int32_t mistakeKind = 0;         // +0x0D0: 0 a spin, 1 a wobble
    float mistakeTimer = 0.0f;       // +0x0D4
    bool mistakeRight = false;       // +0x0D8
    float mistakeInterval = 240.0f;  // +0x0DC: the mean time between mistakes (0x004BEAF0)
    float trafficTime = 0.0f;        // +0x0E0
    float behindTime = 0.0f;         // +0x0E4
    float swerveTimer = 0.0f;        // +0x0E8
    float swerveSide = 0.0f;         // +0x0EC
    float raceTime = 0.0f;           // +0x0F0
    float lapTime = 0.0f;            // +0x0F4
    float bestLap = 0.0f;            // +0x0F8
    int32_t crossings = 0;           // +0x0FC: of the start line
    int32_t lap = 0;                 // +0x100
    int32_t place = 1;               // +0x104
    int32_t lastPlace = 6;           // +0x108: the place when the gap to the player was last taken
    float playerGap = 0.0f;          // +0x10C (metres)
    int32_t finishPlace = 10;        // +0x110
    int32_t previousPlace = 1;       // +0x114
    bool finished = false;           // +0x118
    bool newLap = false;             // +0x11C
    bool newBestLap = false;         // +0x120
    bool newRecord = false;          // +0x124
    bool newSector = false;          // +0x128
    bool trapPassed = false;         // +0x12C: a speed trap's speed reached
    bool trapNow = false;            // +0x130
    float totalTime = 0.0f;          // +0x134: the race time over every heat (the reset keeps it)
    float points = 0.0f;             // +0x138
    float topSpeed = 0.0f;           // +0x13C (miles an hour)
    float cornerSpeed = 0.0f;        // +0x140
    int32_t corner = 0;              // +0x144: the next corner of the route (the player's)
    int32_t cornerWarning = 0;       // +0x148: the kind of a corner close ahead (CornerWarning)
    bool frontContact = false;       // +0x14C
    std::vector<std::unique_ptr<Neighbour>> neighbours;  // +0x150: sorted by their order each frame
    Neighbour* ahead = nullptr;      // +0x160: the nearest ahead
    Neighbour* behind = nullptr;     // +0x164: the nearest behind
    std::array<float, 5> sectorBest{};  // +0x168: the best time into each sector in a lap
    float sectorDelta = 0.0f;        // +0x17C
    float hornTimer = 0.0f;          // +0x180

    // 0x0049EFE0: the lap's distance; 0 on a point-to-point route before its first point or past its finish, or
    // before the start line is crossed.
    float distance() const;
};

// A corner close ahead as the route's corners give it (the HUD's arrows): gentle (green), medium (yellow), sharp
// (red), each mirrored too.
enum class CornerWarning : int32_t {
    None = 0,
    Gentle = 1,
    Medium = 2,
    Sharp = 3,
    GentleMirrored = 4,
    MediumMirrored = 5,
    SharpMirrored = 6,
};

// What the competitors read of the game around the race.
struct RaceRules {
    RaceMode mode = RaceMode::Career;    // [ctx+0x6C]+0x00
    RaceType type = RaceType::TimedLap;  // [ctx+0x6C]+0x04
    int32_t heat = 0;               // [ctx+0x6C]+0x08: a sprint's heat
    int32_t heats = 0;              // [ctx+0x6C]+0x0C
    int32_t players = 1;            // [ctx+0x6C]+0x58: the players racing
    bool replayRace = false;        // [ctx+0x6C]+0x68: the race is a replay's (0x00426FE0, start_replay)
    float minutes = 0.0f;           // [ctx+0x6C]+0x60: a cruise's (its time in minutes)
    float lapTime = 0.0f;           // [ctx+0x6C]+0x74: a challenge's
    float speed = 0.0f;             // [ctx+0x6C]+0x78
    ChallengeOutcome challenge = ChallengeOutcome::None;  // [ctx+0x6C]+0x7C
    // [ctx+0x64]+0x54: the driver's settings, read as the race goes (none before a driver is made or loaded).
    const std::optional<save::Settings>* settings = nullptr;
    bool replay = false;            // [ctx+0x9C]+0x828
    // [ctx+0x80] / [0x00728C40]: the career, read as the race goes (its profile's +0x00 and +0x628, the crews' money,
    // the bets on the race).
    Career* career = nullptr;
    float arcadeSkill = 0.0f;       // the arcade race's +0x24
    bool arcadeNitro = false;       // the arcade race's +0x04
    RaceSession* session = nullptr;  // [ctx+0x80]+0x30: the drivers
    Arcade* arcade = nullptr;        // [ctx+0x8C]: the arcade's progress the race's results go into
    RaceCar* player = nullptr;      // the first player's car
    // The network game ([0x0075EF8C]): this machine only watching ([0x0075EF9C]), the session's stage
    // ([0x0075EF90]) and a finish of a car of this machine sent (0x004EA990, its race time and best lap).
    bool network = false;
    bool spectator = false;
    std::function<net::SessionStage()> netStage;
    std::function<void(const Competitor&)> netFinished;
    // 0x004BEA60 on the race HUD's crew orders (Game/Race/Hud): the racer's lines flashed and, the orders closed,
    // the orders opened, at once (the frame's time for their lines).
    std::function<void(int32_t racer, float frameTime)> ordersFlash;

    // [ctx+0x64]+0xA0: the game options' A.I. DIFFICULTY (the settings' +0x4C; the options window gives 0 NORMAL and
    // 1 HARD). The stock code tests it against 0 in some places and against 1 in others.
    uint32_t aiDifficulty() const;
};

// The race's state (0x004A0220's object, the race manager's +0x1C).
// A racer the career race's chatter follows (0x40 bytes, 0x004B26E0).
struct ChatterEntry {
    int32_t racer = 0;           // +0x00: the session's racer
    int32_t lastPlace = -1;      // +0x04
    int32_t lastCrossings = 0;   // +0x08
    int32_t done = 0;            // +0x0C
    int32_t lastLevel = 0;       // +0x10
    int32_t nitroOutSaid = 0;    // +0x14
    float nitroTimer = 0.0f;     // +0x18 (not set by the stock: heap left as it was)
    float spinTimer = 0.0f;      // +0x1C
    float trafficTimer = 0.0f;   // +0x20
    float behindTimer = 0.0f;    // +0x24 (not set by the stock)
    float playerSpinTimer = 0.0f;  // +0x28
    int32_t rivalArmed = 1;      // +0x2C
    int32_t pullsSaid = 0;       // +0x30
    float delay = 5.0f;          // +0x34: a line may come once below 0
    int32_t skip = 1;            // +0x38: the first change not spoken of
};

// The career race's chatter (the career's +0x68; 0x004B2930): the player's crewmates and the racer bet on, a line of
// the race's speech when their places, levels, nitro, spins and contacts change. It draws rand() and gives a spun
// crewmate the order 1 with the sound or without it.
struct Chatter {
    std::vector<ChatterEntry> crew;    // +0x00
    bool hasBet = false;               // +0x10
    ChatterEntry bet;
    int32_t player = -1;               // +0x14: the player's own racer
    int32_t speakerMember = -1;        // +0x18
    int32_t speakerPlace = -1;         // +0x1C
    int32_t queued = 0;                // +0x20
};

class RaceState {
public:
    RaceState(const Route& route, int32_t laps, const RaceRules& rules);

    // 0x0049FCE0 / 0x0049AFF0 (in the car's making): a competitor for the car at the end of the list, reset
    // (0x0049B5D0: 4 rand() for the car's grid throttle).
    Competitor& add(RaceCar& car, CrtRandom& random, float frameTime);
    // 0x004A0030: every competitor knows every other, in the list's order.
    void linkNeighbours();
    // 0x0049FDF0: the sectors, each competitor reset (4 + 1 rand() each), the count by the race's type.
    void reset(CrtRandom& random, float frameTime);
    // 0x0049FDF0's count: 4.005 s for a replay's race or one of type 4, 4 s for type 3 and past 9, else 14 s.
    float startCount() const;

    // 0x004A0220: the count, every competitor's frame (0x0049EF00), the places, the leader and spins. The start
    // comes as the count goes below 0 (the fixed build's: reaches 0).
    struct Events {
        bool beep = false;  // a whole second of the count passed (hk_airhorn at 44000 Hz)
        bool go = false;    // the start (55000 Hz)
    };
    Events update(float dt, float frameTime, CrtRandom& random);

    const Route& route() const { return *m_route; }
    const RaceRules& rules() const { return m_rules; }
    RaceRules& rules() { return m_rules; }
    // The first player's car, once it is made.
    void setPlayer(RaceCar* car) { m_rules.player = car; }
    std::vector<std::unique_ptr<Competitor>>& competitors() { return m_competitors; }

    int32_t laps = 0;                 // +0x34
    float count = 0.0f;               // +0x38: 99 while the fly-by plays
    int32_t second = 3;               // +0x3C
    float bestLap = 0.0f;             // +0x40: the race's best lap so far (0x0049FDB0)
    float respawnTimer = 0.0f;        // +0x44: one respawn in 5 s
    std::vector<Competitor*> order;   // +0x10: by the race's distance
    std::vector<Competitor*> finishers;  // +0x20
    Competitor* leader = nullptr;     // +0x48
    Competitor* spinner = nullptr;    // +0x4C
    bool leaderPending = false;       // +0x50
    bool leaderChanged = false;       // +0x54
    float leaderTimer = 0.0f;         // +0x58
    bool spinEvent = false;           // +0x5C
    float eventHold = 5.0f;           // +0x60
    int32_t finishersShown = 0;       // +0x64: the finishers the HUD has told of (0x0044A4C0); 0 at each reset

    // 0x004BEAF0 (from 0x004BDF00 in the game's frame, before the race's): each driver of the race session with a car
    // chooses its level (a rival 0x004BEFD0, the player's crew 0x004BF210 with its orders 0x004BF280), its nerves grow
    // on the high level with the pressure of the cars close by and fall on the others, and the mean time between its
    // mistakes follows.
    void driverLevels(float frameTime);

    // 0x0049FDB0: a lap time better than the race's best (or the first) becomes the best.
    bool betterLap(float time);
    // 0x0049BE50 / 0x0049FF30: the competitor finishes: its place among the finishers, points for the first three;
    // an arcade race's result (0x0042B290 with 100).
    void finish(Competitor& c);
    // 0x0042B290: an arcade race (mode 0) off the network won: with the cheat "win all races" (the career's +0x64 +
    // 0x04) always; else a race or point-to-point (5, 7) whose first car (the player's) finished first, a sprint (6) by
    // `place` 1; a showoff (4) by the cruise's score over the race's target (0x00420390, 0x0042B240, both cut); other
    // types never. Won: the arcade's race completed (0x0042B350) with the first car's model.
    void arcadeResult(int32_t place);
    // 0x00450C28 (hud_challenge_results' enter): the challenge's outcome (+0x7C) still 0 becomes 1 and in an arcade
    // race the race is completed (0x0042B350).
    void challengeResult();

    Chatter chatter;

private:
    void start();  // 0x004A00D0
    // 0x004B26E0: the chatter's racers of the session: the player (crew 0, member 0), the crewmates, the racer bet on.
    void buildChatter();
    // 0x004B2930 with the frame's time.
    void chatterFrame(float frameTime, CrtRandom& random);
    // 0x004B2540 / 0x004B1F60: a line (-1 none).
    int32_t betLine(ChatterEntry& p, float t, CrtRandom& random);
    int32_t crewLine(ChatterEntry& p, float t, CrtRandom& random);
    Competitor* competitorOf(int32_t racer) const;
    Competitor* atPlace(int32_t place) const;  // 0x0049FD90
    void ordersFlash(int32_t racer, float frameTime);  // 0x004BEA60
    void events(float frameTime);  // 0x004A0130

    const Route* m_route;
    RaceRules m_rules;
    std::vector<std::unique_ptr<Competitor>> m_competitors;  // +0x00
};

// 0x00489740: the body's tilt held: a torque of -40000 ((pitch / pi)^2 - 0.3) about x when above 0 (Stock problem:
// the pitch, an arcsine, never gets there; the square loses its side. The fixed build's pitch over pi / 2 with its
// side: from 49.3 degrees on) and -40000 roll / pi about z; pitch and roll past 60 degrees clamped with their angular
// momentum taken away.
void tiltGuard(RaceCar& car);

// 0x00487D50: the slipstream: the body's drag factor back to 1; the first other car of the race's list (from its
// start, the car itself left out) whose point 6 m behind it along its velocity is within 447 cm of the car's origin
// cuts the drag by its speed squared x 7e-5 (at most 0.5) and becomes the car's followed (+0x440), the car its follower
// (+0x444), with the distance by 447 cm in +0x448.
void slipstream(RaceCar& car, const std::vector<RaceCar*>& cars);

// 0x004888B0: the nitro asked for (with a nitro part and some left), the impact's fall, the nitro left kept by the car,
// the brakes' wear while braking, the ghost time; touching the route above 20 m/s the car slows by 30 m/s each second.
void carFrame(RaceCar& car, float dt, float frameTime, RaceType raceType);

// 0x0048FEA0 in part, as the race is set up (before the cars settle on the grid): the effects object starts from the
// car's origin at rest.
void startJolt(RaceCar& car);

// 0x0048FA40 / 0x0048FEA0: the car's effects made from the race's (tracks_tarmac when dry, the nitro's flames at each
// exhaust, Heat_Haze, the explosion's and the chassis' effects, the scrape's sparks and flakes of TARMAC_DRY, each
// wheel's skids of tarmac and sand and its sand or spray, the tyres' squeals of tarmac and sand); in a split screen
// race the wheels' effects are not made. Every wheel's surface counts as changed. With the sound on ([0x0075F860]) each
// effect whose sounds loop draws one value (the loading thread's) as it is made.
void makeEffects(RaceCar& car, const EffectLibrary& library, const SurfaceEffects& surfaces, RaceMode raceMode,
                 RaceType raceType, bool wet, CrtRandom& random, bool sound);

// 0x00488B50: what the damage of the front bumper, the rear bumper and the bonnet does: the lights go out; all three
// past 0.99 and the steering pulls to a side drawn (rand() & 1) by 0.05 a frame from then on; the turbo goes with the
// front bumper and the bonnet past 0.99 and the rear past 0.5; the nitro leaks (the player's, 0.5 % a frame) once the
// front bumper is past 0.99; the neon flickers past 0.5 and is lost past 0.99.
void damageEffects(RaceCar& car, CrtRandom& random);

// 0x00487FE0: the wheels' looks from the tyres' contacts; on a rough surface (sand, grass) a drag and random jolts
// (rand(): 1 a wheel, 3 more on a jolt); a car with fewer than two wheels down for 4 s is put back on its line. Then
// 0x004924A0: the change of the car's velocity over the frame and, on the frames the game runs the impacts (every
// other one, 0x004916F0), the side of the body's contacts and their hits: the damage to the car's grid and the
// other's, the hits counted for the career (the sparks and the sounds are the game's: Main's impacts).
// Without a race (a car of a garage's scene, 0x00488EE0) the parts that read the race are left out: the car put
// back on its line (0x00487080 reads the competitor, which such a car has not: Stock problem, not followed), the
// cruise and the impacts (0x004916F0 writes the competitor's +0x14C at a contact ahead: likewise).
// True when the car was put back on its line (the caller blacks out the screen for a lone player's car,
// 0x0048880E).
bool wheelEffects(RaceCar& car, float dt, float frameTime, CrtRandom& random, ContactPools& pools, RaceState* race,
                  bool impacts);

// 0x004890B0's draw: a car with a neon that flickers draws one value a frame; true when it is a multiple of 4
// ([0x007253D4]), the neon dark this frame.
bool neonFlicker(RaceCar& car, CrtRandom& random);

// 0x0049F150: the competitor's next point (+0x80) the end of its line's segment nearest to a point (centimetres,
// 0x0049F010: the distance to the segment); on a circuit the segment from the last point back to the first counts too,
// giving 0. A point-to-point route's segments end at its finish.
void placeOnLine(Competitor& c, const Vec4& point);

// 0x00487080: the car put back on its line at the point before the next, 0.3 m up, facing the line (a type-4 race
// keeps its heading), left to settle for 45 steps of 0.05 s; 3 s through other cars; its +0x4DC 1 (read by nothing).
void respawn(RaceCar& car, const RaceRules& rules, ContactPools& pools);

// 0x004975A0, 0x00487080, 0x00497BB0: the tracker's point the line's nearest to the body's origin (centimetres,
// 0x00407B20; 0x0041BAA0).
void placeTracker(RaceCar& car);

// 0x0041A000 at the end of the car's frame, with the chassis node's place in the world (centimetres): its point the
// nearest at first; back one while behind the point before, past its point's plane on to the next (from the last to
// the first); its share the distance along the line to its place over the last point's.
void trackLine(RaceCar& car, const Vec4& place);

// 0x00497C50 for one car after its frame: the controls of its competitor while the AI drives it (or while the count
// is above 5): the reverse gear, the throttle squared, the brake cubed, the steering, the handbrake, the nitro, the
// horn.
void applyControls(RaceCar& car, RaceState& race, CrtRandom& random, float frameTime);

// [0x007689CC], set by nothing: the cars the AI does not drive held on their brakes (0x00497FA4), a player's controls
// not read (0x0041E278), every competitor on the FAST line (0x0049B3F6, [0x00721C94] 0).
inline constexpr bool kHoldCars = false;

// 0x00488FA0: the nitro asked for: on while the car has some left or the race is a replay's; started now, the HUD's
// flash for 3 frames (+0x188, [0x00716170]), each call one less; +0x18C whether it changed.
void askNitro(RaceCar& car, bool on, bool replayRace);

// 0x0041E200's nitro of a player's car (+0x184 a nitro part): with the arcade race's endless nitro (0x0042BA40, the
// race's +0x04) once the count is 0 the body's nitro 15 (+0x37C) and the gate; else NITRO held at a count of 0 the
// gate, otherwise off; the gate: on with a nitro part, the throttle over 0.9, no brake and a forward gear (2 up).
// Then on, it goes off with the throttle below 0.8 or the brake over 0.35.
void playerNitro(RaceCar& car, bool asked, float count, bool unlimited, bool replayRace);

// 0x0049F360: a competitor's horn: it sounds while traffic or a swerve holds it up, then stays quiet for 5 s and a
// random 0 .. 9 s more. True while it sounds.
bool hornSounds(Competitor& c, float frameTime, CrtRandom& random);

}  // namespace oj
