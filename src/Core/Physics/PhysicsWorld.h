#pragma once
// The physics of a race (the physics manager 0x00425D40 and its world 0x0053F110; docs/spec/16-physics.md, "The
// world"): the bodies stepped against the route's collision and each other with the frame's time smoothed and split.

#include "Core/Formats/Cld.h"
#include "Core/Physics/Contact.h"
#include "Core/Physics/RigidBody.h"
#include "Core/Physics/TrackCollision.h"

#include <array>
#include <vector>

namespace oj {

class PhysicsWorld {
public:
    explicit PhysicsWorld(const Cld& cld) : m_cld(cld), m_collision(cld) {}
    // 0x0053DEA0: a world without a route (the garage's test track scene): its bodies meet only its planes
    // (collision().planes) and each other; the grid's bounds stay unset, every body in the last cell.
    PhysicsWorld();

    // 0x0053E450: the body steps in this world, its contacts from the world's pools and collision; a primary body
    // (Primary) on the first list, any other on the second. With the grid made the body goes in it and is woken.
    // False when the body is in a world already.
    bool add(RigidBody& body);

    // 0x0053E500: the body out of the world: out of its cell, every body's near bodies forgotten, then a body not
    // primary off the active list and the second list, a primary one off the first list only (Stock problem: it
    // stays on the active list and steps; the body's active flag is kept either way, so a body not primary added
    // again never goes back on the active list).
    void remove(RigidBody& body);

    // 0x0053EDF0: a body woken goes on the active list and finds its near bodies at once; one put to sleep leaves it
    // (the last of the list takes its place). Nothing when it is so already.
    void activate(RigidBody& body, bool on);

    ContactPools& pools() { return m_pools; }
    TrackCollision& collision() { return m_collision; }  // [0x0075F0C8]

    // 0x00425D40: the frame's time smoothed (0x00425C00), then 1 step, 2 over 0.05 s, 3 over 0.1 s of it; before each
    // step the pools are emptied and the world steps (0x0053F110).
    void update(float frameTime);
    // 0x00425D40's steps alone, of the time smoothed last (the race's frame smooths the time every frame, paused or
    // not, and steps only while the race goes on).
    void stepFrame();
    // 0x00425E87: the steps of the time smoothed last ([0x0071593C]): 1, 2 over 0.05 s, 3 over 0.1 s; counted every
    // frame of the race, paused or not, before its steps.
    void countSteps();
    // 0x00426300 / 0x004261F0 (+0x78 0): the next 50 frames take their own time again.
    void restartSmoothing() { m_frames = 0; }

    // 0x00425C00: the time a frame steps: the mean of the last 16 frames (each within 0 .. 0.2 s) less a tenth of
    // the error kept so far, within 0 .. 0.2; the first 50 frames take their own time, at most 0.066 s.
    float smoothTime(float frameTime);

    // 0x0053F110: the primary bodies into the cells of the grid; the next of them in turn finds the bodies near it,
    // the next active body in turn goes into its cell and finds its near bodies; the primary bodies step, then the
    // active ones (a body woken in the step steps in it, one going to sleep lets the last of the list skip it); then
    // the nodes follow the bodies of the two lists.
    void step(float dt);

    // 0x0053E8D0: the grid over the route made (anew), every body placed in it; a body of either list not active
    // goes on the active list and finds its near bodies (the bodies of the second list, a primary one being active
    // from its making).
    void makeGrid();

    const std::vector<RigidBody*>& primary() const { return m_primary; }
    const std::vector<RigidBody*>& secondary() const { return m_secondary; }
    const std::vector<RigidBody*>& active() const { return m_active; }

    float stepTime() const { return m_time; }
    // The steps the last frame was split into ([0x0071593C]).
    unsigned steps() const { return m_steps; }

private:
    // A cell of the 16 x 16 grid over the route (+0x74, 8 bytes): the cells around it and the first body in it.
    struct Cell {
        uint8_t x0 = 0, z0 = 0, x1 = 0, z1 = 0;
        RigidBody* first = nullptr;
    };

    void placeInGrid(RigidBody& b);  // 0x0053E7E0 / 0x0053E770
    void findNear(RigidBody& b);     // 0x0053EE90

    const Cld& m_cld;
    ContactPools m_pools;
    TrackCollision m_collision;
    std::vector<RigidBody*> m_primary;    // +0x44
    std::vector<RigidBody*> m_secondary;  // +0x54
    std::vector<RigidBody*> m_active;     // +0x64
    std::array<float, 16> m_times{};   // +0x2C
    size_t m_next = 0;                 // +0x6C
    float m_error = 0.0f;              // +0x70
    float m_time = 0.0f;               // +0x74
    unsigned m_steps = 1;
    uint32_t m_frames = 0;             // +0x78
    std::array<Cell, 256> m_cells{};   // +0x74
    float m_minX = 0.0f, m_minZ = 0.0f, m_maxX = 0.0f, m_maxZ = 0.0f;  // +0x874 .. +0x880
    bool m_gridMade = false;           // +0x910
};

}  // namespace oj
