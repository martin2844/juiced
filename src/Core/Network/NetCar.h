#pragma once
// A race car's state as the network's session passes it (0x40 bytes, the 0x14 message's; docs/spec/28-network.md,
// "The car's state"): made of our car (0x004E7740), applied to another machine's car at once (0x004E73E0) or by
// steps towards it (0x004E6E30), and carried on by its own velocities (0x004E6B60).

#include <cstdint>

namespace oj {

class PhysicsWorld;
struct RaceCar;

namespace net {

// 0x004E7740: the state of our car: the body's position and orientation as they are, its velocities in its frame,
// the wheels' (IDs 0 .. 3) and the engine's spins, the driver's pedals and steering, the race's distance, the gear,
// the engine on, the race paused (`paused`, [G+0x34]+0x20), the nitro asked (+0x190) and the horn sounding (`horn`,
// +0x4C0's +0x48). Bit 4 of [0x3D] is cleared (the session sets it) and [0x3C] is not written.
void carState(const RaceCar& car, bool paused, bool horn, uint8_t* state);

// 0x004E73E0: the state taken as it is: the position (unless the car is parked), the orientation, the velocities,
// the spins, the pedals (the throttle squared, the brake cubed: Stock problem, the sender's were sent as they are),
// the steering, the engine on and the gear; the race paused takes the body out of the world (else into it), the
// nitro and the horn (its frequency 1300 first; `horn` the game's, 0x00497E8E's).
void applyState(RaceCar& car, const uint8_t* state, PhysicsWorld* world, void (*horn)(RaceCar&, bool));

// 0x004E6E30: towards the state: the position by 0.07 (y by 0.21), at once past 30 m (y 3 m), the orientation by
// 0.07 of the way, the velocities and the wheels' spins by 0.07; the rest as 0x004E73E0 (the position whatever
// the parking).
void smoothState(RaceCar& car, const uint8_t* state, PhysicsWorld* world, void (*horn)(RaceCar&, bool));

// 0x004E6B60: the state carried on for dt seconds: the position by its velocity (turned to the world), the
// orientation by its angular velocity.
void extrapolate(uint8_t* state, float dt);

}  // namespace net
}  // namespace oj
