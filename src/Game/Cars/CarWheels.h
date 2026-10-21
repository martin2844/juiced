#pragma once
// The wheels' looks of a car in a world (the vehicle's +0x1A0, 0x9C bytes each; docs/spec/18-racers.md): the race's
// cars and the garage's test track scene's.

#include "Core/Physics/CarBody.h"
#include "Core/Base/Matrix.h"
#include "Game/Cars/CarModel.h"

#include <array>

namespace oj {

struct WheelPose {
    render::ModelNode* node = nullptr;  // +0x00: WHEEL_FL, WHEEL_FR, WHEEL_RL, WHEEL_RR
    Vec4 base{};                        // +0x04: the slot's place (its local matrix's x, y and z)
    Vec4 last{};                        // +0x14: the node's place in the world last frame
    Vec4 moved{};                       // +0x24: since then
    float rest = 0.0f;                  // +0x34: less the tyre's sag at rest (centimetres; 0x00485D20)
    float speed = 0.0f;                 // +0x38
    float travel = 0.0f;                // +0x3C: the suspension's travel (centimetres), eased
    float steer = 0.0f;                 // +0x40: eased
    float spin = 0.0f;                  // +0x44: the turn of the wheel, within ±pi
    bool right = false;                 // +0x48: FR and RR face the other way
    std::array<float, 3> scale{};       // +0x4C .. +0x54: the local matrix's diagonal
    bool blurred = false;               // +0x5C: its blurred mesh shown
};

// 0x00485270 for WHEEL_FL, WHEEL_FR (right), WHEEL_RL and WHEEL_RR (right): each pose takes its slot's node, place and
// scale and the node's place in the world. Then 0x00485D20: each wheel's ray at its node across the car, its rest sag
// kept by the pose (+0x1D4).
void makeWheelPoses(std::array<WheelPose, 4>& wheels, scene::CarModel& model, CarBody& body);

// 0x00487670: each wheel's node: its place in the world against the last frame's (the speed over the step); the turn
// by the wheel's spin (a front wheel whose steering pulls, the car's +0x64 with its side +0x4F8, turns at 0.05 of it),
// held within ±pi; past 45 rad/s the blurred mesh, under it the wheel's mesh again (the wheel that pulls keeps its
// mesh; on the stand, +0x19C, only driven wheels blur and the others do not turn); the travel eased toward the tyre's
// compression x 100 by 150 x dt at most; the steering toward its (3 - 2|s|) s (the wheel that pulls 0.5 rad off) x the
// wheel's MaxSteerAngle by 4 x dt at most; the node's matrix its scale at its place raised by the travel, turned about
// y by -steer (pi - steer on the right) and about x by the turn (its opposite on the right). speeds: each wheel's speed
// kept by the car (its +0x1D8), may be null.
void wheelsFrame(std::array<WheelPose, 4>& wheels, CarBody& body, scene::CarModel& model, bool pulls, int32_t side,
                 bool onStand, float dt, std::array<float, 4>* speeds);

}  // namespace oj
