#pragma once
// CMSCutScene: the garage's test track; the cars of the garage's scenes (CMSCutScene's, car_rolling's).

#include "Core/Physics/PhysicsWorld.h"
#include "Core/Race/Racer.h"
#include "Game/Cars/CarSound.h"
#include "Game/Cars/CarWheels.h"
#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace oj::frontend {

// A car's step of the cut scene's script (its +0x50), each for its time; past FullThrottle it goes on counting.
enum class CutCarStage : int32_t {
    Waiting = 0,       // braked, no throttle
    Revving = 1,       // braked, full throttle in first gear
    Braked = 2,        // braked, no throttle, the push 0
    Rolling = 3,       // pushed forward, the push rising
    Launching = 4,     // the throttle rising with the time, pushed
    Driving = 5,       // a quarter throttle, automatic
    Easing = 6,        // throttle 0.16, automatic
    FullThrottle = 7,  // full throttle, automatic
};

// The cut scene camera's shot (the record's +0x24): looking at the cars from three places, then the end.
enum class CutCameraShot : int32_t { Start = 0, Sweep = 1, CloseUp = 2, Follow = 3, End = 4 };

// A car of CMSCutScene: the race's car (0x00485530, +0x40 x 4) and the script's lists of the record.
struct CutCar {
    std::unique_ptr<CarBody> body;
    std::unique_ptr<scene::CarModel> model;
    RaceCar car;
    Car* record = nullptr;  // the garage's car
    std::array<WheelPose, 4> wheels{};
    render::Node* chassis = nullptr;           // _CHASS
    render::ModelNode* brakeLight = nullptr;   // _BRKLIGHT
    render::ModelNode* reverseLight = nullptr; // _REVLIGHT
    int shadow = -1;      // +0x30: gen_shadow in CAR3D's list
    CutCarStage stage = CutCarStage::Waiting;  // +0x50
    float timer = 0.0f;   // +0x60
    float push = 0.0f;    // +0x70: the velocity given in stages 3 and 4
    bool onStand = false;  // the car's +0x19C: car_rolling's
    std::array<render::Node*, 2> exhaustNodes{};  // +0x14, +0x18
    std::array<Vec4, 2> exhaustPlaces{};          // +0x1C, +0x2C
    std::unique_ptr<CarSound> sound;              // +0x498: car_rolling's (0x004DD300)
};

// The window of the record 0x007220A8.
class CutScene {
public:
    // 0x004C6B80
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004C7030
    void leave(ui::Screen& s);
    // 0x004C7BF0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    scene::View3d* sceneView(ui::Screen& s);
    // 0x004C72D0, for each car: its gen_shadow's matrix the identity at the body's centre of gravity (x 100,
    // [0x00718FB0] = 36 cm lower) turned about y by minus its yaw (+0x21C); then by its stage: 0 brake 1, throttle 0,
    // by hand; 1 brake and throttle 1, neutral, by hand; 2 brake 1, throttle 0, by hand, its push (+0x70) 0; 3 no
    // brakes, no throttle, by hand, neutral, the velocity (0, 0, push) in its frame (0x004F2F80) and the push up by the
    // window's step within 0 .. 1; 4 no brakes, the throttle the stage's time squared (1 from 1 s), by hand, neutral,
    // the velocity (0, 0, push); 5 no brake, throttle 0.25, automatic, the steering 0 (0x004203E0, within -1 .. 1); 6
    // the same with throttle 0.16; then no brake, full throttle, automatic, the steering 0.
    void script(CareerHost& host, ui::Screen& s);
    // 0x004C7870: the camera by its stage: 1 looks at (0, 0, 0) at once and moves 450 cm a second along x, the front
    // end's black 1 - the stage's time while under 1 s; 2 looks at (0, 50, 100), 25 cm a second back along x, the black
    // likewise; 3 looks at (0, 0, 0), 150 cm a second along z, the black likewise (and the music told, not made); 4 the
    // black 1.
    void camera(CareerHost& host, ui::Screen& s);
    // 0x004A4060: the test track's race (type 9, 99999 laps, the morning, dry, Angel SuperSpeedway's Speedway_R1, one
    // player, the settings' +0x84 for +0x54), loading asked for, the cruise's +0x10 set.
    void testTrackRace(CareerHost& host);

    std::unique_ptr<PhysicsWorld> m_world;     // +0x0C
    Vec4 m_place{};                            // +0x14: the camera's
    CutCameraShot m_stage = CutCameraShot::Start;  // +0x24: the camera's
    float m_timer = 0.0f;                      // +0x28
    std::vector<std::unique_ptr<CutCar>> m_cars;  // +0x40, count +0x80
};

// 0x00485530 for a car of a garage's scene (not of the AI; the record's wear and nitro kept): its body, model,
// drive, wheels and lights; the view draws the model.
std::unique_ptr<CutCar> makeSceneCar(WindowHost& host, scene::View3d& view, Car& record,
                                     const CarDynamics& dynamics, const Vec4& place);
// 0x004DD300: the car's engine sounds in the window's 3D world.
void makeSceneSound(WindowHost& host, CutCar& car, const Car& record);
// 0x00488EE0 for a car of a garage's scene: its frame (the wheels, the lights, the exhausts, the effects, the
// sounds), the model following the body.
void cutCarFrame(WindowHost& host, CutCar& car, float step, ContactPools& pools);

}  // namespace oj::frontend
