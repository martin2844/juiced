#pragma once
// A player's control of a race car (0x0041E200; docs/spec/16-physics.md, "The driver's control"): the DRIVER1
// actions of the controls file made into the car's throttle, brake, steering, handbrake and gears.

#include "Core/Physics/CarBody.h"
#include "Core/Data/Controls.h"
#include "Core/Input/Input.h"

#include <array>
#include <vector>

namespace oj {

// The DRIVER1 actions of a frame: the analogue ones 0 .. 1 (steering -1 .. 1), the one-shot ones true on the frame
// their key goes down.
struct DriverActions {
    float steering = 0.0f;
    float throttle = 0.0f;
    float brake = 0.0f;
    float handbrake = 0.0f;
    float reverse = 0.0f;
    bool gearUp = false;
    bool gearDown = false;
    bool rearView = false;    // held
    bool changeView = false;  // CHANGE_CAMERA went down
    bool select = false;      // SELECT went down (RETURN, OneShot; the controller's +0x50)
    bool nextCar = false;     // NEXTCAR went down (Page Down, OneShot; +0x70)
    float cameraPan = 0.0f;   // DRIVER_CAMERA_PAN (N -1, M +1; the controller's +0xE8)
    bool pause = false;       // PAUSE held (DIGITAL, not one-shot)
    bool nitro = false;       // NITRO held (the controller's +0x68)
    float horn = 0.0f;        // DRIVER_HORN (H; the controller's +0x6C)
    bool reset = false;       // RESET held (+0x64; not in the PC's DRIVER1 map, its lines there commented out)
    // DRIVER_TACTICS_UP, _RIGHT, _DOWN, _LEFT, DRIVER_TACTICS3_UP, _DOWN went down (W, S, Q, A, X, Z): the crew's
    // first, second and third driver up and down.
    std::array<bool, 6> tactics{};
};

// The DRIVER1 actions of a race player's controller (its +0x08; 0x004080D0's slots, read by 0x004015D0 and
// 0x00401640): -1 slots (another map registered) and slots past the device's actions read 0. player: the input
// manager's player whose device's controller it is.
class DriverKeys {
public:
    DriverActions update(const input::Input& device, int player = 0);
};

class DriverControl {
public:
    // The profile's settings of the player: the automatic gearbox (settings +0x30) and the steering sensitivity
    // (+0x70, 0 .. 1).
    bool automaticGearbox = true;
    float steeringSensitivity = 0.5f;
    // The steering wheel's settings: the wheel on ([G+0x64]+0x78, STEERING WHEEL of opts_wheel_dx9; the game's), not
    // speed sensitive (+0xAC of the player), the registry's MultiTurn ([0x0075F78C]).
    bool wheel = false;
    bool notSpeedSensitive = false;
    bool multiTurn = false;

    // 0x0041E200: the car's controls from the actions, dt the race's step time, count the race's count before the
    // start (from 10 up the actions are not read; above 0 the handbrake holds the car).
    void apply(CarBody& car, const DriverActions& a, float dt, float count = 0.0f);
    // 0x0041E3F9 .. 0x0041E68D: the wheel's turn (+0x2C) toward the steering's target at the frame's step: with the
    // steering wheel the target by MultiTurn ((sensitivity + 1) x the input), by not speed sensitive (the input
    // shaped by |s| to the power 1.65 - 1.65 x sensitivity + 0.8, no lessening at speed) or as without one with that
    // power; without it the input shaped by the power 1.1 - 1.1 x sensitivity + 0.07 (1.00: 0.05) and lessened at speed
    // (from 1 to 0.25 at 140 mph). It is reached at the steering speed (x 3 coming back; 3000 with the wheel). Returns
    // the turn.
    float steerToward(float input, float speed, float dt);

    float steer = 0.0f;                // +0x2C: the wheel's turn
    bool shiftedUp = false;            // +0x30: a gear up by hand this frame
    float sensitivity = 0.83f;         // +0x4C "CAR SPECIFIC SENSITIVITY"
    float speedSensitivity = 0.0017f;  // +0x50 "SPEED DEPENDANT SENSITIVITY" (a wheel's)
    float steerSpeed = 0.61f;          // +0x54: the turn per second towards the input

    // 0x0041FFB0, as the controller is made: its tweaks (+0x3C, 0x4C bytes each: a name of 64, the value's address, its
    // lowest and highest), read by nothing found.
    struct Tweak {
        const char* name;
        float DriverControl::*value;
        float lowest, highest;
    };
    static constexpr Tweak kTweaks[] = {
        {"CAR SPECIFIC SENSITIVITY", &DriverControl::sensitivity, 0.6f, 1.0f},
        {"SPEED DEPENDANT SENSITIVITY", &DriverControl::speedSensitivity, 0.001f, 0.003f},
        {"STEERING SPEED", &DriverControl::steerSpeed, 0.5f, 1.5f},
    };
};

}  // namespace oj
