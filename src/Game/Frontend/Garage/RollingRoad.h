#pragma once
// car_rolling: the garage's dyno.

#include "Core/Physics/PhysicsWorld.h"
#include "Game/Frontend/Career/CutScene.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/Frontend/Garage/Graph.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>
#include <memory>

namespace oj::frontend {

// car_rolling's run (its +0x78): braked until SELECT, the throttle after the count, the gears run up, braking after
// the top gear's top, the results shown.
enum class DynoState : int32_t { Ready = 0, Starting = 1, Running = 2, Braking = 3, Results = 4 };

// The window of the record 0x007228D0 (vtable 0x006F60EC).
class RollingRoad {
public:
    // 0x004DD490
    void enter(GarageHost& host, ui::Screen& s);
    // 0x004DDF90
    void leave(GarageHost& host, ui::Screen& s);
    // 0x004DF260
    void update(GarageHost& host, ui::Screen& s, uint32_t event);
    // 0x004DF660
    void draw(GarageHost& host, ui::Screen& s);

private:
    scene::View3d* frameView(ui::Screen& s);
    // MSG: the text (0x005B19C0), its transition started again (0x005B2A50) and its highlight (vtable +0x0C,
    // 0x005B2930).
    void showMessage(GarageHost& host, ui::Screen& s, size_t text, bool highlighted);
    // 0x004DE350 by the state: 1 no brake, the throttle 1 while the countdown is over 3 s, else 0, by hand, neutral,
    // the traction control's light and text hidden; 2 by hand, no brake, the throttle 0.49 in neutral, else 1; past 0.6
    // of the last TorqueCurveX point (rpm, 0x0044D930) in neutral or past 0.97 of it the wait (+0x5C) runs down by the
    // window's step within 0 .. 2 s, and at 0 the next gear (0x004F7610) with 2 s to wait, or with the last gear
    // reached state 3, each with (0, -10000, 0) on the external force; the last: WARNING, MSG "RUN COMPLETE" (100) not
    // highlighted; 3 the lights hidden, the brake 0.512, no throttle; under 10 m/s forward state 4: SEL_ shown,
    // WARNING, MSG "EXIT DYNO" (101) highlighted, the DATA elements going out and the RESULT_ ones coming in,
    // RESULT_SPEED, RESULT_TORQUE and RESULT_BHP the texts of DATA_MAXSPEED, DATA_MAXTORQUE and DATA_MAXBHP,
    // RESULT_SPEED_OLD, RESULT_BHP_OLD and RESULT_TORQUE_OLD the car's last run (+0x94, +0x90, +0x8C; "%03d" cut, "---"
    // for -1 and below), RESULT_KEY5 "LAST RUN" (1600); the run's bests kept by the car; RESULT_BHPREV the revs of the
    // best power ("%04d", cut); the graph's drawn points and next point 0, its scales 136 / (the best x 1.2); 4
    // nothing; else brake 1, no throttle, by hand.
    void script(GarageHost& host, ui::Screen& s);
    // 0x004DEBA0: DATA_REVS the revs ("%03d", cut), the best of them (+0x54) in DATA_MAXREVS; DATA_GEAR "R", "N", "1"
    // .. "6"; DATA_SPEED the speed forward (under 0 none) in mph (x 2.235) or km/h (x 3.6) by the settings, the best
    // (+0x48) in DATA_MAXSPEED. In state 2: DATA_TORQUE the engine's torque out (+0x360, under 0 none) x 0.29 (x
    // 1.335818 in newton metres) and rand() & 3 - 2, the best (+0x4C) in DATA_MAXTORQUE; DATA_BHP the torque x the
    // engine's speed x 0.000481, a new best (+0x50) keeping its revs and rand() % 10 - 5 (+0x58), in DATA_MAXBHP; in
    // fourth gear past the graph's revs without the traction control at work (+0x388) the graph's next point the torque
    // and the power each with rand() % 6 - 3, the point after, the graph's revs 74.62 more.
    void readings(GarageHost& host, ui::Screen& s);
    // 0x004DF180: a driven front wheel (0) turns the first road's rollers about x by -0.0175 x its spin, a driven rear
    // wheel (2) the second road's by 0.0175 x its spin (0x00534A10).
    void rollers();

    std::unique_ptr<PhysicsWorld> m_world;  // +0x0C
    std::unique_ptr<CutCar> m_car;          // +0x14
    int m_revs = -1;        // +0x18 DATA_REVS
    int m_speed = -1;       // +0x1C DATA_SPEED
    int m_gear = -1;        // +0x20 DATA_GEAR
    int m_torque = -1;      // +0x24 DATA_TORQUE
    int m_bhp = -1;         // +0x28 DATA_BHP
    int m_maxSpeed = -1;    // +0x2C DATA_MAXSPEED
    int m_maxTorque = -1;   // +0x30 DATA_MAXTORQUE
    int m_maxBhp = -1;      // +0x34 DATA_MAXBHP
    int m_maxRevs = -1;     // +0x38 DATA_MAXREVS
    int m_msg = -1;         // +0x3C MSG
    int m_tcLight = -1;     // +0x40 DATA_TCLIGHT
    int m_tcLightText = -1;  // +0x44 DATA_TCLIGHTTEXT
    float m_topSpeed = 0.0f;   // +0x48
    float m_topTorque = 0.0f;  // +0x4C
    float m_topBhp = 0.0f;     // +0x50
    float m_topRevs = 0.0f;    // +0x54
    float m_bhpRevs = 0.0f;    // +0x58: the revs at the top power
    float m_shiftWait = 0.0f;  // +0x5C
    std::array<render::Node*, 4> m_rollers{};  // +0x60 .. +0x6C: ROLLINGFRONT, ROLLINGREAR of each road
    std::unique_ptr<Graph> m_graph;  // +0x70: RESULT
    float m_graphRevs = 0.0f;        // +0x74: the revs of the graph's next point
    DynoState m_state = DynoState::Ready;  // +0x78
    float m_countdown = 0.0f;        // +0x7C
    uint32_t m_testGear = 4;         // +0x80
};

}  // namespace oj::frontend
