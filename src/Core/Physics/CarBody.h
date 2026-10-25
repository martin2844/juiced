#pragma once
// A car's body in the physics (vtable 0x006F6A68, made by 0x004F6B50; docs/spec/16-physics.md, "The car"): the rigid
// body with the engine's and the wheels' spins in its state, the drive through the clutch and the gears, the brakes,
// the ABS, the traction control and the automatic gearbox.

#include "Core/Physics/Dynamics.h"
#include "Core/Physics/RigidBody.h"

#include <cstdint>
#include <vector>

namespace oj {

// A wheel (0x24 bytes).
struct CarWheel {
    const WheelDef* def = nullptr;  // +0x00
    size_t index = 0;               // +0x04: its spin in the state
    float spin = 0.0f;              // +0x08 (rad/s)
    float brakeTorque = 0.0f;       // +0x0C: brakes, handbrake and axle against the spin
    float tyreTorque = 0.0f;        // +0x10: the tyre's pull on the rim
    float torque = 0.0f;            // +0x14: the sum
    float touched = 0.0f;           // +0x18: cleared before each step
    float brake = 0.0f;             // +0x1C: the brake applied, 0 .. 1
    float handbrake = 0.0f;         // +0x20
};

// The car definition the body is made from: each car keeps its own copy (the constraint writes into the torque
// curve).
struct CarBodyDef {
    explicit CarBodyDef(const CarDynamics& d) : carDef(d) {}
    CarDynamics carDef;
};

class CarBody : private CarBodyDef, public RigidBody {
public:
    CarBody(const CarDynamics& def, const Vec4& origin);
    CarBody(const CarBody&) = delete;
    CarBody& operator=(const CarBody&) = delete;

    const CarDynamics& dynamics() const { return carDef; }

    // 0x00488B50: the turbo lost to the damage, its boost back to 1.
    void loseTurbo() { carDef.turbo = 1.0f; }
    // 0x004896A0: the turbo back. Stock problem: at 1.3, whatever the car's own (its turbo part's 1.25 .. 1.4, a
    // stock turbo's 1.01); the fixed build's the Turbo the car was made with.
    void restoreTurbo() { carDef.turbo = m_fittedTurbo; }
    // The definition's traction control (+0xC8 +0x174; CMSCutScene clears it each frame, 0x004C72D0, car_rolling
    // sets it, 0x004DE350).
    void setAsr(bool on) { carDef.asr = on; }
    // The definition's DragCoefficient (+0xC8 +0xD4; car_rolling's 0 at its enter, 0x004DD8D8).
    void setDrag(float drag) { carDef.body.drag = drag; }
    // 0x0048EF70: each tyre contact's LateralMu (+0x54) and the ratio of its grips again (+0x70).
    void setTyreGrip(float lateralMu);
    // 0x00485CC5: the LateralMu of the last tyre contact (0 without one).
    float tyreGrip() const;
    // As the race makes the car (0x00485530, after the body), by its drive (0x0048CE20: 0 front, 1 rear, 2 all
    // wheels). An AI's car (0x00486D20): the centre of gravity's z (+0xC8) 0.25 with the rear wheels driven, else 0.1,
    // SteeringSpeed (+0x158, read by nothing) 2, the mass (+0xAC) at most 1666 and LocalGravity (+0xD0) x 1.2; another
    // (0x00486E00): LocalGravity 0.05 more with the rear wheels driven, 0.05 less with all of them. The body's
    // inverse mass and inertia stay those of its making (0x004F00C0).
    void raceTune(bool ai, int32_t drive) {
        BodyDef& b = carDef.body;
        if (ai) {
            b.centreOfGravity[2] = drive == 1 ? 0.25f : 0.1f;
            carDef.steeringSpeed = 2.0f;
            if (b.mass > 1666.0f) b.mass = 1666.0f;
            b.localGravity = b.localGravity * 1.2f;
            return;
        }
        if (drive == 1) b.localGravity = b.localGravity + 0.05f;
        if (drive == 2) b.localGravity = b.localGravity - 0.05f;
    }
    // 0x00485D20, as the race makes the car: each line collider (a wheel's ray, by its ID) takes the x and z of its
    // wheel's node (bases, centimetres); with a tyre contact of that ID (the first, 0x004EF240) whose stiffness is
    // above 0, its length becomes its point's y + the tyre's sag at rest, LocalGravity / stiffness x the contact's mass
    // x 1000. The sag of each wheel (0 without one).
    std::array<float, 4> restWheels(const std::array<Vec4, 4>& bases);
    // 0x00485DFC, a car not of the AI (the call's seventh argument 0): the centre of gravity's z (+0xC8, metres) from
    // the front and rear wheels' nodes (z, centimetres) by FrontWeightDistribution (+0x184):
    // ((front - rear) x the share + rear) x 0.01.
    void weighAxles(float frontZ, float rearZ) {
        carDef.body.centreOfGravity[2] = ((frontZ - rearZ) * carDef.frontWeightDistribution + rearZ) * 0.01f;
    }
    // 0x0048F080: every wheel's BrakeMu (+0x20).
    void setBrakeGrip(float brakeMu) {
        for (WheelDef& w : carDef.wheels) w.brakeMu = brakeMu;
    }

    // [3] 0x004F6F30: the time factor, the nitro, the ABS, the handbrake, the traction control, the automatic
    // gearbox, the body's step, the clutch.
    void step(float dt) override;

    // [2] 0x004F6590: also a tyre contact (at most 8, of the 64 of the pool); a tyre's touch already made is taken
    // again only when deeper. The wheel's +0x18 keeps the deepest compression, at most MaxCompression.
    void addTouch(const Touch& t) override;

    // [5] 0x004F5E60: also the engine's and the wheels' spins; with the clutch in and a gear the driven wheels turn
    // with the engine.
    void setState(std::vector<float>& s) override;

    // [6] 0x004F5F30
    void derivative(std::vector<float>& out, float dt) override;

    // [7] 0x004F6050: the tyres' values for their forces, the body's forces, then the engine's and the wheels' torques.
    void forces(float dt) override;

    // [9] 0x004F6A80: the engine held between the first and the last speed of its curve, the redline of the nitro.
    void constrain(std::vector<float>& s, float dt) override;

    // [10] 0x004F6730: the engine taken up and the clutch let in.
    void updateClutch(float dt);

    // [11] 0x004F6870: the automatic gearbox.
    void automaticGearbox(float dt);

    // 0x004F7710: another gear; the clutch opens again while the engine is on.
    void shift(uint32_t gear);

    // 0x004F7610: 0 reverse, 1 neutral, 2 .. 7 the forward gears (more is 7).
    void setGear(uint32_t gear);

    // 0x004F7550: the turbo's boost of the engine's speed, 0 .. 1 (the HUD's turbo dial).
    float turboBoost() const { return boost(); }

    // 0x00486E90 (the car's physics): at rest, unturned, in first gear, the engine on and still, no throttle, the
    // wheels (IDs 0 .. 3) still.
    void resetForStart();

    std::vector<CarWheel> wheels;           // +0x314
    float peakTorque = 0.0f;                // +0x324: TorqueCurveY's last x MaxTorque x 2.8
    float engineInertiaInverse = 0.0f;      // +0x328
    float drivenInertia = 0.0f;             // +0x32C: of the driven wheels
    float drivenInertiaInverse = 0.0f;      // +0x330
    size_t engineIndex = 0;                 // +0x334: the engine's spin in the state
    bool engineOn = false;                  // +0x338
    bool clutchIn = false;                  // +0x33C
    float clutch = 0.0f;                    // +0x340: 0 .. 1 while it is let in
    float throttle = 0.0f;                  // +0x344: the driver's
    float throttleOut = 0.0f;               // +0x348: after the traction control
    float brake = 0.0f;                     // +0x34C
    float handbrake = 0.0f;                 // +0x350
    float steering = 0.0f;                  // +0x354: -1 .. 1
    float engineSpeed = 0.0f;               // +0x358 (rad/s)
    float engineTorque = 0.0f;              // +0x35C
    float engineTorqueOut = 0.0f;           // +0x360
    bool automatic = false;                 // +0x364
    uint32_t gear = 0;                      // +0x368
    uint32_t previousGear = 0;              // +0x36C
    float ratio = 0.0f;                     // +0x370: of the gear
    float ratioInverse = 0.0f;              // +0x374
    bool nitroOn = false;                   // +0x378
    float nitroLeft = 0.0f;                 // +0x37C (s)
    float nitroRate = 1.0f;                 // +0x380
    bool absActive = false;                 // +0x384
    bool asrActive = false;                 // +0x388
    bool asrMode = true;                    // +0x38C
    size_t tyreFirst = 0;                   // +0x390: the step's tyre contacts in the pool
    size_t tyreCount = 0;                   // +0x394
    float shiftTimer = 0.0f;                // +0x398

    // The step's tyre contacts; those of the last step until the next is gathered (the pool may hold other bodies'
    // contacts there by then, as in the stock game).
    Contact& tyre(size_t i) { return pools->tyre[tyreFirst + i]; }

    CarWheel* wheelOf(int32_t id);  // 0x004F7500: the wheel of a definition ID, none without one

private:
    float m_fittedTurbo = 1.0f;     // the fixed build's: the definition's Turbo when the body was made
    void engineDrive();             // 0x004F5C60
    float boost() const;            // 0x004F7550
    void updatePeakTorque();        // 0x004F5B60
};

// 0x004975A0 for one car: the body put on its grid place (centimetres of the world) facing the heading, then let
// settle in neutral: steps of 0.02 s with the pools emptied, first `firstSteps` (75 for the first car of the game,
// 50 after) turning it back to the heading when a tyre first touches ground facing up, then 50 more. With a world,
// its grid is made anew (0x0053E8D0) before the car is placed. refill: the tank filled (+0x37C the definition's
// NitroTime; not in a replay's race).
void settleOnGrid(CarBody& car, ContactPools& pools, const Vec4& place, float heading, int& firstSteps,
                  class PhysicsWorld* world = nullptr, bool refill = true);

}  // namespace oj
